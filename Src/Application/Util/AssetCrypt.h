#pragma once
#include <cstdint>
#include <cstddef>

// ==========================================================
// AssetCrypt
//   Lightweight XOR obfuscation for assets.pak.
//   Symmetric: Apply() both encrypts and decrypts.
//   MUST stay in sync with tools/pack_assets.ps1 (same key/formula).
//   Goal: stop casual ripping (not unbreakable encryption).
// ==========================================================
namespace AssetCrypt
{
    inline void Apply(uint8_t* data, size_t len)
    {
        static const uint8_t K[8] = { 0x5A, 0xC3, 0x91, 0x2E, 0x7F, 0xB4, 0x68, 0xD1 };
        for (size_t i = 0; i < len; ++i)
        {
            data[i] ^= static_cast<uint8_t>(K[i & 7] ^ static_cast<uint8_t>(i * 31u + 7u));
        }
    }
}
