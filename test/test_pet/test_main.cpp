// Game-logic unit tests. Run with: pio test -e test
#include <string.h>
#include <unity.h>

#include "../../src/game/minigames.h"
#include "../../src/game/pet.h"
#include "../../src/game/rules.h"
#include "../../src/hal/timeutil.h"
#include "../../src/ui/sprites.h"

using namespace pet;

namespace {

// 2026-10-21 10:00 local - daytime, so the pet is awake.
const uint32_t kStart = timeutil::toEpoch(2026, 10, 21, 10, 0, 0);

State hatched(uint32_t& now) {
  State s;
  newPet(s, now, 42);
  now += rules::kEggMinutes * 60;
  tick(s, now);
  return s;
}

// A diligent owner checking in every 15 minutes.
void care(State& s) {
  medicine(s);
  clean(s);
  if (s.tantrum) scold(s);
  if (s.asleep) {
    s.lightsOff = true;
    return;
  }
  for (int i = 0; i < 3 && feedMeal(s) == Result::Ok; ++i) {
  }
  if (s.happiness < 60) {
    if (s.energy >= 30) playReward(s, Game::LeftRight, rules::kLeftRightWin);
    else feedSnack(s);
  }
}

void liveWithCare(State& s, uint32_t& now, uint32_t minutes) {
  for (uint32_t m = 0; m < minutes && isAlive(s); m += 15) {
    now += 15 * 60;
    tick(s, now);
    care(s);
  }
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_egg_hatches() {
  State s;
  newPet(s, kStart, 1);
  tick(s, kStart + (rules::kEggMinutes - 1) * 60);
  TEST_ASSERT_EQUAL(Stage::Egg, s.stage);
  tick(s, kStart + rules::kEggMinutes * 60);
  TEST_ASSERT_EQUAL(Stage::Baby, s.stage);
}

void test_needs_decay_while_awake() {
  uint32_t now = kStart;
  State s = hatched(now);
  const float before = s.fullness;
  now += 3600;
  tick(s, now);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, before - rules::kFullnessDecay[1], s.fullness);
}

void test_sleep_schedule() {
  uint32_t now = kStart;
  State s = hatched(now);
  liveWithCare(s, now, 50);  // grow up a bit so evolution doesn't interfere
  const uint32_t night = timeutil::toEpoch(2026, 10, 21, 22, 0, 0);
  liveWithCare(s, now, (night - now) / 60);
  TEST_ASSERT_TRUE(s.asleep);
  TEST_ASSERT_EQUAL(Result::Refused, feedMeal(s));
  const uint32_t morning = timeutil::toEpoch(2026, 10, 22, 8, 30, 0);
  liveWithCare(s, now, (morning - now) / 60);
  TEST_ASSERT_FALSE(s.asleep);
}

void test_lights_on_at_night_is_a_mistake() {
  uint32_t now = kStart;
  State s = hatched(now);
  liveWithCare(s, now, 70);
  const uint8_t mistakes = s.careMistakes;
  // Jump to bedtime with a full belly and leave the lights on.
  s.fullness = s.happiness = s.hygiene = 100;
  now = timeutil::toEpoch(2026, 10, 21, 21, 0, 0);
  s.lastUpdate = now;
  now += 60 * 60;
  tick(s, now);
  TEST_ASSERT_TRUE(s.asleep);
  TEST_ASSERT_EQUAL_UINT8(mistakes + 1, s.careMistakes);
}

void test_nap_restores_energy() {
  uint32_t now = kStart;
  State s = hatched(now);
  s.fullness = s.happiness = s.hygiene = 100;
  s.energy = 5;
  TEST_ASSERT_EQUAL(Result::Refused, canPlay(s));
  toggleLights(s);
  TEST_ASSERT_TRUE(s.napping);
  TEST_ASSERT_TRUE(s.asleep);
  TEST_ASSERT_EQUAL(Result::Refused, feedMeal(s));
  TEST_ASSERT_EQUAL_UINT32(5, napMinutesLeft(s));
  now += 4 * 60;
  tick(s, now);
  TEST_ASSERT_TRUE(s.napping);  // still dozing
  now += 1 * 60;  // a full nap is 5 minutes
  tick(s, now);
  TEST_ASSERT_FALSE(s.asleep);
  TEST_ASSERT_FALSE(s.napping);
  TEST_ASSERT_FALSE(s.lightsOff);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 100, s.energy);
  TEST_ASSERT_EQUAL(Result::Ok, canPlay(s));
}

