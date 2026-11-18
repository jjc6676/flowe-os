#include "LorcanaScene.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cstdio>

#include "../Fonts.h"
#include "AppScenes.h"

namespace {

constexpr int kMarginX = 20;
constexpr int kHeaderH = 46;  // same chrome as the other app scenes
constexpr int kLoreScale = 6;
constexpr int kInkScale = 4;
constexpr int kPipD = 22;     // ink pip diameter
constexpr int kPipGap = 8;
constexpr int kPipsPerRow = 12;
constexpr int kMaxPipRows = 2;  // beyond 24 cards the pips stop; the numbers carry it

constexpr const char* kPrefsNs = "lorcana";

// The game. File-static so the sleep face can read it without the scene.
struct Game {
  uint8_t lore = 0;
  uint8_t inkTotal = 0;  // cards in the inkwell
  uint8_t inkReady = 0;  // of those, not yet spent this turn
};
Game g;
bool gLoaded = false;

void loadGame() {
  if (gLoaded) return;
  gLoaded = true;
  Preferences p;
  if (!p.begin(kPrefsNs, true)) return;  // first run: namespace not created yet
  g.lore = p.getUChar("lore", 0);
  g.inkTotal = p.getUChar("inkT", 0);
  g.inkReady = p.getUChar("inkR", 0);
  p.end();
  if (g.lore > LorcanaScene::kMaxLore) g.lore = LorcanaScene::kMaxLore;
  if (g.inkTotal > LorcanaScene::kMaxInk) g.inkTotal = LorcanaScene::kMaxInk;
  if (g.inkReady > g.inkTotal) g.inkReady = g.inkTotal;
}

void saveGame() {
  Preferences p;
  if (!p.begin(kPrefsNs, false)) return;
  p.putUChar("lore", g.lore);
  p.putUChar("inkT", g.inkTotal);
  p.putUChar("inkR", g.inkReady);
  p.end();
}

// One row of pips per kPipsPerRow cards: filled = ready, outline = spent.
void drawPips(Gfx& gfx, const int cx, const int top) {
  const int total = g.inkTotal;
  if (total == 0) return;
  const int shown = total < kPipsPerRow * kMaxPipRows ? total : kPipsPerRow * kMaxPipRows;
  for (int i = 0; i < shown; i++) {
    const int row = i / kPipsPerRow;
    const int left = shown - row * kPipsPerRow;
    const int rowCount = left < kPipsPerRow ? left : kPipsPerRow;
    const int rowW = rowCount * kPipD + (rowCount - 1) * kPipGap;
    const int col = i % kPipsPerRow;
    const int x = cx - rowW / 2 + col * (kPipD + kPipGap);
    const int y = top + row * (kPipD + kPipGap);
    if (i < g.inkReady) {
      gfx.fillRoundedRect(x, y, kPipD, kPipD, kPipD / 2, true);
    } else {
      gfx.drawRoundedRect(x, y, kPipD, kPipD, kPipD / 2, 2, true);
    }
  }
}

int pipBlockH() {
  if (g.inkTotal == 0) return 0;
  const int shown = g.inkTotal < kPipsPerRow * kMaxPipRows ? g.inkTotal : kPipsPerRow * kMaxPipRows;
  const int rows = (shown + kPipsPerRow - 1) / kPipsPerRow;
  return rows * kPipD + (rows - 1) * kPipGap;
}

// The lore number and, at 20+, an inverted "YOU WIN" bar under it.
// Returns the y just below what it drew.
int drawLore(Gfx& gfx, const int cx, int y) {
  gfx.drawTextCentered(kFontBold, cx, y, "LORE");
  y += gfx.lineHeight(kFontBold) + 4;
  char num[8];
  snprintf(num, sizeof(num), "%u", static_cast<unsigned>(g.lore));
  gfx.drawTextScaledCentered(kFontBold, cx, y, num, kLoreScale);
  y += gfx.lineHeightScaled(kFontBold, kLoreScale);
  if (g.lore >= LorcanaScene::kWinLore) {
    const int barW = 220;
    const int barH = gfx.lineHeight(kFontBold) + 14;
    gfx.fillRoundedRect(cx - barW / 2, y, barW, barH, 8, true);
    gfx.drawTextCentered(kFontBold, cx, y + 7, "YOU WIN", false);
    y += barH;
  } else {
    char toGo[24];
    snprintf(toGo, sizeof(toGo), "%d to win", LorcanaScene::kWinLore - g.lore);
    gfx.drawTextCentered(kFontRegular, cx, y + 4, toGo);
    y += gfx.lineHeight(kFontRegular) + 8;
  }
  return y;
}

// "ready / total" plus the pips. Returns the y just below what it drew.
int drawInk(Gfx& gfx, const int cx, int y) {
  gfx.drawTextCentered(kFontBold, cx, y, "INK  ready / inkwell");
  y += gfx.lineHeight(kFontBold) + 4;
  char ink[16];
  snprintf(ink, sizeof(ink), "%u / %u", static_cast<unsigned>(g.inkReady),
           static_cast<unsigned>(g.inkTotal));
  gfx.drawTextScaledCentered(kFontBold, cx, y, ink, kInkScale);
  y += gfx.lineHeightScaled(kFontBold, kInkScale) + 8;
  drawPips(gfx, cx, y);
  y += pipBlockH();
  return y;
}

}  // namespace

