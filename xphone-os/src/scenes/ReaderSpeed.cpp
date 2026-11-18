// custom build — Speed Read: the open book, one short phrase at a time.
//
// Part of ReaderScene (declared in ReaderScene.h), kept in its own file so
// the upstream reader stays easy to diff. See the comment block on the
// members in ReaderScene.h for the why; this file is the how.
//
// Controls while speed reading:
//   OK (PLAY/PAUSE)  = start / stop
//   side Up / Down   = faster / slower (25 wpm steps, 100..600)
//   Left / Right     = previous / next phrase (works playing or paused)
//   BACK (EXIT)      = back to the normal page, at the phrase you were on

#include <Arduino.h>
#include <Preferences.h>

#include <cstdio>
#include <cstring>

#include "../Fonts.h"
#include "../reader/Page.h"
#include "../reader/Section.h"
#include "../reader/TextBlock.h"
#include "ReaderScene.h"

namespace {

constexpr const char* kSpPrefsNs = "speedrd";
constexpr int kSpHeaderH = 46;
constexpr int kSpMarginX = 24;
constexpr int kSpMaxPhraseChars = 36;  // two comfortable lines at 22pt bold

bool endsPhrase(const char* w) {
  size_t n = strlen(w);
  // Skip closing quotes/brackets so `said."` still ends the phrase.
  while (n > 0) {
    const unsigned char c = static_cast<unsigned char>(w[n - 1]);
    if (c == '"' || c == '\'' || c == ')' || c == ']') {
      n--;
      continue;
    }
    // UTF-8 right quotes (U+2019, U+201D) end in 0x99 / 0x9D after E2 80.
    if (n >= 3 && static_cast<unsigned char>(w[n - 3]) == 0xE2 &&
        static_cast<unsigned char>(w[n - 2]) == 0x80 && (c == 0x99 || c == 0x9D)) {
      n -= 3;
      continue;
    }
    break;
  }
  if (n == 0) return false;
  const char c = w[n - 1];
  return c == '.' || c == '!' || c == '?' || c == ';' || c == ':' || c == ',';
}

}  // namespace

void ReaderScene::enterSpeed() {
  _spMode = true;
  _spPlaying = false;
  _spNeedWords = true;
  _spNoWords = false;
  _spEnd = false;
  _spEdge = 0;
  _wcMode = false;
  Preferences p;
  if (p.begin(kSpPrefsNs, true)) {
    const uint16_t wpm = p.getUShort("wpm", _spWpm);
    if (wpm >= kSpMinWpm && wpm <= kSpMaxWpm) _spWpm = wpm;
    p.end();
  }
  markDirty();
}

void ReaderScene::exitSpeed() {
  _spMode = false;
  _spPlaying = false;
  Preferences p;
  if (p.begin(kSpPrefsNs, false)) {
    p.putUShort("wpm", _spWpm);
    p.end();
  }
  markDirty();  // the normal page, at the place speed reading reached
}

bool ReaderScene::speedAddWord(const char* w, size_t len) {
  while (len > 0 && (*w == ' ' || *w == '\t')) {
    w++;
    len--;
  }
  if (len == 0) return true;
  if (_spCount >= kSpMaxWords) return false;
  uint16_t used = 0;
  if (_spCount > 0) {
    const uint16_t prev = _spOff[_spCount - 1];
    used = static_cast<uint16_t>(prev + strlen(_spPool + prev) + 1);
  }
  if (used + len + 1 > kSpPool) return false;
  memcpy(_spPool + used, w, len);
  _spPool[used + len] = '\0';
  _spOff[_spCount++] = used;
  return true;
}

void ReaderScene::speedLoadWords(Gfx& gfx) {
  _spCount = 0;
  _spNoWords = false;
  if (_fbp) {
    // Package books expose word boxes only for the page last drawn, so draw
    // it into the framebuffer first; renderSpeed() clears it right after.
    renderFbp(gfx);
    if (!_fbp->hasWordBoxes() || _fbp->wordBoxesPage() != _fbpPage) {
      _spNoWords = !_fbp->hasWordBoxes();
    } else {
      const uint16_t n = _fbp->pageWords(_wcWords, kMaxPageWords);
      char buf[96];
      for (uint16_t i = 0; i < n; i++) {
        _fbp->wordText(_wcWords[i], buf, sizeof(buf));
        // A word hyphenated across lines: join the halves into one word.
        if ((_wcWords[i].flags & 1) && i + 1 < n) {
          size_t len = strlen(buf);
          if (len > 0 && buf[len - 1] == '-') buf[--len] = '\0';
          _fbp->wordText(_wcWords[i + 1], buf + len, sizeof(buf) - len);
          i++;
        }
        if (!speedAddWord(buf, strlen(buf))) break;
      }
    }
  } else if (_section && _section->pageCount > 0) {
    if (_section->currentPage >= static_cast<int>(_section->pageCount))
      _section->currentPage = _section->pageCount - 1;
    if (_section->currentPage < 0) _section->currentPage = 0;
    auto page = _section->loadPageFromSectionFile();
    if (page) {
      bool full = false;
      for (const auto& el : page->elements) {
        if (full) break;
        if (!el || el->getTag() != reader::TAG_PageLine) continue;
        const auto* line = static_cast<const reader::PageLine*>(el.get());
        const auto& block = line->getBlock();
        if (!block) continue;
        for (const std::string& w : block->getWords()) {
          if (!speedAddWord(w.c_str(), w.size())) {
            full = true;
            break;
          }
        }
      }
    }
    saveProgress();      // the same crash-safe resume the page view writes
    maybeArmPrefetch();  // and the same quiet next-chapter indexing
  }
  _spNeedWords = false;

  _spIdx = 0;
  if (_spEdge < 0 && _spCount > 0) {
    // Arrived by going back a page: start on its last phrase.
    uint16_t i = 0, start = 0;
    while (i < _spCount) {
      start = i;
      i = static_cast<uint16_t>(i + speedPhraseLen(i));
    }
    _spIdx = start;
  }
  _spEdge = 0;
}

