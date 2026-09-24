// ============================================================================
//  Drift Project — GARAGE / CAR SELECT
//  Reference C++ for kdframework. Needs drift_common.h (palette + Draw:: API)
//  and drift_kit.h (Kit::keycap, Kit::statBar, Deco::dotField).
//
//  Canvas 1536 x 864, origin top-left. Font Archivo. Flush-left, 0 radius,
//  2px rules. Three-color system: ink text/rules, acid for the selection and
//  the stat fills, grey for the inactive.
// ============================================================================

#include "drift_common.h"
#include "drift_kit.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
    constexpr float PADX = 64.f, PADY = 44.f;
    constexpr float CANVAS_W = 1536.f, CANVAS_H = 864.f;

    const Color DOT   {Pal::INK.r, Pal::INK.g, Pal::INK.b, 102};
    const Color LOOPS {Pal::INK.r, Pal::INK.g, Pal::INK.b, 56};   // ~0.22 alpha
    const Color TICK  {Pal::INK.r, Pal::INK.g, Pal::INK.b, 128};
}

// ============================================================================
//  MODEL
// ============================================================================
struct CarStat  { std::string label; float value; };   // value 0..1

struct GarageCar {
    std::string brand, model, tier;
    Color       swatch;                 // thumbnail / body color
    int         artSlot;                // Draw::image slot for the side view
    std::vector<CarStat> stats;
};

struct GarageModel {
    std::string credits = "\u00A5 125,000";
    std::vector<std::string> brands = {
        "ALL", "NEXUS", "KATANA", "VORTEX", "PHOENIX", "OUTLAW"
    };
    int activeBrand = 0;

    std::vector<GarageCar> cars = {
        { "NEXUS", "NX7", "A", Pal::ACID,          11,
          { {"SPEED",.70f}, {"ACCELERATION",.62f}, {"HANDLING",.80f},
            {"DRIFT",.90f}, {"BRAKING",.55f} } },
        { "KATANA", "KT-R", "B", {0xE0,0x56,0x3C}, 12,
          { {"SPEED",.66f}, {"ACCELERATION",.71f}, {"HANDLING",.74f},
            {"DRIFT",.82f}, {"BRAKING",.60f} } },
        { "VORTEX", "V8", "S", {0x2C,0x2C,0x2C},   13,
          { {"SPEED",.88f}, {"ACCELERATION",.84f}, {"HANDLING",.68f},
            {"DRIFT",.72f}, {"BRAKING",.66f} } },
        { "PHOENIX", "PX-2", "B", Pal::PINK,       14,
          { {"SPEED",.58f}, {"ACCELERATION",.64f}, {"HANDLING",.86f},
            {"DRIFT",.88f}, {"BRAKING",.62f} } },
        { "OUTLAW", "OL9", "C", Pal::SAND,         15,
          { {"SPEED",.52f}, {"ACCELERATION",.56f}, {"HANDLING",.70f},
            {"DRIFT",.76f}, {"BRAKING",.50f} } },
    };
    int selected = 0;
    float spinAngle = 0.f;   // radians, drives the background loop rotation
};

// ============================================================================
//  DECORATION — the tangled wobbly loops behind the car stage.
//
//  Three irregular closed curves, each drawn at several rotations with a
//  slight per-copy scale. The irregularity IS the motif: do not substitute
//  scaled ellipses or concentric circles, that reads as a target, not a
//  tire trail. Sampled here as point arrays so any polyline API can draw them.
// ============================================================================
namespace {

    // Cubic bezier sampler: emits `steps` points into out[] (x,y interleaved).
    void sampleCubic(float x0,float y0, float x1,float y1,
                     float x2,float y2, float x3,float y3,
                     int steps, float* out, int& n) {
        for (int i = 0; i <= steps; ++i) {
            float t = i / (float)steps, u = 1 - t;
            float b0 = u*u*u, b1 = 3*u*u*t, b2 = 3*u*t*t, b3 = t*t*t;
            out[n++] = b0*x0 + b1*x1 + b2*x2 + b3*x3;
            out[n++] = b0*y0 + b1*y1 + b2*y2 + b3*y3;
        }
    }

    // Each loop is 4-5 cubic segments, centered on the origin.
    // Control points transcribed from the reference curves.
    struct Loop { const float* segs; int segCount; };

    // wl1 — the widest, flattest loop
    const float WL1[] = {
        -210,-6,  -150,-84,  -50,-96,   44,-70,
          44,-70,  150,-40,   236,-26,  202,42,
         202,42,   172,100,    56,82,   -54,80,
         -54,80,  -158,78,   -244,60,  -210,-6,
    };
    // wl2 — tilted, pinched on the left
    const float WL2[] = {
        -176,34,  -214,-38,  -104,-90,  -6,-80,
          -6,-80,  124,-66,   222,-30,  172,30,
         172,30,   134,82,     30,60,   -66,92,
         -66,92,  -148,118,  -150,92,  -176,34,
    };
    // wl3 — rounder, shifted up
    const float WL3[] = {
        -150,-34,  -74,-96,   66,-78,  152,-46,
         152,-46,  214,-22,  196,34,   146,58,
         146,58,    74,92,   -46,66,  -126,58,
        -126,58,  -196,50,  -204,18,  -150,-34,
    };

