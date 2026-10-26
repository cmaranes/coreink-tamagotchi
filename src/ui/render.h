// Drawing of every screen onto a 200x200 1-bit canvas.
#pragma once
#include <M5Unified.h>

#include "../game/minigames.h"
#include "../game/pet.h"
#include "sprites.h"

namespace ui {

// Palette indices of the 1-bit canvas.
constexpr int PAPER = 0;
constexpr int INK = 1;

constexpr int kWidth = 200;
constexpr int kHeight = 200;

enum class MenuItem : uint8_t { Feed, Play, Light, Clean, Medicine, Scold, More, Count };
constexpr int kMenuCount = static_cast<int>(MenuItem::Count);

struct View {
  const pet::State* pet = nullptr;
  uint32_t now = 0;
  int battery = -1;
  bool charging = false;
  int menuCursor = 0;       // -1 hides the cursor
  bool altFrame = false;    // idle animation frame
  const char* message = nullptr;               // speech bubble text
  const sprites::Sprite* prop = nullptr;       // item shown next to the pet
  bool forcePose = false;   // draw Grogu using the Force instead of the stage sprite
  bool poweredOff = false;  // final frame before the device powers down
};

void initCanvas(M5Canvas& c);

struct SpriteOpts {
  bool eyesClosed = false;
  bool sad = false;
  int color = INK;
  int look = 0;  // shift eyes and mouth by this many sprite pixels (-1 left, +1 right)
  int paper = -1;  // colour for ',' pixels; -1 leaves them transparent
};
void drawSprite(M5Canvas& c, const sprites::Sprite& s, int x, int y, int scale,
                const SpriteOpts& o = SpriteOpts());

// One row of a pop-up list (feed, games, shop...).
struct ListRow {
  const char* label = "";
  const sprites::Sprite* icon = nullptr;
  char right[8] = "";  // right-aligned note, e.g. "x3"
};
constexpr int kListMax = 8;

void drawHome(M5Canvas& c, const View& v);
// Pop-up list over the home screen; scrolls to keep `cursor` visible.
void drawListPopup(M5Canvas& c, const View& v, const char* title, const char* titleRight,
                   const ListRow* rows, int n, int cursor);
void drawStats(M5Canvas& c, const View& v);
void drawDead(M5Canvas& c, const View& v);
// `reveal`: show which way Grogu looked in the last round.
void drawLeftRight(M5Canvas& c, const View& v, const games::LeftRight& g, bool reveal);
// `showing`: the sequence is on screen to memorise; otherwise the player's turn.
void drawForce(M5Canvas& c, const View& v, const games::ForceSeq& g, bool showing);
// `reveal`: show the number drawn after the last guess.
void drawHighLow(M5Canvas& c, const View& v, const games::HighLow& g, bool reveal);
// `reveal`: the clothes are sorted, show the stars.
void drawTidy(M5Canvas& c, const View& v, const games::Tidy& g, bool reveal);
// `phase`: before the first round, the things that spark joy to memorise;
// after a call, whether it was right.
void drawJoy(M5Canvas& c, const View& v, const games::Joy& g, bool phase);
// Dress-up: rows are hat, neck, done; `row` is the selected one.
constexpr int kDressRows = 3;
void drawDress(M5Canvas& c, const View& v, pet::Hat hat, pet::Neck neck, int row);
constexpr int kAlbumPages = 3;
void drawAlbum(M5Canvas& c, const View& v, const pet::Album& a, int page);

}  // namespace ui
