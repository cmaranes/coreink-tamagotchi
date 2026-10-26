// Tunable game constants. All rates are "points per hour" on a 0..100 scale
// unless noted otherwise. Tweak these and re-run the simulator / tests.
#pragma once
#include <stdint.h>

namespace rules {

// --- Life stages (minutes spent in a stage before evolving) ---------------
constexpr uint32_t kEggMinutes   = 5;
constexpr uint32_t kBabyMinutes  = 60;
constexpr uint32_t kChildMinutes = 24 * 60;
constexpr uint32_t kTeenMinutes  = 48 * 60;
// Adults die of old age after this many days, indexed Jedi, Mando, Gloton, Grump.
constexpr uint32_t kAdultLifeDays[] = {25, 20, 14, 10};
// Evolution at the end of Teen, checked in this order:
//   more mistakes than this         -> Grump
//   enough Force wins + discipline  -> Jedi
//   too many snacks or overweight   -> Gloton
//   otherwise                       -> Mando
constexpr uint8_t kMaxMistakesForGoodAdult = 3;
constexpr uint16_t kJediForceWins = 8;
constexpr float kJediDiscipline = 60;
constexpr uint16_t kGlotonSnacks = 25;

// --- Sleep schedule (hour of the RTC clock) -------------------------------
constexpr uint8_t kSleepHour = 21;  // falls asleep at 21:00
constexpr uint8_t kWakeHour  = 8;   // wakes up at 08:00
// Minutes asleep with the lights on before it counts as a care mistake.
constexpr uint32_t kLightsOnMistakeMin = 30;
// Daytime naps: turn the lights off while tired. Wakes up once energy is full.
constexpr float kNapBelowEnergy = 80;  // won't nap when more rested than this
constexpr float kNapRegen = 1200;      // energy per hour while napping: empty to full in 5 min
// Waking it at night: lights on while it sleeps. It is grumpy about it, and
// falls asleep again by itself after a while (or as soon as the lights go off).
constexpr float kNightWakeHappiness = 10;
constexpr uint8_t kNightWakeMaxMin = 60;

// --- Decay while awake, indexed by Stage
//     (Egg, Baby, Child, Teen, AdultJedi, AdultMando, AdultGloton, AdultGrump)
constexpr float kFullnessDecay[]  = {0, 24, 14, 10, 7, 8, 12, 10};
constexpr float kHappinessDecay[] = {0, 20, 12,  9, 6, 7,  8, 10};
constexpr float kHygieneDecay = 5;      // base hygiene loss
constexpr float kHygienePerPoop = 6;    // extra loss per poop on screen
constexpr float kEnergyDecay = 6;       // awake
constexpr float kEnergyRegen = 14;      // asleep, lights off
constexpr float kEnergyRegenLightsOn = 6;
constexpr float kSleepDecayFactor = 0.25f;  // fullness/happiness decay while asleep

// --- Poop ------------------------------------------------------------------
constexpr uint32_t kPoopIntervalMin[] = {0, 50, 110, 170, 240, 230, 180, 200};
constexpr uint8_t kMaxPoop = 4;

// --- Health ----------------------------------------------------------------
constexpr float kHealthLossSick   = 5;
constexpr float kHealthLossStarve = 8;   // fullness at 0
constexpr float kHealthLossSad    = 2;   // happiness at 0
constexpr float kHealthLossTired  = 2;   // energy at 0
constexpr float kHealthRegen      = 3;   // when well cared for
constexpr float kWellCaredThreshold = 30;
// Chance (percent) per hour of getting sick under bad conditions; they add up.
constexpr uint8_t kSickChanceDirty   = 25;  // poop >= 3
constexpr uint8_t kSickChanceGrimy   = 20;  // hygiene < 20
constexpr uint8_t kSickChanceStarved = 10;  // fullness == 0
constexpr uint8_t kSickChanceFat     = 10;  // weight > kHeavyWeight

// --- Neglect ---------------------------------------------------------------
// Minutes a need (fullness/happiness at 0, untreated sickness) can be ignored
// before it becomes a care mistake. Repeats every interval.
constexpr uint32_t kNeglectMistakeMin = 30;

// --- Weight ----------------------------------------------------------------
constexpr uint8_t kMinWeight = 5;
constexpr uint8_t kHeavyWeight = 40;
constexpr uint8_t kStartWeight = 10;

// --- Actions ---------------------------------------------------------------
constexpr float kMealFullness = 35;
constexpr uint8_t kMealWeight = 1;
constexpr float kSnackHappiness = 20;
constexpr float kSnackFullness = 5;
constexpr uint8_t kSnackWeight = 2;
constexpr float kMedicineHappiness = 10;
constexpr float kFullEnough = 95;
constexpr float kTooTiredToPlay = 10;

// --- Minigames -------------------------------------------------------------
constexpr float kGameWinHappiness = 25;
constexpr float kGameLoseHappiness = 10;
constexpr float kGameEnergy = 15;
constexpr float kGameFullness = 5;
constexpr float kKnobBonusHappiness = 10;  // extra per win once the knob is owned
constexpr uint8_t kLeftRightRounds = 5;
constexpr uint8_t kLeftRightWin = 3;       // correct guesses needed to win
constexpr uint8_t kForceStartLen = 3;
constexpr uint8_t kForceMaxLen = 8;
constexpr uint8_t kForceWinLen = 4;        // longest sequence repeated to count as a win
constexpr uint8_t kHighLowTop = 9;         // numbers go from 1 to this
constexpr uint8_t kHighLowRounds = 8;      // a perfect streak ends the game
constexpr uint8_t kHighLowWin = 4;         // right guesses in a row needed to win
// Ordenar: sort piles of folded clothes from small to big by swapping two at a time.
constexpr uint8_t kTidyPiles = 6;
constexpr uint8_t kTidyMinSwaps = 3;       // a shuffle always needs at least this many swaps
constexpr uint8_t kTidySlack = 2;          // extra swaps over the minimum that still earn 2 stars
// ¿Alegría?: memorise what sparks joy, then keep it or thank it and let it go.
constexpr uint8_t kJoyPool = 6;            // different things that can show up
constexpr uint8_t kJoyFavs = 3;            // of those, how many spark joy
constexpr uint8_t kJoyRounds = 8;
constexpr uint8_t kJoyWin = 6;             // right calls needed to win

// --- Vestir (dress-up) -------------------------------------------------------
constexpr float kDressHappiness = 15;      // a new outfit cheers it up...
constexpr uint8_t kDressCooldownMin = 60;  // ...at most once per this many minutes

// --- Shop (prices in ranas), indexed by Item -------------------------------
//     (Macaron, EggSoup, Frog, Bacta, Knob)
constexpr uint8_t kItemPrice[] = {3, 2, 1, 4, 6};
constexpr uint8_t kMaxItemStack = 9;
constexpr float kMacaronHappiness = 30;
constexpr uint8_t kMacaronWeight = 2;
constexpr float kEggSoupFullness = 40;
constexpr uint8_t kEggSoupWeight = 1;
constexpr float kFrogFullness = 15;
constexpr float kFrogHappiness = 10;
constexpr float kBactaHealth = 30;

// --- Discipline / tantrums ---------------------------------------------------
constexpr float kStartDiscipline = 20;
// Chance (percent) per awake hour at discipline 0; scales down to 0 at 100.
constexpr uint8_t kTantrumChance = 20;
constexpr float kScoldDiscipline = 25;
constexpr float kUnfairScoldHappiness = 15;
constexpr uint32_t kTantrumMistakeMin = 30;

// --- Random events -----------------------------------------------------------
constexpr uint8_t kEventChance = 15;  // percent per awake hour
constexpr float kMandoHappiness = 20;
constexpr uint16_t kFrogHopFrogs = 2;
constexpr float kMacaronTheftHappiness = 15;
constexpr uint8_t kMacaronTheftWeight = 2;
constexpr float kMacaronTheftDiscipline = 10;

// --- Attention / catch-up ---------------------------------------------------
constexpr float kNeedThreshold = 25;  // below this a stat "calls" the owner
// Max time simulated in one go (e.g. device left unpowered for a week).
constexpr uint32_t kMaxCatchUpMin = 72 * 60;

}  // namespace rules
