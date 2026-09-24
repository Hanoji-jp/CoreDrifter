// ============================================================================
//  Drift Project — MULTIPLAYER screens: LOBBY, ROOM, MATCHMAKING, RESULTS.
//  Reference C++ for kdframework. Canvas 1536 x 864, flush-left, 0 radius,
//  2px rules. Scoring is drift-run based: no laps, no race positions.
// ============================================================================

#include "drift_common.h"
#include "drift_kit.h"
#include <algorithm>
#include <cstdio>

namespace {
    constexpr float PADX = 64.f, CANVAS_W = 1536.f, CANVAS_H = 864.f;
    // Guide / tick alphas used across every screen (very faint by design).
    const Color GUIDE {Pal::INK.r, Pal::INK.g, Pal::INK.b, 18};
    const Color TICK  {Pal::INK.r, Pal::INK.g, Pal::INK.b, 56};
    const Color DOT   {Pal::INK.r, Pal::INK.g, Pal::INK.b, 115};

    // Title: "WORD / ACCENTWORD" with the second half in acid.
    void splitTitle(float x, float y, const std::string& head,
                    const std::string& accent, float px) {
        Draw::text(x, y, head, px, Weight::Heavy, Pal::INK);
        float w = Draw::textWidth(head + " / ", px, Weight::Heavy);
        Draw::text(x + Draw::textWidth(head + " ", px, Weight::Heavy), y, "/",
                   px, Weight::Regular, Pal::MUTE);
        Draw::text(x + w, y, accent, px, Weight::Heavy, Pal::ACID);
    }

    // Right-aligned label-over-value block.
    void statRight(float rightX, float y, const std::string& label,
                   const std::string& value) {
        Draw::text(rightX, y, label, 12, Weight::Bold, Pal::MUTE, Align::Right);
        Draw::text(rightX, y + 16, value, 24, Weight::Bold, Pal::INK, Align::Right);
    }
}

// ============================================================================
//  LOBBY — room browser
// ============================================================================
struct LobbyRoom {
    std::string name, mode, track, players;
    int         ping;
};

struct LobbyModel {
    std::vector<std::string> filters = { "ALL", "RANKED", "CASUAL", "PRIVATE" };
    int activeFilter = 0;
    std::vector<LobbyRoom> rooms = {
        { "MIDNIGHT RUN", "RANKED",  "NEXUS TOUGE",   "6 / 8", 24 },
        { "TOUGE NIGHTS", "CASUAL",  "KATANA PASS",   "4 / 8", 38 },
        { "APEX HUNTERS", "RANKED",  "VORTEX BAY",    "8 / 8", 52 },
        { "SLIDE SCHOOL", "CASUAL",  "TRAINING YARD", "2 / 8", 18 },
        { "OUTLAW TAG",   "CASUAL",  "DOWNTOWN LOOP", "5 / 8", 96 },
        { "TIER A ONLY",  "PRIVATE", "PHOENIX RIDGE", "3 / 8", 31 },
    };
    int selected = 0;
};

