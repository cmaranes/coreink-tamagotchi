#include "render.h"

#include <stdio.h>

#include "../hal/timeutil.h"

namespace ui {

namespace {

constexpr int kStatusH = 20;
constexpr int kMenuY = 152;
constexpr int kPetTop = kStatusH + 1;
constexpr int kPetH = kMenuY - kPetTop;
constexpr int kSlotW = kWidth / kMenuCount;

const sprites::Sprite* const kMenuIcons[] = {
    &sprites::kIconFeedSprite,     &sprites::kIconPlaySprite,  &sprites::kIconLightSprite,
    &sprites::kIconCleanSprite,    &sprites::kIconMedicineSprite, &sprites::kIconScoldSprite,
    &sprites::kIconMoreSprite,
};
const char* const kMenuLabels[] = {"Comer", "Jugar", "Luz", "Lavar", "Cura", "Regañar", "Más"};

const sprites::Sprite* const kItemIcons[] = {
    &sprites::kMacaronSprite, &sprites::kEggSoupSprite, &sprites::kFrogSprite,
    &sprites::kBactaSprite,   &sprites::kKnobSprite,
};
const sprites::Sprite* const kEventIcons[] = {
    &sprites::kMandoSprite, &sprites::kFrogSprite, &sprites::kMacaronSprite,
};
const sprites::Sprite* const kSymIcons[] = {
    &sprites::kIconArrowUpSprite, &sprites::kIconArrowDownSprite, &sprites::kIconDotSprite,
};

// Things for ¿Alegría?, indexed like games::Joy::item.
const sprites::Sprite* const kJoyIcons[] = {
    &sprites::kKnobSprite, &sprites::kFrogSprite,  &sprites::kMacaronSprite,
    &sprites::kSockSprite, &sprites::kShirtSprite, &sprites::kBookSprite,
};
static_assert(sizeof(kJoyIcons) / sizeof(kJoyIcons[0]) == rules::kJoyPool, "one icon per thing");

const sprites::Sprite* const kHatSprites[] = {
    nullptr, &sprites::kHatBeanieSprite, &sprites::kHatCrownSprite, &sprites::kHatBowSprite,
    &sprites::kHatShadesSprite,
};
const sprites::Sprite* const kNeckSprites[] = {
    nullptr, &sprites::kNeckScarfSprite, &sprites::kNeckBowTieSprite,
    &sprites::kNeckNecklaceSprite,
};
static_assert(sizeof(kHatSprites) / sizeof(kHatSprites[0]) == pet::kHatCount, "one per hat");
static_assert(sizeof(kNeckSprites) / sizeof(kNeckSprites[0]) == pet::kNeckCount, "one per neck");

const sprites::Sprite& petSprite(pet::Stage st) {
  switch (st) {
    case pet::Stage::Egg: return sprites::kEggSprite;
    case pet::Stage::Baby: return sprites::kBabySprite;
    case pet::Stage::Child: return sprites::kChildSprite;
    case pet::Stage::Teen: return sprites::kTeenSprite;
    case pet::Stage::AdultJedi: return sprites::kAdultJediSprite;
    case pet::Stage::AdultMando: return sprites::kAdultMandoSprite;
    case pet::Stage::AdultGloton: return sprites::kAdultGlotonSprite;
    case pet::Stage::AdultGrump: return sprites::kAdultGrumpSprite;
    default: return sprites::kGhostSprite;
  }
}

// Font0 only has ASCII glyphs, so Spanish characters are drawn from these
// 5x8 bitmaps in the same style (Font0 cells are 6x8 including spacing).
struct Glyph {
  uint16_t unicode;
  const char* rows[8];
};
const Glyph kGlyphs[] = {
    {0xE1, {"...#.", "..#..", ".###.", "....#", ".####", "#...#", ".####", "....."}},  // á
    {0xE9, {"...#.", "..#..", ".###.", "#...#", "#####", "#....", ".###.", "....."}},  // é
    {0xED, {"...#.", "..#..", ".....", ".##..", "..#..", "..#..", ".###.", "....."}},  // í
    {0xF3, {"...#.", "..#..", ".###.", "#...#", "#...#", "#...#", ".###.", "....."}},  // ó
    {0xFA, {"...#.", "..#..", "#...#", "#...#", "#...#", "#..##", ".##.#", "....."}},  // ú
    {0xFC, {".#.#.", ".....", "#...#", "#...#", "#...#", "#..##", ".##.#", "....."}},  // ü
    {0xF1, {".##.#", "#..#.", ".....", "#.##.", "##..#", "#...#", "#...#", "....."}},  // ñ
    {0xD1, {".##.#", "#..#.", "#...#", "##..#", "#.#.#", "#..##", "#...#", "....."}},  // Ñ
    {0xC1, {"...#.", "..#..", ".###.", "#...#", "#####", "#...#", "#...#", "....."}},  // Á
    {0xC9, {"...#.", "..#..", "#####", "#....", "####.", "#....", "#####", "....."}},  // É
    {0xCD, {"...#.", "..#..", ".###.", "..#..", "..#..", "..#..", ".###.", "....."}},  // Í
    {0xD3, {"...#.", "..#..", ".###.", "#...#", "#...#", "#...#", ".###.", "....."}},  // Ó
    {0xDA, {"...#.", "..#..", "#...#", "#...#", "#...#", "#...#", ".###.", "....."}},  // Ú
    {0xA1, {".....", "..#..", ".....", "..#..", "..#..", "..#..", "..#..", "..#.."}},  // ¡
    {0xBF, {".....", "..#..", ".....", "..#..", ".#...", "#...#", ".###.", "....."}},  // ¿
};

// Decodes one UTF-8 code point (up to U+07FF, enough for Spanish).
uint16_t nextCodePoint(const char*& p) {
  const auto b = static_cast<unsigned char>(*p++);
  if (b < 0x80 || (b & 0xE0) != 0xC0 || !*p) return b;
  return static_cast<uint16_t>(((b & 0x1F) << 6) | (static_cast<unsigned char>(*p++) & 0x3F));
}

int textWidthPx(const char* s, int size) {
  int n = 0;
  while (*s) {
    nextCodePoint(s);
    ++n;
  }
  return n * 6 * size;
}

void text(M5Canvas& c, const char* s, int x, int y, int size, textdatum_t datum, int color = INK) {
  c.setFont(&fonts::Font0);
  c.setTextSize(size);
  c.setTextDatum(top_left);
  c.setTextColor(color);
  const int w = textWidthPx(s, size);
  if (datum == top_center) x -= w / 2;
  else if (datum == top_right) x -= w;

  while (*s) {
    const uint16_t u = nextCodePoint(s);
    if (u < 0x80) {
      const char ascii[2] = {static_cast<char>(u), '\0'};
      c.drawString(ascii, x, y);
    } else {
      for (const auto& g : kGlyphs) {
        if (g.unicode != u) continue;
        for (int row = 0; row < 8; ++row) {
          for (int col = 0; col < 5; ++col) {
            if (g.rows[row][col] == '#') c.fillRect(x + col * size, y + row * size, size, size, color);
          }
        }
      }
    }
    x += 6 * size;
  }
}

void drawBattery(M5Canvas& c, int x, int y, int pct, bool charging) {
  // 22x10 battery outline with fill, nub on the right.
  c.drawRect(x, y, 20, 10, INK);
  c.fillRect(x + 20, y + 3, 2, 4, INK);
  if (pct >= 0) {
    const int w = (pct * 16 + 50) / 100;
    c.fillRect(x + 2, y + 2, w, 6, INK);
  }
  if (charging) {
    // little lightning bolt cut out of the fill
    c.drawLine(x + 11, y + 1, x + 8, y + 5, PAPER);
    c.drawLine(x + 8, y + 5, x + 12, y + 5, PAPER);
    c.drawLine(x + 12, y + 5, x + 9, y + 9, PAPER);
  }
}

void drawStatusBar(M5Canvas& c, const View& v) {
  const auto t = timeutil::fromEpoch(v.now);
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.hour, t.minute);
  text(c, buf, 3, 3, 2, top_left);