uint16_t ReaderScene::speedPhraseLen(const uint16_t idx) const {
  if (idx >= _spCount) return 0;
  // Words per flash from the pace: about 0.8 s of reading per flash, which
  // keeps every flash longer than the panel's own refresh.
  int target = static_cast<int>((static_cast<uint32_t>(_spWpm) * 800u + 30000u) / 60000u);
  if (target < 1) target = 1;
  if (target > 6) target = 6;
  uint16_t n = 0;
  int chars = 0;
  for (uint16_t i = idx; i < _spCount && n < target; i++) {
    const char* w = _spPool + _spOff[i];
    const int len = static_cast<int>(strlen(w));
    if (n > 0 && chars + len > kSpMaxPhraseChars) break;
    n++;
    chars += len + 1;
    if (endsPhrase(w)) break;  // let punctuation close the phrase
  }
  return n > 0 ? n : 1;
}

uint32_t ReaderScene::speedIntervalMs(const uint16_t words) const {
  const uint32_t ms = static_cast<uint32_t>(words) * 60000u / (_spWpm ? _spWpm : 300);
  return ms < kSpMinFlashMs ? kSpMinFlashMs : ms;
}

void ReaderScene::speedTurn(const bool forward) {
  if (_fbp) {
    const uint16_t last = _fbp->pageCount() ? static_cast<uint16_t>(_fbp->pageCount() - 1) : 0;
    if (forward && _fbpPage >= last) {
      _spEnd = true;
      _spPlaying = false;
      markDirty();
      return;
    }
    if (!forward && _fbpPage == 0) return;
    _spNeedWords = true;
    _spEdge = forward ? 0 : -1;
    _spEnd = false;
    fbpTurn(forward);
    return;
  }
  if (!_epub || !_section) return;
  const bool lastPage = _section->currentPage + 1 >= static_cast<int>(_section->pageCount);
  if (forward && lastPage && _spine + 1 >= _epub->getSpineItemsCount()) {
    _spEnd = true;
    _spPlaying = false;
    markDirty();
    return;
  }
  if (!forward && _section->currentPage <= 0 && _spine <= 0) return;
  _spNeedWords = true;
  _spEdge = forward ? 0 : -1;
  _spEnd = false;
  pageTurn(forward);
}

void ReaderScene::speedAdvance() {
  if (_spNeedWords) return;  // a page is still on its way
  const uint16_t len = speedPhraseLen(_spIdx);
  if (_spCount > 0 && _spIdx + len < _spCount) {
    _spIdx = static_cast<uint16_t>(_spIdx + len);
    markDirty();
    return;
  }
  speedTurn(true);
}

void ReaderScene::speedBack() {
  if (_spNeedWords) return;
  _spEnd = false;
  if (_spIdx == 0 || _spCount == 0) {
    speedTurn(false);
    return;
  }
  uint16_t i = 0, prev = 0;
  while (i < _spIdx) {
    prev = i;
    i = static_cast<uint16_t>(i + speedPhraseLen(i));
  }
  _spIdx = prev;
  markDirty();
}