void test_no_nap_when_rested_and_lights_on_wakes() {
  uint32_t now = kStart;
  State s = hatched(now);
  s.energy = 90;
  toggleLights(s);
  TEST_ASSERT_FALSE(s.asleep);
  toggleLights(s);
  s.energy = 20;
  toggleLights(s);
  TEST_ASSERT_TRUE(s.napping);
  toggleLights(s);  // wake it early
  TEST_ASSERT_FALSE(s.asleep);
  TEST_ASSERT_FALSE(s.napping);
  now += 30 * 60;
  tick(s, now);
  TEST_ASSERT_FALSE(s.asleep);
}

void test_nap_into_night_becomes_sleep() {
  uint32_t now = timeutil::toEpoch(2026, 10, 21, 20, 52, 0);  // hatches 20:57, naps past 21:00
  State s = hatched(now);
  s.fullness = s.happiness = s.hygiene = 100;
  s.energy = 0;
  toggleLights(s);
  TEST_ASSERT_TRUE(s.napping);
  now = timeutil::toEpoch(2026, 10, 22, 2, 0, 0);
  tick(s, now);
  TEST_ASSERT_TRUE(s.asleep);
  TEST_ASSERT_FALSE(s.napping);
  TEST_ASSERT_TRUE(s.lightsOff);
}

void test_wake_at_night() {
  uint32_t now = kStart;
  State s = hatched(now);
  liveWithCare(s, now, 70);
  s.fullness = s.happiness = s.hygiene = s.energy = 100;
  now = timeutil::toEpoch(2026, 10, 21, 22, 0, 0);
  s.lastUpdate = now;
  now += 60;
  tick(s, now);
  TEST_ASSERT_TRUE(s.asleep);
  toggleLights(s);  // lights off: still asleep
  TEST_ASSERT_TRUE(s.asleep);
  const float joy = s.happiness;
  toggleLights(s);  // lights on: wakes up, a bit grumpy
  TEST_ASSERT_FALSE(s.asleep);
  TEST_ASSERT_TRUE(s.nightAwake);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, joy - rules::kNightWakeHappiness, s.happiness);
  s.fullness = 50;
  TEST_ASSERT_EQUAL(Result::Ok, feedMeal(s));
  TEST_ASSERT_EQUAL(Result::Ok, canPlay(s));
  now += 10 * 60;
  tick(s, now);
  TEST_ASSERT_FALSE(s.asleep);  // stays up for a while
  toggleLights(s);              // lights off: back to bed
  TEST_ASSERT_TRUE(s.asleep);
  TEST_ASSERT_FALSE(s.nightAwake);

  // Left up with the lights on, it dozes off again by itself.
  toggleLights(s);
  TEST_ASSERT_TRUE(s.nightAwake);
  const uint8_t mistakes = s.careMistakes;
  now += rules::kNightWakeMaxMin * 60;
  tick(s, now);
  TEST_ASSERT_TRUE(s.asleep);
  TEST_ASSERT_FALSE(s.nightAwake);
  TEST_ASSERT_FALSE(s.lightsOff);
  TEST_ASSERT_EQUAL_UINT8(mistakes, s.careMistakes);  // grace period before the lights count
  now += rules::kLightsOnMistakeMin * 60;
  tick(s, now);
  TEST_ASSERT_EQUAL_UINT8(mistakes + 1, s.careMistakes);
}

void test_night_wake_ends_in_the_morning() {
  uint32_t now = timeutil::toEpoch(2026, 10, 22, 7, 30, 0);
  State s = hatched(now);
  s.fullness = s.happiness = s.hygiene = 100;
  now += 60;  // the hatching minute doesn't run the sleep schedule
  tick(s, now);
  TEST_ASSERT_TRUE(s.asleep);
  s.lightsOff = true;
  toggleLights(s);
  TEST_ASSERT_TRUE(s.nightAwake);
  now = timeutil::toEpoch(2026, 10, 22, 10, 0, 0);
  tick(s, now);
  TEST_ASSERT_FALSE(s.asleep);
  TEST_ASSERT_FALSE(s.nightAwake);
}

