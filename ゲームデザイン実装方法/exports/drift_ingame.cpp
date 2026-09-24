// ============================================================================
//  Drift Project — IN-GAME screens: DRIFT RUN HUD, DRIFT SCORING, PAUSE,
//  COUNTDOWN, TOASTS.
//
//  These sit over live camera footage, so the ground inverts: paper-colored
//  type and 2px rules on the dark image, acid for the numbers that matter.
//  Treatment "C": corner brackets and rules only — no filled panels except
//  the acid chips. Canvas 1536 x 864. Drift scoring, so no laps or positions.
// ============================================================================

#include "drift_common.h"
#include "drift_kit.h"
#include <cmath>
#include <cstdio>

namespace {
    constexpr float PADX = 64.f, CANVAS_W = 1536.f, CANVAS_H = 864.f;

    // On footage, "ink" becomes paper and the ground is the image itself.
    const Color PAPER = Pal::BG;                       // #f1f0ec
    const Color DIM   {Pal::BG.r, Pal::BG.g, Pal::BG.b, 190};   // ~75% label
    const Color FAINT {Pal::BG.r, Pal::BG.g, Pal::BG.b, 76};    // ~30% rule
    const Color GROUND{0x1C, 0x1C, 0x1A};              // fallback if no camera

    // Label over a big value, right- or left-aligned.
    void readout(float x, float y, const std::string& label,
                 const std::string& value, float px, Color valueColor,
                 Align a = Align::Left) {
        Draw::text(x, y, label, 11, Weight::Heavy, DIM, a);
        Draw::text(x, y + 16, value, px, Weight::Heavy, valueColor, a);
    }
}

// ============================================================================
//  DRIFT RUN HUD — moved to its own file: drift_run_hud.cpp
//  (RunHudModel, JudgeScore and drawRunHud live there so the HUD can be
//  compiled and iterated on by itself. Include it alongside this file.)
// ============================================================================

// ============================================================================
//  DRIFT SCORING OVERLAY — the judgment moment
// ============================================================================
struct ScoringModel {
    std::string verdict = "PERFECT";      // PERFECT / GOOD / CLIP
    float       chain   = 4.2f;
    int         angleDeg = 47;
    long        chainScore = 3180;
    float       chainHold  = 0.64f;       // 0..1 remaining
    std::vector<std::pair<std::string,bool>> ladder = {
        {"PERFECT",true}, {"GOOD",false}, {"PERFECT",true}, {"CLIP",false}
    };
    int cameraSlot = 2;
};

