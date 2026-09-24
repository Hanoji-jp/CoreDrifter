// ============================================================================
//  drift_kit.h — the shared UI-kit primitives and decoration motifs.
//  Definitions live in drift_menu_elements.cpp / drift_settings.cpp; this
//  header just declares them so every screen file can compose with them.
// ============================================================================
#pragma once
#include "drift_common.h"

namespace Kit {

    enum class BtnVariant { Primary, Secondary, Ghost, Disabled };

    // Flush-left button — label starts at the left padding edge, never centered.
    void  button(float x, float y, float w, float h, const std::string& label,
                 BtnVariant v, const std::string& trailing = "");
    // Square icon button. style 0 = solid ink, 1 = outline, 2 = acid.
    void  iconButton(float x, float y, float s, const std::string& glyph, int style);
    // Tab button — active fills acid, inactive is muted.
    void  tab(float x, float y, const std::string& label, bool active);
    // Two-part tag  [ left | right ] , right cell optionally acid-filled.
    void  tag(float x, float y, const std::string& left, const std::string& right,
              bool rightAcid);
    // Horizontal bar: ink track, acid fill (v = 0..1).
    void  statBar(float x, float y, float w, const std::string& label, float v);
    // Menu / list row. active fills acid, otherwise a bottom rule.
    void  menuRow(float x, float y, float w, const std::string& glyph,
                  const std::string& label, bool active);
    // Modal header strip: colored bar + title + close glyph.
    void  windowHeader(float x, float y, float w, const std::string& title,
                       Color bg, Color fg);
    // Bordered key glyph + trailing caption. Returns width consumed.
    float keycap(float x, float y, const std::string& key,
                 const std::string& caption);
    // [ ON | OFF ] segmented control. accent = ON segment fills ink, not acid.
    void  toggle(float x, float y, bool on, bool accent);
    // < VALUE >
    void  stepper(float x, float y, const std::string& value);

    // ---- additions used by the lobby / in-game screens --------------------
    // Small state chip: acid fill when hot, otherwise 2px outline at low alpha.
    void  chip(float x, float y, const std::string& label, bool hot, Color ink);
    // Table header strip: ink bar with white 800-weight column labels.
    void  tableHead(float x, float y, float w, const std::string* cols,
                    const float* colX, int n);
}

namespace Deco {
    // Same-size circles stepping toward the upper-left (title motif).
    void driftCircles(float startX, float startY, float r, int n,
                      float stepX, float stepY, Color c);
    void boxedX(float x, float y, float s);
    void dotField(float x, float y, int cols, int rows, float gap, Color c);
    void barcode(float x, float y, float h, int n);

    // Faint vertical construction guide, optionally interrupted: draws
    // [y0,y1] only. Pair with cropTick() at each break.
    void guide(float x, float y0, float y1, Color c);
    void cropTick(float x, float y, Color c);
    // Corner frame brackets for the in-game ground (line-only treatment).
    void cornerBrackets(float inset, float len, float w, float h,
                        float t, Color c);
}
