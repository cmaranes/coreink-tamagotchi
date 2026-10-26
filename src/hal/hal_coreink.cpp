// CoreInk implementation of the HAL (real hardware).
#if defined(ARDUINO)

#include <Preferences.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "hal.h"
#include "timeutil.h"

namespace hal {

namespace {
WakeReason gWake = WakeReason::PowerOn;
Preferences gPrefs;
// Start at the limit: after a reboot the panel has lost its previous frame,
// so a fast (differential) refresh comes out garbled or black.
int gFullEvery = 12;
}  // namespace

void begin() {
  // Read the wake cause *before* M5.begin() touches the RTC.
  const auto cause = esp_sleep_get_wakeup_cause();

  auto cfg = M5.config();
  cfg.clear_display = false;  // keep the last e-ink image while booting
  cfg.disable_rtc_irq = false;  // keep the alarm flag so getIRQstatus() below can see it
  cfg.serial_baudrate = 115200;  // clock sync over USB, see readClockSync()
  M5.begin(cfg);
  M5.Speaker.setVolume(64);

  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    gWake = WakeReason::Timer;  // deep sleep on USB power
  } else if (M5.Rtc.getIRQstatus()) {
    gWake = WakeReason::Timer;  // RTC alarm switched the power back on
  } else {
    gWake = WakeReason::PowerOn;  // side power button
  }
  M5.Rtc.disableIRQ();
  M5.Rtc.clearIRQ();

  // A dead RTC backup / first boot: start the clock from the build time.
  m5::rtc_datetime_t dt;
  if (M5.Rtc.getVoltLow() || !M5.Rtc.getDateTime(&dt) || dt.date.year < 2024) {
    setClock(timeutil::buildEpoch());
  }

  gPrefs.begin("tama", false);
}

WakeReason wakeReason() { return gWake; }

uint32_t now() {
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt)) return 0;
  return timeutil::toEpoch(dt.date.year, dt.date.month, dt.date.date, dt.time.hours,
                           dt.time.minutes, dt.time.seconds);
}

void setClock(uint32_t epoch) {
  const auto c = timeutil::fromEpoch(epoch);
  m5::rtc_datetime_t dt;
  dt.date.year = c.year;
  dt.date.month = c.month;
  dt.date.date = c.day;
  dt.date.weekDay = (timeutil::daysFromCivil(c.year, c.month, c.day) + 4) % 7;
  dt.time.hours = c.hour;
  dt.time.minutes = c.minute;
  dt.time.seconds = c.second;
  M5.Rtc.setDateTime(dt);
}

// Set the RTC from the PC: a line "T<seconds since 1970, local time>" on the
// USB serial port. Returns true when the clock was changed.
namespace {
bool readClockSync() {
  static char line[16];
  static int len = 0;
  bool synced = false;
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch != '\n' && ch != '\r') {
      if (len < static_cast<int>(sizeof(line)) - 1) line[len++] = ch;
      continue;
    }
    line[len] = '\0';
    if (len > 1 && line[0] == 'T') {
      const uint32_t epoch = strtoul(line + 1, nullptr, 10);
      if (epoch > 1700000000u) {
        setClock(epoch);
        Serial.printf("clock set %lu\n", static_cast<unsigned long>(epoch));
        synced = true;
      }
    }
    len = 0;
  }
  return synced;
}
}  // namespace

Event poll() {
  if (readClockSync()) return Event::Refresh;
  M5.update();
  // CoreInk dial: BtnA = up (G37), BtnB = press (G38), BtnC = down (G39).
  if (M5.BtnA.wasPressed()) return Event::Up;
  if (M5.BtnC.wasPressed()) return Event::Down;
  if (M5.BtnB.wasPressed()) return Event::Select;
  if (M5.BtnEXT.wasPressed()) return Event::Back;  // top button (G5)
  if (M5.BtnPWR.wasPressed()) return Event::Refresh;  // side button
  return Event::None;
}

void delayMs(uint32_t ms) { M5.delay(ms); }
uint32_t millis32() { return ::millis(); }

bool load(void* data, size_t len) {
  return gPrefs.getBytes("state", data, len) == len;
}

void save(const void* data, size_t len) { gPrefs.putBytes("state", data, len); }

int batteryPercent() { return M5.Power.getBatteryLevel(); }

bool charging() { return M5.Power.isCharging() == m5::Power_Class::is_charging_t::is_charging; }

void beep(uint16_t freq, uint32_t ms) { M5.Speaker.tone(freq, ms); }

void led(bool on) { M5.Power.setLed(on ? 255 : 0); }

void present(M5Canvas& canvas, bool fast) {
  // Partial refreshes leave ghosting; clean up periodically with a full one.
  if (fast && ++gFullEvery < 12) {
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  } else {
    gFullEvery = 0;
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
  }
  M5.Display.startWrite();
  canvas.pushSprite(&M5.Display, 0, 0);
  M5.Display.endWrite();
  M5.Display.waitDisplay();
}

void sleepFor(uint32_t seconds) {
  gPrefs.end();
  M5.Display.waitDisplay();
  // Any button wakes it: side (G27), top (G5) and the dial (G37 up, G38 press,
  // G39 down). All are active low with external pull-ups.
  static constexpr gpio_num_t kWakePins[] = {GPIO_NUM_27, GPIO_NUM_5, GPIO_NUM_37, GPIO_NUM_38,
                                             GPIO_NUM_39};
  auto anyDown = [] {
    for (auto pin : kWakePins) if (!digitalRead(pin)) return true;
    return false;
  };
  while (anyDown()) M5.delay(10);  // don't wake on the press that's still down
  // M5.Power.timerSleep can't power the CoreInk off (G19 isn't an RTC GPIO), so
  // it light-sleeps and then restarts on wake, which re-inits the panel (black
  // screen). Light-sleep here instead and resume. The side button can't wake
  // it from light sleep on the device; the top button and the dial can.
  // Power the panel off but keep its frame RAM so the next refresh can be fast.
  M5.Display.powerSaveOn();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  for (auto pin : kWakePins) gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
  const auto cause = esp_sleep_get_wakeup_cause();
  for (auto pin : kWakePins) gpio_wakeup_disable(pin);
  M5.Display.powerSaveOff();
  gWake = cause == ESP_SLEEP_WAKEUP_TIMER ? WakeReason::Timer : WakeReason::Button;
  gPrefs.begin("tama", false);
  M5.update();  // swallow the wake press so it isn't seen as a Refresh
  M5.update();
}

uint32_t timeScale() { return 1; }

}  // namespace hal

#endif  // ARDUINO