void drawScoring(const ScoringModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, GROUND);
    Draw::image(0, 0, CANVAS_W, CANVAS_H, m.cameraSlot);

    // Only two brackets here — the frame stays open where the type lands.
    Draw::line(40, 96, 40, 40, 2, PAPER);   Draw::line(40, 40, 150, 40, 2, PAPER);
    Draw::line(1496, 768, 1496, 824, 2, PAPER);
    Draw::line(1496, 824, 1386, 824, 2, PAPER);

    // ── verdict chip + the headline
    {
        float w = Draw::textWidth(m.verdict, 15, Weight::Heavy) + 32;
        Draw::rect(PADX, 150, w, 34, Pal::ACID);
        Draw::text(PADX + 16, 159, m.verdict, 15, Weight::Heavy, Pal::INK);
    }
    Draw::text(PADX, 200, "CHAIN", 150, Weight::Heavy, PAPER);

    char buf[32];
    std::snprintf(buf, sizeof buf, "x%.1f", m.chain);
    Draw::text(PADX, 330, buf, 118, Weight::Heavy, Pal::ACID);
    Draw::text(PADX + Draw::textWidth(buf, 118, Weight::Heavy) + 18, 400,
               "MULTIPLIER", 16, Weight::Heavy, PAPER);

    // ── angle arc: half-circle track, acid sweep to the current angle
    {
        const float cx = 220, cy = 620, r = 140;
        const float PI = 3.14159265f;
        Draw::arc(cx, cy, r, PI, 2 * PI, 2, FAINT);            // 180 deg track
        float t = m.angleDeg / 70.f;                            // 70 deg = full
        Draw::arc(cx, cy, r, PI, PI + PI * t, 6, Pal::ACID);
        float a = PI + PI * t;
        Draw::line(cx, cy, cx + std::cos(a) * r, cy + std::sin(a) * r, 2, PAPER);
    }
    std::snprintf(buf, sizeof buf, "%d\u00B0", m.angleDeg);
    readout(220, 744, "DRIFT ANGLE", buf, 46, PAPER, Align::Center);

    // ── this chain: score + depleting hold gauge (drains right to left)
    std::snprintf(buf, sizeof buf, "%ld", m.chainScore);
    readout(CANVAS_W - PADX, 640, "THIS CHAIN", buf, 88, PAPER, Align::Right);
    Draw::text(CANVAS_W - PADX, 742, "CHAIN HOLD", 12, Weight::Heavy, DIM, Align::Right);
    {
        float w = 340, x = CANVAS_W - PADX - w, y = 762;
        Draw::rect(x, y, w, 14, PAPER);
        Draw::rect(x + w * (1 - m.chainHold), y, w * m.chainHold, 14, Pal::ACID);
    }

    // ── recent-judgment ladder, top right
    for (int i = 0; i < (int)m.ladder.size(); ++i) {
        const auto& j = m.ladder[i];
        float w = Draw::textWidth(j.first, 12, Weight::Heavy) + 24;
        Kit::chip(CANVAS_W - PADX - w, 64 + i * 40, j.first, j.second, PAPER);
    }
}

// ============================================================================
//  PAUSE — dimmed footage, menu, live score
// ============================================================================
struct PauseModel {
    struct Item { std::string glyph, label; };
    std::vector<Item> items = {
        {"\u25B6","RESUME"}, {"\u21BB","RESTART RUN"}, {"\u2699","SETTINGS"},
        {"\u25A4","CONTROLS"}, {"\u21E5","QUIT TO LOBBY"}
    };
    int active = 0;
    std::vector<std::pair<std::string,std::string>> board = {
        {"01   KAZE_9", "14,020"}, {"02   DRIVER01", "12,480"}, {"03   MIRA.S", "11,340"}
    };
    int myRow = 1;
    int cameraSlot = 3;
};