  drawBattery(c, kWidth - 26, 5, v.battery, v.charging);
  if (v.battery >= 0) {
    snprintf(buf, sizeof(buf), "%d%%", v.battery);
    text(c, buf, kWidth - 30, 6, 1, top_right);
  }

  if (pet::needsAttention(*v.pet)) {
    // Inverted "!" call indicator, like the attention icon on the original.
    c.fillRoundRect(98, 2, 16, 16, 3, INK);
    text(c, "!", 106, 3, 2, top_center, PAPER);
  }
  c.drawFastHLine(0, kStatusH, kWidth, INK);
}

void drawBubble(M5Canvas& c, const char* msg, int fg, int bg) {
  // Long messages drop to the small font rather than spill off screen.
  const int size = textWidthPx(msg, 2) + 12 > kWidth ? 1 : 2;
  const int w = textWidthPx(msg, size) + 12;
  const int x = (kWidth - w) / 2;
  const int y = kPetTop + 4;
  c.fillRoundRect(x, y, w, 22, 6, bg);
  c.drawRoundRect(x, y, w, 22, 6, fg);
  text(c, msg, kWidth / 2, y + (size == 2 ? 4 : 8), size, top_center, fg);
}

// Rounded box with a "?" for album entries not discovered yet.
void drawUnknown(M5Canvas& c, int x, int y, int w, int h) {
  c.drawRoundRect(x, y, w, h, 4, INK);
  const int size = h >= 40 ? 3 : 2;
  text(c, "?", x + w / 2, y + (h - 8 * size) / 2, size, top_center);
}

// Frog icon followed by a count, right-aligned at x.
void drawFrogs(M5Canvas& c, int right, int y, unsigned frogs, int color = INK) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", frogs);
  const int w = textWidthPx(buf, 2);
  text(c, buf, right, y, 2, top_right, color);
  drawSprite(c, sprites::kFrogSprite, right - w - 18, y, 2, {false, false, color});
}

