// ============================================================================
//  Drift Project — MODAL / SELECTION-WINDOW screen + reusable modal()
//  Reference C++ for kdframework. Shares conventions with drift_settings.cpp
//  and drift_menu_elements.cpp: the Draw:: API, Pal:: palette, Weight/Align,
//  and Kit:: primitives (button / windowHeader). Canvas 1536 x 864,
//  flush-left labels, 0 radius, 2px rules.
// ============================================================================

#include <string>
#include <vector>

// --- shared decls (see the other two files) ---------------------------------
struct Color { unsigned char r, g, b, a = 255; };
namespace Pal {
    constexpr Color INK  {0x14,0x14,0x14}, ACID {0xCF,0xE0,0x21},
                    GREY {0xC6,0xC6,0xC6}, BG   {0xF1,0xF0,0xEC},
                    WHITE{0xFF,0xFF,0xFF}, MUTE {0x8A,0x8A,0x88},
                    PINK {0xE7,0x9E,0xC0}, BLUE {0x4A,0x97,0xB8},
                    SAND {0xC9,0xC2,0xB4};
}
enum class Align { Left, Center, Right };
enum class Weight { Regular = 400, Bold = 700, Heavy = 900 };
namespace Draw {
    void rect(float x, float y, float w, float h, Color c);
    void rectLine(float x, float y, float w, float h, float t, Color c);
    void line(float x1, float y1, float x2, float y2, float t, Color c);
    void text(float x, float y, const std::string& s, float px,
              Weight wt, Color c, Align a = Align::Left);
    float textWidth(const std::string& s, float px, Weight wt);
}
namespace Kit {
    enum class BtnVariant { Primary, Secondary, Ghost, Disabled };
    void button(float x, float y, float w, float h, const std::string& label,
                BtnVariant v, const std::string& trailing = "");
    void windowHeader(float x, float y, float w, const std::string& title,
                      Color bg, Color fg);
}

// ============================================================================
//  MODAL DATA MODEL
// ============================================================================
enum class ModalKind { Confirm, Notice, Delete, Information };

struct ModalButton {
    std::string label;
    bool        primary;   // true = solid ink, false = outlined ghost
};

struct Modal {
    ModalKind                kind;
    std::string              title;
    std::vector<std::string> body;    // one entry per line (centered)
    std::vector<ModalButton> buttons;
};

// Header color per kind (INK text except DELETE which is white-on-blue).
static void headerColors(ModalKind k, Color& bg, Color& fg) {
    switch (k) {
        case ModalKind::Confirm:     bg = Pal::ACID; fg = Pal::INK;   break;
        case ModalKind::Notice:      bg = Pal::PINK; fg = Pal::INK;   break;
        case ModalKind::Delete:      bg = Pal::BLUE; fg = Pal::WHITE; break;
        case ModalKind::Information: bg = Pal::SAND; fg = Pal::INK;   break;
    }
}

// ============================================================================
//  modal() — draw one selection window at (x,y) with a fixed width.
//  Height grows with the body/buttons. Returns the height consumed.
// ============================================================================
float modal(float x, float y, float w, const Modal& m) {
    Color hbg, hfg; headerColors(m.kind, hbg, hfg);
    const float headH = 34;
    const float bodyPadTop = 32, lineH = 26, gapToBtns = 28;
    const float btnH = 44, btnPadX = 30, btnGap = 14, bodyPadBot = 24;

    float bodyH = bodyPadTop + m.body.size()*lineH + gapToBtns + btnH + bodyPadBot;
    float totalH = headH + bodyH;

    // Frame
    Draw::rect(x, y, w, totalH, Pal::BG);
    Draw::rectLine(x, y, w, totalH, 2, Pal::INK);

    // Header
    Kit::windowHeader(x, y, w, m.title, hbg, hfg);

    // Body lines (centered)
    float cx = x + w * 0.5f;
    float ly = y + headH + bodyPadTop;
    for (const auto& ln : m.body) {
        Draw::text(cx, ly, ln, 17, Weight::Bold, Pal::INK, Align::Center);
        ly += lineH;
    }

    // Buttons row (centered as a group)
    float totalBtnW = 0;
    std::vector<float> widths;
    for (const auto& b : m.buttons) {
        float bw = Draw::textWidth(b.label, 15, Weight::Heavy) + btnPadX*2;
        widths.push_back(bw);
        totalBtnW += bw;
    }
    totalBtnW += btnGap * (m.buttons.size() - 1);

    float bx = cx - totalBtnW * 0.5f;
    float by = y + totalH - bodyPadBot - btnH;
    for (size_t i = 0; i < m.buttons.size(); ++i) {
        const auto& b = m.buttons[i];
        Kit::button(bx, by, widths[i], btnH, b.label,
                    b.primary ? Kit::BtnVariant::Primary
                              : Kit::BtnVariant::Secondary);
        bx += widths[i] + btnGap;
    }
    return totalH;
}

// ============================================================================
//  MODAL EXAMPLES SCREEN  (four windows side by side)
// ============================================================================
void drawModals() {
    const float PADX = 64, PADY = 48;
    Draw::rect(0, 0, 1536, 864, Pal::BG);

    Draw::text(PADX, PADY, "SELECTION WINDOW EXAMPLES", 22, Weight::Heavy, Pal::INK);

    std::vector<Modal> modals = {
        { ModalKind::Confirm, "CONFIRM",
          { "Purchase this vehicle?", "NX7" },
          { {"YES", false}, {"NO", true} } },

        { ModalKind::Notice, "NOTICE",
          { "Changes have been saved." },
          { {"OK", true} } },

        { ModalKind::Delete, "DELETE",
          { "This data will be deleted.", "This action cannot be undone." },
          { {"DELETE", true}, {"CANCEL", false} } },

        { ModalKind::Information, "INFORMATION",
          { "Connect to online services?" },
          { {"ONLINE", true}, {"OFFLINE", false} } },
    };

    const int cols = 4;
    const float gap = 28;
    const float colW = (1536 - PADX*2 - gap*(cols-1)) / cols;
    const float top  = PADY + 60;

    for (int i = 0; i < (int)modals.size(); ++i)
        modal(PADX + i*(colW + gap), top, colW, modals[i]);

    // ── Legend (color roles)
    float ly = 700;
    auto swatch = [&](float x, Color c, const std::string& label){
        Draw::rect(x, ly, 32, 16, c);
        if (c.r > 0x30 || c.g > 0x30) {} // (no border needed on light chips)
        Draw::text(x + 44, ly + 1, label, 14, Weight::Bold, Pal::INK);
        return x + 44 + Draw::textWidth(label, 14, Weight::Bold) + 34;
    };
    float x = PADX;
    x = swatch(x, Pal::ACID, "PRIMARY");
    x = swatch(x, Pal::GREY, "SECONDARY");
    x = swatch(x, Pal::INK,  "ACCENT");
}

// ----------------------------------------------------------------------------
//  Wire into the shared frame()/Transition loop:
//     case 3: drawModals(); break;
//  modal() is standalone — call it anywhere to pop a window over a live scene
//  (draw a translucent ink backdrop first if you want a dialog overlay):
//     Draw::rect(0,0,1536,864, {0x14,0x14,0x14, 130});
//     modal(1536*0.5f - 200, 320, 400, someModal);
// ----------------------------------------------------------------------------