void test_feed_full_not_needed() {
  uint32_t now = kStart;
  State s = hatched(now);
  s.fullness = 99;
  TEST_ASSERT_EQUAL(Result::NotNeeded, feedMeal(s));
  s.fullness = 10;
  TEST_ASSERT_EQUAL(Result::Ok, feedMeal(s));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10 + rules::kMealFullness, s.fullness);
}

void test_poop_and_clean() {
  uint32_t now = kStart;
  State s = hatched(now);
  now += rules::kPoopIntervalMin[1] * 60;
  tick(s, now);
  TEST_ASSERT_EQUAL_UINT8(1, s.poop);
  TEST_ASSERT_EQUAL(Result::Ok, clean(s));
  TEST_ASSERT_EQUAL_UINT8(0, s.poop);
  TEST_ASSERT_EQUAL(Result::NotNeeded, clean(s));
}

void test_starvation_kills() {
  uint32_t now = kStart;
  State s = hatched(now);
  now += 3 * 24 * 3600;  // nobody home for 3 days
  tick(s, now);
  TEST_ASSERT_EQUAL(Stage::Dead, s.stage);
  TEST_ASSERT_TRUE(s.cause != Cause::None);
  TEST_ASSERT_EQUAL(Mood::Dead, mood(s));
}

void test_good_care_makes_good_adult() {
  uint32_t now = kStart;
  State s = hatched(now);
  liveWithCare(s, now, (rules::kBabyMinutes + rules::kChildMinutes + rules::kTeenMinutes) + 60);
  TEST_ASSERT_TRUE_MESSAGE(isAlive(s), "pet died despite good care");
  TEST_ASSERT_LESS_OR_EQUAL_UINT8(rules::kMaxMistakesForGoodAdult, s.careMistakes);
  TEST_ASSERT_TRUE(s.stage == Stage::AdultMando || s.stage == Stage::AdultGloton);
}

void test_many_mistakes_make_bad_adult() {
  uint32_t now = kStart;
  State s = hatched(now);
  liveWithCare(s, now, rules::kBabyMinutes + rules::kChildMinutes + 30);
  TEST_ASSERT_EQUAL(Stage::Teen, s.stage);
  s.careMistakes = rules::kMaxMistakesForGoodAdult + 5;
  liveWithCare(s, now, rules::kTeenMinutes);
  TEST_ASSERT_EQUAL(Stage::AdultGrump, s.stage);
}

// A teen one minute away from growing up, with clean counters.
State almostAdult(uint32_t& now) {
  State s = hatched(now);
  s.stage = Stage::Teen;
  s.stageMinutes = rules::kTeenMinutes - 1;
  s.careMistakes = 0;
  s.weight = rules::kStartWeight;
  s.snacksEaten = 0;
  s.forceWins = 0;
  s.discipline = 0;
  s.fullness = s.happiness = s.hygiene = s.health = s.energy = 100;
  return s;
}

void test_evolution_branches() {
  uint32_t now = kStart;
  State s = almostAdult(now);
  stepMinute(s, now);
  TEST_ASSERT_EQUAL(Stage::AdultMando, s.stage);

  s = almostAdult(now);
  s.forceWins = rules::kJediForceWins;
  s.discipline = rules::kJediDiscipline;
  stepMinute(s, now);
  TEST_ASSERT_EQUAL(Stage::AdultJedi, s.stage);

  s = almostAdult(now);
  s.forceWins = rules::kJediForceWins;  // trained but undisciplined: not a Jedi
  s.snacksEaten = rules::kGlotonSnacks;
  stepMinute(s, now);
  TEST_ASSERT_EQUAL(Stage::AdultGloton, s.stage);

  s = almostAdult(now);
  s.forceWins = rules::kJediForceWins;
  s.discipline = 100;
  s.careMistakes = rules::kMaxMistakesForGoodAdult + 1;  // mistakes win over everything
  stepMinute(s, now);
  TEST_ASSERT_EQUAL(Stage::AdultGrump, s.stage);
}

void test_adult_dies_of_old_age() {
  uint32_t now = kStart;
  State s = almostAdult(now);
  s.stage = Stage::AdultGloton;
  s.stageMinutes = rules::kAdultLifeDays[2] * 24 * 60 - 1;
  stepMinute(s, now);
  TEST_ASSERT_EQUAL(Stage::Dead, s.stage);
  TEST_ASSERT_EQUAL(Cause::OldAge, s.cause);
  TEST_ASSERT_EQUAL(Stage::AdultGloton, s.lastStage);
}

