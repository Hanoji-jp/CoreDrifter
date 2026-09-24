// ============================================================================
//  Drift Project — MAIN MENU + ELEMENTS (UI kit) screens
//  Reference C++ for kdframework. Shares the conventions of
//  drift_settings.cpp: the Draw:: API, Pal:: palette, Weight/Align enums,
//  and the Kit:: primitives (keycap / toggle / stepper). Compile alongside it
//  or copy those decls in. Canvas 1536 x 864, flush-left, 0 radius, 2px rules.
// ============================================================================

#include <string>
#include <vector>
#include <cmath>

// --- from drift_settings.cpp (declared here for standalone reading) ---------
struct Color { unsigned char r, g, b, a = 255; };
namespace Pal {
    constexpr Color INK  {0x14,0x14,0x14}, ACID {0xCF,0xE0,0x21},
                    GREY {0xC6,0xC6,0xC6}, BG   {0xF1,0xF0,0xEC},
                    WHITE{0xFF,0xFF,0xFF}, MUTE {0x8A,0x8A,0x88};
    // modal-header accents
    constexpr Color PINK {0xE7,0x9E,0xC0}, BLUE {0x4A,0x97,0xB8},
                    SAND {0xC9,0xC2,0xB4};
}
enum class Align { Left, Center, Right };
enum class Weight { Regular = 400, Bold = 700, Heavy = 900 };
namespace Draw {
    void rect(float x, float y, float w, float h, Color c);
    void rectLine(float x, float y, float w, float h, float t, Color c);
    void line(float x1, float y1, float x2, float y2, float t, Color c);
    void circle(float cx, float cy, float r, float t, Color c);   // outlined
    void text(float x, float y, const std::string& s, float px,
              Weight wt, Color c, Align a = Align::Left);
    float textWidth(const std::string& s, float px, Weight wt);
    void image(float x, float y, float w, float h, int slotId);   // grayscale art
}

// ============================================================================
//  Extra kit primitives used by these two screens
// ============================================================================
namespace Kit {

    enum class BtnVariant { Primary, Secondary, Ghost, Disabled };

    // Flush-left button (label starts at the left padding edge, never centered).
    void button(float x, float y, float w, float h, const std::string& label,
                BtnVariant v, const std::string& trailing = "") {
        const float pad = 16;
        switch (v) {
            case BtnVariant::Primary:
                Draw::rect(x, y, w, h, Pal::ACID);
                Draw::text(x + pad, y + h*0.5f - 8, label, 15, Weight::Heavy, Pal::INK);
                break;
            case BtnVariant::Secondary:
                Draw::rectLine(x, y, w, h, 2, Pal::INK);
                Draw::text(x + pad, y + h*0.5f - 8, label, 15, Weight::Heavy, Pal::INK);
                break;
            case BtnVariant::Ghost:
                Draw::text(x + pad, y + h*0.5f - 8, label, 15, Weight::Heavy, Pal::INK);
                break;
            case BtnVariant::Disabled:
                Draw::rectLine(x, y, w, h, 2, {Pal::INK.r,Pal::INK.g,Pal::INK.b,110});
                Draw::text(x + pad, y + h*0.5f - 8, label, 15, Weight::Heavy,
                           {Pal::INK.r,Pal::INK.g,Pal::INK.b,110});
                break;
        }
        if (!trailing.empty())
            Draw::text(x + w - pad - Draw::textWidth(trailing,15,Weight::Heavy),
                       y + h*0.5f - 8, trailing, 15, Weight::Heavy, Pal::INK);
    }

    // Square icon button. style 0=solid ink, 1=outline, 2=acid.
    void iconButton(float x, float y, float s, const std::string& glyph, int style) {
        Color fill = style==0 ? Pal::INK : style==2 ? Pal::ACID : Pal::BG;
        Color ink  = style==0 ? Pal::WHITE : Pal::INK;
        if (style==1) Draw::rectLine(x, y, s, s, 2, Pal::INK);
        else          Draw::rect(x, y, s, s, fill);
        Draw::text(x + s*0.5f, y + s*0.5f - 10, glyph, 20, Weight::Bold, ink, Align::Center);
    }

    // Tab button (active = acid fill, else muted).
    void tab(float x, float y, const std::string& label, bool active) {
        float w = Draw::textWidth(label, 15, Weight::Heavy) + 32;
        if (active) Draw::rect(x, y, w, 34, Pal::ACID);
        Draw::text(x + 16, y + 9, label, 15, Weight::Heavy,
                   active ? Pal::INK : Pal::MUTE);
    }

