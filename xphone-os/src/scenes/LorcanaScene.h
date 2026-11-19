#pragma once

// custom build — Lorcana: one player's lore and ink drops, counted on the
// device.
//
// Nothing here talks to the phone. Both numbers live in NVS ("lorcana"
// namespace) and are written on every change (one loop tick after the press,
// so the number is drawn first), so a game survives sleep, a power-off and a
// firmware update.
//
// Controls (X4: Up/Down are the two side buttons):
//   side Up        = lore +1 (20 wins; it keeps counting past 20)
//   side Down      = lore -1
//   Left           = ink drops -1
//   Right          = ink drops +1
//   hold CONFIRM   = new game (both back to 0). A tap only shows the hint,
//                    so a stray press mid-game never wipes the score.
//   BACK           = launcher
//
// renderDormant() is the sleep face used when the device sleeps FROM this
// scene, so the score stays on the glass between turns.

#include <cstdint>

#include "../Scene.h"

class LorcanaScene : public Scene {
 public:
  void onEnter() override;
  void onExit() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;
  // Long-press dot on the CONFIRM (NEW GAME) tab.
  uint8_t longPressSlots() const override { return 0x02; }

  // Sleep face: lore and ink, big. Always draws. Never flushes.
  static bool renderDormant(Gfx& gfx);

  static constexpr int kWinLore = 20;
  static constexpr int kMax = 99;

 private:
  XpRect loreRect() const;
  XpRect inkRect() const;
  XpRect hintRect() const;

  bool _confirmHint = false;  // a tap on NEW GAME: say "hold" in the hint line

  // Layout cached by render() for partial repaints; empty until first paint.
  int16_t _w = 0;
  int16_t _loreTop = 0, _loreH = 0, _inkTop = 0, _inkH = 0, _hintTop = 0, _hintH = 0;
};