    // Build one loop's points, rotated by `rot` and scaled, translated to (cx,cy).
    void drawLoop(const float* segs, int segs4, float cx, float cy,
                  float rot, float scale, float t, Color c) {
        float pts[4 * 4 * 2 * 9];   // segments x steps x 2, generous
        int   n = 0;
        const int STEPS = 8;
        for (int s = 0; s < segs4; ++s) {
            const float* p = segs + s * 8;
            sampleCubic(p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7],
                        STEPS, pts, n);
        }
        // rotate + scale + translate in place
        float ca = std::cos(rot), sa = std::sin(rot);
        for (int i = 0; i < n; i += 2) {
            float x = pts[i] * scale, y = pts[i+1] * scale;
            pts[i]   = cx + x * ca - y * sa;
            pts[i+1] = cy + x * sa + y * ca;
        }
        Draw::polyline(pts, n / 2, t, c);
    }

    // The full tangle: 12 copies cycling the three loops.
    void driftLoops(float cx, float cy, float spin, Color c) {
        struct Copy { const float* segs; float rotDeg, scale; };
        static const Copy copies[] = {
            { WL1,   0.f, 1.00f }, { WL2,  28.f, 1.00f },
            { WL3,  54.f, 1.00f }, { WL1,  80.f, 1.16f },
            { WL2, 108.f, 0.88f }, { WL3, 134.f, 1.10f },
            { WL1, 162.f, 0.94f }, { WL2, 196.f, 1.22f },
            { WL3, 228.f, 0.98f }, { WL1, 262.f, 1.08f },
            { WL2, 300.f, 0.92f }, { WL3, 332.f, 1.14f },
        };
        const float DEG = 3.14159265f / 180.f;
        for (const Copy& k : copies)
            drawLoop(k.segs, 4, cx, cy, spin + k.rotDeg * DEG, k.scale, 1, c);
    }
}

