#include "HjSaveFile.h"

#include "../../Framework/Utility/KdUtility.h"   // KdAssetIStream(配ってある既定を読む)

#include <fstream>
#include <map>
#include <cstdint>

namespace
{
	// exe と同じ場所へ置く。Asset/ の下には作らない。
	//
	// 配布ビルドは Asset/ を exe へ埋め込むので、そこへ書くと
	// exe の隣に Asset/Data/ が生える。アセットが素で置いてあるように
	// 見えるし、消していいのかも分からない
	const char* kPath = "save.dat";

	// 名前で引ける形で持つ。
	// 並び順に意味を持たせると、項目を足したときに全部ずれる
	std::map<std::string, std::string> g_items;

	bool g_loaded = false;

	template<class T> void WriteBin(std::ostream& o, const T& v)
	{
		o.write(reinterpret_cast<const char*>(&v), sizeof(T));
	}

	template<class T> bool ReadBin(std::istream& i, T& v)
	{
		return static_cast<bool>(i.read(reinterpret_cast<char*>(&v), sizeof(T)));
	}
}

namespace HjSaveFile
{
	void Load()
	{
		g_items.clear();
		g_loaded = true;

		std::ifstream f(kPath, std::ios::binary);
		if (!f) { return; }   // 初回起動。異常ではない

		char magic[4] = {};
		f.read(magic, 4);
		if (!f || std::string(magic, 4) != "HSAV") { return; }

		uint32_t version = 0, count = 0;
		if (!ReadBin(f, version)) { return; }
		if (!ReadBin(f, count))   { return; }

		// 壊れたファイルで延々と読まない
		if (count > 4096) { return; }

		for (uint32_t i = 0; i < count; ++i)
		{
			uint32_t keyLen = 0;
			if (!ReadBin(f, keyLen) || keyLen > 1024) { return; }

			std::string key(keyLen, '\0');
			if (!f.read(key.data(), keyLen)) { return; }

			uint32_t dataLen = 0;
			if (!ReadBin(f, dataLen) || dataLen > 64u * 1024u * 1024u) { return; }

			std::string data(dataLen, '\0');
			if (dataLen > 0 && !f.read(data.data(), dataLen)) { return; }

			g_items[key] = std::move(data);
		}
	}

	void Save()
	{
		std::ofstream f(kPath, std::ios::binary | std::ios::trunc);
		if (!f) { return; }

		f.write("HSAV", 4);
		WriteBin<uint32_t>(f, 1);
		WriteBin<uint32_t>(f, static_cast<uint32_t>(g_items.size()));

		for (const auto& kv : g_items)
		{
			WriteBin<uint32_t>(f, static_cast<uint32_t>(kv.first.size()));
			f.write(kv.first.data(), static_cast<std::streamsize>(kv.first.size()));

			WriteBin<uint32_t>(f, static_cast<uint32_t>(kv.second.size()));
			f.write(kv.second.data(), static_cast<std::streamsize>(kv.second.size()));
		}
	}

	bool Has(const std::string& key)
	{
		if (!g_loaded) { Load(); }
		return g_items.find(key) != g_items.end();
	}

	std::string Get(const std::string& key)
	{
		if (!g_loaded) { Load(); }

		const auto it = g_items.find(key);
		return (it == g_items.end()) ? std::string() : it->second;
	}

	void Set(const std::string& key, const std::string& value)
	{
		if (!g_loaded) { Load(); }
		g_items[key] = value;
	}
}

//----------------------------------------------------------
// save.dat の1項目を読む
//
// 無ければ、配ってある既定を読む。
// 既定を配りたいもの(車のセッティング、後処理の設定)は、
// 遊ぶ側が触るまで pak の値で動いてほしい
//----------------------------------------------------------
HjSaveIStream::HjSaveIStream(const std::string& key, const char* assetPath)
{
	if (HjSaveFile::Has(key))
	{
		str(HjSaveFile::Get(key));
		return;
	}

	if (assetPath)
	{
		KdAssetIStream a(assetPath);
		if (a)
		{
			std::string content(
				(std::istreambuf_iterator<char>(a)),
				std::istreambuf_iterator<char>());

			str(content);
			return;
		}
	}

	// どちらも無い。if(!ifs) が真になるようにする
	setstate(std::ios::failbit);
}

//----------------------------------------------------------
// 閉じた時点で書き出す
//
// 溜めておくと、途中で落ちたときに設定が丸ごと消える
//----------------------------------------------------------
HjSaveOStream::~HjSaveOStream()
{
	HjSaveFile::Set(m_key, str());
	HjSaveFile::Save();
}