    // Two-part tag  [ left | right ] , right cell optionally acid-filled.
    void tag(float x, float y, const std::string& left, const std::string& right,
             bool rightAcid) {
        float lw = Draw::textWidth(left, 13, Weight::Heavy) + 22;
        float rw = Draw::textWidth(right,13, Weight::Heavy) + 22;
        Draw::rectLine(x, y, lw, 30, 2, Pal::INK);
        Draw::text(x + 11, y + 8, left, 13, Weight::Heavy, Pal::INK);
        if (rightAcid) Draw::rect(x + lw, y, rw, 30, Pal::ACID);
        Draw::rectLine(x + lw, y, rw, 30, 2, Pal::INK);
        Draw::text(x + lw + 11, y + 8, right, 13, Weight::Heavy, Pal::INK);
    }

    // Horizontal stat bar: ink track, acid fill (0..1).
    void statBar(float x, float y, float w, const std::string& label, float v) {
        Draw::text(x, y, label, 11, Weight::Bold, Pal::INK);
        float by = y + 18, h = 14;
        Draw::rect(x, by, w, h, Pal::INK);
        Draw::rect(x, by, w * v, h, Pal::ACID);
    }

    // Menu / list row. active = acid fill, else bottom rule.
    void menuRow(float x, float y, float w, const std::string& glyph,
                 const std::string& label, bool active) {
        float h = 48;
        if (active) Draw::rect(x, y, w, h, Pal::ACID);
        else Draw::line(x, y + h, x + w, y + h, 2, {Pal::INK.r,Pal::INK.g,Pal::INK.b,80});
        Draw::text(x + 14, y + h*0.5f - 9, glyph, 18, Weight::Heavy, Pal::INK);
        Draw::text(x + 46, y + h*0.5f - 9, label, 15, Weight::Heavy, Pal::INK);
    }

    // Window (modal) header bar: colored strip + title + close glyph.
    void windowHeader(float x, float y, float w, const std::string& title,
                      Color bg, Color fg) {
        float h = 34;
        Draw::rect(x, y, w, h, bg);
        Draw::rectLine(x, y, w, h, 2, Pal::INK);
        Draw::text(x + 12, y + 9, title, 13, Weight::Heavy, fg);
        Draw::text(x + w - 22, y + 9, "x", 13, Weight::Heavy, fg);
    }
}

// ============================================================================
//  DECORATION LAYER (shared motifs; pass a running clock for the animated bits)
// ============================================================================
namespace Deco {

    // Same-size circles stepping toward the upper-left (title motif).
    void driftCircles(float startX, float startY, float r, int n,
                      float stepX, float stepY, Color c) {
        for (int i = 0; i < n; ++i)
            Draw::circle(startX - i*stepX, startY - i*stepY, r, 1, c);
    }

    // Boxed X.
    void boxedX(float x, float y, float s) {
        Draw::rectLine(x, y, s, s, 5, Pal::ACID);
        Draw::line(x+7, y+7, x+s-7, y+s-7, 4, Pal::INK);
        Draw::line(x+s-7, y+7, x+7, y+s-7, 4, Pal::INK);
    }

    // Dot field: rows x cols of small dots (kdframework: filled circles).
    void dotField(float x, float y, int cols, int rows, float gap, Color c) {
        for (int r = 0; r < rows; ++r)
            for (int cc = 0; cc < cols; ++cc)
                Draw::rect(x + cc*gap, y + r*gap, 2.4f, 2.4f, c); // or small circle
    }

    // Barcode: vertical lines of varied length.
    void barcode(float x, float y, float h, int n) {
        for (int i = 0; i < n; ++i)
            Draw::line(x + i*5, y, x + i*5, y + h * (0.4f + 0.6f*((i*37)%10)/10.f),
                       2, Pal::INK);
    }
}

// ============================================================================
//  MAIN MENU SCREEN
// ============================================================================
struct MenuModel {
    struct Item { std::string glyph, label; };
    std::vector<Item> items = {
        {"\u25B6", "PLAY"}, {"\u2302", "GARAGE"}, {"\u2699", "SETTINGS"},
        {"\u25A5", "STATISTICS"}, {"\u21E5", "QUIT"}
    };
    int active = 0;
};

