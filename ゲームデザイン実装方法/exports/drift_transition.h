// ============================================================================
//  drift_transition.h — PANEL-WIPE scene transition (engine-agnostic).
//
//  An acid panel sweeps across the screen and the next scene's name rides it.
//  Two phases, ~0.4s each:
//    COVER  : panel grows left→right, hiding the old scene   (t: 0→1)
//    REVEAL : panel slides off to the right, showing the new (t: 0→1)
//  The scene swap happens at the seam, under full cover.
//
//  Usage:
//    Transition fx;
//    fx.go(sceneIndex);          // request a change (ignored if busy)
//    fx.update(dtSeconds);       // once per frame — advances timing + swap
//    draw scene fx.current ...   // render the current scene
//    fx.draw();                  // overlay panel + next-scene label on top
// ============================================================================
#pragma once
#include "drift_common.h"
#include <cmath>

enum class Phase { Idle, Cover, Reveal };

struct Transition {
    Phase phase   = Phase::Idle;
    float t       = 0.f;          // 0..1 within the current phase
    float dur     = 0.40f;        // seconds per phase
    int   current = 0;            // scene currently shown
    int   target  = 0;            // scene we are heading to

    std::vector<std::string> names = {
        "MENU", "SETTINGS", "GARAGE", "MODAL", "ELEMENTS"
    };

    // Layout constants (1536x864 space) — tweak to taste.
    float canvasW = 1536.f, canvasH = 864.f;
    float labelPx = 122.f;        // ~8vw
    float labelX  = 120.f;        // flush-left inset inside the panel
    float slidePx = 28.f;         // label slide distance

    bool busy() const { return phase != Phase::Idle; }

    // Request a scene change. Ignored mid-transition or if already there.
    void go(int scene) {
        if (busy() || scene == current) return;
        target = scene;
        phase  = Phase::Cover;
        t      = 0.f;
    }

    // easeInOut — matches cubic-bezier(.7,0,.2,1) closely enough.
    static float ease(float x) {
        return x < 0.5f ? 4 * x * x * x
                        : 1 - std::pow(-2 * x + 2, 3) / 2;
    }

    // Advance timing. Call once per frame with delta-seconds.
    void update(float dt) {
        if (phase == Phase::Idle) return;
        t += dt / dur;
        if (t < 1.f) return;
        t = 0.f;
        if (phase == Phase::Cover) {
            current = target;      // swap the scene under full cover
            phase   = Phase::Reveal;
        } else {
            phase = Phase::Idle;   // reveal done
        }
    }

    // Draw the overlay on top of the already-rendered scene.
    void draw() const {
        if (phase == Phase::Idle) return;
        float e = ease(t);

        // Panel: full height; width grows on cover, whole thing slides on reveal.
        float coverW = (phase == Phase::Cover) ? e * canvasW : canvasW;
        float offX   = (phase == Phase::Reveal) ? e * canvasW : 0.f;
        Draw::rect(offX, 0, coverW, canvasH, Pal::ACID);

        // Next-scene name, black, flush toward the panel's left edge.
        const std::string& label = names[target];
        float slide = (phase == Phase::Cover) ? (1 - e) * slidePx : e * slidePx;
        float lx = offX + labelX + (phase == Phase::Cover ? -slide : slide);
        Draw::text(lx, canvasH * 0.5f - labelPx * 0.5f, label, labelPx,
                   Weight::Heavy, Pal::INK);
    }
};