// ============================================================================
//  GARAGE SCREEN
// ============================================================================
void drawGarage(const GarageModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, Pal::BG);
    const GarageCar& car = m.cars[m.selected];

    // ── Decoration (behind everything)
    driftLoops(1050, 430, m.spinAngle, LOOPS);
    Deco::dotField(70,  560, 11, 8, 13, DOT);
    Deco::dotField(250, 560,  6, 8, 13, {DOT.r, DOT.g, DOT.b, 56});
    Deco::dotField(620, 120,  9, 6, 13, {DOT.r, DOT.g, DOT.b, 90});
    Deco::dotField(1200,740, 15, 3, 13, {DOT.r, DOT.g, DOT.b, 97});
    // Right-edge crop ticks
    for (float y : { 200.f, 440.f, 600.f }) {
        Draw::line(1500, y, 1516, y, 1.4f, TICK);
        Draw::line(1508, y - 8, 1508, y + 8, 1.4f, TICK);
    }

    // ── Header:  GARAGE / CAR SELECT ............... CREDITS
    Draw::text(PADX, PADY, "GARAGE", 56, Weight::Heavy, Pal::INK);
    float gw = Draw::textWidth("GARAGE ", 56, Weight::Heavy);
    Draw::text(PADX + gw, PADY, "/", 56, Weight::Regular, Pal::MUTE);
    float sw = Draw::textWidth("GARAGE / ", 56, Weight::Heavy);
    Draw::text(PADX + sw, PADY, "CAR SELECT", 56, Weight::Heavy, Pal::ACID);

    Draw::text(CANVAS_W - PADX, PADY + 4, "CREDITS", 12, Weight::Bold,
               Pal::MUTE, Align::Right);
    Draw::text(CANVAS_W - PADX, PADY + 22, m.credits, 24, Weight::Bold,
               Pal::INK, Align::Right);

    // ── Brand list (left column) — active fills acid, rest at ~65%
    const float bx = PADX, by = PADY + 100;
    for (int i = 0; i < (int)m.brands.size(); ++i) {
        float y = by + i * 46;
        bool active = (i == m.activeBrand);
        if (active) {
            float w = Draw::textWidth(m.brands[i], 18, Weight::Heavy) + 60;
            Draw::rect(bx - 12, y - 6, w, 38, Pal::ACID);
        }
        Color ink = active ? Pal::INK
                           : Color{Pal::INK.r, Pal::INK.g, Pal::INK.b, 166};
        Draw::text(bx,      y, "\u25A6",     18, Weight::Heavy, ink);
        Draw::text(bx + 34, y, m.brands[i],  18, Weight::Heavy, ink);
    }

    // ── Car stage
    const float sx = PADX + 256;              // stage origin x
    const float sy = PADY + 100;
    Draw::text(sx, sy, "\u25B6 " + car.brand, 20, Weight::Heavy, Pal::INK);
    Draw::text(sx, sy + 26, car.model, 96, Weight::Heavy, Pal::INK);

    // TIER | grade
    {
        const float ty = sy + 136, h = 32;
        float lw = Draw::textWidth("TIER", 14, Weight::Bold) + 24;
        float rw = Draw::textWidth(car.tier, 14, Weight::Heavy) + 32;
        Draw::rectLine(sx, ty, lw, h, 2, Pal::INK);
        Draw::text(sx + 12, ty + 8, "TIER", 14, Weight::Bold, Pal::INK);
        Draw::rectLine(sx + lw, ty, rw, h, 2, Pal::INK);
        Draw::text(sx + lw + 16, ty + 8, car.tier, 14, Weight::Heavy, Pal::INK);
    }

    // Acid ground slab, sheared — the car art sits over it, overlapping the top.
    // Shear matches the deck's clip: 6% in at top-left, 6% out at bottom-right.
    {
        const float px = sx + 230, py = sy + 90, pw = 500, ph = 320;
        const float shear = pw * 0.06f;
        const float quad[] = {
            px + shear, py,          px + pw,          py,
            px + pw - shear, py + ph, px,              py + ph,
            px + shear, py
        };
        // Filled shear: if your framework has no polygon fill, draw it as a
        // rect and two triangles, or clip a rect to the same quad.
        Draw::polyline(quad, 5, 2, Pal::ACID);
        Draw::rect(px + shear, py, pw - shear * 2, ph, Pal::ACID);
    }
    Draw::image(sx + 230, sy + 40, 540, 380, car.artSlot);

    // ── Stat bars (right of the stage)
    {
        const float stx = CANVAS_W - PADX - 300, sty = sy + 60;
        for (int i = 0; i < (int)car.stats.size(); ++i) {
            const CarStat& st = car.stats[i];
            float y = sty + i * 56;
            Draw::text(stx, y, st.label, 14, Weight::Bold, Pal::INK);
            Draw::rect(stx, y + 24, 300, 16, Pal::INK);
            Draw::rect(stx, y + 24, 300 * st.value, 16, Pal::ACID);
        }
    }

    // ── Thumbnail strip with prev / next arrows
    {
        const float ty = 640, th = 120, aw = 36;
        Draw::rectLine(PADX, ty, aw, th, 2, Pal::INK);
        Draw::text(PADX + aw * 0.5f, ty + th * 0.5f - 10, "<", 20,
                   Weight::Bold, Pal::INK, Align::Center);
        float rightAx = CANVAS_W - PADX - aw;
        Draw::rectLine(rightAx, ty, aw, th, 2, Pal::INK);
        Draw::text(rightAx + aw * 0.5f, ty + th * 0.5f - 10, ">", 20,
                   Weight::Bold, Pal::INK, Align::Center);

        const int   n    = (int)m.cars.size();
        const float gap  = 14;
        const float band = rightAx - (PADX + aw) - gap * 2;
        const float cw   = (band - gap * (n - 1)) / n;
        float x = PADX + aw + gap;
        for (int i = 0; i < n; ++i) {
            bool sel = (i == m.selected);
            Draw::rect(x, ty, cw, th, m.cars[i].swatch);
            Draw::image(x, ty, cw, th, m.cars[i].artSlot);
            // Selection reads as a heavier border, never a glow or a shadow.
            Draw::rectLine(x, ty, cw, th, sel ? 3.f : 2.f, Pal::INK);
            x += cw + gap;
        }
    }

    // ── Footer keycaps
    {
        const float ky = 792;
        float u1 = Kit::keycap(PADX, ky, "ENTER", "SELECT");
        float u2 = Kit::keycap(PADX + u1 + 30, ky, "F1", "CAR INFO");
        Kit::keycap(PADX + u1 + u2 + 60, ky, "ESC", "BACK");
    }
}

// ----------------------------------------------------------------------------
//  Use:
//     GarageModel garage;
//     void frame(float dt) {
//         garage.spinAngle += dt * 0.052f;   // ~3 deg/sec, matches the deck
//         drawGarage(garage);
//     }
//     // selection: garage.selected = idx;  brand filter: garage.activeBrand = i;
//
//  Porting notes:
//   • Draw::text takes y as the TOP edge here; if kdframework's origin is the
//     baseline, add the ascent once inside your Draw::text wrapper.
//   • driftLoops() samples the bezier control points every frame. If that shows
//     up in a profile, sample once into a static buffer at init and only apply
//     the rotation per frame — the point count is fixed.
//   • The sheared acid slab needs a polygon fill. The rect+outline fallback
//     above is close; a real quad fill is cleaner if you have one.
//   • Coordinates are in 1536x864 space — scale by your framebuffer.
// ----------------------------------------------------------------------------