void drawMenu(const MenuModel& m) {
    const float PADX = 54, PADY = 40;
    Draw::rect(0, 0, 1536, 864, Pal::BG);

    // ── Decoration (behind content)
    Deco::driftCircles(700, 620, 120, 6, 12, 12, {Pal::INK.r,Pal::INK.g,Pal::INK.b,98});
    Deco::boxedX(700, 636, 54);
    Deco::dotField(1170, 120, 12, 9, 13, {Pal::INK.r,Pal::INK.g,Pal::INK.b,120});
    Deco::barcode(26, 200, 22, 6);

    // ── Interrupted vertical construction lines + crop ticks at the breaks
    {
        const Color gl {Pal::INK.r, Pal::INK.g, Pal::INK.b, 18};  // ~0.07 alpha
        const Color tk {Pal::INK.r, Pal::INK.g, Pal::INK.b, 56};  // ~0.22 alpha
        struct Seg { float x, y0, y1; };
        const Seg segs[] = {
            {470,   0, 180}, {470, 240, 560},   // column 1, split
            {770, 120, 430}, {770, 500, 864},   // column 2, split
            {1120,  0, 120}, {1120, 300, 700},  // column 3, split
        };
        for (auto& s : segs) Draw::line(s.x, s.y0, s.x, s.y1, 1, gl);
        auto tick = [&](float x, float y){ Draw::line(x-5, y, x+5, y, 1.5f, tk); };
        tick(470,180); tick(470,240); tick(770,430); tick(770,500);
        tick(1120,120); tick(1120,300);
    }

    // ── Top strip: kicker + driver tag
    Draw::text(PADX + 34, PADY, "BUILT TO SLIDE.", 13, Weight::Heavy, Pal::INK);
    Draw::text(PADX + 34, PADY + 18, "EST. 2024", 13, Weight::Heavy, Pal::INK);
    Draw::line(PADX + 34, PADY + 38, PADX + 160, PADY + 38, 2, Pal::INK);
    Kit::tag(1536 - PADX - 210, PADY, "DRIVER01", "LV.23", true);

    // ── Title
    float ty = PADY + 54;
    Draw::text(PADX, ty, "DRIFT", 150, Weight::Heavy, Pal::INK);
    Draw::text(PADX, ty + 118, "PROJECT", 106, Weight::Heavy, Pal::ACID);

    // ── Black tagline bar
    float bY = ty + 236;
    std::string tagline = "CHASE CONTROL, NOT GLORY.  +";
    float bw = Draw::textWidth(tagline, 14, Weight::Heavy) + 30;
    Draw::rect(PADX, bY, bw, 40, Pal::INK);
    Draw::text(PADX + 15, bY + 12, tagline, 14, Weight::Heavy, Pal::WHITE);

    // ── Menu list
    float listX = PADX, listY = bY + 72, listW = 400;
    for (int i = 0; i < (int)m.items.size(); ++i)
        Kit::menuRow(listX, listY + i*50, listW,
                     m.items[i].glyph, m.items[i].label, i == m.active);

    // ── Footer
    float fy = listY + m.items.size()*50 + 30;
    Draw::rect(PADX, fy + 3, 12, 12, Pal::ACID);
    Draw::text(PADX + 22, fy, "Ver. 0.1.0", 12, Weight::Bold, Pal::INK);
    Draw::text(PADX + 150, fy, "// WELCOME BACK, DRIVER.", 12, Weight::Bold, Pal::INK);

    // ── Masked hero photo (grayscale) + acid duotone slice
    float ix = 820, iy = 128, iw = 590, ih = 540;
    Draw::image(ix, iy, iw, ih, /*slotId*/ 1);
    Draw::rect(ix + iw*0.56f, iy, iw*0.44f, ih,
               {Pal::ACID.r, Pal::ACID.g, Pal::ACID.b, 150}); // multiply-ish overlay
    Draw::rect(ix, iy + ih - 30, 160, 30, Pal::ACID);

    // ── Now-playing chip
    float nx = 1090, ny = 780, nw = 346, nh = 60;
    Draw::rect(nx, ny, nw, nh, Pal::WHITE);
    Draw::rectLine(nx, ny, nw, nh, 2, Pal::INK);
    Draw::text(nx + 60, ny + 14, "MIDNIGHT DRIVE", 14, Weight::Heavy, Pal::INK);
    Draw::text(nx + 60, ny + 34, "KORDHELL", 11, Weight::Bold, Pal::MUTE);
    Kit::iconButton(nx + nw - 52, ny + 7, 46, "\u25B6", 0);
}

