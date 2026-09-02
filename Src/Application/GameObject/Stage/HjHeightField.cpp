#include "HjHeightField.h"

#include <fstream>

namespace TC = TerrainConst;

//----------------------------------------------------------
// 画像から読む
//----------------------------------------------------------
bool HjHeightField::LoadFromFile(const std::string& path)
{
	// 標高データを使わない。平らな地面にする。
	//
	// 実測の起伏は大きすぎて、道を引く邪魔になる。
	// まず平地で道の形を決めて、起伏は後から足す
	if (!TerrainConst::UseHeightData) { return false; }

	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) { return false; }

	int   nx = 0, nz = 0;
	float cell = 0.0f;

	ifs.read(reinterpret_cast<char*>(&nx),   sizeof(nx));
	ifs.read(reinterpret_cast<char*>(&nz),   sizeof(nz));
	ifs.read(reinterpret_cast<char*>(&cell), sizeof(cell));

	if (!ifs || nx < 2 || nz < 2 || cell <= 0.0f) { return false; }

	// 大きすぎるものを弾く。壊れたファイルで巨大な確保をしない
	const long long count = static_cast<long long>(nx) * nz;
	if (count > 64LL * 1024 * 1024) { return false; }

	std::vector<float> h(static_cast<size_t>(count));
	ifs.read(reinterpret_cast<char*>(h.data()),
	         static_cast<std::streamsize>(count * sizeof(float)));

	// 途中で切れていたら使わない。
	// 半分だけ読むと、地形の下半分が0になって崖ができる
	if (ifs.gcount() != static_cast<std::streamsize>(count * sizeof(float)))
	{
		return false;
	}

	// 使う所だけ切り出す。
	//
	// 実測の範囲そのままだと11km四方あり、格子もメッシュも
	// 大きくなりすぎる。峠として走るのは1〜2km
	if (TerrainConst::UseCrop)
	{
		const int want = std::max(4, static_cast<int>(TerrainConst::CropSize / cell));
		const int half = want / 2;

		// 中心をマス目で出す。ずれの指定は中央からの距離(m)
		const int cx = nx / 2 + static_cast<int>(TerrainConst::CropCenterX / cell);
		const int cz = nz / 2 + static_cast<int>(TerrainConst::CropCenterZ / cell);

		// 端からはみ出さない所へ寄せる
		const int x0 = std::clamp(cx - half, 0, std::max(0, nx - want));
		const int z0 = std::clamp(cz - half, 0, std::max(0, nz - want));
		const int w  = std::min(want, nx - x0);
		const int d  = std::min(want, nz - z0);

		if (w >= 2 && d >= 2 && (w < nx || d < nz))
		{
			std::vector<float> cut(static_cast<size_t>(w) * d);
			for (int z = 0; z < d; ++z)
			{
				for (int x = 0; x < w; ++x)
				{
					cut[static_cast<size_t>(z) * w + x] =
						h[static_cast<size_t>(z0 + z) * nx + (x0 + x)];
				}
			}
			m_sizeX    = w;
			m_sizeZ    = d;
			m_cellSize = cell;
			m_height   = std::move(cut);
			return true;
		}
	}

	m_sizeX    = nx;
	m_sizeZ    = nz;
	m_cellSize = cell;
	m_height   = std::move(h);
	return true;
}

//----------------------------------------------------------
// 仮の地形
//
// 実データが無いうちに「車が地面に乗るか」を確かめるためのもの。
// 山だけだと車を置ける平らな所が無いので、真ん中に谷を通す
//----------------------------------------------------------
void HjHeightField::BuildTestTerrain()
{
	// 平らな地面。
	//
	// 実測の起伏は大きすぎて、道を引く邪魔になる。
	// まず平地で道の形を決めて、起伏は後から足す。
	//
	// 山を作らないのは、そこに道を通すと結局
	// 切り通しと盛り土の調整に戻ってしまうから
	m_sizeX = TerrainConst::TestSize;
	m_sizeZ = TerrainConst::TestSize;
	m_cellSize = TerrainConst::CellSize;

	m_height.assign(static_cast<size_t>(m_sizeX) * m_sizeZ,
	                TerrainConst::FlatHeight);
}

