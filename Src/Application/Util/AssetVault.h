#pragma once
#include <cstdint>
#include <string>
#include <vector>

//==========================================================
// AssetVault
//   exe に埋め込んだ（難読化された）assets.pak を、ディスクに
//   展開せずメモリ上に保持し、パス指定でバイト列を返す簡易VFS。
//   各ローダー（テクスチャ／モデル／音声）はまずここを引き、
//   見つかればメモリから読む＝ディスクに平文アセットを出さない。
//
//   Debug/Release（開発ビルド）では何も保持しない＝常に false を
//   返すので、ローダーは従来どおりディスクから読む。
//==========================================================
namespace AssetVault
{
    // 埋め込み pak をメモリへ読み込み索引を作る。起動時に一度呼ぶ。
    void Init();

    // 指定パスのアセットが pak 内にあるか（パスは "Asset/..." 形式、大小無視）
    bool Exists(const std::string& path);

    // 指定パスのアセットをバイト列で取得（成功で true）
    bool Read(const std::string& path, std::vector<uint8_t>& out);

    // 指定パスのアセットのサイズ（バイト数）を取得（成功で true）
    bool Size(const std::string& path, size_t& out);
}