void drawLobby(const LobbyModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, Pal::BG);

    // Deco: interrupted guides + a dot field in the dead corner.
    Deco::guide(360, 0, 200, GUIDE);   Deco::guide(360, 260, CANVAS_H, GUIDE);
    Deco::guide(1290, 0, 140, GUIDE);  Deco::guide(1290, 300, CANVAS_H, GUIDE);
    Deco::cropTick(360, 200, TICK);    Deco::cropTick(360, 260, TICK);
    Deco::cropTick(1290, 140, TICK);   Deco::cropTick(1290, 300, TICK);
    Deco::dotField(1210, 760, 11, 4, 13, DOT);
    Draw::rect(1400, 60, 34, 70, Pal::ACID);

    // Header
    splitTitle(PADX, 44, "LOBBY", "ROOM LIST", 56);
    statRight(CANVAS_W - PADX, 52, "ONLINE NOW", "1,284 DRIVERS");

    // Filter chips
    float fx = PADX, fy = 152;
    for (int i = 0; i < (int)m.filters.size(); ++i) {
        bool on = (i == m.activeFilter);
        float w = Draw::textWidth(m.filters[i], 14, Weight::Heavy) + 32;
        if (on) Draw::rect(fx, fy, w, 38, Pal::ACID);
        else    Draw::rectLine(fx, fy, w, 38, 2, Pal::INK);
        Draw::text(fx + 16, fy + 11, m.filters[i], 14, Weight::Heavy, Pal::INK);
        fx += w + 10;
    }

    // Table
    const float tx = PADX, ty = 212, tw = CANVAS_W - PADX * 2;
    const float colX[5] = { tx + 20, tx + 700, tx + 870, tx + 1130, tx + 1270 };
    const std::string cols[5] = { "ROOM NAME", "MODE", "TRACK", "PLAYERS", "PING" };
    Draw::rectLine(tx, ty, tw, 44 + 74 * m.rooms.size(), 2, Pal::INK);
    Kit::tableHead(tx, ty, tw, cols, colX, 5);

    for (int i = 0; i < (int)m.rooms.size(); ++i) {
        const LobbyRoom& r = m.rooms[i];
        float ry = ty + 44 + i * 74;
        bool sel  = (i == m.selected);
        bool full = (r.players == "8 / 8");
        if (sel) Draw::rect(tx + 2, ry, tw - 4, 74, Pal::ACID);
        if (i > 0) Draw::line(tx, ry, tx + tw, ry, 2, Pal::INK);

        // Full rooms read back at ~45% — same treatment as a disabled control.
        Color ink = full ? Color{Pal::INK.r, Pal::INK.g, Pal::INK.b, 115} : Pal::INK;
        float cy = ry + 37 - 8;
        Draw::text(colX[0], cy, r.name,    16, Weight::Heavy, ink);
        Draw::text(colX[1], cy, r.mode,    14, Weight::Bold,  ink);
        Draw::text(colX[2], cy, r.track,   14, Weight::Bold,  Pal::MUTE);
        Draw::text(colX[3], cy, r.players, 15, Weight::Heavy, ink);
        // High ping warns in a deep accent step rather than a new hue.
        Color pc = (r.ping > 80) ? Pal::ACCENT_700 : ink;
        Draw::text(colX[4], cy, std::to_string(r.ping) + "MS", 14, Weight::Heavy, pc);
    }

    // Footer actions
    float by = ty + 44 + 74 * m.rooms.size() + 34;
    Kit::button(PADX, by, 210, 50, "> QUICK JOIN", Kit::BtnVariant::Primary);
    Kit::button(PADX + 240, by, 210, 50, "CREATE ROOM", Kit::BtnVariant::Secondary);
    float used = Kit::keycap(CANVAS_W - PADX - 300, by + 10, "F5", "REFRESH");
    Kit::keycap(CANVAS_W - PADX - 300 + used + 30, by + 10, "ESC", "BACK");
}

// ============================================================================
//  ROOM — ready-up grid
// ============================================================================
struct RoomSlot {
    std::string name, lv;      // empty name => open slot
    bool        host, ready;
    Color       swatch;
};

struct RoomModel {
    std::string title = "MIDNIGHT RUN", mode = "RANKED", track = "NEXUS TOUGE";
    std::vector<RoomSlot> slots = {
        { "DRIVER01", "LV.23", true,  true,  Pal::ACID },
        { "KAZE_9",   "LV.31", false, true,  {0xE0,0x56,0x3C} },
        { "NOCTURN",  "LV.18", false, true,  {0x2C,0x2C,0x2C} },
        { "MIRA.S",   "LV.44", false, true,  Pal::PINK },
        { "TAKT",     "LV.12", false, true,  Pal::SAND },
        { "V0RTEX",   "LV.27", false, false, Pal::BLUE },
        { "SLIDEBOY", "LV.9",  false, true,  Pal::GREY },
        { "",         "",      false, false, {0,0,0,0} },
    };
    int readyCount = 6;
};

