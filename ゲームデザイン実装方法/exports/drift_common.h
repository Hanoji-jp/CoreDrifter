// ============================================================================
//  drift_common.h — shared foundation for the Drift Project screens.
//  Palette, enums, and the Draw:: immediate-mode API the screens render
//  through. Map the Draw:: function bodies to kdframework in ONE .cpp.
//  Canvas 1536 x 864, origin top-left, flush-left, 0 radius, 2px rules.
// ============================================================================
#pragma once
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
//  Color + 3-color palette (+ modal-header accents)
// ----------------------------------------------------------------------------
struct Color { unsigned char r, g, b, a = 255; };

namespace Pal {
    constexpr Color INK   {0x14, 0x14, 0x14};   // accent / text / rules
    constexpr Color ACID  {0xCF, 0xE0, 0x21};   // primary
    constexpr Color GREY  {0xC6, 0xC6, 0xC6};   // secondary
    constexpr Color BG    {0xF1, 0xF0, 0xEC};   // ground
    constexpr Color WHITE {0xFF, 0xFF, 0xFF};
    constexpr Color MUTE  {0x8A, 0x8A, 0x88};   // disabled / muted label
    constexpr Color PINK  {0xE7, 0x9E, 0xC0};   // NOTICE header
    constexpr Color BLUE  {0x4A, 0x97, 0xB8};   // DELETE header
    constexpr Color SAND  {0xC9, 0xC2, 0xB4};   // INFORMATION header
    // Deep accent step — for warning-weight text that must stay legible on the
    // light ground (the accent itself is only 3:1, fine for chrome, not copy).
    constexpr Color ACCENT_700 {0x8E, 0x1C, 0x0B};
}

enum class Align  { Left, Center, Right };
enum class Weight { Regular = 400, Bold = 700, Heavy = 900 };

// ----------------------------------------------------------------------------
//  Drawing API — replace bodies with kdframework calls in one translation unit.
// ----------------------------------------------------------------------------
namespace Draw {
    void  rect(float x, float y, float w, float h, Color c);
    void  rectLine(float x, float y, float w, float h, float t, Color c);
    void  line(float x1, float y1, float x2, float y2, float t, Color c);
    void  circle(float cx, float cy, float r, float t, Color c);   // outlined
    void  disc(float cx, float cy, float r, Color c);              // filled
    void  arc(float cx, float cy, float r, float a0, float a1,     // radians
              float t, Color c);
    void  polyline(const float* xy, int count, float t, Color c);  // open path
    void  text(float x, float y, const std::string& s, float px,
               Weight wt, Color c, Align a = Align::Left);
    float textWidth(const std::string& s, float px, Weight wt);
    void  image(float x, float y, float w, float h, int slotId);   // grayscale art
    void  pushClip(float x, float y, float w, float h);
    void  popClip();
}