void drawMenu(M5Canvas& c, const View& v) {
  c.drawFastHLine(0, kMenuY, kWidth, INK);
  for (int i = 0; i < kMenuCount; ++i) {
    const int slotX = i * kSlotW + (kWidth - kSlotW * kMenuCount) / 2;
    const int ix = slotX + (kSlotW - 24) / 2;
    const bool sel = i == v.menuCursor;
    int color = INK;
    if (sel) {
      // Slots are narrower than some labels, so the label sits below the
      // highlight in ink and may overhang the neighbouring (empty) space.
      c.fillRoundRect(slotX + 1, kMenuY + 3, kSlotW - 2, 33, 5, INK);
      color = PAPER;
      text(c, kMenuLabels[i], slotX + kSlotW / 2, kMenuY + 38, 1, top_center);
    }
    SpriteOpts o;
    o.color = color;
    drawSprite(c, *kMenuIcons[i], ix, sel ? kMenuY + 7 : kMenuY + 12, 2, o);
  }
}

}  // namespace

void initCanvas(M5Canvas& c) {
  c.setColorDepth(1);
  c.createSprite(kWidth, kHeight);
  c.createPalette();
  c.setPaletteColor(PAPER, 255, 255, 255);
  c.setPaletteColor(INK, 0, 0, 0);
  c.fillSprite(PAPER);
}

void drawSprite(M5Canvas& c, const sprites::Sprite& s, int x, int y, int scale,
                const SpriteOpts& o) {
  for (int row = 0; row < s.h; ++row) {
    const char* line = s.rows[row];
    const char* below = row + 1 < s.h ? s.rows[row + 1] : nullptr;
    bool lineEnded = false;
    for (int col = 0; col < s.w; ++col) {
      const char ch = lineEnded ? '.' : line[col];
      if (ch == '\0') {
        lineEnded = true;
        continue;
      }
      bool on = false;
      int dx = 0;
      int color = o.color;
      switch (ch) {
        case '#': on = true; break;
        case ',':
          on = o.paper >= 0;
          color = o.paper;
          break;
        case 'e':
          on = !o.eyesClosed || !below || below[col] != 'e';
          dx = o.look;
          break;
        case 'm':
          on = !o.sad;
          dx = o.look;
          break;
        case 'n':
          on = o.sad;
          dx = o.look;
          break;
        default: break;
      }
      if (on) c.fillRect(x + (col + dx) * scale, y + row * scale, scale, scale, color);
    }
  }
}

namespace {

// Hat and neck item over a pet sprite drawn at (x, y). The baby sits two rows
// lower in its sprite than the others.
void drawOutfit(M5Canvas& c, pet::Stage st, pet::Hat hat, pet::Neck neck, int x, int y,
                int scale, int fg, int bg) {
  if (st == pet::Stage::Egg || st == pet::Stage::Dead) return;
  const int headTop = st == pet::Stage::Baby ? 3 : 1;
  SpriteOpts o;
  o.color = fg;
  o.paper = bg;
  if (const sprites::Sprite* n = kNeckSprites[static_cast<int>(neck)]) {
    drawSprite(c, *n, x, y + (headTop + 12) * scale, scale, o);
  }
  if (const sprites::Sprite* h = kHatSprites[static_cast<int>(hat)]) {
    drawSprite(c, *h, x, y + (headTop - 3) * scale, scale, o);
  }
}

}  // namespace