void drawRoom(const RoomModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, Pal::BG);

    Deco::guide(768, 0, 120, GUIDE);  Deco::guide(768, 700, CANVAS_H, GUIDE);
    Deco::cropTick(768, 120, TICK);   Deco::cropTick(768, 700, TICK);
    Deco::dotField(60, 790, 10, 3, 13, DOT);
    // Acid corner mark laid over the top-right, with a diagonal hatch.
    Draw::rect(1400, 52, 34, 76, Pal::ACID);
    Draw::line(1400, 128, 1434, 52, 1.5f, Pal::INK);
    Draw::line(1400, 106, 1420, 52, 1.5f, Pal::INK);

    // Header
    Draw::text(PADX, 44, "ROOM \u00B7 " + m.mode, 13, Weight::Heavy, Pal::MUTE);
    Draw::text(PADX, 62, m.title, 52, Weight::Heavy, Pal::INK);
    Kit::tag(CANVAS_W - PADX - 300, 52, "TRACK", m.track, true);

    // 4 x 2 slot grid
    const float gx = PADX, gy = 178, gap = 18;
    const float cw = (CANVAS_W - PADX * 2 - gap * 3) / 4.f, ch = 232;
    for (int i = 0; i < (int)m.slots.size(); ++i) {
        const RoomSlot& s = m.slots[i];
        float x = gx + (i % 4) * (cw + gap);
        float y = gy + (i / 4) * (ch + gap);
        bool empty = s.name.empty();

        // Empty slots drop to ~35%; filled slots get a paper fill.
        Color edge = empty ? Color{Pal::INK.r, Pal::INK.g, Pal::INK.b, 90} : Pal::INK;
        if (!empty) Draw::rect(x, y, cw, ch, Pal::WHITE);
        Draw::rectLine(x, y, cw, ch, 2, edge);

        char num[8]; std::snprintf(num, sizeof num, "P%02d", i + 1);
        Draw::text(x + 18, y + 18, num, 11, Weight::Heavy, Pal::MUTE);
        if (s.host) {
            float hw = Draw::textWidth("HOST", 10, Weight::Heavy) + 16;
            Draw::rect(x + cw - 18 - hw, y + 15, hw, 22, Pal::INK);
            Draw::text(x + cw - 10 - hw, y + 20, "HOST", 10, Weight::Heavy, Pal::WHITE);
        }

        // Car color swatch — dashed outline when the slot is open.
        float sy = y + 46;
        if (empty) Draw::rectLine(x + 18, sy, cw - 36, 44, 2, edge);
        else       Draw::rect(x + 18, sy, cw - 36, 44, s.swatch);

        Draw::text(x + 18, sy + 58, empty ? "OPEN" : s.name, 19, Weight::Heavy,
                   empty ? Pal::MUTE : Pal::INK);
        Draw::text(x + 18, y + ch - 34,
                   empty ? "WAITING FOR PLAYER" : s.lv, 12, Weight::Bold, Pal::MUTE);

        if (!empty) {
            const char* rl = s.ready ? "READY" : "NOT READY";
            float rw = Draw::textWidth(rl, 11, Weight::Heavy) + 20;
            float rx = x + cw - 18 - rw, ry = y + ch - 40;
            if (s.ready) Draw::rect(rx, ry, rw, 26, Pal::ACID);
            else         Draw::rectLine(rx, ry, rw, 26, 2, Pal::INK);
            Draw::text(rx + 10, ry + 7, rl, 11, Weight::Heavy, Pal::INK);
        }
    }

    // Ready progress + start
    float by = gy + ch * 2 + gap + 34;
    Draw::text(PADX, by + 4, "READY", 12, Weight::Heavy, Pal::INK);
    Draw::text(PADX + 56, by + 4, std::to_string(m.readyCount), 12, Weight::Heavy, Pal::ACID);
    Draw::text(PADX + 74, by + 4, "/ 8", 12, Weight::Heavy, Pal::INK);
    float barX = PADX + 130, barW = CANVAS_W - PADX * 2 - 130 - 220;
    Draw::rect(barX, by, barW, 14, Pal::INK);
    Draw::rect(barX, by, barW * (m.readyCount / 8.f), 14, Pal::ACID);
    Kit::button(CANVAS_W - PADX - 200, by - 18, 200, 50, "START RACE",
                Kit::BtnVariant::Primary, "\u2197");

    // Keycaps
    float ky = by + 56;
    float u1 = Kit::keycap(PADX, ky, "SPACE", "READY");
    float u2 = Kit::keycap(PADX + u1 + 30, ky, "T", "CHAT");
    Kit::keycap(PADX + u1 + u2 + 60, ky, "ESC", "LEAVE ROOM");
}

// ============================================================================
//  MATCHMAKING — searching state
// ============================================================================
struct MatchModel {
    int   found = 4, capacity = 8;
    int   elapsedSec = 23, pingMs = 24;
    std::string tier = "A", estWait = "0:40", mode = "DRIFT", region = "ASIA";
    float blinkPhase = 0.f;   // 0..1, drives the next-slot pip blink
};

