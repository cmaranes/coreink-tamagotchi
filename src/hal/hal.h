// Thin hardware abstraction so the same app runs on the CoreInk and in the
// SDL simulator. Implemented in hal_coreink.cpp (ARDUINO) and hal_sim.cpp.
#pragma once
#include <M5Unified.h>
#include <stddef.h>
#include <stdint.h>

namespace hal {

enum class Event : uint8_t {
  None,
  Up,
  Down,
  Select,
  Back,
  Refresh,      // force a full e-ink refresh
  // Simulator-only developer shortcuts:
  FastForward,  // +1 hour
  ToggleSpeed,  // 1x <-> 60x
  NewPet,
  SleepNow,     // pretend the idle timeout fired
};

enum class WakeReason : uint8_t { PowerOn, Timer, Button };

void begin();
WakeReason wakeReason();

// Seconds since 1970 in *local* time as kept by the RTC.
uint32_t now();
void setClock(uint32_t epoch);

Event poll();  // call often; returns one event per call
void delayMs(uint32_t ms);
uint32_t millis32();

bool load(void* data, size_t len);
void save(const void* data, size_t len);

int batteryPercent();  // -1 if unknown
bool charging();
void beep(uint16_t freq, uint32_t ms);
void led(bool on);

// Push the canvas to the panel. fast = partial refresh (may ghost).
void present(M5Canvas& canvas, bool fast);

// Sleep for `seconds`, or until the side button is pressed (wakeReason()
// tells which). Returns on both the device (light sleep) and the simulator.
void sleepFor(uint32_t seconds);

// Speed multiplier for simulated time (1 on the device).
uint32_t timeScale();

}  // namespace hal
