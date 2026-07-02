#include "AssetVault.h"

#ifdef DISTRIBUTE_BUILD

#include <Windows.h>
#include "../../resource.h"
#include "AssetCrypt.h"
#include <unordered_map>
#include <cstring>
#include <cctype>

namespace
{
    // pak 全体のバイト列（メモリ常駐）。索引はここへのオフセットを指す。
    std::vector<uint8_t> g_buf;
    // 正規化パス -> (offset, size)
    std::unordered_map<std::string, std::pair<size_t, uint32_t>> g_index;

    // パス正規化：小文字化＋バックスラッシュ→スラッシュ＋先頭"./"除去
    std::string Normalize(const std::string& in)
    {
        std::string s = in;
        for (char& c : s)
        {
            if (c == '\\') { c = '/'; }
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (s.size() >= 2 && s[0] == '.' && s[1] == '/') { s.erase(0, 2); }
        return s;
    }

    bool ReadU32(const uint8_t*& p, const uint8_t* end, uint32_t& out)
    {
        if (p + 4 > end) { return false; }
        std::memcpy(&out, p, 4);
        p += 4;
        return true;
    }
}

namespace AssetVault
{
    void Init()
    {
        HMODULE hMod = GetModuleHandle(nullptr);
        HRSRC   hRes = FindResource(hMod, MAKEINTRESOURCE(IDR_ASSETS_PAK), RT_RCDATA);
        if (!hRes) { return; }
        HGLOBAL hData = LoadResource(hMod, hRes);
        if (!hData) { return; }
        const uint32_t size = static_cast<uint32_t>(SizeofResource(hMod, hRes));
        const uint8_t* src  = static_cast<const uint8_t*>(LockResource(hData));
        if (!src || size < 12) { return; }

        // メモリへコピーして常駐し、XOR難読化を解除（ディスクには出さない）
        g_buf.assign(src, src + size);
        AssetCrypt::Apply(g_buf.data(), g_buf.size());

        // 索引を作成
        const uint8_t* p   = g_buf.data();
        const uint8_t* end = p + g_buf.size();
        if (std::memcmp(p, "KPAK", 4) != 0) { g_buf.clear(); return; }
        p += 4;

        uint32_t version = 0, count = 0;
        if (!ReadU32(p, end, version)) { return; }
        if (!ReadU32(p, end, count))   { return; }

        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t pathLen = 0;
            if (!ReadU32(p, end, pathLen)) { return; }
            if (p + pathLen > end)         { return; }
            std::string rel(reinterpret_cast<const char*>(p), pathLen);
            p += pathLen;

            uint32_t dataLen = 0;
            if (!ReadU32(p, end, dataLen)) { return; }
            if (p + dataLen > end)         { return; }

            const size_t offset = static_cast<size_t>(p - g_buf.data());
            g_index[Normalize(rel)] = { offset, dataLen };
            p += dataLen;
        }
    }

    bool Exists(const std::string& path)
    {
        return g_index.find(Normalize(path)) != g_index.end();
    }

    bool Read(const std::string& path, std::vector<uint8_t>& out)
    {
        auto it = g_index.find(Normalize(path));
        if (it == g_index.end()) { return false; }
        const size_t   off = it->second.first;
        const uint32_t len = it->second.second;
        if (off + len > g_buf.size()) { return false; }
        out.assign(g_buf.begin() + off, g_buf.begin() + off + len);
        return true;
    }

    bool Size(const std::string& path, size_t& out)
    {
        auto it = g_index.find(Normalize(path));
        if (it == g_index.end()) { return false; }
        out = it->second.second;
        return true;
    }
}

#else // !DISTRIBUTE_BUILD

namespace AssetVault
{
    void Init() {}
    bool Exists(const std::string&) { return false; }
    bool Read(const std::string&, std::vector<uint8_t>&) { return false; }
    bool Size(const std::string&, size_t&) { return false; }
}

#endif
