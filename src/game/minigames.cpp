#include "minigames.h"

#include <string.h>

namespace games {

namespace {

uint32_t nextRandom(uint32_t& rng) {
  // xorshift32, same as pet.cpp
  uint32_t x = rng ? rng : 0x9E3779B9u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng = x;
  return x;
}

void fillSequence(ForceSeq& g) {
  for (uint8_t i = 0; i < g.len; ++i) g.seq[i] = static_cast<uint8_t>(nextRandom(g.rng) % 3);
  g.pos = 0;
}

// Fewest swaps to sort a permutation of 1..n: n minus its number of cycles.
uint8_t minSwapsToSort(const uint8_t* p, uint8_t n) {
  bool seen[rules::kTidyPiles] = {};
  uint8_t cycles = 0;
  for (uint8_t i = 0; i < n; ++i) {
    if (seen[i]) continue;
    cycles++;
    for (uint8_t j = i; !seen[j]; j = p[j] - 1) seen[j] = true;
  }
  return n - cycles;
}

uint8_t drawJoyItem(Joy& g) {
  // Half the time something that sparks joy, so both answers come up often.
  const bool fav = nextRandom(g.rng) >> 9 & 1;
  for (;;) {
    const uint8_t it = static_cast<uint8_t>(nextRandom(g.rng) % rules::kJoyPool);
    if (sparksJoy(g, it) == fav) return it;
  }
}

}  // namespace

void start(LeftRight& g, uint32_t seed) {
  memset(&g, 0, sizeof(g));
  g.rng = seed;
}

bool guess(LeftRight& g, Dir d) {
  if (done(g)) return false;
  g.lastLook = static_cast<uint8_t>(nextRandom(g.rng) >> 7 & 1);
  g.lastHit = g.lastLook == d;
  if (g.lastHit) {
    g.correct++;
    g.history |= 1u << g.round;
  }
  g.round++;
  return g.lastHit;
}

bool done(const LeftRight& g) { return g.round >= rules::kLeftRightRounds; }

void start(ForceSeq& g, uint32_t seed) {
  memset(&g, 0, sizeof(g));
  g.rng = seed;
  g.len = rules::kForceStartLen;
  fillSequence(g);
}

Step input(ForceSeq& g, Sym s) {
  if (g.over) return Step::Wrong;
  if (g.seq[g.pos] != s) {
    g.over = true;
    return Step::Wrong;
  }
  if (++g.pos < g.len) return Step::Next;
  g.best = g.len;
  if (g.len >= rules::kForceMaxLen) {
    g.over = true;
    return Step::Finished;
  }
  g.len++;
  fillSequence(g);
  return Step::Complete;
}

void start(HighLow& g, uint32_t seed) {
  memset(&g, 0, sizeof(g));
  g.rng = seed;
  g.shown = static_cast<uint8_t>(1 + nextRandom(g.rng) % rules::kHighLowTop);
}

bool guess(HighLow& g, bool higher) {
  if (done(g)) return false;
  // Never the same number twice, so every guess is either right or wrong.
  uint8_t n = static_cast<uint8_t>(1 + nextRandom(g.rng) % (rules::kHighLowTop - 1));
  if (n >= g.shown) n++;
  g.prev = g.shown;
  g.shown = n;
  g.lastHit = higher == (n > g.prev);
  g.round++;
  if (g.lastHit) g.streak++;
  g.over = !g.lastHit || g.streak >= rules::kHighLowRounds;
  return g.lastHit;
}

bool done(const HighLow& g) { return g.over; }

void start(Tidy& g, uint32_t seed) {
  memset(&g, 0, sizeof(g));
  g.rng = seed;
  g.held = -1;
  for (uint8_t i = 0; i < rules::kTidyPiles; ++i) g.piles[i] = i + 1;
  do {
    for (uint8_t i = rules::kTidyPiles - 1; i > 0; --i) {
      const uint8_t j = static_cast<uint8_t>(nextRandom(g.rng) % (i + 1));
      const uint8_t t = g.piles[i];
      g.piles[i] = g.piles[j];
      g.piles[j] = t;
    }
    g.minSwaps = minSwapsToSort(g.piles, rules::kTidyPiles);
  } while (g.minSwaps < rules::kTidyMinSwaps);
}

void move(Tidy& g, int dir) {
  g.cursor = static_cast<uint8_t>((g.cursor + rules::kTidyPiles + dir) % rules::kTidyPiles);
}

void press(Tidy& g) {
  if (done(g)) return;
  if (g.held < 0) {
    g.held = static_cast<int8_t>(g.cursor);
    return;
  }
  if (g.held != g.cursor) {
    const uint8_t t = g.piles[g.held];
    g.piles[g.held] = g.piles[g.cursor];
    g.piles[g.cursor] = t;
    if (g.swaps < 255) g.swaps++;
  }
  g.held = -1;
}

void putDown(Tidy& g) { g.held = -1; }

bool done(const Tidy& g) {
  for (uint8_t i = 0; i < rules::kTidyPiles; ++i) {
    if (g.piles[i] != i + 1) return false;
  }
  return true;
}

uint8_t stars(const Tidy& g) {
  if (!done(g)) return 0;
  if (g.swaps <= g.minSwaps) return 3;
  return g.swaps <= g.minSwaps + rules::kTidySlack ? 2 : 1;
}

void start(Joy& g, uint32_t seed) {
  memset(&g, 0, sizeof(g));
  g.rng = seed;
  while (__builtin_popcount(g.favs) < rules::kJoyFavs) {
    g.favs |= 1u << (nextRandom(g.rng) % rules::kJoyPool);
  }
  g.item = drawJoyItem(g);
}

bool sparksJoy(const Joy& g, uint8_t item) { return g.favs >> item & 1; }

bool guess(Joy& g, bool keep) {
  if (done(g)) return false;
  g.lastItem = g.item;
  g.lastHit = keep == sparksJoy(g, g.item);
  if (g.lastHit) {
    g.correct++;
    g.history |= 1u << g.round;
  }
  g.round++;
  g.item = drawJoyItem(g);
  return g.lastHit;
}

bool done(const Joy& g) { return g.round >= rules::kJoyRounds; }

}  // namespace games
