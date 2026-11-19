#include "LorcanaScene.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cstdio>

#include "../Fonts.h"
#include "AppScenes.h"

namespace {

constexpr int kMarginX = 20;
constexpr int kHeaderH = 46;  // same chrome as the other app scenes

constexpr const char* kPrefsNs = "lorcana";

// The game. File-static so the sleep face can read it without the scene.
struct Game {
  uint8_t lore = 0;
  uint8_t ink = 0;  // ink drops
};
Game g;
bool gLoaded = false;

void loadGame() {
  if (gLoaded) return;
  gLoaded = true;
  Preferences p;
  if (!p.begin(kPrefsNs, true)) return;  // first run: namespace not created yet
  g.lore = p.getUChar("lore", 0);
  g.ink = p.getUChar("ink", 0);
  p.end();
  if (g.lore > LorcanaScene::kMax) g.lore = LorcanaScene::kMax;
  if (g.ink > LorcanaScene::kMax) g.ink = LorcanaScene::kMax;
}

// A counter press draws first and saves a moment later (see handleInput), so
// the flash write is not between the button and the number.
bool gSaveDue = false;

void saveGame() {
  gSaveDue = false;
  const uint32_t t0 = millis();
  Preferences p;
  if (!p.begin(kPrefsNs, false)) return;
  p.putUChar("lore", g.lore);
  p.putUChar("ink", g.ink);
  p.end();
  Serial.printf("[xphone-os] lorcana: saved in %lu ms\n", static_cast<unsigned long>(millis() - t0));
}

// Label, big number, and under lore either "N to win" or an inverted
// "YOU WIN" bar. Returns the y just below what it drew.
int drawCounter(Gfx& gfx, const int cx, int y, const char* label, const uint8_t value,
                const bool isLore) {
  gfx.drawTextCentered(kFontBold, cx, y, label);
  y += gfx.lineHeight(kFontBold) + 2;
  char num[8];
  snprintf(num, sizeof(num), "%u", static_cast<unsigned>(value));
  gfx.drawTextCentered(kFontDigitsXL, cx, y, num);
  y += gfx.lineHeight(kFontDigitsXL);
  if (!isLore) return y;
  if (value >= LorcanaScene::kWinLore) {
    const int barW = 220;
    const int barH = gfx.lineHeight(kFontBold) + 14;
    gfx.fillRoundedRect(cx - barW / 2, y, barW, barH, 8, true);
    gfx.drawTextCentered(kFontBold, cx, y + 7, "YOU WIN", false);
    y += barH;
  } else {
    char toGo[24];
    snprintf(toGo, sizeof(toGo), "%d to win", LorcanaScene::kWinLore - value);
    gfx.drawTextCentered(kFontRegular, cx, y + 4, toGo);
    y += gfx.lineHeight(kFontRegular) + 8;
  }
  return y;
}

}  // namespace

void LorcanaScene::onEnter() {
  loadGame();
  _confirmHint = false;
  markDirty();
}

void LorcanaScene::onExit() {
  if (gSaveDue) saveGame();  // long-press BACK leaves without a handleInput tick
}

void LorcanaScene::handleInput(Input& in) {
  // The save a counter press left behind: it runs on the tick after the
  // press, when the new number is already on its way to the glass.
  if (gSaveDue) saveGame();
  if (in.wasPressed(Btn::Back)) {
    showLauncher();
    return;
  }

  // Input suppresses the tap on release once a long press fires, so a hold
  // on NEW GAME is never also read as a tap.
  if (in.wasLongPressed(Btn::Confirm)) {
    g = Game{};
    saveGame();
    _confirmHint = false;
    markDirty();
    return;
  }
  if (in.wasPressed(Btn::Confirm)) {
    _confirmHint = true;
    markDirty(hintRect());
    return;
  }

  bool loreChanged = false;
  bool inkChanged = false;
  if (in.wasPressed(Btn::Up) && g.lore < kMax) {
    g.lore++;
    loreChanged = true;
  }
  if (in.wasPressed(Btn::Down) && g.lore > 0) {
    g.lore--;
    loreChanged = true;
  }
  if (in.wasPressed(Btn::Right) && g.ink < kMax) {
    g.ink++;
    inkChanged = true;
  }
  if (in.wasPressed(Btn::Left) && g.ink > 0) {
    g.ink--;
    inkChanged = true;
  }
  if (!loreChanged && !inkChanged) return;

  gSaveDue = true;  // written on the next tick, behind the repaint
  if (loreChanged) markDirty(loreRect());
  if (inkChanged) markDirty(inkRect());
  if (_confirmHint) {  // any other press clears the "hold" reminder
    _confirmHint = false;
    markDirty(hintRect());
  }
}