// ============================================================================
//  ELEMENTS / UI-KIT SCREEN  (state catalog for implementation reference)
// ============================================================================
void drawElements() {
    const float PADX = 64, PADY = 48;
    Draw::rect(0, 0, 1536, 864, Pal::BG);

    // Title
    Draw::text(PADX, PADY, "ELEMENTS", 56, Weight::Heavy, Pal::INK);
    float tw = Draw::textWidth("ELEMENTS ", 56, Weight::Heavy);
    Draw::text(PADX + tw, PADY + 6, "/ UI KIT", 56, Weight::Heavy, Pal::ACID);

    // 3-column grid of labeled cells
    const float gx = PADX, gy = PADY + 100;
    const float colW = (1536 - PADX*2 - 96) / 3.f, colGap = 48;
    const float rowH = 240;
    auto cellX = [&](int c){ return gx + c*(colW + colGap); };
    auto cellY = [&](int r){ return gy + r*rowH; };
    auto head  = [&](int c, int r, const std::string& t){
        float x = cellX(c), y = cellY(r);
        Draw::text(x, y, t, 12, Weight::Heavy, Pal::INK);
        Draw::line(x, y + 20, x + colW, y + 20, 2, Pal::INK);
        return std::pair<float,float>{x, y + 36};
    };

    // 01 BUTTON
    { auto [x,y] = head(0,0,"01 - BUTTON");
      Kit::button(x, y,       220, 46, "PRIMARY",   Kit::BtnVariant::Primary, "\u2197");
      Kit::button(x, y+58,    220, 46, "SECONDARY", Kit::BtnVariant::Secondary);
      Kit::button(x, y+116,   120, 46, "GHOST",     Kit::BtnVariant::Ghost);
      Kit::button(x+134, y+116,120,46, "DISABLED",  Kit::BtnVariant::Disabled); }

    // 02 ICON BUTTON
    { auto [x,y] = head(1,0,"02 - ICON BUTTON");
      Kit::iconButton(x,        y, 52, "\u25B6", 0);
      Kit::iconButton(x+66,     y, 52, "+",      1);
      Kit::iconButton(x+132,    y, 52, "x",      2); }

    // 03 TOGGLE
    { auto [x,y] = head(2,0,"03 - TOGGLE");
      Kit::toggle(x, y,     true,  false);   // ON
      Kit::toggle(x, y+48,  false, false);   // OFF
      Kit::toggle(x, y+96,  true,  true);  } // ON accent

    // 04 TAB BUTTON
    { auto [x,y] = head(0,1,"04 - TAB BUTTON");
      Kit::tab(x, y,    "GAMEPLAY", true);
      Kit::tab(x, y+44, "CONTROLS", false);
      Kit::tab(x, y+88, "GRAPHICS", false); }

    // 05 STEPPER / SELECT
    { auto [x,y] = head(1,1,"05 - STEPPER / SELECT");
      Kit::stepper(x, y,    "NORMAL");
      Kit::stepper(x, y+50, "KM/H"); }

    // 06 KEYCAP
    { auto [x,y] = head(2,1,"06 - KEYCAP");
      float u = Kit::keycap(x, y, "ENTER", "SELECT");
      Kit::keycap(x, y+42, "ESC", "BACK"); (void)u; }

    // 07 TAG / BADGE
    { auto [x,y] = head(0,2,"07 - TAG / BADGE");
      Kit::tag(x, y, "DRIVER01", "LV.23", true);
      Kit::tag(x, y+44, "TIER", "A", false); }

    // 08 STAT BAR
    { auto [x,y] = head(1,2,"08 - STAT BAR");
      Kit::statBar(x, y,    240, "SPEED", 0.72f);
      Kit::statBar(x, y+50, 240, "DRIFT", 0.92f); }

    // 09 WINDOW HEADERS (4 accent colors)
    { auto [x,y] = head(2,2,"09 - WINDOW HEADER");
      Kit::windowHeader(x, y,     260, "CONFIRM",     Pal::ACID, Pal::INK);
      Kit::windowHeader(x, y+42,  260, "NOTICE",      Pal::PINK, Pal::INK);
      Kit::windowHeader(x, y+84,  260, "DELETE",      Pal::BLUE, Pal::WHITE);
      Kit::windowHeader(x, y+126, 260, "INFORMATION", Pal::SAND, Pal::INK); }
}

// ----------------------------------------------------------------------------
//  Wire into the same frame()/Transition loop as drift_settings.cpp:
//     case 0: drawMenu(menu);      break;
//     case 4: drawElements();      break;
//  All coordinates are in 1536x864 space.
// ----------------------------------------------------------------------------
