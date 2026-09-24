// ============================================================================
//  Drift Project — SETTINGS screen
//  Reference C++ for kdframework. Foundation (palette + Draw:: API) lives in
//  drift_common.h; the scene transition lives in drift_transition.h.
// ============================================================================

#include "drift_common.h"
#include "drift_transition.h"
#include <algorithm>

// ============================================================================
//  SETTINGS DATA MODEL
// ============================================================================
enum class Ctrl { Stepper, Toggle };

struct SettingRow {
    std::string label;
    Ctrl        kind;
    std::string value;   // Stepper: shown text.  Toggle: unused.
    bool        on;      // Toggle only.
    bool        accent;  // Toggle only: ON segment is INK instead of ACID.
};

struct SettingsModel {
    std::vector<std::string> tabs = {
        "GAMEPLAY", "CONTROLS", "GRAPHICS", "AUDIO", "UI", "OTHER"
    };
    int activeTab = 0;

    std::vector<SettingRow> rows = {
        { "DIFFICULTY",        Ctrl::Stepper, "NORMAL",    false, false },
        { "TRANSMISSION",      Ctrl::Stepper, "AUTOMATIC", false, false },
        { "TRACTION CONTROL",  Ctrl::Toggle,  "",          true,  true  },
        { "STABILITY ASSIST",  Ctrl::Toggle,  "",          true,  false },
        { "ABS",               Ctrl::Toggle,  "",          true,  false },
        { "STEERING LINE",     Ctrl::Toggle,  "",          true,  false },
        { "DAMAGE",            Ctrl::Stepper, "NORMAL",    false, false },
        { "REWIND",            Ctrl::Toggle,  "",          true,  false },
        { "UNITS",             Ctrl::Stepper, "KM/H",      false, false },
    };
};

// ----------------------------------------------------------------------------
//  Small reusable "part" draws (the 6 kit primitives)
// ----------------------------------------------------------------------------
namespace Kit {

    // Keycap: bordered glyph + trailing caption. Returns width consumed.
    float keycap(float x, float y, const std::string& key,
                 const std::string& caption) {
        float pad = 10.f, kw = Draw::textWidth(key, 16, Weight::Bold) + pad * 2;
        float kh = 30.f;
        Draw::rectLine(x, y, kw, kh, 2, Pal::INK);
        Draw::text(x + pad, y + kh * 0.5f - 8, key, 16, Weight::Bold, Pal::INK);
        float cx = x + kw + 9;
        Draw::text(cx, y + kh * 0.5f - 8, caption, 14, Weight::Bold, Pal::INK);
        return (cx + Draw::textWidth(caption, 14, Weight::Bold)) - x;
    }

    // Toggle: [ ON | OFF ] segmented control.
    void toggle(float x, float y, bool on, bool accent) {
        const float segW = 62, h = 34, pad = 18;
        Draw::rectLine(x, y, segW * 2, h, 2, Pal::INK);
        Color onFill  = accent ? Pal::INK : Pal::ACID;
        Color onText  = accent ? Pal::WHITE : Pal::INK;
        // ON segment
        if (on) Draw::rect(x + 2, y + 2, segW - 2, h - 4, onFill);
        Draw::text(x + segW * 0.5f, y + h * 0.5f - 7, "ON", 14, Weight::Heavy,
                   on ? onText : Pal::MUTE, Align::Center);
        // OFF segment
        if (!on) Draw::rect(x + segW, y + 2, segW - 2, h - 4, Pal::GREY);
        Draw::text(x + segW * 1.5f, y + h * 0.5f - 7, "OFF", 14, Weight::Heavy,
                   !on ? Pal::INK : Pal::MUTE, Align::Center);
    }

    // Stepper:  ◄  VALUE  ►
    void stepper(float x, float y, const std::string& value) {
        const float h = 34, gap = 18;
        Draw::text(x, y + h * 0.5f - 8, "<", 16, Weight::Bold, Pal::INK);
        Draw::text(x + 30, y + h * 0.5f - 8, value, 16, Weight::Bold, Pal::INK);
        float vx = x + 30 + std::max(110.f,
                    Draw::textWidth(value, 16, Weight::Bold));
        Draw::text(vx + gap, y + h * 0.5f - 8, ">", 16, Weight::Bold, Pal::INK);
    }
}

