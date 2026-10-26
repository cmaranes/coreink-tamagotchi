// Pure game logic: no Arduino / M5 / SDL includes so it builds anywhere and
// can be unit-tested on the PC.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace pet {

enum class Stage : uint8_t {
  Egg,
  Baby,
  Child,
  Teen,
  AdultJedi,    // lots of Force training and discipline
  AdultMando,   // well raised
  AdultGloton,  // too many sweets
  AdultGrump,   // too many care mistakes
  Dead,
};
constexpr int kAdultCount = 4;

enum class Mood : uint8_t { Happy, Neutral, Sad, Sick, Sleeping, Dead };

enum class Result : uint8_t { Ok, Refused, NotNeeded };

enum class Cause : uint8_t { None, Illness, Starvation, Neglect, OldAge };

// Shop items. Knob is a permanent toy; the rest are consumed when used.
enum class Item : uint8_t { Macaron, EggSoup, Frog, Bacta, Knob, Count };
constexpr int kItemCount = static_cast<int>(Item::Count);

// Random events, rolled while awake and shown the next time the owner looks.
enum class Event : uint8_t { None, MandoVisit, FrogHop, MacaronTheft, Count };
constexpr int kEventCount = static_cast<int>(Event::Count) - 1;

enum class Game : uint8_t { LeftRight, Force, HighLow, Tidy, Joy };

// Dress-up. Worn over the sprite on the home screen.
enum class Hat : uint8_t { None, Beanie, Crown, Bow, Shades, Count };
enum class Neck : uint8_t { None, Scarf, BowTie, Necklace, Count };
constexpr int kHatCount = static_cast<int>(Hat::Count);
constexpr int kNeckCount = static_cast<int>(Neck::Count);

struct State {
  Stage stage;
  Stage lastStage;  // stage before death, for the memorial screen
  Cause cause;
  uint8_t poop;
  uint8_t weight;
  uint8_t careMistakes;
  bool sick;
  bool asleep;
  bool lightsOff;
  bool lightsMistakeGiven;  // only one lights mistake per night
  bool napping;             // daytime nap (asleep until energy is full); fits old padding
  bool nightAwake;          // woken up at night with the lights; fits old padding

  float fullness;
  float happiness;
  float energy;
  float hygiene;
  float health;

  uint32_t ageMinutes;
  uint32_t stageMinutes;
  uint32_t poopTimer;
  uint32_t neglectMinutes;
  uint32_t lightsOnMinutes;
  uint32_t lastUpdate;  // epoch seconds (RTC local time)
  uint32_t rng;
  uint32_t generation;  // how many pets have been raised

  float discipline;
  uint16_t frogs;        // "ranas", the currency
  uint16_t snacksEaten;  // since Child, for evolution
  uint16_t forceWins;    // since Child, for evolution
  uint8_t items[kItemCount];
  bool tantrum;          // fake call: scold it
  uint8_t pendingEvent;  // Event, already applied, waiting to be shown
  uint8_t nightAwakeMinutes;  // fits old padding
  uint8_t hat : 4;            // Hat; fits old padding
  uint8_t neck : 4;           // Neck
  uint8_t dressCooldown;      // minutes until a new outfit cheers it up again; fits old padding
  uint32_t tantrumMinutes;
};
// New fields go into padding so existing saves still load (unpack checks the size).
static_assert(sizeof(State) == 88, "State layout changed: bump kSaveVersion");

// A fresh egg. `now` is epoch seconds, `seed` any non-zero-ish number.
void newPet(State& s, uint32_t now, uint32_t seed, uint32_t generation = 1);

// Advance the simulation to `now` (epoch seconds). Processes whole minutes,
// capped at rules::kMaxCatchUpMin. Returns the number of minutes simulated.
uint32_t tick(State& s, uint32_t now);

// Single-minute step at the given epoch time; exposed for tests.
void stepMinute(State& s, uint32_t epoch);

Result feedMeal(State& s);
Result feedSnack(State& s);
// Whether a minigame may start (Refused when asleep, sick, tired or an egg).
Result canPlay(const State& s);
bool gameWon(Game g, uint8_t score);
// Apply the cost and reward of a finished minigame; returns ranas earned.
// `score` is correct guesses (LeftRight), longest sequence repeated (Force) or
// right guesses in a row (HighLow), stars 1..3 (Tidy) or right calls (Joy).
uint8_t playReward(State& s, Game g, uint8_t score);
Result clean(State& s);
Result medicine(State& s);
// During the day, lights off with energy below rules::kNapBelowEnergy starts a
// nap; lights on wakes it up early. At night, lights on wakes it for a while
// (rules::kNightWakeMaxMin) and lights off sends it back to bed.
Result toggleLights(State& s);
// Minutes until a running nap ends by itself (0 when not napping).
uint32_t napMinutesLeft(const State& s);
// Put on an outfit. Ok when it cheered it up, NotNeeded when nothing changed
// or it was dressed up too recently to care.
Result dress(State& s, Hat h, Neck n);
Hat hatOf(const State& s);    // Hat::None for out-of-range values
Neck neckOf(const State& s);
// Ok when it was a tantrum; NotNeeded (and a bit sadder) when it wasn't.
Result scold(State& s);
Result buy(State& s, Item it);
Result useItem(State& s, Item it);
// Takes the pending event (if any) for display; its effects were already applied.
Event takeEvent(State& s);

Mood mood(const State& s);
bool isAlive(const State& s);
// True if something needs the owner (used for beep/LED on wake).
bool needsAttention(const State& s);
// Hour of day (0..23) for an epoch time, treating the RTC as local time.
uint8_t hourOf(uint32_t epoch);
bool isSleepTime(uint32_t epoch);

// --- Album: what has been seen across all generations ----------------------
struct Album {
  uint8_t adults;  // bit per adult stage (Jedi, Mando, Gloton, Grump)
  uint8_t items;   // bit per Item ever owned
  uint8_t events;  // bit per Event (bit 0 = MandoVisit)
  uint8_t bestForce;
  uint8_t bestLeftRight;
  uint8_t bestHighLow;
  uint8_t bestTidy : 4;  // stars; fits the save's padding
  uint8_t bestJoy : 4;
};

// Record everything visible in `s` (stage reached, items owned, pending event).
void albumUpdate(Album& a, const State& s);
void albumNoteScore(Album& a, Game g, uint8_t score);
bool albumHasAdult(const Album& a, Stage st);

// --- Persistence -----------------------------------------------------------
constexpr uint32_t kSaveMagic = 0x54414D41;  // "TAMA"
constexpr uint16_t kSaveVersion = 2;

struct SaveBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  State state;
  uint32_t crc;
};

uint32_t crc32(const uint8_t* data, size_t len);
void pack(const State& s, SaveBlob& out);
// Returns false if the blob is corrupt or from an incompatible version.
bool unpack(const SaveBlob& in, State& out);

}  // namespace pet