void test_shop() {
  uint32_t now = kStart;
  State s = hatched(now);
  s.frogs = 2;
  TEST_ASSERT_EQUAL(Result::Refused, buy(s, Item::Macaron));  // costs 3
  TEST_ASSERT_EQUAL_UINT16(2, s.frogs);
  s.frogs = 10;
  TEST_ASSERT_EQUAL(Result::Ok, buy(s, Item::Macaron));
  TEST_ASSERT_EQUAL_UINT16(10 - rules::kItemPrice[0], s.frogs);
  TEST_ASSERT_EQUAL_UINT8(1, s.items[static_cast<int>(Item::Macaron)]);
  TEST_ASSERT_EQUAL(Result::Ok, buy(s, Item::Knob));
  TEST_ASSERT_EQUAL(Result::NotNeeded, buy(s, Item::Knob));  // only one knob
  TEST_ASSERT_EQUAL(Result::Refused, useItem(s, Item::Knob));
  TEST_ASSERT_EQUAL(Result::Refused, useItem(s, Item::Frog));  // none owned
}

void test_items() {
  uint32_t now = kStart;
  State s = hatched(now);
  for (int i = 0; i < kItemCount; ++i) s.items[i] = 1;

  s.happiness = 10;
  const uint8_t w = s.weight;
  TEST_ASSERT_EQUAL(Result::Ok, useItem(s, Item::Macaron));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10 + rules::kMacaronHappiness, s.happiness);
  TEST_ASSERT_EQUAL_UINT8(w + rules::kMacaronWeight, s.weight);
  TEST_ASSERT_EQUAL_UINT16(1, s.snacksEaten);
  TEST_ASSERT_EQUAL(Result::Refused, useItem(s, Item::Macaron));  // used up

  s.fullness = 99;
  TEST_ASSERT_EQUAL(Result::NotNeeded, useItem(s, Item::EggSoup));
  TEST_ASSERT_EQUAL_UINT8(1, s.items[static_cast<int>(Item::EggSoup)]);  // not consumed
  s.fullness = 20;
  TEST_ASSERT_EQUAL(Result::Ok, useItem(s, Item::EggSoup));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 20 + rules::kEggSoupFullness, s.fullness);

  s.fullness = 20;
  s.happiness = 20;
  TEST_ASSERT_EQUAL(Result::Ok, useItem(s, Item::Frog));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 20 + rules::kFrogFullness, s.fullness);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 20 + rules::kFrogHappiness, s.happiness);

  s.sick = true;
  s.health = 40;
  TEST_ASSERT_EQUAL(Result::Ok, useItem(s, Item::Bacta));
  TEST_ASSERT_FALSE(s.sick);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40 + rules::kBactaHealth, s.health);
}

void test_scold_and_tantrum() {
  uint32_t now = kStart;
  State s = hatched(now);
  s.happiness = 50;
  TEST_ASSERT_EQUAL(Result::NotNeeded, scold(s));  // unfair
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50 - rules::kUnfairScoldHappiness, s.happiness);

  s.tantrum = true;
  TEST_ASSERT_TRUE(needsAttention(s));
  const float d = s.discipline;
  TEST_ASSERT_EQUAL(Result::Ok, scold(s));
  TEST_ASSERT_FALSE(s.tantrum);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, d + rules::kScoldDiscipline, s.discipline);

  // Ignored tantrum becomes a care mistake.
  s.tantrum = true;
  s.tantrumMinutes = 0;
  s.fullness = s.happiness = s.hygiene = 100;
  const uint8_t m = s.careMistakes;
  for (uint32_t i = 0; i < rules::kTantrumMistakeMin; ++i) stepMinute(s, now + i * 60);
  TEST_ASSERT_FALSE(s.tantrum);
  TEST_ASSERT_EQUAL_UINT8(m + 1, s.careMistakes);
}

