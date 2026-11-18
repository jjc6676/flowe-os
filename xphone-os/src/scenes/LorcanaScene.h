#pragma once

// flowe-lorcana — Lorcana: one player's lore and ink, counted on the device.
//
// Nothing here talks to the phone. The three numbers live in NVS
// ("lorcana" namespace) and are written on every change, so a game survives
// sleep, a power-off and a firmware update.
//
// Controls (X4: Up/Down are the two side buttons):
//   side Up        = lore +1 (20 wins; it keeps counting past 20)
//   side Down      = lore -1
//   CONFIRM        = ink a card (inkwell +1, and that ink is ready)
//   hold CONFIRM   = new turn: ready all ink
//   Left           = spend 1 ink
//   Right          = undo a spend (ready +1, never above the inkwell)
//   hold Right     = take a card back out of the inkwell (fix a mis-press)
//   hold Left      = new game (lore 0, inkwell empty)
//   BACK           = launcher
//
// renderDormant() is the sleep face used when the device sleeps FROM this
// scene, so the score stays on the glass between turns.

#include <cstdint>

#include "../Scene.h"

class LorcanaScene : public Scene {
 public:
  void onEnter() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;
  // Long-press dots on the CONFIRM, LEFT and RIGHT tabs.
  uint8_t longPressSlots() const override { return 0x0E; }

  // Sleep face: lore and ink, big. Always draws (a zeroed game is still a
  // game). Never flushes.
  static bool renderDormant(Gfx& gfx);

  static constexpr int kWinLore = 20;
  static constexpr int kMaxLore = 99;
  static constexpr int kMaxInk = 60;  // a whole deck, more than any real game

 private:
  XpRect loreRect() const;
  XpRect inkRect() const;

  // Layout cached by render() for partial repaints; empty until first paint.
  int16_t _w = 0, _h = 0;
  int16_t _loreTop = 0, _loreH = 0, _inkTop = 0, _inkH = 0;
};