void drawMatchmaking(const MatchModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, Pal::BG);

    // Tangled wobbly drift loops, slowly rotating (see Deco note below).
    // Deco::driftLoops(1120, 432, m.spinAngle);   // if you port the loop set
    Deco::dotField(70, 740, 12, 4, 13, DOT);
    Deco::guide(470, 0, 240, GUIDE);  Deco::guide(470, 320, CANVAS_H, GUIDE);
    Deco::cropTick(470, 240, TICK);   Deco::cropTick(470, 320, TICK);

    Draw::text(PADX, 44, "MATCHMAKING \u00B7 RANKED", 13, Weight::Heavy, Pal::INK);
    Draw::text(PADX, 68, "SEARCHING", 118, Weight::Heavy, Pal::INK);
    float sw = Draw::textWidth("SEARCHING", 118, Weight::Heavy);
    Draw::text(PADX + sw, 68, "...", 118, Weight::Heavy, Pal::ACID);

    // Inverted strapline
    const std::string strap = "FINDING DRIVERS OF SIMILAR TIER  +";
    float stw = Draw::textWidth(strap, 14, Weight::Heavy) + 32;
    Draw::rect(PADX, 210, stw, 40, Pal::INK);
    Draw::text(PADX + 16, 222, strap, 14, Weight::Heavy, Pal::WHITE);

    // Slot pips: filled = found, the next one blinks, the rest sit at 40%.
    const float ps = 74, pgap = 12;
    for (int i = 0; i < m.capacity; ++i) {
        float x = PADX + i * (ps + pgap), y = 320;
        bool filled = (i < m.found);
        bool next   = (i == m.found);
        Color edge = Pal::INK;
        if (filled)      Draw::rect(x, y, ps, ps, Pal::ACID);
        else if (next && m.blinkPhase < 0.5f) Draw::rect(x, y, ps, ps, Pal::ACID);
        else edge = Color{Pal::INK.r, Pal::INK.g, Pal::INK.b, 102};
        Draw::rectLine(x, y, ps, ps, 2, edge);
    }

    // Read-out row
    char buf[96];
    float ry = 418;
    std::snprintf(buf, sizeof buf, "DRIVERS FOUND %d / %d", m.found, m.capacity);
    Draw::text(PADX, ry, buf, 13, Weight::Heavy, Pal::INK);
    std::snprintf(buf, sizeof buf, "ELAPSED 00:%02d", m.elapsedSec);
    Draw::text(PADX + 300, ry, buf, 13, Weight::Heavy, Pal::INK);
    std::snprintf(buf, sizeof buf, "REGION %s \u00B7 PING %dMS",
                 m.region.c_str(), m.pingMs);
    Draw::text(PADX + 520, ry, buf, 13, Weight::Heavy, Pal::INK);

    // Three-cell panel; the last cell carries the acid.
    const float cw = 200, cy2 = 486, chh = 92;
    Draw::rectLine(PADX, cy2, cw * 3, chh, 2, Pal::INK);
    const std::string lbl[3] = { "TIER", "EST. WAIT", "MODE" };
    const std::string val[3] = { m.tier, m.estWait, m.mode };
    for (int i = 0; i < 3; ++i) {
        float x = PADX + i * cw;
        if (i == 2) Draw::rect(x, cy2, cw, chh, Pal::ACID);
        if (i < 2)  Draw::line(x + cw, cy2, x + cw, cy2 + chh, 2, Pal::INK);
        Draw::text(x + 20, cy2 + 18, lbl[i], 11, Weight::Bold, i == 2 ? Pal::INK : Pal::MUTE);
        Draw::text(x + 20, cy2 + 40, val[i], 26, Weight::Heavy, Pal::INK);
    }

    Kit::button(PADX, 636, 210, 50, "CANCEL SEARCH", Kit::BtnVariant::Secondary);
    Kit::keycap(PADX + 250, 646, "TAB", "EXPAND SEARCH");
}

// ============================================================================
//  RESULTS — drift standings (best run + judge breakdown, no lap times)
// ============================================================================
struct ResultRow {
    std::string rank, driver, bestRun, judge, pts;
    int         total;      // drives the bar length
};