XpRect LorcanaScene::loreRect() const {
  if (_w <= 0) return XpRect{};  // no layout yet -> full repaint
  return XpRect{0, _loreTop, _w, _loreH};
}

XpRect LorcanaScene::inkRect() const {
  if (_w <= 0) return XpRect{};
  return XpRect{0, _inkTop, _w, _inkH};
}

XpRect LorcanaScene::hintRect() const {
  if (_w <= 0) return XpRect{};
  return XpRect{0, _hintTop, _w, _hintH};
}

const char* const* LorcanaScene::softKeys() const {
  // Slots: BACK, CONFIRM, LEFT, RIGHT.
  static constexpr const char* kKeys[4] = {"HOME", "NEW", "INK -", "INK +"};
  return kKeys;
}

void LorcanaScene::render(Gfx& gfx) {
  const int w = gfx.width();
  const int h = gfx.height();
  const int cx = w / 2;
  _w = static_cast<int16_t>(w);

  // --- Header -----------------------------------------------------------------
  gfx.drawText(kFontBold, kMarginX, 8, "Lorcana");
  gfx.fillRect(0, kHeaderH - 2, w, 2, true);

  // Fixed bands so a partial repaint always covers the whole number, even
  // when the win bar appears.
  const int hintH = gfx.lineHeight(kFontSmall) + 12;
  const int hintTop = h - Scene::SOFTKEY_BAR_H - hintH - 4;
  const int loreTop = kHeaderH + 16;
  const int inkTop = loreTop + (hintTop - loreTop) * 55 / 100;
  _loreTop = static_cast<int16_t>(loreTop);
  _loreH = static_cast<int16_t>(inkTop - loreTop);
  _inkTop = static_cast<int16_t>(inkTop);
  _inkH = static_cast<int16_t>(hintTop - inkTop);
  _hintTop = static_cast<int16_t>(hintTop);
  _hintH = static_cast<int16_t>(hintH);

  drawCounter(gfx, cx, loreTop, "LORE", g.lore, true);
  gfx.fillRect(kMarginX * 3, inkTop - 10, w - kMarginX * 6, 1, true);
  drawCounter(gfx, cx, inkTop + 6, "INK DROPS", g.ink, false);

  // --- Hint (the side buttons and the hold are not discoverable otherwise) ---
  const char* hint = _confirmHint ? "Hold NEW to start a new game"
                                  : "Side buttons: lore +/-   Hold NEW: new game";
  gfx.drawTextCentered(_confirmHint ? kFontBold : kFontSmall, cx, hintTop + 6, hint);
}

bool LorcanaScene::renderDormant(Gfx& gfx) {
  loadGame();
  if (gSaveDue) saveGame();  // going to sleep: nothing may be left unwritten
  const int w = gfx.width();
  const int cx = w / 2;
  int y = 52;
  gfx.drawTextCentered(kFontBold, cx, y, "Lorcana");
  constexpr int kRuleW = 56;
  gfx.fillRect(cx - kRuleW / 2, y + gfx.lineHeight(kFontBold) + 10, kRuleW, 2, true);
  y = 104;
  y = drawCounter(gfx, cx, y, "LORE", g.lore, true);
  y += 20;
  drawCounter(gfx, cx, y, "INK DROPS", g.ink, false);
  return true;
}