void drawHome(M5Canvas& c, const View& v) {
  const pet::State& s = *v.pet;
  c.fillSprite(PAPER);
  drawStatusBar(c, v);

  const bool dark = s.asleep && s.lightsOff;
  const int fg = dark ? PAPER : INK;
  if (dark) c.fillRect(0, kPetTop, kWidth, kPetH, INK);

  // Pet position: centred, or shifted left to make room for poop.
  const int scale = 4;
  const bool posing = v.forcePose && s.stage != pet::Stage::Egg;
  const sprites::Sprite& spr = posing ? sprites::kForcePoseSprite : petSprite(s.stage);
  const int pw = spr.w * scale;
  const int ph = spr.h * scale;
  int px = s.poop ? 24 : (kWidth - pw) / 2;
  int py = kPetTop + (kPetH - ph) / 2 + 6;
  const pet::Mood m = pet::mood(s);
  if (!v.poweredOff && v.altFrame) {
    if (s.stage == pet::Stage::Egg) px += 4;          // egg wobbles
    else if (m != pet::Mood::Sleeping) py -= 4;      // others hop
  }

  SpriteOpts o;
  o.eyesClosed = m == pet::Mood::Sleeping;
  o.sad = m == pet::Mood::Sad || s.sick;
  o.color = fg;
  drawSprite(c, spr, px, py, scale, o);
  drawOutfit(c, s.stage, pet::hatOf(s), pet::neckOf(s), px, py, scale, fg, dark ? INK : PAPER);

  if (s.asleep) {
    text(c, "z", px + pw - 22, py + 2, 2, top_left, fg);
    text(c, "Z", px + pw - 10, py - 12, 3, top_left, fg);
  }
  if (s.sick) drawSprite(c, sprites::kSkullSprite, px + 4, py, 2, {false, false, fg});

  for (int i = 0; i < s.poop; ++i) {
    drawSprite(c, sprites::kPoopSprite, 136 + (i % 2) * 30, kMenuY - 30 - (i / 2) * 30, 3,
               {false, false, fg});
  }

  if (v.prop) {
    // Small props at x3, bigger ones (Mando) at x2; keep them on screen.
    const int ps = v.prop->w > 8 ? 2 : 3;
    const int w = v.prop->w * ps;
    const int x = px + pw + 2 > kWidth - w - 2 ? kWidth - w - 2 : px + pw + 2;
    drawSprite(c, *v.prop, x, py + ph - v.prop->h * ps - 14, ps, {false, false, fg});
  }

  if (v.poweredOff) {
    drawBubble(c, s.asleep ? "zzz..." : "¡adiós!", fg, dark ? INK : PAPER);
  } else if (v.message) {
    drawBubble(c, v.message, fg, dark ? INK : PAPER);
  }

  if (v.poweredOff) {
    c.drawFastHLine(0, kMenuY, kWidth, INK);
    text(c, "pulsa el botón lateral", kWidth / 2, kMenuY + 12, 1, top_center);
    text(c, "para despertarme", kWidth / 2, kMenuY + 26, 1, top_center);
  } else {
    drawMenu(c, v);
  }
}

void drawListPopup(M5Canvas& c, const View& v, const char* title, const char* titleRight,
                   const ListRow* rows, int n, int cursor) {
  View under = v;
  under.message = nullptr;
  under.prop = nullptr;
  drawHome(c, under);
  constexpr int kVisible = 4, kRowH = 22;
  const int x = 16, y = 24, w = 168, h = 30 + kVisible * kRowH;
  c.fillRoundRect(x, y, w, h, 8, PAPER);
  c.drawRoundRect(x, y, w, h, 8, INK);
  c.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 6, INK);
  // A reaction (e.g. after buying) replaces the title while it lasts.
  if (v.message) text(c, v.message, x + 10, y + 11, 1, top_left);
  else text(c, title, x + 10, y + 7, 2, top_left);
  if (titleRight) text(c, titleRight, x + w - 10, y + 7, 2, top_right);
  if (!titleRight && v.pet) drawFrogs(c, x + w - 10, y + 7, v.pet->frogs);

  int first = cursor - kVisible + 1;
  if (first < 0) first = 0;
  for (int i = first; i < n && i < first + kVisible; ++i) {
    const int iy = y + 28 + (i - first) * kRowH;
    const bool sel = i == cursor;
    if (sel) c.fillRoundRect(x + 6, iy - 1, w - 12, kRowH - 2, 4, INK);
    const int col = sel ? PAPER : INK;
    if (rows[i].icon) {
      const int sc = rows[i].icon->w > 8 ? 1 : 2;
      drawSprite(c, *rows[i].icon, x + 12, iy + 2, sc, {false, false, col});
    }
    text(c, rows[i].label, x + 34, iy + 2, 2, top_left, col);
    text(c, rows[i].right, x + w - 12, iy + 2, 2, top_right, col);
  }
  // Scroll hints.
  if (first > 0) c.fillTriangle(x + w / 2 - 4, y + 27, x + w / 2 + 4, y + 27, x + w / 2, y + 23, INK);
  if (first + kVisible < n) {
    const int by = y + h - 4;
    c.fillTriangle(x + w / 2 - 4, by - 4, x + w / 2 + 4, by - 4, x + w / 2, by, INK);
  }
}