void drawPause(const PauseModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, GROUND);
    Draw::image(0, 0, CANVAS_W, CANVAS_H, m.cameraSlot);
    // Dim the frozen frame so the menu can carry the contrast.
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, {Pal::INK.r, Pal::INK.g, Pal::INK.b, 184});

    Deco::guide(620, 0, 200, {PAPER.r, PAPER.g, PAPER.b, 51});
    Deco::guide(620, 280, CANVAS_H, {PAPER.r, PAPER.g, PAPER.b, 51});
    Deco::cropTick(620, 200, {PAPER.r, PAPER.g, PAPER.b, 102});
    Deco::cropTick(620, 280, {PAPER.r, PAPER.g, PAPER.b, 102});
    Draw::rect(1440, 60, 34, 76, Pal::ACID);

    Draw::text(PADX, 110, "RACE PAUSED", 13, Weight::Heavy, Pal::ACID);
    Draw::text(PADX, 134, "PAUSE", 96, Weight::Heavy, PAPER);

    // Menu rows — active fills acid, the rest carry a bottom rule.
    const float mx = PADX, my = 278, mw = 420, mh = 56;
    for (int i = 0; i < (int)m.items.size(); ++i) {
        float y = my + i * mh;
        bool on = (i == m.active);
        if (on) Draw::rect(mx, y, mw, mh, Pal::ACID);
        else    Draw::line(mx, y + mh, mx + mw, y + mh, 2,
                           {PAPER.r, PAPER.g, PAPER.b, 90});
        Color ink = on ? Pal::INK : PAPER;
        Draw::text(mx + 18, y + 18, m.items[i].glyph, 18, Weight::Heavy, ink);
        Draw::text(mx + 60, y + 18, m.items[i].label, 20, Weight::Heavy, ink);
    }

    // Live score panel (drift scores, not gaps)
    const float px2 = CANVAS_W - PADX - 420, py = 130, pw = 420, rh = 54;
    Draw::text(px2, py, "CURRENT SCORE", 11, Weight::Heavy, DIM);
    float by = py + 24;
    Draw::rectLine(px2, by, pw, rh * m.board.size(), 2, PAPER);
    for (int i = 0; i < (int)m.board.size(); ++i) {
        float y = by + i * rh;
        bool lead = (i == 0);
        if (lead) Draw::rect(px2 + 2, y + 2, pw - 4, rh - 4, Pal::ACID);
        if (i > 0) Draw::line(px2, y, px2 + pw, y, 2, PAPER);
        Color ink = lead ? Pal::INK
                  : (i == m.myRow ? PAPER : Color{PAPER.r, PAPER.g, PAPER.b, 190});
        Weight wt = lead ? Weight::Heavy : (i == m.myRow ? Weight::Heavy : Weight::Bold);
        Draw::text(px2 + 16, y + 18, m.board[i].first, 15, wt, ink);
        Draw::text(px2 + pw - 16, y + 18, m.board[i].second, 15, wt, ink, Align::Right);
    }

    float ky = by + rh * m.board.size() + 30;
    float u = Kit::keycap(px2, ky, "ESC", "RESUME");
    Kit::keycap(px2 + u + 24, ky, "R", "RESTART");
}

// ============================================================================
//  COUNTDOWN — run start
// ============================================================================
struct CountdownModel {
    int         count = 3;                       // 3, 2, 1 then GO
    std::string course = "NEXUS TOUGE \u00B7 RUN 1 OF 2";
    std::string strap  = "GET READY";
    int         gridSize = 8, mySlot = 1;        // 0-indexed
    float       pulse = 0.f;                     // 0..1, scales the numeral
    int         cameraSlot = 4;
};

void drawCountdown(const CountdownModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, GROUND);
    Draw::image(0, 0, CANVAS_W, CANVAS_H, m.cameraSlot);

    Deco::cornerBrackets(40, 110, CANVAS_W, CANVAS_H, 2, PAPER);
    Draw::line(768, 120, 768, 300, 1, {PAPER.r, PAPER.g, PAPER.b, 46});
    Draw::line(768, 600, 768, 760, 1, {PAPER.r, PAPER.g, PAPER.b, 46});

    Draw::text(768, 236, m.course, 14, Weight::Heavy, PAPER, Align::Center);

    // The numeral is the whole screen. Pulse it 1.00 -> 1.16 each second.
    float scale = 1.f + 0.16f * std::sin(m.pulse * 3.14159265f);
    Draw::text(768, 270, std::to_string(m.count), 400 * scale,
               Weight::Heavy, Pal::ACID, Align::Center);

    {
        float w = Draw::textWidth(m.strap, 15, Weight::Heavy) + 40;
        Draw::rect(768 - w * 0.5f, 588, w, 38, PAPER);
        Draw::text(768, 598, m.strap, 15, Weight::Heavy, Pal::INK, Align::Center);
    }

    // Grid order pips — the player's slot carries the acid.
    const float pw = 70, ph = 44, pgap = 12;
    float total = m.gridSize * pw + (m.gridSize - 1) * pgap;
    float x0 = 768 - total * 0.5f, y = CANVAS_H - 70 - ph;
    for (int i = 0; i < m.gridSize; ++i) {
        float x = x0 + i * (pw + pgap);
        bool mine = (i == m.mySlot);
        if (mine) Draw::rect(x, y, pw, ph, Pal::ACID);
        Draw::rectLine(x, y, pw, ph, 2, PAPER);
        char lbl[8]; std::snprintf(lbl, sizeof lbl, "P%d", i + 1);
        Draw::text(x + pw * 0.5f, y + 14, lbl, 13, Weight::Heavy,
                   mine ? Pal::INK : PAPER, Align::Center);
    }
}

