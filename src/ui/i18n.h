// On-screen text in several languages. The strings live in strings.def; the
// language is picked at runtime (More -> Language) and stored by the HAL.
#pragma once
#include <stdint.h>

#include "../game/pet.h"

namespace i18n {

enum class Lang : uint8_t { En, Es, Count };
constexpr int kLangCount = static_cast<int>(Lang::Count);

enum class Str : uint16_t {
#define S(id, en, es) id,
#include "strings.def"
#undef S
  Count
};

void setLang(Lang l);  // out-of-range values fall back to English
Lang lang();
const char* tr(Str s);
const char* tr(Str s, Lang l);

const char* stageName(pet::Stage st);
const char* causeName(pet::Cause c);
const char* itemName(pet::Item it);
const char* hatName(pet::Hat h);
const char* neckName(pet::Neck n);

}  // namespace i18n
