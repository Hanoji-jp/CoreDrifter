#include "HjModLoader.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace
{
	// 拡張子を小文字で取る。大文字で保存されていることがある
	std::string LowerExt(const std::string& path)
	{
		std::string ext = fs::path(path).extension().string();
		for (char& c : ext)
		{
			if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c - 'A' + 'a'); }
		}
		return ext;
	}
}

//----------------------------------------------------------
// 読み込む前に見る
//----------------------------------------------------------
HjModLoader::Result HjModLoader::PreCheck(const std::string& path)
{
	std::error_code ec;

	if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec))
	{
		return Result::NotFound;
	}

	const std::string ext = LowerExt(path);
	if (ext != ModConst::ExtGltf && ext != ModConst::ExtGlb)
	{
		return Result::BadFormat;
	}

	const std::uintmax_t bytes = fs::file_size(path, ec);
	if (ec) { return Result::NotFound; }

	if (static_cast<int>(bytes / 1024) > ModConst::MaxFileSizeKb)
	{
		return Result::TooLarge;
	}

	return Result::Ok;
}

//----------------------------------------------------------
// 読み込んだ後に見る
//----------------------------------------------------------
bool HjModLoader::IsWithinBudget(const KdModelWork& work)
{
	const auto data = work.GetData();
	if (!data) { return false; }

	const auto& nodes = data->GetOriginalNodes();
	if (static_cast<int>(nodes.size()) > ModConst::MaxNodes) { return false; }

	// 部品ごとの点を数える。
	// 上限を超えた時点で打ち切る。全部数えてから判断すると、
	// 「重すぎるので弾きたい」データを最後まで舐めることになる
	int total = 0;
	for (const auto& n : nodes)
	{
		if (!n.m_spMesh) { continue; }

		total += static_cast<int>(n.m_spMesh->GetVertexPositions().size());
		if (total > ModConst::MaxVertices) { return false; }
	}

	return true;
}

//----------------------------------------------------------
// 読み込み
//----------------------------------------------------------
HjModLoader::Result HjModLoader::Load(KdModelWork& work, const std::string& path)
{
	// 標準のまま。差し替えないので何もしない
	if (path.empty() || path == ModConst::StockMark) { return Result::Ok; }

	const Result pre = PreCheck(path);
	if (pre != Result::Ok) { return pre; }

	// 失敗したときに戻せるよう、いまのものを控える
	const auto before = work.GetData();

	work.SetModelData(path);

	// 読み込みは成否を返さないので、中身が入ったかで見る
	if (!work.GetData())
	{
		work.SetModelData(before);
		return Result::LoadFailed;
	}

	if (!IsWithinBudget(work))
	{
		work.SetModelData(before);
		return Result::TooHeavy;
	}

	return Result::Ok;
}

//----------------------------------------------------------
// 理由の文
//----------------------------------------------------------
const char* HjModLoader::Message(Result r)
{
	switch (r)
	{
	case Result::Ok:         return "";
	case Result::NotFound:   return U8("ファイルが見つかりません");
	case Result::BadFormat:  return U8("gltf / glb ではありません");
	case Result::TooLarge:   return U8("ファイルが大きすぎます");
	case Result::LoadFailed: return U8("読み込めませんでした(データが壊れています)");
	case Result::TooHeavy:   return U8("モデルが重すぎます(頂点か部品が多すぎる)");
	}
	return "";
}