//----------------------------------------------------------
float HjHeightField::HeightAtCell(int ix, int iz) const
{
	// 外を指されたら端の値を返す。
	// 0を返すと、地形の縁で崖ができる
	ix = std::clamp(ix, 0, m_sizeX - 1);
	iz = std::clamp(iz, 0, m_sizeZ - 1);
	return m_height[static_cast<size_t>(iz) * m_sizeX + ix];
}

void HjHeightField::SetHeightAtCell(int ix, int iz, float h)
{
	if (ix < 0 || iz < 0 || ix >= m_sizeX || iz >= m_sizeZ) { return; }
	m_height[static_cast<size_t>(iz) * m_sizeX + ix] = h;
}

//----------------------------------------------------------
// 位置 → マス目の座標
//
// 地形の中心をワールドの原点に置く。
// 端を原点にすると、地形を差し替えたときに中身がずれる
//----------------------------------------------------------
void HjHeightField::ToCell(float x, float z, float& outCx, float& outCz) const
{
	outCx = (x + GetWorldW() * 0.5f) / m_cellSize;
	outCz = (z + GetWorldD() * 0.5f) / m_cellSize;
}

bool HjHeightField::Contains(float x, float z) const
{
	if (!IsValid()) { return false; }

	float cx = 0.0f, cz = 0.0f;
	ToCell(x, z, cx, cz);

	return cx >= 0.0f && cz >= 0.0f
	    && cx <= static_cast<float>(m_sizeX - 1)
	    && cz <= static_cast<float>(m_sizeZ - 1);
}

//----------------------------------------------------------
// 高さ
//
// 探さない。位置からどのマス目かが直接分かるので、
// 4点読んで混ぜるだけで済む。
// メッシュへ光線を飛ばしていたときと比べ物にならない
//----------------------------------------------------------
float HjHeightField::HeightAt(float x, float z) const
{
	if (!IsValid()) { return TC::OutsideHeight; }
	if (!Contains(x, z)) { return TC::OutsideHeight; }

	float cx = 0.0f, cz = 0.0f;
	ToCell(x, z, cx, cz);

	const int ix = static_cast<int>(floorf(cx));
	const int iz = static_cast<int>(floorf(cz));

	// マス目の中のどこにいるか(0〜1)
	const float fx = cx - static_cast<float>(ix);
	const float fz = cz - static_cast<float>(iz);

	const float h00 = HeightAtCell(ix,     iz);
	const float h10 = HeightAtCell(ix + 1, iz);
	const float h01 = HeightAtCell(ix,     iz + 1);
	const float h11 = HeightAtCell(ix + 1, iz + 1);

	// 横に混ぜてから、縦に混ぜる
	const float a = h00 + (h10 - h00) * fx;
	const float b = h01 + (h11 - h01) * fx;
	return a + (b - a) * fz;
}

//----------------------------------------------------------
// 斜面の向き
//
// 隣の高さとの差から求める。
// サスペンションが押し返す向きに要るので、高さと対で用意する
//----------------------------------------------------------
Math::Vector3 HjHeightField::NormalAt(float x, float z) const
{
	if (!IsValid()) { return Math::Vector3::Up; }

	const float d = m_cellSize * TC::NormalStep;

	// 前後左右の高さの差から傾きを出す。
	// 片側だけ見ると、坂の途中で向きが半マスぶんずれる
	const float hl = HeightAt(x - d, z);
	const float hr = HeightAt(x + d, z);
	const float hb = HeightAt(x, z - d);
	const float hf = HeightAt(x, z + d);

	// 外を含んだら、その場は平らとして扱う。
	// 極端な値が混ざると、縁で法線が跳ねる
	if (hl <= TC::OutsideHeight || hr <= TC::OutsideHeight
	 || hb <= TC::OutsideHeight || hf <= TC::OutsideHeight)
	{
		return Math::Vector3::Up;
	}

	Math::Vector3 n(hl - hr, 2.0f * d, hb - hf);
	n.Normalize();
	return n;
}

//----------------------------------------------------------
void HjHeightField::SampleAt(float x, float z,
                             float& outHeight, Math::Vector3& outNormal) const
{
	outHeight = HeightAt(x, z);
	outNormal = NormalAt(x, z);
}