void LorcanaScene::onEnter() {
  loadGame();
  markDirty();
}

void LorcanaScene::handleInput(Input& in) {
  if (in.wasPressed(Btn::Back)) {
    showLauncher();
    return;
  }

  bool loreChanged = false;
  bool inkChanged = false;

  // Long presses first: Input suppresses the tap on release once a long
  // press fires, so each press is one or the other, never both.
  if (in.wasLongPressed(Btn::Left)) {  // new game
    g = Game{};
    saveGame();
    markDirty();
    return;
  }
  if (in.wasLongPressed(Btn::Confirm) && g.inkReady != g.inkTotal) {  // new turn
    g.inkReady = g.inkTotal;
    inkChanged = true;
  }
  if (in.wasLongPressed(Btn::Right) && g.inkTotal > 0) {  // un-ink a card
    g.inkTotal--;
    if (g.inkReady > g.inkTotal) g.inkReady = g.inkTotal;
    inkChanged = true;
  }

  if (in.wasPressed(Btn::Up) && g.lore < kMaxLore) {
    g.lore++;
    loreChanged = true;
  }
  if (in.wasPressed(Btn::Down) && g.lore > 0) {
    g.lore--;
    loreChanged = true;
  }
  if (in.wasPressed(Btn::Confirm) && g.inkTotal < kMaxInk) {  // ink a card
    g.inkTotal++;
    g.inkReady++;
    inkChanged = true;
  }
  if (in.wasPressed(Btn::Left) && g.inkReady > 0) {  // spend
    g.inkReady--;
    inkChanged = true;
  }
  if (in.wasPressed(Btn::Right) && g.inkReady < g.inkTotal) {  // undo a spend
    g.inkReady++;
    inkChanged = true;
  }

  if (!loreChanged && !inkChanged) return;
  saveGame();
  if (loreChanged) markDirty(loreRect());
  if (inkChanged) markDirty(inkRect());
}

XpRect LorcanaScene::loreRect() const {
  if (_w <= 0) return XpRect{};  // no layout yet -> full repaint
  return XpRect{0, _loreTop, _w, _loreH};
}

XpRect LorcanaScene::inkRect() const {
  if (_w <= 0) return XpRect{};
  return XpRect{0, _inkTop, _w, _inkH};
}

const char* const* LorcanaScene::softKeys() const {
  // Slots: BACK, CONFIRM, LEFT, RIGHT.
  static constexpr const char* kKeys[4] = {"HOME", "INK", "SPEND", "UNDO"};
  return kKeys;
}

void LorcanaScene::render(Gfx& gfx) {
  const int w = gfx.width();
  const int h = gfx.height();
  const int cx = w / 2;
  _w = static_cast<int16_t>(w);
  _h = static_cast<int16_t>(h);

  // --- Header -----------------------------------------------------------------
  gfx.drawText(kFontBold, kMarginX, 8, "Lorcana");
  gfx.fillRect(0, kHeaderH - 2, w, 2, true);

  // Fixed bands so a partial repaint always covers the whole number, even
  // when the win bar appears or a pip row is added.
  const int bottom = h - Scene::SOFTKEY_BAR_H - gfx.lineHeight(kFontSmall) * 2 - 16;
  const int loreTop = kHeaderH + 14;
  const int inkTop = loreTop + (bottom - loreTop) * 52 / 100;
  _loreTop = static_cast<int16_t>(loreTop);
  _loreH = static_cast<int16_t>(inkTop - loreTop);
  _inkTop = static_cast<int16_t>(inkTop);
  _inkH = static_cast<int16_t>(bottom - inkTop);

  drawLore(gfx, cx, loreTop);
  gfx.fillRect(kMarginX * 3, inkTop - 8, w - kMarginX * 6, 1, true);
  drawInk(gfx, cx, inkTop + 4);

  // --- Hints (side buttons and holds are not discoverable otherwise) ---------
  const int hintY = bottom + 6;
  gfx.drawTextCentered(kFontSmall, cx, hintY, "Side buttons: lore +/-   Hold INK: new turn");
  gfx.drawTextCentered(kFontSmall, cx, hintY + gfx.lineHeight(kFontSmall) + 2,
                       "Hold UNDO: un-ink a card   Hold SPEND: new game");
}

bool LorcanaScene::renderDormant(Gfx& gfx) {
  loadGame();
  const int w = gfx.width();
  const int cx = w / 2;
  int y = 52;
  gfx.drawTextCentered(kFontBold, cx, y, "Lorcana");
  constexpr int kRuleW = 56;
  gfx.fillRect(cx - kRuleW / 2, y + gfx.lineHeight(kFontBold) + 10, kRuleW, 2, true);
  y = 110;
  y = drawLore(gfx, cx, y);
  y += 24;
  drawInk(gfx, cx, y);
  return true;
}
