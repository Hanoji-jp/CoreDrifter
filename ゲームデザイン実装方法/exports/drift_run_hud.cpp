// ============================================================================
//  Drift Project — DRIFT RUN HUD
//  Reference C++ for kdframework. Standalone: needs only drift_common.h
//  (palette + Draw:: API) and drift_kit.h (Kit::chip, Deco::cornerBrackets).
//
//  Ground treatment "C": the HUD sits over live camera footage, so the ground
//  inverts — paper-colored type and 2px rules on the dark image, acid reserved
//  for the numbers that decide the run. No filled panels except the acid chips.
//
//  Drift scoring, not circuit racing: no laps, no race position. What the
//  driver reads is angle, clipping points, judge scores and the chain.
//
//  Canvas 1536 x 864, origin top-left. Font Archivo. Flush-left, 0 radius.
// ============================================================================

#include "drift_common.h"
#include "drift_kit.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
    constexpr float PADX = 64.f, CANVAS_W = 1536.f, CANVAS_H = 864.f;

    // On footage the roles swap: "ink" becomes paper, the image is the ground.
    const Color PAPER = Pal::BG;                                // #f1f0ec
    const Color DIM   {Pal::BG.r, Pal::BG.g, Pal::BG.b, 190};   // ~75% — labels
    const Color FAINT {Pal::BG.r, Pal::BG.g, Pal::BG.b, 76};    // ~30% — rules
    const Color GROUND{0x1C, 0x1C, 0x1A};                       // if no camera
}

// ============================================================================
//  MODEL — feed live sim values in; the HUD is a pure function of this.
// ============================================================================
struct JudgeScore { std::string label; int value; };

struct RunHudModel {
    // run context
    int         run = 1, runsTotal = 2;
    std::string course = "NEXUS TOUGE";
    int         section = 2, sectionsTotal = 4;
    std::string sectionNote = "UPHILL";

    // live drift angle
    int   angleDeg   = 47;      // current angle in degrees
    int   angleMax   = 70;      // angle that fills the meter
    int   angleIdeal = 12;      // tick index carrying the ideal-angle marker
    int   angleTicks = 18;

    // judging
    std::vector<JudgeScore> judge = { {"ANGLE",92}, {"LINE",88}, {"STYLE",95} };

    // course progress
    int   clipTotal = 4, clipHit = 2;

    // score
    long  runScore = 12480;
    float chain    = 4.2f;

    // drivetrain
    int   speedKmh = 184, gear = 4;
    float rpm      = 0.68f;     // 0..1, drives the tach ticks

    int   cameraSlot = 1;       // Draw::image slot for the track camera
};

