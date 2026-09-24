// ============================================================================
//  drift_garage_scene.h — B1 "NIGHT PIT" garage room, 3D scene constants.
//
//  Units: meters. Origin: turntable center at floor level.
//  Axes:  +X right, +Y up, +Z toward the camera.
//  Same numbers as "Garage B1 Handoff.dc.html" — keep the two in step.
// ============================================================================
#pragma once

namespace GarageScene {

struct Vec3  { float x, y, z; };
struct RGB   { unsigned char r, g, b; };

// ---- palette ---------------------------------------------------------------
constexpr RGB FLOOR     {0x33,0x32,0x2F};   // concrete
constexpr RGB WALL      {0x26,0x26,0x24};   // painted concrete
constexpr RGB SHUTTER_A {0x3B,0x3A,0x37};   // slat face
constexpr RGB SHUTTER_B {0x2C,0x2B,0x28};   // slat gap
constexpr RGB FRAME     {0x4A,0x49,0x45};   // door / shutter frames
constexpr RGB PAPER     {0xF1,0xF0,0xEC};   // tubes, floor lines, wall text
constexpr RGB ACID      {0xCF,0xE0,0x21};   // turntable ring, wheel stop
constexpr RGB AMBIENT   {0x1C,0x1C,0x1A};

// ---- room ------------------------------------------------------------------
constexpr float ROOM_W = 14.0f;             // X -7 .. +7
constexpr float ROOM_D = 10.0f;             // Z -5 .. +5
constexpr float ROOM_H = 5.0f;
constexpr float BACK_WALL_Z = -5.0f;
constexpr float FLOOR_ROUGHNESS = 0.85f;
constexpr float WALL_ROUGHNESS  = 0.90f;

// ---- back wall fixtures ----------------------------------------------------
constexpr Vec3  SHUTTER_CENTER {0.0f, 1.7f, -4.95f};
constexpr float SHUTTER_W = 6.0f, SHUTTER_H = 3.4f;
constexpr float SHUTTER_SLAT_PITCH = 0.12f;

constexpr Vec3  SIDE_DOOR_CENTER {-5.8f, 1.05f, -4.95f};
constexpr float SIDE_DOOR_W = 1.2f, SIDE_DOOR_H = 2.1f, SIDE_DOOR_RECESS = 0.1f;

// Five wall-mounted fluorescent tubes (emissive only, cast no shadows).
constexpr int   TUBE_COUNT = 5;
constexpr float TUBE_X[TUBE_COUNT] = { -4.8f, -2.4f, 0.0f, 2.4f, 4.8f };
constexpr float TUBE_Y = 4.6f, TUBE_Z = -4.9f;
constexpr float TUBE_LEN = 1.8f, TUBE_THICK = 0.06f;

// Painted wall text "PIT 02", 18% opacity.
constexpr Vec3  WALL_TEXT_POS {4.6f, 3.1f, -4.98f};
constexpr float WALL_TEXT_H = 0.6f, WALL_TEXT_ALPHA = 0.18f;

// ---- floor markings (paint, 55% opacity) -----------------------------------
constexpr float LINE_W = 0.12f, LINE_ALPHA = 0.55f;
constexpr float BAY_LINE_X = 5.5f;          // two lines at X = ±5.5, full depth
constexpr float CROSS_LINE_Z = -3.2f;       // one line across, X -5.5 .. +5.5

// Acid wheel stop behind the turntable.
constexpr Vec3  WHEEL_STOP_CENTER {0.0f, 0.06f, -3.4f};
constexpr Vec3  WHEEL_STOP_SIZE   {3.6f, 0.12f, 0.25f};

// ---- turntable -------------------------------------------------------------
constexpr Vec3  TURNTABLE_CENTER {0.0f, 0.01f, 0.0f};
constexpr float TURNTABLE_OUTER_D = 5.6f;
constexpr float RING_BAND_W = 0.14f;        // emissive acid band at the rim
constexpr float SPIN_DEG_PER_SEC = 15.0f;   // one turn every 24 s
constexpr float MANUAL_SPIN_DECEL_SEC = 0.4f;   // Q / E release ease-out
constexpr float CAR_START_YAW_DEG = -35.0f;

// ---- camera (fixed, never shakes) ------------------------------------------
constexpr Vec3  CAMERA_POS    {0.0f, 1.6f, 7.5f};
constexpr Vec3  CAMERA_TARGET {0.0f, 0.7f, 0.0f};
constexpr float CAMERA_VFOV_DEG = 38.0f;    // 16:9

// ---- lighting --------------------------------------------------------------
// Key: one rectangular area light straight above the turntable.
constexpr Vec3  KEY_LIGHT_POS {0.0f, 4.8f, 0.0f};
// Bloom: keep the threshold high so the ring never clips past ACID.
constexpr float BLOOM_THRESHOLD = 0.9f;

} // namespace GarageScene