// ============================================================================
//  TOASTS — in-run notifications
// ============================================================================
struct Toast {
    std::string kicker, text, value;
    bool        hot;        // acid kicker + acid value
    float       age;        // seconds since raised; slide in over 0.24s
};

struct ToastModel {
    std::vector<Toast> stack = {
        { "CHAIN", "CHAIN BANKED",       "+3,180", true,  0.4f },
        { "BEST",  "NEW PERSONAL BEST",  "14,020", true,  1.1f },
        { "CLIP",  "CP3 MISSED",         "-450",   false, 2.0f },
        { "RIVAL", "KAZE_9 LEADS",       "-1,540", false, 3.2f },
    };
    int cameraSlot = 5;
};

// One toast. Returns the height consumed so the stack can advance.
float toast(float x, float y, float w, const Toast& t) {
    const float h = 54;
    // Slide in from the left over 240ms, then hold.
    float k = std::min(t.age / 0.24f, 1.f);
    float dx = (1 - k) * -40.f;
    x += dx;

    Draw::rect(x, y, w, h, {Pal::INK.r, Pal::INK.g, Pal::INK.b, 128});
    Draw::rectLine(x, y, w, h, 2, PAPER);

    float kw = Draw::textWidth(t.kicker, 11, Weight::Heavy) + 20;
    if (t.hot) Draw::rect(x + 16, y + 15, kw, 24, Pal::ACID);
    else       Draw::rectLine(x + 16, y + 15, kw, 24, 2, PAPER);
    Draw::text(x + 26, y + 20, t.kicker, 11, Weight::Heavy,
               t.hot ? Pal::INK : PAPER);

    Draw::text(x + 16 + kw + 16, y + 18, t.text, 17, Weight::Heavy, PAPER);
    Draw::text(x + w - 16, y + 17, t.value, 17, Weight::Heavy,
               t.hot ? Pal::ACID : PAPER, Align::Right);
    return h;
}

void drawToasts(const ToastModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, GROUND);
    Draw::image(0, 0, CANVAS_W, CANVAS_H, m.cameraSlot);

    Draw::line(40, 96, 40, 40, 2, PAPER);   Draw::line(40, 40, 150, 40, 2, PAPER);
    Draw::line(1496, 768, 1496, 824, 2, PAPER);
    Draw::line(1496, 824, 1386, 824, 2, PAPER);

    Draw::text(PADX, 120, "NOTIFICATION STACK", 11, Weight::Heavy, DIM);
    float y = 152;
    for (const Toast& t : m.stack) y += toast(PADX, y, 520, t) + 14;

    Draw::text(CANVAS_W - PADX, 700, "ANATOMY", 11, Weight::Heavy, DIM, Align::Right);
    const char* notes[3] = { "KICKER CHIP \u00B7 MESSAGE \u00B7 VALUE",
                             "2PX RULE \u00B7 NO FILL EXCEPT ACID",
                             "SLIDES IN FROM LEFT, 240MS" };
    for (int i = 0; i < 3; ++i)
        Draw::text(CANVAS_W - PADX, 724 + i * 22, notes[i], 13, Weight::Bold,
                   DIM, Align::Right);
}

// ----------------------------------------------------------------------------
//  Wire into the shared frame()/Transition loop (drift_transition.h):
//     case  9: drawRunHud(runHud);       break;
//     case 10: drawScoring(scoring);     break;
//     case 11: drawPause(pause);         break;
//     case 12: drawCountdown(countdown); break;
//     case 13: drawToasts(toasts);       break;
//
//  Every model here is plain data driven by the sim: feed the live values in
//  and the screens redraw. Animated members (pulse, age, blinkPhase) expect
//  delta-seconds accumulated by the caller, same contract as Transition.
// ----------------------------------------------------------------------------