// ============================================================================
//  SETTINGS SCREEN
// ============================================================================
void drawSettings(const SettingsModel& m) {
    const float PADX = 64, PADY = 48;
    Draw::rect(0, 0, 1536, 864, Pal::BG);

    // ── Faint vertical construction guides (behind content) + crop ticks
    const Color guide {Pal::INK.r, Pal::INK.g, Pal::INK.b, 18};   // ~0.07 alpha
    const Color tick  {Pal::INK.r, Pal::INK.g, Pal::INK.b, 56};   // ~0.22 alpha
    for (float gx : { 360.f, 1300.f, 1400.f })
        Draw::line(gx, 0, gx, 864, 1, guide);
    auto cropTick = [&](float x, float y){          // small +
        Draw::line(x - 5, y, x + 5, y, 1.5f, tick);
        Draw::line(x, y - 5, x, y + 5, 1.5f, tick);
    };
    cropTick(360, 70); cropTick(1300, 70); cropTick(360, 800);

    // ── Title with acid underline (raised toward the top edge)
    float titleY = PADY - 12;
    Draw::text(PADX, titleY, "SETTINGS", 64, Weight::Heavy, Pal::INK);
    float tw = Draw::textWidth("SETTINGS", 64, Weight::Heavy);
    Draw::rect(PADX, titleY + 72, tw, 4, Pal::ACID);

    // ── Top-right "++" marks
    Draw::text(1536 - 124, PADY - 4, "+ +", 30, Weight::Regular, Pal::INK);

    // ── Right rail: vertical text  (no rotation API — stack glyphs top→down).
    //   If kdframework can rotate text, prefer drawing the string at -90°.
    {
        const std::string rail = "FOCUS // ADAPT // OVERCOME";
        float rx = 1510, ry = 70, step = 19;
        for (char c : rail) {
            if (c != ' ')
                Draw::text(rx, ry, std::string(1, c), 15, Weight::Heavy,
                           Pal::INK, Align::Center);
            ry += (c == ' ') ? step * 0.5f : step;
        }
    }

    // ── Acid corner mark + diagonal hatch, laid OVER the rail text top.
    Draw::rect(1476, 60, 40, 80, Pal::ACID);
    Draw::line(1476, 140, 1516, 60, 1.5f, Pal::INK);
    Draw::line(1476, 116, 1500, 60, 1.5f, Pal::INK);

    // ── Right rail: descending +/◆ column
    { float cx = 1476, cy = 620;
      Draw::text(cx, cy,       "+", 18, Weight::Regular, Pal::INK, Align::Center);
      Draw::text(cx, cy + 40,  "+", 18, Weight::Regular, Pal::INK, Align::Center);
      Draw::text(cx, cy + 80,  "\u25C6", 12, Weight::Regular, Pal::INK, Align::Center);
      Draw::text(cx, cy + 108, "\u25C6", 12, Weight::Regular, Pal::INK, Align::Center);
      Draw::text(cx, cy + 136, "\u25C6", 12, Weight::Regular, Pal::INK, Align::Center); }

    // ── Rows panel sits high, just under the title; tabs are dropped lower.
    float panelX = PADX + 280, panelY = 150;
    // ── Tabs (left column) — intentionally lowered relative to the panel top
    float tabX = PADX, tabY = panelY + 60;
    for (int i = 0; i < (int)m.tabs.size(); ++i) {
        float rowY = tabY + i * 46;
        bool active = (i == m.activeTab);
        if (active) {
            float w = Draw::textWidth(m.tabs[i], 20, Weight::Heavy) + 28;
            Draw::rect(tabX - 14, rowY - 6, w, 38, Pal::ACID);
        }
        Draw::text(tabX, rowY, m.tabs[i], 20, Weight::Heavy,
                   active ? Pal::INK : Pal::MUTE);
    }

    // ── Rows panel (right, bordered, 2px row rules)
    float panelW = 880, rowH = 66;
    float panelH = rowH * m.rows.size();
    Draw::rectLine(panelX, panelY, panelW, panelH, 2, Pal::INK);

    for (int i = 0; i < (int)m.rows.size(); ++i) {
        const SettingRow& r = m.rows[i];
        float ry = panelY + i * rowH;
        if (i > 0) Draw::line(panelX, ry, panelX + panelW, ry, 2, Pal::INK);

        Draw::text(panelX + 24, ry + rowH * 0.5f - 8, r.label, 16,
                   Weight::Bold, Pal::INK);

        float ctrlX = panelX + panelW - 24 - 158; // right-aligned control area
        float ctrlY = ry + rowH * 0.5f - 17;
        if (r.kind == Ctrl::Toggle) Kit::toggle(ctrlX, ctrlY, r.on, r.accent);
        else                        Kit::stepper(ctrlX, ctrlY, r.value);
    }

    // ── Footer keycaps
    float fy = panelY + panelH + 30;
    float used = Kit::keycap(PADX, fy, "R", "RESET TO DEFAULT");
    Kit::keycap(PADX + used + 32, fy, "ESC", "BACK");
}

// ============================================================================
//  PANEL-WIPE TRANSITION  — moved to drift_transition.h (struct Transition).
// ============================================================================

// ============================================================================
//  MAIN LOOP SKETCH
// ============================================================================
//
//   SettingsModel settings;
//   Transition    fx;
//   int           scene = 1;   // start on SETTINGS
//
//   void onKey(int k) {
//       if (k == KEY_2) fx.go(1);      // jump to SETTINGS with a wipe
//       // ...map keys/buttons to scene indices...
//   }
//
//   void frame(float dt) {
//       fx.update(dt);
//       switch (fx.current) {
//           case 1: drawSettings(settings); break;
//           // case 0: drawMenu();  case 2: drawGarage(); ...
//       }
//       fx.draw();                     // overlay + next-scene label on top
//   }
//
//  Notes for porting:
//   • The wipe needs no clipping if you animate the panel rect width (as above).
//     Draw::pushClip is only needed if you prefer clipping a full-size panel.
//   • `Transition` is engine-agnostic: feed it real delta-seconds and it runs.
//   • All layout numbers are in 1536x864 space — scale by your framebuffer.
// ============================================================================