void drawStats(M5Canvas& c, const View& v) {
  const pet::State& s = *v.pet;
  c.fillSprite(PAPER);
  drawStatusBar(c, v);

  char buf[32];
  snprintf(buf, sizeof(buf), "%s  #%u", pet::stageName(s.stage), (unsigned)s.generation);
  text(c, buf, 6, 26, 2, top_left);
  const uint32_t days = s.ageMinutes / (24 * 60);
  const uint32_t hours = s.ageMinutes / 60 % 24;
  snprintf(buf, sizeof(buf), "Edad %ud %uh  Peso %ug", (unsigned)days, (unsigned)hours,
           (unsigned)s.weight);
  text(c, buf, 6, 46, 1, top_left);

  drawFrogs(c, kWidth - 6, 41, s.frogs);

  struct Bar {
    const char* label;
    float value;
  } bars[] = {{"Comida", s.fullness},  {"Alegría", s.happiness}, {"Energía", s.energy},
              {"Limpieza", s.hygiene}, {"Salud", s.health},      {"Disciplina", s.discipline}};
  int y = 58;
  for (const auto& b : bars) {
    text(c, b.label, 3, y + 3, 1, top_left);
    c.drawRect(66, y, 130, 13, INK);
    const int w = static_cast<int>(b.value * 126 / 100 + 0.5f);
    c.fillRect(68, y + 2, w, 9, INK);
    y += 16;
  }
  snprintf(buf, sizeof(buf), "Descuidos: %u", (unsigned)s.careMistakes);
  text(c, buf, 6, y + 3, 1, top_left);
  if (s.sick) text(c, "ENFERMO", 194, y + 3, 1, top_right);
  text(c, "botón superior: volver", kWidth / 2, kHeight - 12, 1, top_center);
}

void drawDead(M5Canvas& c, const View& v) {
  const pet::State& s = *v.pet;
  c.fillSprite(PAPER);
  drawStatusBar(c, v);
  const sprites::Sprite& ghost = sprites::kGhostSprite;
  drawSprite(c, ghost, (kWidth - ghost.w * 3) / 2, v.altFrame ? 22 : 25, 3);
  text(c, "D.E.P.", kWidth / 2, 98, 3, top_center);
  char buf[40];
  snprintf(buf, sizeof(buf), "%s, %u días", pet::stageName(s.lastStage),
           (unsigned)(s.ageMinutes / (24 * 60)));
  text(c, buf, kWidth / 2, 128, 1, top_center);
  snprintf(buf, sizeof(buf), "murió de %s", pet::causeName(s.cause));
  text(c, buf, kWidth / 2, 142, 1, top_center);
  c.fillRoundRect(30, 166, 140, 24, 6, INK);
  text(c, "pulsa: nuevo huevo", kWidth / 2, 174, 1, top_center, PAPER);
}

void drawLeftRight(M5Canvas& c, const View& v, const games::LeftRight& g, bool reveal) {
  c.fillSprite(PAPER);
  drawStatusBar(c, v);
  text(c, "¿Izquierda o derecha?", kWidth / 2, kPetTop + 5, 1, top_center);

  const sprites::Sprite& spr = petSprite(v.pet->stage);
  const int scale = 3;
  int px = (kWidth - spr.w * scale) / 2;
  const int py = kPetTop + 18;
  SpriteOpts o;
  if (reveal) {
    const bool left = g.lastLook == games::Left;
    o.look = left ? -1 : 1;
    o.sad = !g.lastHit;
    px += left ? -30 : 30;
    const sprites::Sprite& mark = g.lastHit ? sprites::kCheckSprite : sprites::kCrossSprite;
    drawSprite(c, mark, left ? kWidth - 44 : 20, py + 24, 3);
  } else {
    text(c, "?", 30, py + 22, 3, top_center);
    text(c, "?", kWidth - 30, py + 22, 3, top_center);
  }
  drawSprite(c, spr, px, py, scale, o);

  // One box per round: tick, cross or still empty.
  constexpr int kBox = 18, kGap = 6;
  const int bx = (kWidth - rules::kLeftRightRounds * (kBox + kGap) + kGap) / 2;
  for (int i = 0; i < rules::kLeftRightRounds; ++i) {
    const int x = bx + i * (kBox + kGap);
    c.drawRoundRect(x, kMenuY - 26, kBox, kBox, 3, INK);
    if (i < g.round) {
      const sprites::Sprite& m = (g.history >> i & 1) ? sprites::kCheckSprite : sprites::kCrossSprite;
      drawSprite(c, m, x + 1, kMenuY - 25, 2);
    }
  }

  c.drawFastHLine(0, kMenuY, kWidth, INK);
  drawSprite(c, sprites::kIconArrowUpSprite, 14, kMenuY + 12, 2);
  text(c, "izquierda", 42, kMenuY + 20, 1, top_left);
  drawSprite(c, sprites::kIconArrowDownSprite, 104, kMenuY + 12, 2);
  text(c, "derecha", 132, kMenuY + 20, 1, top_left);
}

