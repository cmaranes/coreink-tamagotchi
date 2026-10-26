// CoreInk Tamagotchi - application / screen state machine.
// Shared by the device build and the SDL simulator (see hal/).
#include <M5Unified.h>
#include <stdio.h>

#include "game/minigames.h"
#include "game/pet.h"
#include "hal/hal.h"
#include "ui/render.h"

namespace {

constexpr uint32_t kIdleSleepMs = 45 * 1000;  // awake time without input before power-down
constexpr uint32_t kSleepSeconds = 15 * 60;  // RTC wake-up interval while "off"
constexpr uint32_t kAnimMs = 8 * 1000;        // idle animation frame period
constexpr uint32_t kMessageMs = 2500;         // speech bubble lifetime
constexpr uint32_t kEventMessageMs = 4000;    // longer for random events
constexpr uint32_t kRevealMs = 1500;          // left/right result on screen
constexpr uint32_t kJoyMemoriseMs = 4000;     // things that spark joy on screen
constexpr uint32_t kTidyDoneMs = 2500;        // sorted shelf and stars on screen

enum class Screen : uint8_t {
  Home,
  Feed,    // list: meal, snack, owned foods
  Cure,    // list: medicine, bacta (only when bacta is owned)
  Games,   // list: minigames
  More,    // list: shop, info, album
  Shop,    // list: items for sale
  Stats,
  Album,
  LeftRight,
  Force,
  HighLow,
  Tidy,
  Joy,
  Dress,
  Dead,
};

// Everything that has to survive a power cut.
struct Persist {
  pet::SaveBlob blob;
  uint32_t screenSig;  // signature of what the e-ink shows while powered off
  bool attentionAlerted;
  pet::Album album;    // kept across generations
};
// hal::load() needs the exact size, so a new field must fit the padding.
static_assert(sizeof(Persist) == 112, "Persist layout changed: old saves would be lost");

M5Canvas gCanvas(&M5.Display);
pet::State gPet;
Persist gPersist;

Screen gScreen = Screen::Home;
int gCursor = 0;
bool gAltFrame = false;
const char* gMsg = nullptr;
char gMsgBuf[32];
const sprites::Sprite* gProp = nullptr;
bool gPose = false;  // Force pose while the current message is shown
uint32_t gMsgUntil = 0;
uint32_t gLastInput = 0;
uint32_t gLastAnim = 0;
bool gDirty = true;
bool gFullRefresh = true;

// Pop-up lists: rows plus the action each row triggers.
ui::ListRow gRows[ui::kListMax];
int gActions[ui::kListMax];
int gRowCount = 0;
int gListCursor = 0;
int gAlbumPage = 0;

// Minigames.
games::LeftRight gLR;
games::ForceSeq gForce;
games::HighLow gHL;
games::Tidy gTidy;
games::Joy gJoy;
// LeftRight, HighLow, Joy: showing a result / Force, Joy before round 1: memorising /
// Tidy: sorted, showing the stars.
bool gPhase = false;
uint32_t gPhaseUntil = 0;

// Dress-up: outfit being tried on, and the selected row.
pet::Hat gDressHat = pet::Hat::None;
pet::Neck gDressNeck = pet::Neck::None;
int gDressRow = 0;

// List actions.
enum Action : int {
  kMeal = 100,
  kSnack,
  kMedicine,
  kLeftRight,
  kForceGame,
  kHighLowGame,
  kTidyGame,
  kJoyGame,
  kDressUp,
  kOpenShop,
  kOpenStats,
  kOpenAlbum,
  // Item actions are the Item index (0 .. kItemCount-1).
};

const sprites::Sprite* const kItemSprites[] = {
    &sprites::kMacaronSprite, &sprites::kEggSoupSprite, &sprites::kFrogSprite,
    &sprites::kBactaSprite,   &sprites::kKnobSprite,
};

uint32_t seed() { return hal::now() ^ (hal::millis32() * 2654435761u); }

void savePet() {
  pet::albumUpdate(gPersist.album, gPet);
  pet::pack(gPet, gPersist.blob);
  hal::save(&gPersist, sizeof(gPersist));
}

void loadPet() {
  memset(&gPersist, 0, sizeof(gPersist));
  if (hal::load(&gPersist, sizeof(gPersist)) && pet::unpack(gPersist.blob, gPet)) return;
  memset(&gPersist, 0, sizeof(gPersist));
  pet::newPet(gPet, hal::now(), seed());
  savePet();
}

ui::View makeView() {
  ui::View v;
  v.pet = &gPet;
  v.now = hal::now();
  v.battery = hal::batteryPercent();
  v.charging = hal::charging();
  v.menuCursor = gCursor;
  v.altFrame = gAltFrame;
  v.message = gMsg;
  v.prop = gProp;
  v.forcePose = gPose;
  return v;
}

const char* listTitle() {
  switch (gScreen) {
    case Screen::Feed: return "Comer";
    case Screen::Cure: return "Cura";
    case Screen::Games: return "Jugar";
    case Screen::More: return "Más";
    case Screen::Shop: return "Tienda";
    default: return "";
  }
}

void render() {
  ui::View v = makeView();
  switch (gScreen) {
    case Screen::Home: ui::drawHome(gCanvas, v); break;
    case Screen::Feed:
    case Screen::Cure:
    case Screen::Games:
    case Screen::More:
    case Screen::Shop:
      v.menuCursor = -1;
      ui::drawListPopup(gCanvas, v, listTitle(), nullptr, gRows, gRowCount, gListCursor);
      break;
    case Screen::Stats: ui::drawStats(gCanvas, v); break;
    case Screen::Album: ui::drawAlbum(gCanvas, v, gPersist.album, gAlbumPage); break;
    case Screen::LeftRight: ui::drawLeftRight(gCanvas, v, gLR, gPhase); break;
    case Screen::Force: ui::drawForce(gCanvas, v, gForce, gPhase); break;
    case Screen::HighLow: ui::drawHighLow(gCanvas, v, gHL, gPhase); break;
    case Screen::Tidy: ui::drawTidy(gCanvas, v, gTidy, gPhase); break;
    case Screen::Joy: ui::drawJoy(gCanvas, v, gJoy, gPhase); break;
    case Screen::Dress: ui::drawDress(gCanvas, v, gDressHat, gDressNeck, gDressRow); break;
    case Screen::Dead: ui::drawDead(gCanvas, v); break;
  }
  hal::present(gCanvas, !gFullRefresh);
  gFullRefresh = false;
  gDirty = false;
}

void say(const char* msg, const sprites::Sprite* prop = nullptr, uint32_t ms = kMessageMs) {
  gMsg = msg;
  gProp = prop;
  gPose = false;
  gMsgUntil = hal::millis32() + ms;
  gDirty = true;
}

void chirp() { hal::beep(2600, 60); }
void buzz() { hal::beep(500, 150); }

void alert() {
  hal::beep(3000, 80);
  hal::delayMs(120);
  hal::beep(3000, 80);
}

void goHome() {
  gScreen = Screen::Home;
  gDirty = true;
}

void startNewPet() {
  pet::newPet(gPet, hal::now(), seed(), gPet.generation + 1);
  gScreen = Screen::Home;
  gCursor = 0;
  gPersist.attentionAlerted = false;
  say("¡Nuevo huevo!");
  gFullRefresh = true;
  savePet();
}

// Visual summary of the powered-off frame: if it hasn't changed we skip the
// e-ink refresh on timer wake-ups to save battery.
uint32_t screenSignature() {
  uint32_t sig = static_cast<uint32_t>(gPet.stage);
  sig = sig * 31 + static_cast<uint32_t>(pet::mood(gPet));
  sig = sig * 31 + gPet.poop;
  sig = sig * 31 + (gPet.sick ? 1 : 0);
  sig = sig * 31 + (gPet.asleep ? 1 : 0) + (gPet.lightsOff ? 2 : 0);
  sig = sig * 31 + (pet::needsAttention(gPet) ? 1 : 0);
  sig = sig * 31 + static_cast<uint32_t>(hal::batteryPercent() / 10);
  return sig;
}

void drawPoweredOff(bool full) {
  ui::View v = makeView();
  v.menuCursor = -1;
  v.message = nullptr;
  v.prop = nullptr;
  v.forcePose = false;
  v.poweredOff = true;
  if (gPet.stage == pet::Stage::Dead) {
    ui::drawDead(gCanvas, v);
  } else {
    ui::drawHome(gCanvas, v);
  }
  // Blank out the clock area; time is meaningless on a frozen screen.
  gCanvas.fillRect(0, 0, 70, 19, ui::PAPER);
  hal::present(gCanvas, !full);
  gPersist.screenSig = screenSignature();
}

// Alert once each time the pet starts needing attention (or dies).
void checkAttention() {
  const bool need = pet::needsAttention(gPet) || gPet.stage == pet::Stage::Dead;
  if (need && !gPersist.attentionAlerted) {
    alert();
    hal::led(true);
    hal::delayMs(200);
    hal::led(false);
  }
  gPersist.attentionAlerted = need;
}

void enterInteractive() {
  gScreen = gPet.stage == pet::Stage::Dead ? Screen::Dead : Screen::Home;
  gCursor = 0;
  gFullRefresh = true;
  gDirty = true;
  gLastInput = hal::millis32();
  gLastAnim = gLastInput;
}

// RTC wake-up interval; shorter while napping so the screen shows it awake on time.
uint32_t sleepSeconds() {
  const uint32_t nap = pet::napMinutesLeft(gPet) * 60;
  return nap && nap < kSleepSeconds ? nap : kSleepSeconds;
}

// Sleep until the side button; on timer wake-ups update the pet quietly.
void sleepUntilButton() {
  for (;;) {
    hal::sleepFor(sleepSeconds());
    if (hal::wakeReason() != hal::WakeReason::Timer) break;
    pet::tick(gPet, hal::now());
    checkAttention();
    if (screenSignature() != gPersist.screenSig) drawPoweredOff(false);
    savePet();
  }
  pet::tick(gPet, hal::now());
  enterInteractive();
  gFullRefresh = false;  // the panel still holds the last frame: a fast refresh is enough
}

// Power down until the side button is pressed.
void powerCycle() {
  pet::tick(gPet, hal::now());
  drawPoweredOff(true);
  savePet();
  sleepUntilButton();
}

// Show a random event that happened (possibly while powered off).
void showPendingEvent() {
  savePet();  // records it in the album before it is taken
  const pet::Event e = pet::takeEvent(gPet);
  switch (e) {
    case pet::Event::MandoVisit:
      say("¡Visita de Mando!", &sprites::kMandoSprite, kEventMessageMs);
      break;
    case pet::Event::FrogHop: say("¡Una rana! +2", &sprites::kFrogSprite, kEventMessageMs); break;
    case pet::Event::MacaronTheft:
      say("¡Robó un macarón!", &sprites::kMacaronSprite, kEventMessageMs);
      gPose = true;
      break;
    default: return;
  }
  chirp();
  savePet();
}

void reactTo(pet::Result r, const char* ok, const char* notNeeded, const char* refused,
             const sprites::Sprite* prop = nullptr) {
  if (gPet.stage == pet::Stage::Egg && r == pet::Result::Refused) refused = "¡Es un huevo!";
  switch (r) {
    case pet::Result::Ok: chirp(); say(ok, prop); break;
    case pet::Result::NotNeeded: say(notNeeded); break;
    case pet::Result::Refused: buzz(); say(refused); break;
  }
  savePet();
}

void addRow(const char* label, const sprites::Sprite* icon, int action, const char* right = "") {
  if (gRowCount >= ui::kListMax) return;
  ui::ListRow& r = gRows[gRowCount];
  r.label = label;
  r.icon = icon;
  snprintf(r.right, sizeof(r.right), "%s", right);
  gActions[gRowCount++] = action;
}

void openList(Screen s) {
  gRowCount = 0;
  gListCursor = 0;
  gMsg = nullptr;  // a stale bubble would show up as the list title
  char buf[8];
  switch (s) {
    case Screen::Feed:
      addRow("Sopa", &sprites::kFoodSprite, kMeal);
      addRow("Galleta", &sprites::kCandySprite, kSnack);
      for (int i = 0; i < pet::kItemCount; ++i) {
        const auto it = static_cast<pet::Item>(i);
        if (it == pet::Item::Bacta || it == pet::Item::Knob || !gPet.items[i]) continue;
        snprintf(buf, sizeof(buf), "x%u", (unsigned)gPet.items[i]);
        addRow(pet::itemName(it), kItemSprites[i], i, buf);
      }
      break;
    case Screen::Cure:
      addRow("Medicina", &sprites::kIconMedicineSprite, kMedicine);
      snprintf(buf, sizeof(buf), "x%u", (unsigned)gPet.items[static_cast<int>(pet::Item::Bacta)]);
      addRow("Bacta", &sprites::kBactaSprite, static_cast<int>(pet::Item::Bacta), buf);
      break;
    case Screen::Games:
      addRow("Izq/Der", &sprites::kIconArrowUpSprite, kLeftRight);
      addRow("Fuerza", &sprites::kSparkleSprite, kForceGame);
      addRow("Más/Menos", &sprites::kIconUpDownSprite, kHighLowGame);
      addRow("Ordenar", &sprites::kIconTidySprite, kTidyGame);
      addRow("¿Alegría?", &sprites::kHeartSprite, kJoyGame);
      addRow("Vestir", &sprites::kIconDressSprite, kDressUp);
      break;
    case Screen::More:
      addRow("Tienda", &sprites::kIconShopSprite, kOpenShop);
      addRow("Info", &sprites::kIconStatsSprite, kOpenStats);
      addRow("Álbum", &sprites::kIconAlbumSprite, kOpenAlbum);
      break;
    case Screen::Shop:
      for (int i = 0; i < pet::kItemCount; ++i) {
        const auto it = static_cast<pet::Item>(i);
        if (it == pet::Item::Knob && gPet.items[i]) {
          snprintf(buf, sizeof(buf), "ya");
        } else {
          snprintf(buf, sizeof(buf), "%u", (unsigned)rules::kItemPrice[i]);
        }
        addRow(pet::itemName(it), kItemSprites[i], i, buf);
      }
      break;
    default: return;
  }
  gScreen = s;
  gDirty = true;
}

// Says why it won't play; true if it won't.
bool refusesToPlay() {
  if (pet::canPlay(gPet) == pet::Result::Ok) return false;
  goHome();
  const char* why = gPet.asleep ? "Zzz..." : gPet.sick ? "Estoy malo" : "Quiero siesta";
  reactTo(pet::Result::Refused, "", "", why);
  return true;
}

void startGame(Screen s) {
  if (refusesToPlay()) return;
  gPhase = false;
  if (s == Screen::LeftRight) {
    games::start(gLR, seed());
  } else if (s == Screen::HighLow) {
    games::start(gHL, seed());
  } else if (s == Screen::Tidy) {
    games::start(gTidy, seed());
  } else if (s == Screen::Joy) {
    games::start(gJoy, seed());
    gPhase = true;  // show what sparks joy
    gPhaseUntil = hal::millis32() + kJoyMemoriseMs;
  } else {
    games::start(gForce, seed());
    gPhase = true;  // show the first sequence
    gPhaseUntil = hal::millis32() + 1500 + 400 * gForce.len;
  }
  gScreen = s;
  gFullRefresh = true;
  gDirty = true;
}

void finishGame(pet::Game g, uint8_t score) {
  const uint8_t frogs = pet::playReward(gPet, g, score);
  pet::albumNoteScore(gPersist.album, g, score);
  gScreen = Screen::Home;
  gFullRefresh = true;
  if (pet::gameWon(g, score)) {
    snprintf(gMsgBuf, sizeof(gMsgBuf), "¡Ganaste! +%u", (unsigned)frogs);
    chirp();
    say(gMsgBuf, &sprites::kFrogSprite);
  } else {
    buzz();
    say("¡Otra vez!");
  }
  savePet();
}

void openDress() {
  if (gPet.asleep) return reactTo(pet::Result::Refused, "", "", "Zzz...");
  if (gPet.stage == pet::Stage::Egg) return reactTo(pet::Result::Refused, "", "", "");
  gDressHat = pet::hatOf(gPet);
  gDressNeck = pet::neckOf(gPet);
  gDressRow = 0;
  gScreen = Screen::Dress;
  gFullRefresh = true;
  gDirty = true;
}

void activateListRow() {
  if (gRowCount == 0) return goHome();
  const int a = gActions[gListCursor];
  const Screen from = gScreen;
  if (from == Screen::Shop) {
    // Stay in the shop to buy more; refresh the counts, then react.
    const pet::Result r = pet::buy(gPet, static_cast<pet::Item>(a));
    const int keep = gListCursor;
    openList(Screen::Shop);
    gListCursor = keep;
    return reactTo(r, "¡Comprado!", "Ya lo tienes", "Faltan ranas");
  }
  goHome();
  switch (a) {
    case kMeal:
      return reactTo(pet::feedMeal(gPet), "¡Ñam!", "Estoy lleno", "¡No!", &sprites::kFoodSprite);
    case kSnack:
      return reactTo(pet::feedSnack(gPet), "¡Qué rico!", "", "¡No!", &sprites::kCandySprite);
    case kMedicine: return reactTo(pet::medicine(gPet), "¡Curado!", "No está malo", "");
    case kLeftRight: return startGame(Screen::LeftRight);
    case kForceGame: return startGame(Screen::Force);
    case kHighLowGame: return startGame(Screen::HighLow);
    case kTidyGame: return startGame(Screen::Tidy);
    case kJoyGame: return startGame(Screen::Joy);
    case kDressUp: return openDress();
    case kOpenShop: return openList(Screen::Shop);
    case kOpenStats:
      gScreen = Screen::Stats;
      gFullRefresh = true;
      return;
    case kOpenAlbum:
      gScreen = Screen::Album;
      gAlbumPage = 0;
      gFullRefresh = true;
      return;
    default: break;
  }
  if (a >= 0 && a < pet::kItemCount) {
    const auto it = static_cast<pet::Item>(a);
    const char* ok = it == pet::Item::Bacta ? "¡Como nuevo!" : "¡Qué rico!";
    const char* notNeeded = it == pet::Item::Bacta ? "Estoy bien" : "Estoy lleno";
    reactTo(pet::useItem(gPet, it), ok, notNeeded, "¡No!", kItemSprites[a]);
  }
}

void activate(ui::MenuItem item) {
  switch (item) {
    case ui::MenuItem::Feed:
      if (gPet.stage == pet::Stage::Egg) return reactTo(pet::Result::Refused, "", "", "");
      if (gPet.asleep) return reactTo(pet::Result::Refused, "", "", "Zzz...");
      openList(Screen::Feed);
      break;
    case ui::MenuItem::Play:
      if (!refusesToPlay()) openList(Screen::Games);
      break;
    case ui::MenuItem::Light: {
      const bool wasNapping = gPet.napping;
      const bool wasAsleep = gPet.asleep;
      const pet::Result r = pet::toggleLights(gPet);
      const char* ok = gPet.napping                ? "¡A la siesta!"
                       : gPet.nightAwake           ? "¡Uaah! ¿Qué pasa?"
                       : wasNapping                ? "¡Ya despierto!"
                       : !wasAsleep && gPet.asleep ? "¡Buenas noches!"
                       : gPet.lightsOff            ? "Luz apagada"
                                                   : "Luz encendida";
      reactTo(r, ok, "", "");
      break;
    }
    case ui::MenuItem::Clean:
      reactTo(pet::clean(gPet), "¡Limpito!", "Ya está limpio", "");
      break;
    case ui::MenuItem::Medicine:
      if (gPet.items[static_cast<int>(pet::Item::Bacta)] && gPet.stage != pet::Stage::Egg) {
        openList(Screen::Cure);
      } else {
        reactTo(pet::medicine(gPet), "¡Curado!", "No está malo", "");
      }
      break;
    case ui::MenuItem::Scold:
      reactTo(pet::scold(gPet), "¡Vale...!", "¿Por qué?", gPet.asleep ? "Zzz..." : "");
      break;
    case ui::MenuItem::More: openList(Screen::More); break;
    default: break;
  }
}

void handleLeftRight(hal::Event e) {
  if (e == hal::Event::Back) return goHome();
  if (gPhase || (e != hal::Event::Up && e != hal::Event::Down)) return;
  const bool hit = games::guess(gLR, e == hal::Event::Up ? games::Left : games::Right);
  hit ? chirp() : buzz();
  gPhase = true;
  gPhaseUntil = hal::millis32() + kRevealMs;
  gDirty = true;
}

void handleHighLow(hal::Event e) {
  if (e == hal::Event::Back) return goHome();
  if (gPhase || (e != hal::Event::Up && e != hal::Event::Down)) return;
  const bool hit = games::guess(gHL, e == hal::Event::Up);
  hit ? chirp() : buzz();
  gPhase = true;
  gPhaseUntil = hal::millis32() + kRevealMs;
  gDirty = true;
}

void handleTidy(hal::Event e) {
  if (gPhase) return;  // admiring the result
  if (e == hal::Event::Back) {
    if (gTidy.held < 0) return goHome();
    games::putDown(gTidy);
  } else if (e == hal::Event::Up || e == hal::Event::Down) {
    games::move(gTidy, e == hal::Event::Up ? -1 : 1);
  } else if (e == hal::Event::Select) {
    const uint8_t swaps = gTidy.swaps;
    games::press(gTidy);
    if (gTidy.swaps != swaps) chirp();
    if (games::done(gTidy)) {
      gPhase = true;
      gPhaseUntil = hal::millis32() + kTidyDoneMs;
    }
  }
  gDirty = true;
}

void handleJoy(hal::Event e) {
  if (e == hal::Event::Back) return goHome();
  if (gPhase || (e != hal::Event::Up && e != hal::Event::Down)) return;
  const bool hit = games::guess(gJoy, e == hal::Event::Up);
  hit ? chirp() : buzz();
  gPhase = true;
  gPhaseUntil = hal::millis32() + kRevealMs;
  gDirty = true;
}

void handleDress(hal::Event e) {
  if (e == hal::Event::Back) {
    gScreen = Screen::Home;
    gFullRefresh = true;
  } else if (e == hal::Event::Up) {
    gDressRow = (gDressRow + ui::kDressRows - 1) % ui::kDressRows;
  } else if (e == hal::Event::Down) {
    gDressRow = (gDressRow + 1) % ui::kDressRows;
  } else if (e == hal::Event::Select) {
    if (gDressRow == 0) {
      gDressHat = static_cast<pet::Hat>((static_cast<int>(gDressHat) + 1) % pet::kHatCount);
    } else if (gDressRow == 1) {
      gDressNeck = static_cast<pet::Neck>((static_cast<int>(gDressNeck) + 1) % pet::kNeckCount);
    } else {
      gScreen = Screen::Home;
      gFullRefresh = true;
      reactTo(pet::dress(gPet, gDressHat, gDressNeck), "¡Qué guapo!", "¡Me gusta!", "Zzz...");
    }
  }
  gDirty = true;
}

void handleForce(hal::Event e) {
  if (e == hal::Event::Back) return goHome();
  if (gPhase) return;  // still memorising
  games::Sym sym;
  if (e == hal::Event::Up) sym = games::Up;
  else if (e == hal::Event::Down) sym = games::Down;
  else if (e == hal::Event::Select) sym = games::Press;
  else return;
  switch (games::input(gForce, sym)) {
    case games::Step::Wrong:
    case games::Step::Finished: return finishGame(pet::Game::Force, gForce.best);
    case games::Step::Complete:
      chirp();
      gPhase = true;
      gPhaseUntil = hal::millis32() + 1500 + 400 * gForce.len;
      break;
    case games::Step::Next: break;
  }
  gDirty = true;
}

void handle(hal::Event e) {
  gLastInput = hal::millis32();
  switch (e) {
    case hal::Event::Refresh: gFullRefresh = gDirty = true; return;
    case hal::Event::NewPet: startNewPet(); return;
    case hal::Event::SleepNow: powerCycle(); return;
    case hal::Event::FastForward:
    case hal::Event::ToggleSpeed: gDirty = true; return;
    default: break;
  }

  switch (gScreen) {
    case Screen::Home:
      if (e == hal::Event::Up) gCursor = (gCursor + ui::kMenuCount - 1) % ui::kMenuCount;
      if (e == hal::Event::Down) gCursor = (gCursor + 1) % ui::kMenuCount;
      if (e == hal::Event::Select) activate(static_cast<ui::MenuItem>(gCursor));
      gDirty = true;
      break;
    case Screen::Feed:
    case Screen::Cure:
    case Screen::Games:
    case Screen::More:
    case Screen::Shop:
      if (gRowCount > 0) {
        if (e == hal::Event::Up) gListCursor = (gListCursor + gRowCount - 1) % gRowCount;
        if (e == hal::Event::Down) gListCursor = (gListCursor + 1) % gRowCount;
      }
      if (e == hal::Event::Back) goHome();
      if (e == hal::Event::Select) activateListRow();
      gDirty = true;
      break;
    case Screen::Stats:
      if (e == hal::Event::Back || e == hal::Event::Select) {
        gScreen = Screen::Home;
        gFullRefresh = true;
      }
      gDirty = true;
      break;
    case Screen::Album:
      if (e == hal::Event::Up || e == hal::Event::Down) gAlbumPage = (gAlbumPage + 1) % ui::kAlbumPages;
      if (e == hal::Event::Back || e == hal::Event::Select) gScreen = Screen::Home;
      gFullRefresh = true;
      gDirty = true;
      break;
    case Screen::LeftRight: handleLeftRight(e); break;
    case Screen::Force: handleForce(e); break;
    case Screen::HighLow: handleHighLow(e); break;
    case Screen::Tidy: handleTidy(e); break;
    case Screen::Joy: handleJoy(e); break;
    case Screen::Dress: handleDress(e); break;
    case Screen::Dead:
      if (e == hal::Event::Select) startNewPet();
      gDirty = true;
      break;
  }
}

// Timed phases of the minigames.
void updateGames(uint32_t ms) {
  if (!gPhase || static_cast<int32_t>(ms - gPhaseUntil) < 0) return;
  if (gScreen == Screen::LeftRight) {
    gPhase = false;
    gDirty = true;
    if (games::done(gLR)) finishGame(pet::Game::LeftRight, gLR.correct);
  } else if (gScreen == Screen::HighLow) {
    gPhase = false;
    gDirty = true;
    if (games::done(gHL)) finishGame(pet::Game::HighLow, gHL.streak);
  } else if (gScreen == Screen::Joy) {
    gPhase = false;
    gDirty = true;
    if (games::done(gJoy)) finishGame(pet::Game::Joy, gJoy.correct);
  } else if (gScreen == Screen::Tidy) {
    gPhase = false;
    finishGame(pet::Game::Tidy, games::stars(gTidy));
  } else if (gScreen == Screen::Force) {
    gPhase = false;
    gDirty = true;
  }
}

void update() {
  const pet::Stage before = gPet.stage;
  if (pet::tick(gPet, hal::now()) > 0) {
    gDirty = true;  // stats and clock changed
    if (gPet.stage != before) {
      gFullRefresh = true;
      if (gPet.stage == pet::Stage::Dead) {
        gScreen = Screen::Dead;
      } else {
        gScreen = Screen::Home;
        say(before == pet::Stage::Egg ? "¡Nació!" : "¡Creció!");
      }
    }
    checkAttention();
    savePet();
  }

  const uint32_t ms = hal::millis32();
  updateGames(ms);
  if (gMsg && static_cast<int32_t>(ms - gMsgUntil) >= 0) {
    gMsg = nullptr;
    gProp = nullptr;
    gPose = false;
    gDirty = true;
  }
  if (!gMsg && gScreen == Screen::Home && gPet.pendingEvent) showPendingEvent();
  if (ms - gLastAnim >= kAnimMs && (gScreen == Screen::Home || gScreen == Screen::Dead)) {
    gLastAnim = ms;
    gAltFrame = !gAltFrame;
    gDirty = true;
  }
  if (ms - gLastInput >= kIdleSleepMs) {
    powerCycle();
  }
}

}  // namespace

void setup() {
  hal::begin();
  ui::initCanvas(gCanvas);
  loadPet();
  // RTC lost power (or the simulator restarted after fast-forwarding):
  // never let the clock run behind the last save.
  if (hal::now() < gPet.lastUpdate) hal::setClock(gPet.lastUpdate);

  if (hal::wakeReason() == hal::WakeReason::Timer) {
    // Woken by the RTC alarm: quick update, maybe beep, then power off again.
    pet::tick(gPet, hal::now());
    checkAttention();
    if (screenSignature() != gPersist.screenSig) drawPoweredOff(false);
    savePet();
    sleepUntilButton();
    return;
  }

  pet::tick(gPet, hal::now());
  checkAttention();
  savePet();
  enterInteractive();
}

void loop() {
  const hal::Event e = hal::poll();
  if (e != hal::Event::None) handle(e);
  update();
  if (gDirty) render();
  hal::delayMs(10);
}
