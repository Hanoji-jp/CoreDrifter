#include "HjModCatalog.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace
{
	// 拡張子を小文字にして返す。
	// 大文字で保存されていることがあるので、そのまま比べると取りこぼす
	std::string LowerExt(const fs::path& p)
	{
		std::string ext = p.extension().string();
		for (char& c : ext)
		{
			if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c - 'A' + 'a'); }
		}
		return ext;
	}

	// このローダで読める形か
	bool IsModelExt(const std::string& ext)
	{
		return ext == ModConst::ExtGltf || ext == ModConst::ExtGlb;
	}
}

//----------------------------------------------------------
// 一覧を作り直す
//----------------------------------------------------------
void HjModCatalog::Rescan()
{
	m_skipped = 0;
	m_body.clear();
	m_wheel.clear();

	ScanDir(ModConst::BodyDir,  m_body);
	ScanDir(ModConst::WheelDir, m_wheel);

	m_scanned = true;
}

//----------------------------------------------------------
// フォルダ1つ分を見る
//----------------------------------------------------------
void HjModCatalog::ScanDir(const char* dir, std::vector<Entry>& out)
{
	std::error_code ec;

	// フォルダが無いのは異常ではない。
	// MODを使わない人には作られないので、黙って空で返す
	if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) { return; }

	// 例外ではなくエラーコードで受ける版を使う。
	// 読めないフォルダがあっても、そこで止まらず次へ進みたい
	for (const auto& ent : fs::directory_iterator(dir, ec))
	{
		if (ec) { break; }
		if (static_cast<int>(out.size()) >= ModConst::MaxEntries) { break; }

		if (!ent.is_regular_file(ec)) { continue; }

		const fs::path& p = ent.path();

		// 形式が違うものは、そもそも読み込みに渡さない。
		// フレームワークのローダは失敗すると assert で止まるので、
		// 「読ませてみて駄目なら諦める」ができない
		if (!IsModelExt(LowerExt(p))) { ++m_skipped; continue; }

		const std::uintmax_t bytes = fs::file_size(p, ec);
		if (ec) { ++m_skipped; continue; }

		// 大きすぎるものを外す。
		// 読み込みの重さと、あとで配布するときの通信量を抑えるため
		const int kb = static_cast<int>(bytes / 1024);
		if (kb > ModConst::MaxFileSizeKb) { ++m_skipped; continue; }

		Entry e;
		e.name   = p.stem().string();
		// フレームワークのアセット管理はパスの文字列をそのまま鍵にするので、
		// 区切りを揃えておく。揃えないと同じファイルが二重に読み込まれる
		e.path   = p.generic_string();
		e.sizeKb = kb;
		out.push_back(std::move(e));
	}
}

//----------------------------------------------------------
// パスから何番目かを引く
//----------------------------------------------------------
int HjModCatalog::IndexOf(Kind kind, const std::string& path) const
{
	const std::vector<Entry>& list = List(kind);
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i].path == path) { return static_cast<int>(i); }
	}
	return -1;
}