void drawForce(M5Canvas& c, const View& v, const games::ForceSeq& g, bool showing) {
  c.fillSprite(PAPER);
  drawStatusBar(c, v);
  char buf[24];
  snprintf(buf, sizeof(buf), "Fuerza - nivel %u", (unsigned)(g.len - rules::kForceStartLen + 1));
  text(c, buf, kWidth / 2, kPetTop + 5, 1, top_center);

  const sprites::Sprite& pose = sprites::kForcePoseSprite;
  const int px = (kWidth - pose.w * 2) / 2;
  drawSprite(c, pose, px, kPetTop + 16, 2);
  if (showing) {
    drawSprite(c, sprites::kSparkleSprite, px - 26, kPetTop + 22, 2);
    drawSprite(c, sprites::kSparkleSprite, px + pose.w * 2 + 10, kPetTop + 38, 2);
  }

  // The sequence: all icons while memorising, then filled in as entered.
  constexpr int kSlot = 24;
  const int sx = (kWidth - g.len * kSlot) / 2;
  const int sy = kPetTop + 74;
  for (int i = 0; i < g.len; ++i) {
    const int x = sx + i * kSlot;
    if (showing || i < g.pos) {
      drawSprite(c, *kSymIcons[g.seq[i]], x, sy, 2);
    } else {
      c.drawRoundRect(x + 3, sy + 3, kSlot - 6, kSlot - 6, 3, INK);
      if (i == g.pos) c.fillRect(x + 3, sy + kSlot + 1, kSlot - 6, 3, INK);
    }
  }

  c.drawFastHLine(0, kMenuY, kWidth, INK);
  if (showing) {
    text(c, "¡Memoriza!", kWidth / 2, kMenuY + 16, 2, top_center);
  } else {
    text(c, "¡Tu turno!", kWidth / 2, kMenuY + 8, 2, top_center);
    text(c, "arriba / abajo / pulsar", kWidth / 2, kMenuY + 30, 1, top_center);
  }
}

void drawHighLow(M5Canvas& c, const View& v, const games::HighLow& g, bool reveal) {
  c.fillSprite(PAPER);
  drawStatusBar(c, v);
  text(c, "¿Mayor o menor?", kWidth / 2, kPetTop + 5, 1, top_center);

  // Two cards: the number to beat, and the next one (hidden until revealed).
  constexpr int kCardW = 50, kCardH = 56;
  const int cy = kPetTop + 20;
  const int lx = 24, rx = kWidth - 24 - kCardW;
  char buf[24];
  c.drawRoundRect(lx, cy, kCardW, kCardH, 5, INK);
  c.drawRoundRect(rx, cy, kCardW, kCardH, 5, INK);
  snprintf(buf, sizeof(buf), "%u", (unsigned)(reveal ? g.prev : g.shown));
  text(c, buf, lx + kCardW / 2, cy + 8, 5, top_center);
  if (reveal) {
    snprintf(buf, sizeof(buf), "%u", (unsigned)g.shown);
    text(c, buf, rx + kCardW / 2, cy + 8, 5, top_center);
    const sprites::Sprite& mark = g.lastHit ? sprites::kCheckSprite : sprites::kCrossSprite;
    drawSprite(c, mark, (kWidth - 24) / 2, cy + 16, 3);
  } else {
    c.fillRoundRect(rx + 4, cy + 4, kCardW - 8, kCardH - 8, 3, INK);
    text(c, "?", rx + kCardW / 2, cy + 8, 5, top_center, PAPER);
    const sprites::Sprite& spr = petSprite(v.pet->stage);
    drawSprite(c, spr, (kWidth - spr.w) / 2, cy + 16, 1);
  }

  snprintf(buf, sizeof(buf), "Racha %u (gana con %u)", (unsigned)g.streak,
           (unsigned)rules::kHighLowWin);
  text(c, buf, kWidth / 2, cy + kCardH + 8, 1, top_center);

  // One box per guess in a row: tick, the final cross, or still empty.
  constexpr int kBox = 18, kGap = 6;
  const int bx = (kWidth - rules::kHighLowRounds * (kBox + kGap) + kGap) / 2;
  for (int i = 0; i < rules::kHighLowRounds; ++i) {
    const int x = bx + i * (kBox + kGap);
    c.drawRoundRect(x, kMenuY - 26, kBox, kBox, 3, INK);
    if (i < g.streak) drawSprite(c, sprites::kCheckSprite, x + 1, kMenuY - 25, 2);
    else if (i == g.streak && g.round > g.streak) drawSprite(c, sprites::kCrossSprite, x + 1, kMenuY - 25, 2);
  }

  c.drawFastHLine(0, kMenuY, kWidth, INK);
  drawSprite(c, sprites::kIconArrowUpSprite, 14, kMenuY + 12, 2);
  text(c, "mayor", 42, kMenuY + 20, 1, top_left);
  drawSprite(c, sprites::kIconArrowDownSprite, 104, kMenuY + 12, 2);
  text(c, "menor", 132, kMenuY + 20, 1, top_left);
}