void test_tantrums_and_events_happen() {
  // Over six days of good care both should show up, deterministically.
  uint32_t now = kStart;
  State a = hatched(now);
  uint32_t nowB = now;
  State b = a;
  int tantrums = 0, events = 0;
  for (int day = 0; day < 6 && isAlive(a); ++day) {
    for (int q = 0; q < 24 * 4; ++q) {
      now += 15 * 60;
      tick(a, now);
      if (a.tantrum) tantrums++;
      if (takeEvent(a) != Event::None) events++;
      care(a);
    }
  }
  TEST_ASSERT_TRUE(tantrums > 0);
  TEST_ASSERT_TRUE(events > 0);
  // Same seed and inputs -> same outcome.
  for (int day = 0; day < 6 && isAlive(b); ++day) {
    for (int q = 0; q < 24 * 4; ++q) {
      nowB += 15 * 60;
      tick(b, nowB);
      takeEvent(b);
      care(b);
    }
  }
  TEST_ASSERT_EQUAL_UINT32(a.rng, b.rng);
  TEST_ASSERT_EQUAL_UINT16(a.frogs, b.frogs);
}

void test_play_reward() {
  uint32_t now = kStart;
  State s = hatched(now);
  s.happiness = 10;
  s.energy = 100;
  TEST_ASSERT_EQUAL(Result::Ok, canPlay(s));
  TEST_ASSERT_EQUAL_UINT8(3, playReward(s, Game::LeftRight, 5));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10 + rules::kGameWinHappiness, s.happiness);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100 - rules::kGameEnergy, s.energy);
  TEST_ASSERT_EQUAL_UINT8(0, playReward(s, Game::LeftRight, 2));  // lost

  const uint16_t wins = s.forceWins;
  TEST_ASSERT_EQUAL_UINT8(rules::kForceWinLen - 2, playReward(s, Game::Force, rules::kForceWinLen));
  TEST_ASSERT_EQUAL_UINT16(wins + 1, s.forceWins);

  s.items[static_cast<int>(Item::Knob)] = 1;
  s.happiness = 10;
  playReward(s, Game::LeftRight, 5);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10 + rules::kGameWinHappiness + rules::kKnobBonusHappiness,
                           s.happiness);

  s.sick = true;
  TEST_ASSERT_EQUAL(Result::Refused, canPlay(s));
}

void test_left_right_game() {
  games::LeftRight g;
  games::start(g, 1234);
  int hits = 0;
  while (!games::done(g)) {
    if (games::guess(g, games::Left)) hits++;
    TEST_ASSERT_EQUAL(g.lastLook == games::Left, g.lastHit);
  }
  TEST_ASSERT_EQUAL_UINT8(rules::kLeftRightRounds, g.round);
  TEST_ASSERT_EQUAL_UINT8(hits, g.correct);
  TEST_ASSERT_EQUAL_INT(hits, __builtin_popcount(g.history));
  TEST_ASSERT_FALSE(games::guess(g, games::Left));  // no extra rounds
}

void test_force_game() {
  games::ForceSeq g;
  games::start(g, 99);
  TEST_ASSERT_EQUAL_UINT8(rules::kForceStartLen, g.len);
  // Repeat sequences perfectly until the end.
  games::Step st = games::Step::Next;
  while (st != games::Step::Finished) {
    const uint8_t len = g.len;
    for (uint8_t i = 0; i < len; ++i) {
      st = games::input(g, static_cast<games::Sym>(g.seq[g.pos]));
      if (i + 1 < len) TEST_ASSERT_EQUAL(games::Step::Next, st);
    }
    TEST_ASSERT_TRUE(st == games::Step::Complete || st == games::Step::Finished);
  }
  TEST_ASSERT_EQUAL_UINT8(rules::kForceMaxLen, g.best);

  games::start(g, 99);
  const auto wrong = static_cast<games::Sym>((g.seq[0] + 1) % 3);
  TEST_ASSERT_EQUAL(games::Step::Wrong, games::input(g, wrong));
  TEST_ASSERT_EQUAL_UINT8(0, g.best);
  TEST_ASSERT_TRUE(g.over);
}

