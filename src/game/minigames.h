// Turn-based minigames (slow e-ink friendly). Pure logic, unit-tested; the
// screens live in ui/render.cpp and main.cpp. Rewards: pet::playReward().
#pragma once
#include <stdint.h>

#include "rules.h"

namespace games {

// --- Izquierda o derecha: guess which way Grogu will look ------------------
enum Dir : uint8_t { Left, Right };

struct LeftRight {
  uint32_t rng;
  uint8_t round;    // rounds played so far
  uint8_t correct;
  uint8_t lastLook; // Dir of the last round (valid when round > 0)
  bool lastHit;
  uint8_t history;  // bit i set if round i was guessed right
};

void start(LeftRight& g, uint32_t seed);
// Play one round; returns true if Grogu looked the guessed way.
bool guess(LeftRight& g, Dir d);
bool done(const LeftRight& g);

// --- Fuerza: repeat a growing sequence of dial moves ------------------------
enum Sym : uint8_t { Up, Down, Press };

enum class Step : uint8_t {
  Wrong,     // game over; score() is the longest sequence repeated
  Next,      // right so far, keep going
  Complete,  // whole sequence repeated; a longer one is ready to show
  Finished,  // repeated the longest possible sequence
};

struct ForceSeq {
  uint32_t rng;
  uint8_t len;  // current sequence length
  uint8_t pos;  // symbols entered so far
  uint8_t best; // longest sequence repeated
  bool over;
  uint8_t seq[rules::kForceMaxLen];
};

void start(ForceSeq& g, uint32_t seed);
Step input(ForceSeq& g, Sym s);

// --- Mayor o menor: will the next number be higher or lower? ----------------
struct HighLow {
  uint32_t rng;
  uint8_t shown;   // number on screen, 1..rules::kHighLowTop
  uint8_t prev;    // number before the last guess (valid when round > 0)
  uint8_t round;   // guesses made
  uint8_t streak;  // right guesses in a row; the score
  bool lastHit;
  bool over;       // a wrong guess, or a perfect streak
};

void start(HighLow& g, uint32_t seed);
// Play one round; returns true if the next number went the guessed way.
bool guess(HighLow& g, bool higher);
bool done(const HighLow& g);

// --- Ordenar: sort the piles of folded clothes, smallest on the left --------
struct Tidy {
  uint32_t rng;
  uint8_t piles[rules::kTidyPiles];  // pile sizes 1..kTidyPiles
  uint8_t cursor;
  int8_t held;       // pile picked up, -1 when none
  uint8_t swaps;
  uint8_t minSwaps;  // fewest swaps that sort the shuffle
};

void start(Tidy& g, uint32_t seed);
// Dial: move the cursor one pile left (-1) or right (+1), wrapping around.
void move(Tidy& g, int dir);
// Press: pick up the pile under the cursor, or swap it with the one held.
// Dropping it back where it was just puts it down.
void press(Tidy& g);
void putDown(Tidy& g);
bool done(const Tidy& g);
uint8_t stars(const Tidy& g);  // 3 perfect, 2 close, 1 sorted at last

// --- ¿Alegría?: keep what sparks joy, thank and let go of the rest ----------
struct Joy {
  uint32_t rng;
  uint8_t favs;      // bit per pool thing that sparks joy
  uint8_t item;      // thing on screen now, 0..rules::kJoyPool-1
  uint8_t lastItem;  // thing judged in the last round (valid when round > 0)
  uint8_t round;
  uint8_t correct;
  bool lastHit;
  uint8_t history;   // bit i set if round i was right
};

void start(Joy& g, uint32_t seed);
// Keep (true) or let go (false) the thing on screen; true if it was the right call.
bool guess(Joy& g, bool keep);
bool done(const Joy& g);
bool sparksJoy(const Joy& g, uint8_t item);

}  // namespace games