// ============================================================================
//  DRAW
// ============================================================================
void drawRunHud(const RunHudModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, GROUND);
    Draw::image(0, 0, CANVAS_W, CANVAS_H, m.cameraSlot);

    // ── Frame: four corner brackets + short center ticks top and bottom.
    //    Nothing else encloses the image; the brackets do all the framing.
    Deco::cornerBrackets(40, 110, CANVAS_W, CANVAS_H, 2, PAPER);
    Draw::line(768, 40,  768, 96,  1, FAINT);
    Draw::line(768, 768, 768, 824, 1, FAINT);

    char buf[64];

    // ── TOP-LEFT — run / course / section
    Draw::text(PADX, 78, "RUN", 12, Weight::Heavy, PAPER);
    std::snprintf(buf, sizeof buf, "%d", m.run);
    Draw::text(PADX + 46, 64, buf, 44, Weight::Heavy, PAPER);
    float runW = Draw::textWidth(buf, 44, Weight::Heavy);
    std::snprintf(buf, sizeof buf, "/%d", m.runsTotal);
    Draw::text(PADX + 46 + runW, 82, buf, 20, Weight::Heavy, DIM);

    Draw::rect(PADX, 122, 230, 2, PAPER);
    Draw::text(PADX, 138, m.course, 16, Weight::Heavy, PAPER);
    std::snprintf(buf, sizeof buf, "SECTION %d / %d \u00B7 %s",
                  m.section, m.sectionsTotal, m.sectionNote.c_str());
    Draw::text(PADX, 166, buf, 12, Weight::Bold, DIM);

    // ── TOP-CENTER — live drift angle + tick meter
    //    Ticks fill up to the current angle. One taller acid tick marks the
    //    ideal angle, so the driver reads "how far off" at a glance.
    std::snprintf(buf, sizeof buf, "%d\u00B0", m.angleDeg);
    Draw::text(768, 56, "DRIFT ANGLE", 11, Weight::Heavy, DIM, Align::Center);
    Draw::text(768, 72, buf, 44, Weight::Heavy, PAPER, Align::Center);
    {
        const float tw = 6, tgap = 3, tallH = 22, baseH = 14;
        const int   n  = m.angleTicks;
        const int   fill = (int)std::lround(n * (m.angleDeg / (float)m.angleMax));
        const float total = n * tw + (n - 1) * tgap;
        const float x0 = 768 - total * 0.5f, y0 = 138;
        for (int i = 0; i < n; ++i) {
            bool  ideal = (i == m.angleIdeal);
            float h = ideal ? tallH : baseH;
            Color c = ideal ? Pal::ACID
                            : (i < fill ? PAPER
                                        : Color{PAPER.r, PAPER.g, PAPER.b, 76});
            Draw::rect(x0 + i * (tw + tgap), y0 + (tallH - h), tw, h, c);
        }
    }

    // ── TOP-RIGHT — judge panel:  LABEL ———————— VALUE
    Draw::text(CANVAS_W - PADX, 60, "JUDGE", 12, Weight::Heavy, PAPER, Align::Right);
    for (int i = 0; i < (int)m.judge.size(); ++i) {
        const float y  = 100 + i * 49;
        const float lx = CANVAS_W - PADX - 260;
        Draw::text(lx, y, m.judge[i].label, 12, Weight::Heavy, PAPER);
        float lw = Draw::textWidth(m.judge[i].label, 12, Weight::Heavy);
        Draw::line(lx + lw + 12, y + 8, CANVAS_W - PADX - 56, y + 8, 2, FAINT);
        Draw::text(CANVAS_W - PADX, y - 6, std::to_string(m.judge[i].value),
                   24, Weight::Heavy, Pal::ACID, Align::Right);
    }

    // ── BOTTOM-LEFT — course line + clipping points
    //    The traveled part of the line is acid and thick; the rest is faint.
    //    These point arrays ARE the course shape — swap them per track.
    Draw::text(PADX, 700, "CLIPPING POINTS", 11, Weight::Heavy, PAPER);
    {
        const float full[] = { 70,830, 124,826, 138,780, 182,768,
                               232,754, 254,796, 304,784 };
        const float done[] = { 70,830, 124,826, 138,780, 182,768 };
        Draw::polyline(full, 7, 2, FAINT);
        Draw::polyline(done, 4, 4, Pal::ACID);
        Draw::disc  (124, 826, 6,    Pal::ACID);   // CP1 hit
        Draw::disc  (182, 768, 6,    Pal::ACID);   // CP2 hit
        Draw::circle(254, 796, 6, 2, PAPER);       // CP3 next
        Draw::circle(304, 784, 6, 2, FAINT);       // CP4 ahead
    }
    for (int i = 0; i < m.clipTotal; ++i) {
        char cp[8]; std::snprintf(cp, sizeof cp, "CP%d", i + 1);
        Kit::chip(PADX + i * 58, 806, cp, i < m.clipHit, PAPER);
    }

    // ── BOTTOM-CENTER — run score + chain multiplier
    std::snprintf(buf, sizeof buf, "%ld", m.runScore);
    Draw::text(768, 742, "RUN SCORE", 11, Weight::Heavy, DIM, Align::Center);
    Draw::text(768, 758, buf, 58, Weight::Heavy, PAPER, Align::Center);
    std::snprintf(buf, sizeof buf, "x%.1f", m.chain);
    Draw::text(768 - 30, 818, buf, 32, Weight::Heavy, Pal::ACID, Align::Right);
    Draw::text(768 - 20, 828, "CHAIN", 11, Weight::Heavy, DIM);

    // ── BOTTOM-RIGHT — tach ticks, speed, gear
    //    Tick height ramps left to right; the last six sit in acid as the
    //    shift zone, and anything past current revs drops to ~35%.
    {
        const int   n = 30;
        const float tw = 5, tgap = 4, maxH = 34;
        const float total = n * tw + (n - 1) * tgap;
        const float x0 = CANVAS_W - PADX - total, y0 = 676;
        const int   lit = (int)std::lround(n * m.rpm);
        for (int i = 0; i < n; ++i) {
            float h = 10 + i * 0.8f;
            Color c = (i >= n - 6) ? Pal::ACID : PAPER;
            if (i > lit) c = Color{c.r, c.g, c.b, 90};
            Draw::rect(x0 + i * (tw + tgap), y0 + (maxH - h), tw, h, c);
        }
    }
    std::snprintf(buf, sizeof buf, "%d", m.speedKmh);
    Draw::text(CANVAS_W - PADX - 96, 716, buf, 126, Weight::Heavy, PAPER, Align::Right);

    const float gx = CANVAS_W - PADX - 84;
    Draw::text(gx, 740, "KM/H", 12, Weight::Heavy, DIM);
    Draw::text(gx, 772, "GEAR", 11, Weight::Heavy, DIM);
    Draw::text(gx, 788, std::to_string(m.gear), 44, Weight::Heavy, Pal::ACID);
}

// ----------------------------------------------------------------------------
//  Use:
//     RunHudModel hud;
//     void frame(float dt) {
//         hud.speedKmh = car.speedKmh();
//         hud.angleDeg = car.driftAngleDeg();
//         hud.rpm      = car.rpmNormalized();
//         hud.runScore = scoring.total();
//         hud.chain    = scoring.multiplier();
//         hud.clipHit  = course.clipsHit();
//         drawRunHud(hud);
//     }
//
//  Porting notes:
//   • All coordinates are in 1536x864 space — scale by your framebuffer.
//   • Draw::text takes y as the TOP edge here. If kdframework's text origin is
//     the baseline, add the ascent once inside your Draw::text wrapper rather
//     than adjusting every call site.
//   • Nothing in this file allocates or holds state; call it every frame.
// ----------------------------------------------------------------------------
