#include "pet.h"

#include <string.h>

#include "rules.h"

namespace pet {

namespace {

constexpr float kPerMinute = 1.0f / 60.0f;

uint32_t nextRandom(State& s) {
  // xorshift32
  uint32_t x = s.rng ? s.rng : 0x9E3779B9u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s.rng = x;
  return x;
}

bool chance(State& s, uint32_t percent) { return (nextRandom(s) % 100) < percent; }

float clamp100(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

int stageIndex(Stage st) {
  int i = static_cast<int>(st);
  return i > static_cast<int>(Stage::AdultGrump) ? static_cast<int>(Stage::AdultGrump) : i;
}

bool isAdult(Stage st) { return st >= Stage::AdultJedi && st <= Stage::AdultGrump; }

int adultIndex(Stage st) { return static_cast<int>(st) - static_cast<int>(Stage::AdultJedi); }

Stage adultFor(const State& s) {
  if (s.careMistakes > rules::kMaxMistakesForGoodAdult) return Stage::AdultGrump;
  if (s.forceWins >= rules::kJediForceWins && s.discipline >= rules::kJediDiscipline)
    return Stage::AdultJedi;
  if (s.snacksEaten >= rules::kGlotonSnacks || s.weight > rules::kHeavyWeight)
    return Stage::AdultGloton;
  return Stage::AdultMando;
}

uint8_t& itemCount(State& s, Item it) { return s.items[static_cast<int>(it)]; }

void die(State& s, Cause cause) {
  s.lastStage = s.stage;
  s.stage = Stage::Dead;
  s.cause = cause;
  s.asleep = false;
  s.sick = false;
}

void evolve(State& s) {
  uint32_t limit = 0;
  switch (s.stage) {
    case Stage::Baby: limit = rules::kBabyMinutes; break;
    case Stage::Child: limit = rules::kChildMinutes; break;
    case Stage::Teen: limit = rules::kTeenMinutes; break;
    default:
      if (!isAdult(s.stage)) return;
      limit = rules::kAdultLifeDays[adultIndex(s.stage)] * 24 * 60;
      break;
  }
  if (s.stageMinutes < limit) return;
  s.stageMinutes = 0;
  switch (s.stage) {
    case Stage::Baby:
      s.stage = Stage::Child;
      s.snacksEaten = 0;
      s.forceWins = 0;
      break;
    case Stage::Child: s.stage = Stage::Teen; break;
    case Stage::Teen: s.stage = adultFor(s); break;
    default: die(s, Cause::OldAge); break;
  }
}

// Tantrums and random events, once per awake hour.
void hourlyRolls(State& s) {
  if (!s.tantrum && s.stage >= Stage::Child && !s.sick &&
      s.fullness > rules::kNeedThreshold && s.happiness > rules::kNeedThreshold) {
    const uint32_t pct =
        static_cast<uint32_t>(rules::kTantrumChance * (100 - s.discipline) / 100 + 0.5f);
    if (pct && chance(s, pct)) {
      s.tantrum = true;
      s.tantrumMinutes = 0;
    }
  }

  if (s.pendingEvent != static_cast<uint8_t>(Event::None) || s.sick ||
      !chance(s, rules::kEventChance))
    return;
  const Event e = static_cast<Event>(1 + nextRandom(s) % kEventCount);
  switch (e) {
    case Event::MandoVisit: {
      // A gift: any consumable (not the knob).
      uint8_t& n = itemCount(s, static_cast<Item>(nextRandom(s) % (kItemCount - 1)));
      if (n < rules::kMaxItemStack) n++;
      s.happiness = clamp100(s.happiness + rules::kMandoHappiness);
      break;
    }
    case Event::FrogHop: s.frogs += rules::kFrogHopFrogs; break;
    case Event::MacaronTheft:
      s.happiness = clamp100(s.happiness + rules::kMacaronTheftHappiness);
      s.weight += rules::kMacaronTheftWeight;
      s.discipline = clamp100(s.discipline - rules::kMacaronTheftDiscipline);
      break;
    default: return;
  }
  s.pendingEvent = static_cast<uint8_t>(e);
}

}  // namespace

void newPet(State& s, uint32_t now, uint32_t seed, uint32_t generation) {
  memset(&s, 0, sizeof(s));
  s.stage = Stage::Egg;
  s.lastStage = Stage::Egg;
  s.cause = Cause::None;
  s.weight = rules::kStartWeight;
  s.fullness = 80;
  s.happiness = 80;
  s.energy = 100;
  s.hygiene = 100;
  s.health = 100;
  s.lastUpdate = now;
  s.rng = seed ? seed : 0x12345678u;
  s.generation = generation;
  s.discipline = rules::kStartDiscipline;
}

uint8_t hourOf(uint32_t epoch) { return static_cast<uint8_t>((epoch / 3600) % 24); }

bool isSleepTime(uint32_t epoch) {
  uint8_t h = hourOf(epoch);
  if (rules::kSleepHour > rules::kWakeHour) return h >= rules::kSleepHour || h < rules::kWakeHour;
  return h >= rules::kSleepHour && h < rules::kWakeHour;
}

void stepMinute(State& s, uint32_t epoch) {
  if (s.stage == Stage::Dead) return;
  s.ageMinutes++;
  s.stageMinutes++;
  if (s.dressCooldown) s.dressCooldown--;

  if (s.stage == Stage::Egg) {
    if (s.stageMinutes >= rules::kEggMinutes) {
      s.stage = Stage::Baby;
      s.stageMinutes = 0;
      s.poopTimer = 0;
    }
    return;
  }

  const int si = stageIndex(s.stage);

  // Sleep schedule. A nap still running at bedtime becomes the night's sleep.
  if (!s.asleep) s.napping = false;
  const bool sleepTime = isSleepTime(epoch);
  if (s.nightAwake) {
    // Woken up at night: dozes off again by itself after a while, lights on or not.
    if (!sleepTime) {
      s.nightAwake = false;
    } else if (++s.nightAwakeMinutes >= rules::kNightWakeMaxMin) {
      s.nightAwake = false;
      s.asleep = true;
      s.lightsOnMinutes = 0;
      s.tantrum = false;
    }
  } else if (sleepTime && (!s.asleep || s.napping)) {
    s.asleep = true;
    s.napping = false;
    s.lightsOnMinutes = 0;
    s.lightsMistakeGiven = false;
    s.tantrum = false;
  } else if (!sleepTime && s.asleep && !s.napping) {
    s.asleep = false;
    s.lightsOff = false;
  }

  // Needs decay.
  const float sleepFactor = s.asleep ? rules::kSleepDecayFactor : 1.0f;
  s.fullness = clamp100(s.fullness - rules::kFullnessDecay[si] * sleepFactor * kPerMinute);
  s.happiness = clamp100(s.happiness - rules::kHappinessDecay[si] * sleepFactor * kPerMinute);
  s.hygiene = clamp100(s.hygiene - (rules::kHygieneDecay + rules::kHygienePerPoop * s.poop) *
                                       kPerMinute);
  if (s.napping) {
    s.energy = clamp100(s.energy + rules::kNapRegen * kPerMinute);
    if (s.energy >= 100) {
      s.asleep = false;
      s.napping = false;
      s.lightsOff = false;
    }
  } else if (s.asleep) {
    const float regen = s.lightsOff ? rules::kEnergyRegen : rules::kEnergyRegenLightsOn;
    s.energy = clamp100(s.energy + regen * kPerMinute);
    if (!s.lightsOff && !s.lightsMistakeGiven &&
        ++s.lightsOnMinutes >= rules::kLightsOnMistakeMin) {
      s.careMistakes++;
      s.lightsMistakeGiven = true;
    }
  } else {
    s.energy = clamp100(s.energy - rules::kEnergyDecay * kPerMinute);
  }

  // Poop (only while awake).
  if (!s.asleep && ++s.poopTimer >= rules::kPoopIntervalMin[si]) {
    s.poopTimer = 0;
    if (s.poop < rules::kMaxPoop) s.poop++;
  }

  // Hourly sickness roll.
  if (!s.sick && s.ageMinutes % 60 == 0) {
    uint32_t pct = 0;
    if (s.poop >= 3) pct += rules::kSickChanceDirty;
    if (s.hygiene < 20) pct += rules::kSickChanceGrimy;
    if (s.fullness <= 0) pct += rules::kSickChanceStarved;
    if (s.weight > rules::kHeavyWeight) pct += rules::kSickChanceFat;
    if (pct && chance(s, pct)) s.sick = true;
  }
  if (!s.asleep && s.ageMinutes % 60 == 0) hourlyRolls(s);

  // An ignored tantrum teaches it that tantrums work.
  if (s.tantrum && ++s.tantrumMinutes >= rules::kTantrumMistakeMin) {
    s.tantrum = false;
    s.careMistakes++;
  }

  // Health.
  float dh = 0;
  if (s.sick) dh -= rules::kHealthLossSick;
  if (s.fullness <= 0) dh -= rules::kHealthLossStarve;
  if (s.happiness <= 0) dh -= rules::kHealthLossSad;
  if (s.energy <= 0) dh -= rules::kHealthLossTired;
  if (dh == 0 && s.fullness > rules::kWellCaredThreshold &&
      s.happiness > rules::kWellCaredThreshold && s.hygiene > rules::kWellCaredThreshold) {
    dh = rules::kHealthRegen;
  }
  s.health = clamp100(s.health + dh * kPerMinute);

  // Neglect -> care mistakes.
  if (s.fullness <= 0 || s.happiness <= 0 || s.sick) {
    if (++s.neglectMinutes >= rules::kNeglectMistakeMin) {
      s.neglectMinutes = 0;
      s.careMistakes++;
    }
  } else {
    s.neglectMinutes = 0;
  }

  if (s.health <= 0) {
    Cause c = Cause::Neglect;
    if (s.sick) c = Cause::Illness;
    else if (s.fullness <= 0) c = Cause::Starvation;
    die(s, c);
    return;
  }

  evolve(s);
}

uint32_t tick(State& s, uint32_t now) {
  if (now <= s.lastUpdate) {
    // Clock went backwards (RTC reset?) -> just resync.
    if (now < s.lastUpdate) s.lastUpdate = now;
    return 0;
  }
  const uint32_t elapsed = now - s.lastUpdate;
  uint32_t minutes = elapsed / 60;
  uint32_t start = s.lastUpdate;
  if (minutes > rules::kMaxCatchUpMin) {
    // Skip the time we refuse to simulate; the pet was "paused".
    start = now - (elapsed % 60) - rules::kMaxCatchUpMin * 60;
    minutes = rules::kMaxCatchUpMin;
  }
  for (uint32_t i = 0; i < minutes; ++i) {
    stepMinute(s, start + (i + 1) * 60);
  }
  s.lastUpdate = start + minutes * 60;
  return minutes;
}

static bool canInteract(const State& s) { return s.stage != Stage::Egg && s.stage != Stage::Dead; }

Result feedMeal(State& s) {
  if (!canInteract(s) || s.asleep) return Result::Refused;
  if (s.fullness >= rules::kFullEnough) return Result::NotNeeded;
  s.fullness = clamp100(s.fullness + rules::kMealFullness);
  s.weight += rules::kMealWeight;
  return Result::Ok;
}

Result feedSnack(State& s) {
  if (!canInteract(s) || s.asleep) return Result::Refused;
  s.happiness = clamp100(s.happiness + rules::kSnackHappiness);
  s.fullness = clamp100(s.fullness + rules::kSnackFullness);
  s.weight += rules::kSnackWeight;
  s.snacksEaten++;
  return Result::Ok;
}

Result canPlay(const State& s) {
  if (!canInteract(s) || s.asleep || s.sick) return Result::Refused;
  if (s.energy < rules::kTooTiredToPlay) return Result::Refused;
  return Result::Ok;
}

bool gameWon(Game g, uint8_t score) {
  switch (g) {
    case Game::LeftRight: return score >= rules::kLeftRightWin;
    case Game::HighLow: return score >= rules::kHighLowWin;
    case Game::Tidy: return score >= 1;  // finished sorting
    case Game::Joy: return score >= rules::kJoyWin;
    default: return score >= rules::kForceWinLen;
  }
}

uint8_t playReward(State& s, Game g, uint8_t score) {
  if (!canInteract(s)) return 0;
  const bool won = gameWon(g, score);
  float joy = won ? rules::kGameWinHappiness : rules::kGameLoseHappiness;
  if (won && itemCount(s, Item::Knob)) joy += rules::kKnobBonusHappiness;
  s.happiness = clamp100(s.happiness + joy);
  s.energy = clamp100(s.energy - rules::kGameEnergy);
  s.fullness = clamp100(s.fullness - rules::kGameFullness);
  if (s.weight > rules::kMinWeight) s.weight--;
  // Ranas: one per correct guess past 2 / per sequence step past 2 / per
  // guess in a row past 3 / per star / per right call past 5.
  const uint8_t base = g == Game::LeftRight ? rules::kLeftRightWin - 1
                       : g == Game::HighLow ? rules::kHighLowWin - 1
                       : g == Game::Tidy    ? 0
                       : g == Game::Joy     ? rules::kJoyWin - 1
                                            : rules::kForceStartLen - 1;
  const uint8_t frogs = score > base ? score - base : 0;
  s.frogs += frogs;
  if (won && g == Game::Force) s.forceWins++;
  return frogs;
}

Result clean(State& s) {
  if (!canInteract(s)) return Result::Refused;
  if (s.poop == 0 && s.hygiene > 90) return Result::NotNeeded;
  s.poop = 0;
  s.hygiene = 100;
  return Result::Ok;
}

Result scold(State& s) {
  if (!canInteract(s) || s.asleep) return Result::Refused;
  if (!s.tantrum) {
    s.happiness = clamp100(s.happiness - rules::kUnfairScoldHappiness);
    return Result::NotNeeded;
  }
  s.tantrum = false;
  s.discipline = clamp100(s.discipline + rules::kScoldDiscipline);
  return Result::Ok;
}

Result buy(State& s, Item it) {
  if (s.stage == Stage::Dead || it >= Item::Count) return Result::Refused;
  uint8_t& n = itemCount(s, it);
  if (n >= (it == Item::Knob ? 1 : rules::kMaxItemStack)) return Result::NotNeeded;
  const uint8_t price = rules::kItemPrice[static_cast<int>(it)];
  if (s.frogs < price) return Result::Refused;
  s.frogs -= price;
  n++;
  return Result::Ok;
}

Result useItem(State& s, Item it) {
  if (!canInteract(s) || it >= Item::Count || it == Item::Knob) return Result::Refused;
  uint8_t& n = itemCount(s, it);
  if (n == 0) return Result::Refused;
  if (s.asleep && it != Item::Bacta) return Result::Refused;
  switch (it) {
    case Item::Macaron:
      s.happiness = clamp100(s.happiness + rules::kMacaronHappiness);
      s.weight += rules::kMacaronWeight;
      s.snacksEaten++;
      break;
    case Item::EggSoup:
      if (s.fullness >= rules::kFullEnough) return Result::NotNeeded;
      s.fullness = clamp100(s.fullness + rules::kEggSoupFullness);
      s.weight += rules::kEggSoupWeight;
      break;
    case Item::Frog:
      if (s.fullness >= rules::kFullEnough) return Result::NotNeeded;
      s.fullness = clamp100(s.fullness + rules::kFrogFullness);
      s.happiness = clamp100(s.happiness + rules::kFrogHappiness);
      break;
    case Item::Bacta:
      if (!s.sick && s.health >= 100) return Result::NotNeeded;
      s.sick = false;
      s.health = clamp100(s.health + rules::kBactaHealth);
      break;
    default: return Result::Refused;
  }
  n--;
  return Result::Ok;
}

Event takeEvent(State& s) {
  const Event e = static_cast<Event>(s.pendingEvent);
  s.pendingEvent = static_cast<uint8_t>(Event::None);
  return e;
}

Result medicine(State& s) {
  if (!canInteract(s)) return Result::Refused;
  if (!s.sick) return Result::NotNeeded;
  s.sick = false;
  s.happiness = clamp100(s.happiness - rules::kMedicineHappiness);
  return Result::Ok;
}

Result toggleLights(State& s) {
  if (s.stage == Stage::Dead) return Result::Refused;
  s.lightsOff = !s.lightsOff;
  if (s.nightAwake && s.lightsOff) {
    // Back to bed.
    s.nightAwake = false;
    s.asleep = true;
    s.tantrum = false;
  } else if (s.asleep && !s.napping && !s.lightsOff && canInteract(s)) {
    // Woken up in the middle of the night.
    s.asleep = false;
    s.nightAwake = true;
    s.nightAwakeMinutes = 0;
    s.happiness = clamp100(s.happiness - rules::kNightWakeHappiness);
  } else if (s.napping && !s.lightsOff) {
    s.asleep = false;
    s.napping = false;
  } else if (!s.asleep && s.lightsOff && canInteract(s) &&
             s.energy < rules::kNapBelowEnergy) {
    s.asleep = true;
    s.napping = true;
  }
  return Result::Ok;
}

uint32_t napMinutesLeft(const State& s) {
  if (!s.napping) return 0;
  return static_cast<uint32_t>((100 - s.energy) * 60 / rules::kNapRegen) + 1;
}

Hat hatOf(const State& s) { return s.hat < kHatCount ? static_cast<Hat>(s.hat) : Hat::None; }

Neck neckOf(const State& s) {
  return s.neck < kNeckCount ? static_cast<Neck>(s.neck) : Neck::None;
}

Result dress(State& s, Hat h, Neck n) {
  if (!canInteract(s) || s.asleep || h >= Hat::Count || n >= Neck::Count) return Result::Refused;
  if (h == hatOf(s) && n == neckOf(s)) return Result::NotNeeded;
  s.hat = static_cast<uint8_t>(h);
  s.neck = static_cast<uint8_t>(n);
  if (s.dressCooldown) return Result::NotNeeded;
  s.happiness = clamp100(s.happiness + rules::kDressHappiness);
  s.dressCooldown = rules::kDressCooldownMin;
  return Result::Ok;
}

bool isAlive(const State& s) { return s.stage != Stage::Dead; }

Mood mood(const State& s) {
  if (s.stage == Stage::Dead) return Mood::Dead;
  if (s.asleep) return Mood::Sleeping;
  if (s.sick) return Mood::Sick;
  if (s.stage == Stage::Egg) return Mood::Neutral;
  if (s.tantrum) return Mood::Sad;
  if (s.fullness < rules::kNeedThreshold || s.happiness < rules::kNeedThreshold ||
      s.hygiene < rules::kNeedThreshold || s.energy < rules::kNeedThreshold)
    return Mood::Sad;
  if (s.happiness > 70) return Mood::Happy;
  return Mood::Neutral;
}

bool needsAttention(const State& s) {
  if (!canInteract(s)) return false;
  if (s.asleep) return !s.lightsOff || s.sick;
  return s.sick || s.tantrum || s.poop >= 2 || s.fullness < rules::kNeedThreshold ||
         s.happiness < rules::kNeedThreshold;
}

void albumUpdate(Album& a, const State& s) {
  const Stage st = s.stage == Stage::Dead ? s.lastStage : s.stage;
  if (isAdult(st)) a.adults |= 1u << adultIndex(st);
  for (int i = 0; i < kItemCount; ++i) {
    if (s.items[i]) a.items |= 1u << i;
  }
  if (s.pendingEvent != static_cast<uint8_t>(Event::None)) a.events |= 1u << (s.pendingEvent - 1);
}

void albumNoteScore(Album& a, Game g, uint8_t score) {
  switch (g) {
    case Game::LeftRight: if (score > a.bestLeftRight) a.bestLeftRight = score; break;
    case Game::HighLow: if (score > a.bestHighLow) a.bestHighLow = score; break;
    case Game::Force: if (score > a.bestForce) a.bestForce = score; break;
    case Game::Tidy: if (score > a.bestTidy && score <= 15) a.bestTidy = score; break;
    case Game::Joy: if (score > a.bestJoy && score <= 15) a.bestJoy = score; break;
  }
}

bool albumHasAdult(const Album& a, Stage st) {
  return isAdult(st) && (a.adults & (1u << adultIndex(st)));
}

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

void pack(const State& s, SaveBlob& out) {
  memset(&out, 0, sizeof(out));
  out.magic = kSaveMagic;
  out.version = kSaveVersion;
  out.size = sizeof(State);
  memcpy(&out.state, &s, sizeof(State));
  out.crc = crc32(reinterpret_cast<const uint8_t*>(&out), offsetof(SaveBlob, crc));
}

bool unpack(const SaveBlob& in, State& out) {
  if (in.magic != kSaveMagic || in.version != kSaveVersion || in.size != sizeof(State))
    return false;
  if (crc32(reinterpret_cast<const uint8_t*>(&in), offsetof(SaveBlob, crc)) != in.crc)
    return false;
  memcpy(&out, &in.state, sizeof(State));
  return true;
}

}  // namespace pet