void test_high_low_game() {
  games::HighLow g;
  games::start(g, 777);
  TEST_ASSERT_TRUE(g.shown >= 1 && g.shown <= rules::kHighLowTop);
  // The best strategy: guess the side with more numbers left.
  while (!games::done(g)) {
    const uint8_t before = g.shown;
    const bool higher = before * 2 <= rules::kHighLowTop;
    const bool hit = games::guess(g, higher);
    TEST_ASSERT_NOT_EQUAL(before, g.shown);
    TEST_ASSERT_EQUAL_UINT8(before, g.prev);
    TEST_ASSERT_EQUAL(higher == (g.shown > before), hit);
  }
  TEST_ASSERT_TRUE(g.streak <= rules::kHighLowRounds);
  TEST_ASSERT_TRUE(g.round == g.streak || g.round == g.streak + 1);
  TEST_ASSERT_FALSE(games::guess(g, true));  // no extra rounds

  // A guess that can't be right (lower than 1) ends the game at once.
  games::start(g, 1);
  g.shown = 1;
  TEST_ASSERT_FALSE(games::guess(g, false));
  TEST_ASSERT_TRUE(games::done(g));
  TEST_ASSERT_EQUAL_UINT8(0, g.streak);

  // Rewards: a win at kHighLowWin, one rana per guess past 3.
  TEST_ASSERT_FALSE(gameWon(Game::HighLow, rules::kHighLowWin - 1));
  TEST_ASSERT_TRUE(gameWon(Game::HighLow, rules::kHighLowWin));
  uint32_t now = kStart;
  State s = hatched(now);
  TEST_ASSERT_EQUAL_UINT8(rules::kHighLowRounds - (rules::kHighLowWin - 1),
                          playReward(s, Game::HighLow, rules::kHighLowRounds));
  TEST_ASSERT_EQUAL_UINT16(0, s.forceWins);
}

void test_tidy_game() {
  games::Tidy g;
  games::start(g, 1234);
  TEST_ASSERT_FALSE(games::done(g));
  TEST_ASSERT_TRUE(g.minSwaps >= rules::kTidyMinSwaps);
  TEST_ASSERT_EQUAL_UINT8(0, games::stars(g));

  // Cursor wraps around.
  games::move(g, -1);
  TEST_ASSERT_EQUAL_UINT8(rules::kTidyPiles - 1, g.cursor);
  games::move(g, 1);
  TEST_ASSERT_EQUAL_UINT8(0, g.cursor);

  // Putting a pile back where it was is not a swap.
  games::press(g);
  TEST_ASSERT_EQUAL_INT8(0, g.held);
  games::press(g);
  TEST_ASSERT_EQUAL_INT8(-1, g.held);
  TEST_ASSERT_EQUAL_UINT8(0, g.swaps);

  // Selection sort: put the right pile in each slot, one swap each.
  for (uint8_t i = 0; i < rules::kTidyPiles; ++i) {
    if (g.piles[i] == i + 1) continue;
    uint8_t j = i;
    while (g.piles[j] != i + 1) j++;
    while (g.cursor != i) games::move(g, 1);
    games::press(g);
    while (g.cursor != j) games::move(g, 1);
    games::press(g);
  }
  TEST_ASSERT_TRUE(games::done(g));
  TEST_ASSERT_EQUAL_UINT8(g.minSwaps, g.swaps);
  TEST_ASSERT_EQUAL_UINT8(3, games::stars(g));
  g.swaps = g.minSwaps + rules::kTidySlack;
  TEST_ASSERT_EQUAL_UINT8(2, games::stars(g));
  g.swaps++;
  TEST_ASSERT_EQUAL_UINT8(1, games::stars(g));

  // One rana per star; finishing always counts as a win.
  TEST_ASSERT_TRUE(gameWon(Game::Tidy, 1));
  uint32_t now = kStart;
  State s = hatched(now);
  TEST_ASSERT_EQUAL_UINT8(3, playReward(s, Game::Tidy, 3));
}

void test_joy_game() {
  games::Joy g;
  games::start(g, 4321);
  TEST_ASSERT_EQUAL_INT(rules::kJoyFavs, __builtin_popcount(g.favs));
  int favsSeen = 0;
  while (!games::done(g)) {
    const uint8_t it = g.item;
    TEST_ASSERT_TRUE(it < rules::kJoyPool);
    if (games::sparksJoy(g, it)) favsSeen++;
    // A perfect KonMari: keep what sparks joy, let go of the rest.
    TEST_ASSERT_TRUE(games::guess(g, games::sparksJoy(g, it)));
    TEST_ASSERT_EQUAL_UINT8(it, g.lastItem);
  }
  TEST_ASSERT_EQUAL_UINT8(rules::kJoyRounds, g.correct);
  TEST_ASSERT_TRUE(favsSeen > 0 && favsSeen < rules::kJoyRounds);
  TEST_ASSERT_FALSE(games::guess(g, true));  // no extra rounds

  games::start(g, 4321);
  TEST_ASSERT_FALSE(games::guess(g, !games::sparksJoy(g, g.item)));
  TEST_ASSERT_EQUAL_UINT8(0, g.correct);

  TEST_ASSERT_FALSE(gameWon(Game::Joy, rules::kJoyWin - 1));
  TEST_ASSERT_TRUE(gameWon(Game::Joy, rules::kJoyWin));
}