struct ResultsModel {
    std::string course = "NEXUS TOUGE \u00B7 2 RUNS", credits = "+ \u00A5 18,400";
    std::vector<ResultRow> rows = {
        { "01", "DRIVER01", "14,020", "92 / 88 / 95", "25", 48250 },
        { "02", "KAZE_9",   "13,610", "90 / 86 / 91", "18", 44100 },
        { "03", "MIRA.S",   "12,480", "88 / 84 / 87", "15", 39880 },
        { "04", "NOCTURN",  "10,905", "82 / 79 / 80", "12", 31240 },
        { "05", "V0RTEX",   "9,740",  "78 / 76 / 74", "10", 28905 },
        { "06", "TAKT",     "7,120",  "70 / 68 / 66", "8",  19470 },
    };
};

void drawResults(const ResultsModel& m) {
    Draw::rect(0, 0, CANVAS_W, CANVAS_H, Pal::BG);

    Deco::dotField(1180, 70, 12, 8, 13, DOT);
    Deco::guide(420, 0, 180, GUIDE);  Deco::guide(420, 250, CANVAS_H, GUIDE);
    Deco::cropTick(420, 180, TICK);   Deco::cropTick(420, 250, TICK);
    // Baseline black bar, bottom-right.
    Draw::rect(960, 840, CANVAS_W - 960, 24, Pal::INK);

    Draw::text(PADX, 44, m.course, 13, Weight::Heavy, Pal::MUTE);
    Draw::text(PADX, 62, "RESULTS", 56, Weight::Heavy, Pal::INK);
    statRight(CANVAS_W - PADX, 52, "CREDITS EARNED", m.credits);

    const float tx = PADX, ty = 190, tw = CANVAS_W - PADX * 2;
    const float colX[5] = { tx + 20, tx + 120, tx + 560, tx + 760, tx + 1290 };
    const std::string cols[5] = { "RANK", "DRIVER", "BEST RUN",
                                  "JUDGE  A / L / S", "PTS" };
    int maxTotal = m.rows.empty() ? 1 : m.rows[0].total;

    Draw::rectLine(tx, ty, tw, 44 + 74 * m.rows.size(), 2, Pal::INK);
    Kit::tableHead(tx, ty, tw, cols, colX, 5);

    for (int i = 0; i < (int)m.rows.size(); ++i) {
        const ResultRow& r = m.rows[i];
        float ry = ty + 44 + i * 74;
        bool first = (i == 0);
        if (first) Draw::rect(tx + 2, ry, tw - 4, 74, Pal::ACID);
        if (i > 0) Draw::line(tx, ry, tx + tw, ry, 2, Pal::INK);

        float cy = ry + 37;
        Draw::text(colX[0], cy - 11, r.rank,    22, Weight::Heavy, Pal::INK);
        Draw::text(colX[1], cy - 9,  r.driver,  17, Weight::Heavy, Pal::INK);
        Draw::text(colX[2], cy - 8,  r.bestRun, 15, Weight::Bold,  Pal::INK);

        // Score bar — inverts to ink on the acid-filled leader row.
        float bw = 130, bh = 12;
        Draw::rect(colX[3], cy - bh * 0.5f, bw, bh, Pal::INK);
        Draw::rect(colX[3], cy - bh * 0.5f, bw * (r.total / (float)maxTotal), bh,
                   first ? Pal::INK : Pal::ACID);
        Draw::text(colX[3] + bw + 12, cy - 7, r.judge, 13, Weight::Heavy, Pal::INK);

        Draw::text(colX[4], cy - 9, r.pts, 18, Weight::Heavy, Pal::INK);
    }

    float by = ty + 44 + 74 * m.rows.size() + 34;
    Kit::button(PADX, by, 200, 50, "> NEXT RUN", Kit::BtnVariant::Primary);
    Kit::button(PADX + 230, by, 200, 50, "SAVE REPLAY", Kit::BtnVariant::Secondary);
    Kit::keycap(CANVAS_W - PADX - 230, by + 10, "ESC", "BACK TO LOBBY");
}

// ----------------------------------------------------------------------------
//  Wire into the shared frame()/Transition loop (drift_transition.h):
//     case 5: drawLobby(lobby);        break;
//     case 6: drawRoom(room);          break;
//     case 7: drawMatchmaking(match);  break;
//     case 8: drawResults(results);    break;
//
//  Deco::driftLoops — the tangled wobbly closed curves behind MATCHMAKING and
//  GARAGE are a set of three irregular bezier loops drawn at ~12 rotations with
//  slight per-copy scale, the whole group rotating about 3 deg/sec. Port them as
//  point arrays through Draw::polyline; do not substitute scaled ellipses, the
//  irregularity is the motif.
// ----------------------------------------------------------------------------