void drawTidy(M5Canvas& c, const View& v, const games::Tidy& g, bool reveal) {
  c.fillSprite(PAPER);
  drawStatusBar(c, v);
  text(c, "Ordena: de menor a mayor", kWidth / 2, kPetTop + 5, 1, top_center);
  char buf[24];
  snprintf(buf, sizeof(buf), "Cambios: %u", (unsigned)g.swaps);
  text(c, buf, kWidth / 2, kPetTop + 17, 1, top_center);

  // Piles of folded clothes on a shelf; the one picked up floats above it.
  constexpr int kSlot = 30, kShirtW = 22, kShirtH = 8, kBase = 130;
  const int x0 = (kWidth - rules::kTidyPiles * kSlot) / 2;
  c.fillRect(x0 - 4, kBase + 1, rules::kTidyPiles * kSlot + 8, 2, INK);
  for (int i = 0; i < rules::kTidyPiles; ++i) {
    const int x = x0 + i * kSlot + (kSlot - kShirtW) / 2;
    const bool held = i == g.held;
    const int lift = held ? 12 : 0;
    for (int j = 0; j < g.piles[i]; ++j) {
      const int y = kBase - (j + 1) * kShirtH - lift;
      if (held) {
        c.fillRoundRect(x, y + 1, kShirtW, kShirtH - 1, 2, INK);
        c.drawFastHLine(x + 4, y + 4, kShirtW - 8, PAPER);
      } else {
        c.drawRoundRect(x, y + 1, kShirtW, kShirtH - 1, 2, INK);
        c.drawFastHLine(x + 4, y + 4, kShirtW - 8, INK);
      }
    }
    if (!reveal && i == g.cursor) {
      const int cx = x0 + i * kSlot + kSlot / 2;
      c.fillTriangle(cx - 5, kBase + 14, cx + 5, kBase + 14, cx, kBase + 6, INK);
    }
  }

  c.drawFastHLine(0, kMenuY, kWidth, INK);
  if (reveal) {
    const uint8_t n = games::stars(g);
    text(c, n == 3 ? "¡Chispa de alegría!" : "¡Ordenado!", kWidth / 2, kMenuY + 6, n == 3 ? 1 : 2,
         top_center);
    for (int i = 0; i < 3; ++i) {
      const int x = kWidth / 2 - 40 + i * 28;
      if (i < n) drawSprite(c, sprites::kSparkleSprite, x, kMenuY + 22, 3);
      else c.drawRoundRect(x + 4, kMenuY + 26, 14, 14, 3, INK);
    }
  } else {
    text(c, "dial: mover", kWidth / 2, kMenuY + 12, 1, top_center);
    text(c, g.held < 0 ? "pulsar: coger" : "pulsar: cambiar aquí", kWidth / 2, kMenuY + 28, 1,
         top_center);
  }
}

void drawJoy(M5Canvas& c, const View& v, const games::Joy& g, bool phase) {
  c.fillSprite(PAPER);
  drawStatusBar(c, v);
  if (phase && g.round == 0) {
    text(c, "Le dan alegría:", kWidth / 2, kPetTop + 8, 2, top_center);
    int k = 0;
    for (uint8_t i = 0; i < rules::kJoyPool; ++i) {
      if (!games::sparksJoy(g, i)) continue;
      const int x = 18 + k++ * 60;
      drawSprite(c, *kJoyIcons[i], x, 60, 4);
      drawSprite(c, sprites::kHeartSprite, x + 8, 100, 2);
    }
    c.drawFastHLine(0, kMenuY, kWidth, INK);
    text(c, "¡Memoriza!", kWidth / 2, kMenuY + 16, 2, top_center);
    return;
  }

  text(c, "¿Te da alegría?", kWidth / 2, kPetTop + 5, 1, top_center);
  const uint8_t shown = phase ? g.lastItem : g.item;
  drawSprite(c, *kJoyIcons[shown], (kWidth - 48) / 2, kPetTop + 18, 6);
  if (phase) {
    const sprites::Sprite& mark = g.lastHit ? sprites::kCheckSprite : sprites::kCrossSprite;
    drawSprite(c, mark, kWidth - 48, kPetTop + 30, 3);
    const char* msg = !g.lastHit                        ? "¡Uy!"
                      : games::sparksJoy(g, g.lastItem) ? "¡Alegría!"
                                                        : "¡Gracias, adiós!";
    text(c, msg, kWidth / 2, kPetTop + 74, 2, top_center);
  }

  // One box per round: tick, cross or still empty.
  constexpr int kBox = 18, kGap = 6;
  const int bx = (kWidth - rules::kJoyRounds * (kBox + kGap) + kGap) / 2;
  for (int i = 0; i < rules::kJoyRounds; ++i) {
    const int x = bx + i * (kBox + kGap);
    c.drawRoundRect(x, kMenuY - 26, kBox, kBox, 3, INK);
    if (i < g.round) {
      const sprites::Sprite& m = (g.history >> i & 1) ? sprites::kCheckSprite : sprites::kCrossSprite;
      drawSprite(c, m, x + 1, kMenuY - 25, 2);
    }
  }

  c.drawFastHLine(0, kMenuY, kWidth, INK);
  drawSprite(c, sprites::kIconArrowUpSprite, 6, kMenuY + 12, 2);
  text(c, "me lo quedo", 32, kMenuY + 20, 1, top_left);
  drawSprite(c, sprites::kIconArrowDownSprite, 112, kMenuY + 12, 2);
  text(c, "gracias", 138, kMenuY + 20, 1, top_left);
}

