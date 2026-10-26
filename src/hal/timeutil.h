#pragma once
#include <stdint.h>

namespace timeutil {

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm).
inline int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = static_cast<uint32_t>(y - era * 400);
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

inline uint32_t toEpoch(int y, int mo, int d, int h, int mi, int s) {
  return static_cast<uint32_t>(daysFromCivil(y, mo, d)) * 86400u + h * 3600u + mi * 60u + s;
}

struct Civil {
  int year, month, day, hour, minute, second;
};

inline Civil fromEpoch(uint32_t t) {
  int32_t z = static_cast<int32_t>(t / 86400) + 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  const uint32_t d = doy - (153 * mp + 2) / 5 + 1;
  const uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  const int32_t y = static_cast<int32_t>(yoe) + era * 400 + (m <= 2);
  const uint32_t sod = t % 86400;
  return {y, static_cast<int>(m), static_cast<int>(d), static_cast<int>(sod / 3600),
          static_cast<int>(sod / 60 % 60), static_cast<int>(sod % 60)};
}

// Build time (__DATE__ "Oct 21 2026", __TIME__ "12:34:56") as epoch.
inline uint32_t buildEpoch() {
  const char* date = __DATE__;
  const char* time = __TIME__;
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  int mo = 1;
  for (int i = 0; i < 12; ++i) {
    if (date[0] == months[i * 3] && date[1] == months[i * 3 + 1] && date[2] == months[i * 3 + 2]) {
      mo = i + 1;
      break;
    }
  }
  const int d = (date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0');
  const int y = (date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0');
  const int h = (time[0] - '0') * 10 + (time[1] - '0');
  const int mi = (time[3] - '0') * 10 + (time[4] - '0');
  const int s = (time[6] - '0') * 10 + (time[7] - '0');
  return toEpoch(y, mo, d, h, mi, s);
}

}  // namespace timeutil