void ReaderScene::handleSpeedInput(Input& in) {
  if (in.wasPressed(Btn::Back)) {
    exitSpeed();
    return;
  }
  if (in.wasPressed(Btn::Confirm)) {
    if (_spNoWords) return;
    if (_spEnd) {  // at the end: OK leaves, the page view has the finish flow
      exitSpeed();
      return;
    }
    _spPlaying = !_spPlaying;
    _spNextMs = millis() + 400;  // a beat after PLAY before the first change
    markDirty();
    return;
  }
  if (in.wasPressed(Btn::Up) || in.wasPressed(Btn::Down)) {
    const int step = in.wasPressed(Btn::Up) ? 25 : -25;
    int wpm = static_cast<int>(_spWpm) + step;
    if (wpm < kSpMinWpm) wpm = kSpMinWpm;
    if (wpm > kSpMaxWpm) wpm = kSpMaxWpm;
    _spWpm = static_cast<uint16_t>(wpm);
    markDirty();
    return;
  }
  if (in.wasPressed(Btn::Right)) {
    speedAdvance();
    _spNextMs = millis() + speedIntervalMs(speedPhraseLen(_spIdx));
    return;
  }
  if (in.wasPressed(Btn::Left)) {
    speedBack();
    _spNextMs = millis() + speedIntervalMs(speedPhraseLen(_spIdx));
    return;
  }

  if (!_spPlaying) return;
  in.noteActivity();  // reading hands-free is still reading: no nap mid-book
  if (_spNeedWords || isDirty() || SCENES.flushInFlight() || _work != Work::None) return;
  if (static_cast<int32_t>(millis() - _spNextMs) < 0) return;
  if (_spCount == 0) {
    speedTurn(true);  // a page with no text (an image, a rule): move on
  } else {
    speedAdvance();
  }
  _spNextMs = millis() + speedIntervalMs(speedPhraseLen(_spIdx));
}

void ReaderScene::renderSpeed(Gfx& gfx) {
  if (_spNeedWords) speedLoadWords(gfx);
  gfx.clear();

  const int w = gfx.width();
  const int h = gfx.height();
  const int cx = w / 2;

  // --- Header -----------------------------------------------------------------
  gfx.drawText(kFontBold, kSpMarginX, 8, "Speed read");
  char pace[16];
  snprintf(pace, sizeof(pace), "%u wpm", static_cast<unsigned>(_spWpm));
  gfx.drawText(kFontRegular, w - kSpMarginX - gfx.textWidth(kFontRegular, pace), 8, pace);
  gfx.fillRect(0, kSpHeaderH - 2, w, 2, true);

  const int midY = kSpHeaderH + (h - Scene::SOFTKEY_BAR_H - kSpHeaderH) * 2 / 5;

  if (_spNoWords) {
    gfx.drawTextCentered(kFontBold, cx, midY - gfx.lineHeight(kFontBold), "Can't speed read this book");
    gfx.drawTextCentered(kFontRegular, cx, midY + 8, "Send it from the Flowe app again.");
    return;
  }

  // --- The phrase: greedy-wrapped, every line centred -------------------------
  if (_spCount == 0) {
    gfx.drawTextCentered(kFontRegular, cx, midY, "(no text on this page)");
  } else {
    const uint16_t len = speedPhraseLen(_spIdx);
    const int maxW = w - 2 * kSpMarginX;
    char lines[3][96];
    int lineCount = 0;
    lines[0][0] = '\0';
    for (uint16_t i = _spIdx; i < _spIdx + len && i < _spCount; i++) {
      const char* word = _spPool + _spOff[i];
      char trial[96];
      if (lines[lineCount][0])
        snprintf(trial, sizeof(trial), "%s %s", lines[lineCount], word);
      else
        snprintf(trial, sizeof(trial), "%s", word);
      if (lines[lineCount][0] && gfx.textWidth(kFontSpeed, trial) > maxW && lineCount < 2) {
        lineCount++;
        snprintf(lines[lineCount], sizeof(lines[lineCount]), "%s", word);
      } else {
        snprintf(lines[lineCount], sizeof(lines[lineCount]), "%s", trial);
      }
    }
    lineCount++;
    const int lh = gfx.lineHeight(kFontSpeed);
    int y = midY - (lineCount * lh) / 2;
    for (int i = 0; i < lineCount; i++, y += lh) gfx.drawTextCentered(kFontSpeed, cx, y, lines[i]);
  }

  // --- State + where you are ---------------------------------------------------
  const int infoY = h - Scene::SOFTKEY_BAR_H - 2 * gfx.lineHeight(kFontSmall) - 14;
  const char* state = _spEnd ? "End of book - OK to finish" : (_spPlaying ? "" : "Paused - OK to play");
  if (state[0]) gfx.drawTextCentered(kFontBold, cx, infoY - gfx.lineHeight(kFontBold), state);
  char where[48];
  if (_fbp) {
    snprintf(where, sizeof(where), "page %u of %u", static_cast<unsigned>(_fbpPage + 1),
             static_cast<unsigned>(_fbp->pageCount()));
  } else if (_section && _epub) {
    snprintf(where, sizeof(where), "section %d of %d  -  page %d of %u", _spine + 1,
             _epub->getSpineItemsCount(), _section->currentPage + 1, _section->pageCount);
  } else {
    where[0] = '\0';
  }
  gfx.drawTextCentered(kFontSmall, cx, infoY + 6, where);
  gfx.drawTextCentered(kFontSmall, cx, infoY + 6 + gfx.lineHeight(kFontSmall),
                       "Side buttons: faster / slower");
}