void test_dress_up() {
  uint32_t now = kStart;
  State egg;
  newPet(egg, now, 3);
  TEST_ASSERT_EQUAL(Result::Refused, dress(egg, Hat::Crown, Neck::None));

  State s = hatched(now);
  TEST_ASSERT_EQUAL(Hat::None, hatOf(s));
  s.happiness = 50;
  TEST_ASSERT_EQUAL(Result::NotNeeded, dress(s, Hat::None, Neck::None));  // nothing changed
  TEST_ASSERT_EQUAL(Result::Ok, dress(s, Hat::Crown, Neck::Scarf));
  TEST_ASSERT_EQUAL(Hat::Crown, hatOf(s));
  TEST_ASSERT_EQUAL(Neck::Scarf, neckOf(s));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50 + rules::kDressHappiness, s.happiness);

  // Changing again right away is allowed but doesn't cheer it up again.
  TEST_ASSERT_EQUAL(Result::NotNeeded, dress(s, Hat::Bow, Neck::Scarf));
  TEST_ASSERT_EQUAL(Hat::Bow, hatOf(s));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50 + rules::kDressHappiness, s.happiness);
  now += rules::kDressCooldownMin * 60;
  tick(s, now);
  const float joy = s.happiness;
  TEST_ASSERT_EQUAL(Result::Ok, dress(s, Hat::Shades, Neck::BowTie));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, joy + rules::kDressHappiness, s.happiness);

  // Survives a save, and garbage from old saves reads as nothing.
  SaveBlob blob;
  pack(s, blob);
  State out;
  TEST_ASSERT_TRUE(unpack(blob, out));
  TEST_ASSERT_EQUAL(Hat::Shades, hatOf(out));
  TEST_ASSERT_EQUAL(Neck::BowTie, neckOf(out));
  out.hat = 15;
  TEST_ASSERT_EQUAL(Hat::None, hatOf(out));
}

void test_album() {
  Album a;
  memset(&a, 0, sizeof(a));
  uint32_t now = kStart;
  State s = hatched(now);
  s.stage = Stage::AdultJedi;
  s.items[static_cast<int>(Item::Bacta)] = 2;
  s.pendingEvent = static_cast<uint8_t>(Event::FrogHop);
  albumUpdate(a, s);
  TEST_ASSERT_TRUE(albumHasAdult(a, Stage::AdultJedi));
  TEST_ASSERT_FALSE(albumHasAdult(a, Stage::AdultMando));
  TEST_ASSERT_EQUAL_UINT8(1u << static_cast<int>(Item::Bacta), a.items);
  TEST_ASSERT_EQUAL_UINT8(1u << (static_cast<int>(Event::FrogHop) - 1), a.events);

  // A new pet does not erase the album; a dead pet still counts its last stage.
  State d;
  newPet(d, now, 5, 2);
  d.stage = Stage::Dead;
  d.lastStage = Stage::AdultGrump;
  albumUpdate(a, d);
  TEST_ASSERT_TRUE(albumHasAdult(a, Stage::AdultJedi));
  TEST_ASSERT_TRUE(albumHasAdult(a, Stage::AdultGrump));

  albumNoteScore(a, Game::Force, 5);
  albumNoteScore(a, Game::Force, 3);
  TEST_ASSERT_EQUAL_UINT8(5, a.bestForce);
  albumNoteScore(a, Game::HighLow, 6);
  TEST_ASSERT_EQUAL_UINT8(6, a.bestHighLow);
  TEST_ASSERT_EQUAL_UINT8(5, a.bestForce);
  albumNoteScore(a, Game::Tidy, 2);
  albumNoteScore(a, Game::Joy, 7);
  albumNoteScore(a, Game::Tidy, 1);
  TEST_ASSERT_EQUAL_UINT8(2, a.bestTidy);
  TEST_ASSERT_EQUAL_UINT8(7, a.bestJoy);
  TEST_ASSERT_EQUAL_UINT8(6, a.bestHighLow);
}

