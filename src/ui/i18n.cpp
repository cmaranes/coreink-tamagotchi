#include "i18n.h"

namespace i18n {

namespace {

constexpr int kStrCount = static_cast<int>(Str::Count);

const char* const kTable[kLangCount][kStrCount] = {
    {
#define S(id, en, es) en,
#include "strings.def"
#undef S
    },
    {
#define S(id, en, es) es,
#include "strings.def"
#undef S
    },
};

Lang gLang = Lang::En;

}  // namespace

void setLang(Lang l) { gLang = static_cast<int>(l) < kLangCount ? l : Lang::En; }

Lang lang() { return gLang; }

const char* tr(Str s, Lang l) { return kTable[static_cast<int>(l)][static_cast<int>(s)]; }

const char* tr(Str s) { return tr(s, gLang); }

const char* stageName(pet::Stage st) {
  switch (st) {
    case pet::Stage::Egg: return tr(Str::StageEgg);
    case pet::Stage::Baby: return tr(Str::StageBaby);
    case pet::Stage::Child: return tr(Str::StageChild);
    case pet::Stage::Teen: return tr(Str::StageTeen);
    case pet::Stage::AdultJedi: return tr(Str::StageJedi);
    case pet::Stage::AdultMando: return tr(Str::StageMando);
    case pet::Stage::AdultGloton: return tr(Str::StageGloton);
    case pet::Stage::AdultGrump: return tr(Str::StageGrump);
    case pet::Stage::Dead: return tr(Str::Rip);
  }
  return "?";
}

const char* causeName(pet::Cause c) {
  switch (c) {
    case pet::Cause::Illness: return tr(Str::CauseIllness);
    case pet::Cause::Starvation: return tr(Str::CauseStarvation);
    case pet::Cause::Neglect: return tr(Str::CauseNeglect);
    case pet::Cause::OldAge: return tr(Str::CauseOldAge);
    default: return "";
  }
}

const char* itemName(pet::Item it) {
  switch (it) {
    case pet::Item::Macaron: return tr(Str::ItemMacaron);
    case pet::Item::EggSoup: return tr(Str::ItemEggSoup);
    case pet::Item::Frog: return tr(Str::ItemFrog);
    case pet::Item::Bacta: return tr(Str::ItemBacta);
    case pet::Item::Knob: return tr(Str::ItemKnob);
    default: return "?";
  }
}

const char* hatName(pet::Hat h) {
  switch (h) {
    case pet::Hat::None: return tr(Str::None);
    case pet::Hat::Beanie: return tr(Str::HatBeanie);
    case pet::Hat::Crown: return tr(Str::HatCrown);
    case pet::Hat::Bow: return tr(Str::HatBow);
    case pet::Hat::Shades: return tr(Str::HatShades);
    default: return "?";
  }
}

const char* neckName(pet::Neck n) {
  switch (n) {
    case pet::Neck::None: return tr(Str::None);
    case pet::Neck::Scarf: return tr(Str::NeckScarf);
    case pet::Neck::BowTie: return tr(Str::NeckBowTie);
    case pet::Neck::Necklace: return tr(Str::NeckNecklace);
    default: return "?";
  }
}

}  // namespace i18n