void drawDress(M5Canvas& c, const View& v, pet::Hat hat, pet::Neck neck, int row) {
  c.fillSprite(PAPER);
  drawStatusBar(c, v);
  const pet::Stage st = v.pet->stage;
  const int scale = 3, px = 6, py = 50;
  drawSprite(c, petSprite(st), px, py, scale);
  drawOutfit(c, st, hat, neck, px, py, scale, INK, PAPER);

  struct Row {
    const char* label;
    const char* value;
  } rows[kDressRows] = {
      {"Cabeza", pet::hatName(hat)}, {"Cuello", pet::neckName(neck)}, {"¡Listo!", ""}};
  for (int i = 0; i < kDressRows; ++i) {
    const int x = 86, y = 28 + i * 40, w = kWidth - x - 6, h = 36;
    const bool sel = i == row;
    if (sel) c.fillRoundRect(x, y, w, h, 5, INK);
    else c.drawRoundRect(x, y, w, h, 5, INK);
    const int col = sel ? PAPER : INK;
    if (rows[i].value[0]) {
      text(c, rows[i].label, x + 6, y + 4, 1, top_left, col);
      text(c, rows[i].value, x + w / 2, y + 16, 2, top_center, col);
    } else {
      text(c, rows[i].label, x + w / 2, y + 11, 2, top_center, col);
    }
  }

  c.drawFastHLine(0, kMenuY, kWidth, INK);
  text(c, "dial: elegir", kWidth / 2, kMenuY + 12, 1, top_center);
  text(c, row < kDressRows - 1 ? "pulsar: cambiar" : "pulsar: ponérselo", kWidth / 2,
       kMenuY + 28, 1, top_center);
}

void drawAlbum(M5Canvas& c, const View& v, const pet::Album& a, int page) {
  c.fillSprite(PAPER);
  drawStatusBar(c, v);
  char buf[32];
  snprintf(buf, sizeof(buf), "Álbum %d/%d", page + 1, kAlbumPages);
  text(c, buf, kWidth / 2, kPetTop + 4, 1, top_center);

  if (page == 0) {
    // The four adults, 2x2.
    for (int i = 0; i < pet::kAdultCount; ++i) {
      const auto st = static_cast<pet::Stage>(static_cast<int>(pet::Stage::AdultJedi) + i);
      const int cx = i % 2 ? 150 : 50;
      const int y = i < 2 ? 36 : 112;
      if (pet::albumHasAdult(a, st)) {
        drawSprite(c, petSprite(st), cx - 24, y, 2);
        text(c, pet::stageName(st), cx, y + 52, 1, top_center);
      } else {
        drawUnknown(c, cx - 24, y, 48, 48);
        text(c, "???", cx, y + 52, 1, top_center);
      }
    }
  } else if (page == 1) {
    text(c, "Objetos", 6, 36, 1, top_left);
    for (int i = 0; i < pet::kItemCount; ++i) {
      const int x = 8 + i * 38;
      if (a.items >> i & 1) drawSprite(c, *kItemIcons[i], x + 3, 48, 3);
      else drawUnknown(c, x, 46, 30, 28);
    }
    text(c, "Sucesos", 6, 84, 1, top_left);
    for (int i = 0; i < pet::kEventCount; ++i) {
      const int x = 8 + i * 60;
      if (a.events >> i & 1) {
        const sprites::Sprite& ic = *kEventIcons[i];
        const int sc = ic.w > 8 ? 2 : 3;
        drawSprite(c, ic, x + (40 - ic.w * sc) / 2, 96, sc);
      } else {
        drawUnknown(c, x, 96, 40, 32);
      }
    }
  } else {
    text(c, "Récords", 6, 36, 2, top_left);
    struct Line {
      const char* label;
      unsigned value, of;
    } lines[] = {
        {"Fuerza", a.bestForce, rules::kForceMaxLen},
        {"Izq/Der", a.bestLeftRight, rules::kLeftRightRounds},
        {"Más/Menos", a.bestHighLow, rules::kHighLowRounds},
        {"Ordenar", a.bestTidy, 3},
        {"¿Alegría?", a.bestJoy, rules::kJoyRounds},
    };
    int y = 60;
    for (const auto& l : lines) {
      text(c, l.label, 10, y, 1, top_left);
      snprintf(buf, sizeof(buf), "%u/%u", l.value, l.of);
      text(c, buf, kWidth - 10, y, 1, top_right);
      y += 16;
    }
    snprintf(buf, sizeof(buf), "Generación: #%u", (unsigned)v.pet->generation);
    text(c, buf, 10, y + 6, 1, top_left);
  }
  text(c, "dial: página  botón: volver", kWidth / 2, kHeight - 12, 1, top_center);
}

}  // namespace ui