void test_catch_up_is_capped() {
  uint32_t now = kStart;
  State s;
  newPet(s, now, 7);
  const uint32_t minutes = tick(s, now + 10 * 24 * 3600 + 30);
  TEST_ASSERT_EQUAL_UINT32(rules::kMaxCatchUpMin, minutes);
  TEST_ASSERT_EQUAL_UINT32(now + 10 * 24 * 3600, s.lastUpdate);
}

void test_clock_backwards_resyncs() {
  State s;
  newPet(s, kStart, 7);
  TEST_ASSERT_EQUAL_UINT32(0, tick(s, kStart - 500));
  TEST_ASSERT_EQUAL_UINT32(kStart - 500, s.lastUpdate);
}

void test_save_roundtrip_and_corruption() {
  uint32_t now = kStart;
  State s = hatched(now);
  s.careMistakes = 2;
  SaveBlob blob;
  pack(s, blob);
  State out;
  TEST_ASSERT_TRUE(unpack(blob, out));
  TEST_ASSERT_EQUAL_MEMORY(&s, &out, sizeof(State));
  reinterpret_cast<uint8_t*>(&blob.state)[3] ^= 0x40;
  TEST_ASSERT_FALSE(unpack(blob, out));
  pack(s, blob);
  blob.version++;
  TEST_ASSERT_FALSE(unpack(blob, out));
}

void test_time_roundtrip() {
  const uint32_t t = timeutil::toEpoch(2028, 2, 29, 23, 59, 58);
  const auto c = timeutil::fromEpoch(t);
  TEST_ASSERT_EQUAL(2028, c.year);
  TEST_ASSERT_EQUAL(2, c.month);
  TEST_ASSERT_EQUAL(29, c.day);
  TEST_ASSERT_EQUAL(23, c.hour);
  TEST_ASSERT_EQUAL(59, c.minute);
  TEST_ASSERT_EQUAL(58, c.second);
  TEST_ASSERT_EQUAL_UINT32(0, timeutil::toEpoch(1970, 1, 1, 0, 0, 0));
}

void test_sprite_rows_have_correct_width() {
  for (const sprites::Sprite* s : sprites::kAll) {
    for (int r = 0; r < s->h; ++r) {
      TEST_ASSERT_EQUAL_MESSAGE(s->w, strlen(s->rows[r]), s->rows[0]);
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_egg_hatches);
  RUN_TEST(test_needs_decay_while_awake);
  RUN_TEST(test_sleep_schedule);
  RUN_TEST(test_lights_on_at_night_is_a_mistake);
  RUN_TEST(test_nap_restores_energy);
  RUN_TEST(test_no_nap_when_rested_and_lights_on_wakes);
  RUN_TEST(test_nap_into_night_becomes_sleep);
  RUN_TEST(test_wake_at_night);
  RUN_TEST(test_night_wake_ends_in_the_morning);
  RUN_TEST(test_feed_full_not_needed);
  RUN_TEST(test_poop_and_clean);
  RUN_TEST(test_starvation_kills);
  RUN_TEST(test_good_care_makes_good_adult);
  RUN_TEST(test_many_mistakes_make_bad_adult);
  RUN_TEST(test_evolution_branches);
  RUN_TEST(test_adult_dies_of_old_age);
  RUN_TEST(test_shop);
  RUN_TEST(test_items);
  RUN_TEST(test_scold_and_tantrum);
  RUN_TEST(test_tantrums_and_events_happen);
  RUN_TEST(test_play_reward);
  RUN_TEST(test_left_right_game);
  RUN_TEST(test_force_game);
  RUN_TEST(test_high_low_game);
  RUN_TEST(test_tidy_game);
  RUN_TEST(test_joy_game);
  RUN_TEST(test_dress_up);
  RUN_TEST(test_album);
  RUN_TEST(test_catch_up_is_capped);
  RUN_TEST(test_clock_backwards_resyncs);
  RUN_TEST(test_save_roundtrip_and_corruption);
  RUN_TEST(test_time_roundtrip);
  RUN_TEST(test_sprite_rows_have_correct_width);
  return UNITY_END();
}
