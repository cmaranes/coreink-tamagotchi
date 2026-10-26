// Desktop simulator implementation of the HAL (SDL window via M5GFX).
#if !defined(ARDUINO)

#include <chrono>
#include <cstdio>
#include <ctime>

#include "hal.h"
#include "timeutil.h"

namespace hal {

namespace {

// Emulated GPIO numbers for keys. 36..39 are already bound by M5GFX to the
// arrow keys: UP=36, RIGHT=37, DOWN=38, LEFT=39.
constexpr uint8_t kPinUp = 36, kPinRight = 37, kPinDown = 38, kPinLeft = 39;
constexpr uint8_t kPinEnter = 60, kPinBack = 61, kPinFast = 62, kPinSpeed = 63, kPinNew = 64,
                  kPinSleep = 65;

struct KeyBinding {
  uint8_t pin;
  Event event;
};
constexpr KeyBinding kBindings[] = {
    {kPinUp, Event::Up},        {kPinDown, Event::Down},           {kPinRight, Event::Select},
    {kPinEnter, Event::Select}, {kPinLeft, Event::Back},           {kPinBack, Event::Back},
    {kPinFast, Event::FastForward}, {kPinSpeed, Event::ToggleSpeed}, {kPinNew, Event::NewPet},
    {kPinSleep, Event::SleepNow},
};
bool gPrev[sizeof(kBindings) / sizeof(kBindings[0])];

const char* kSaveFile = "sim_save.bin";

WakeReason gWake = WakeReason::PowerOn;
uint32_t gScale = 1;
uint32_t gBase = 0;        // local epoch when the simulator started
double gSimMs = 0;         // simulated milliseconds since start
uint32_t gLastReal = 0;

uint32_t realMs() {
  using namespace std::chrono;
  static const auto t0 = steady_clock::now();
  return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now() - t0).count());
}

void advance() {
  const uint32_t r = realMs();
  gSimMs += static_cast<double>(r - gLastReal) * gScale;
  gLastReal = r;
}

}  // namespace

void begin() {
  setvbuf(stdout, nullptr, _IONBF, 0);  // show logs immediately, even when piped
  auto cfg = M5.config();
  M5.begin(cfg);

  using lgfx::Panel_sdl;
  Panel_sdl::addKeyCodeMapping(SDLK_RETURN, kPinEnter);
  Panel_sdl::addKeyCodeMapping(SDLK_SPACE, kPinEnter);
  Panel_sdl::addKeyCodeMapping(SDLK_ESCAPE, kPinBack);
  Panel_sdl::addKeyCodeMapping(SDLK_BACKSPACE, kPinBack);
  Panel_sdl::addKeyCodeMapping(SDLK_f, kPinFast);
  Panel_sdl::addKeyCodeMapping(SDLK_s, kPinSpeed);
  Panel_sdl::addKeyCodeMapping(SDLK_n, kPinNew);
  Panel_sdl::addKeyCodeMapping(SDLK_z, kPinSleep);
  for (auto& p : gPrev) p = false;

  const time_t t = time(nullptr);
  const tm* lt = localtime(&t);
  gBase = timeutil::toEpoch(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour,
                            lt->tm_min, lt->tm_sec);
  gLastReal = realMs();

  printf(
      "CoreInk Tamagotchi simulator\n"
      "  Up/Down   = dial up/down\n"
      "  Enter/Right/Space = dial press (select)\n"
      "  Esc/Left/Backspace = top button (back)\n"
      "  F = fast-forward 1 hour   S = toggle 1x/60x time\n"
      "  N = new pet   Z = go to sleep now (any key wakes)\n"
      "  1..6 = window zoom\n");
}

WakeReason wakeReason() { return gWake; }

uint32_t now() {
  advance();
  return gBase + static_cast<uint32_t>(gSimMs / 1000.0);
}

void setClock(uint32_t epoch) {
  advance();
  gBase = epoch - static_cast<uint32_t>(gSimMs / 1000.0);
}

Event poll() {
  for (size_t i = 0; i < sizeof(kBindings) / sizeof(kBindings[0]); ++i) {
    const bool down = !m5gfx::gpio_in(kBindings[i].pin);  // keys pull the pin low
    const bool pressed = down && !gPrev[i];
    gPrev[i] = down;
    if (!pressed) continue;
    const Event e = kBindings[i].event;
    if (e == Event::FastForward) {
      advance();
      gSimMs += 3600.0 * 1000.0;
      printf("[sim] +1 hour\n");
    } else if (e == Event::ToggleSpeed) {
      advance();
      gScale = gScale == 1 ? 60 : 1;
      printf("[sim] time speed %ux\n", gScale);
    }
    return e;
  }
  return Event::None;
}

void delayMs(uint32_t ms) { M5.delay(ms); }
uint32_t millis32() { return realMs(); }

bool load(void* data, size_t len) {
  FILE* f = fopen(kSaveFile, "rb");
  if (!f) return false;
  const size_t n = fread(data, 1, len, f);
  fclose(f);
  return n == len;
}

void save(const void* data, size_t len) {
  FILE* f = fopen(kSaveFile, "wb");
  if (!f) return;
  fwrite(data, 1, len, f);
  fclose(f);
}

int batteryPercent() { return 87; }
bool charging() { return false; }

void beep(uint16_t freq, uint32_t ms) { printf("[beep %uHz %ums]\n", freq, ms); }

void led(bool on) { printf("[led %s]\n", on ? "on" : "off"); }

void present(M5Canvas& canvas, bool fast) {
  M5.Display.startWrite();
  if (!fast) {
    // Mimic the black/white flash of a full e-ink refresh.
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.endWrite();
    M5.delay(120);
    M5.Display.startWrite();
  }
  canvas.pushSprite(&M5.Display, 0, 0);
  M5.Display.endWrite();
}

void sleepFor(uint32_t seconds) {
  printf("[sim] power off for %u s (x%u speed) - press any key to wake\n", seconds, gScale);
  const uint32_t target = now() + seconds;
  gWake = WakeReason::Timer;
  while (now() < target) {
    const Event e = poll();
    if (e != Event::None && e != Event::FastForward && e != Event::ToggleSpeed) {
      gWake = WakeReason::Button;
      break;
    }
    M5.delay(20);
  }
  printf("[sim] wake (%s)\n", gWake == WakeReason::Button ? "button" : "timer");
}

uint32_t timeScale() { return gScale; }

}  // namespace hal

#endif  // !ARDUINO
