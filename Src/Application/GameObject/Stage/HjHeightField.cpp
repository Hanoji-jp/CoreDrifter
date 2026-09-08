#include "HjHeightField.h"

#include <fstream>

namespace TC = TerrainConst;

//----------------------------------------------------------
// 画像から読む
//----------------------------------------------------------
//----------------------------------------------------------
// 手で彫った形を読む
//
// UseHeightData に関わらず読む。
// あの旗は「実測の標高を取り込むか」の話で、
// 自分で彫った形を読むかどうかとは別
//----------------------------------------------------------
bool HjHeightField::LoadEdited(const std::string& path)
{
	return ReadRaw(path);
}

//----------------------------------------------------------
bool HjHeightField::LoadFromFile(const std::string& path)
{
	// 標高データを使わない。平らな地面にする。
	//
	// 実測の起伏は大きすぎて、道を引く邪魔になる。
	// まず平地で道の形を決めて、起伏は後から足す
	if (!TerrainConst::UseHeightData) { return false; }

	return ReadRaw(path);
}

//----------------------------------------------------------
bool HjHeightField::ReadRaw(const std::string& path)
{
	KdAssetIStream ifs(path, std::ios::binary);
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
			RefineToCell(TerrainConst::CellSize);
			m_base     = m_height;
			return true;
		}
	}

	m_sizeX    = nx;
	m_sizeZ    = nz;
	m_cellSize = cell;
	m_height   = std::move(h);
	RefineToCell(TerrainConst::CellSize);
	m_base     = m_height;
	return true;
}

//----------------------------------------------------------
// 格子を細かくし直す
//
// ■ 何をするか
// 広さは変えずに、点の数だけ増やす。
// 5m 間隔で持っていた高さを、1.25m 間隔で持ち直す。
//
// ■ なぜ要るか
// 保存済みの高さマップは、マス目が粗かった頃のものが入っている。
// マス目の大きさはファイルの先頭に書いてあるので、定数を変えても
// 読んだ地形は粗いまま。ここで揃えないと、道の切り抜きが
// 相変わらず働かない。
//
// ■ 彫った形は消えない
// 双線形で間を埋めるだけなので、元の点は元の高さのまま通る。
// 次に保存すれば細かい方で書かれるので、この作り直しは1回きり
//----------------------------------------------------------
void HjHeightField::RefineToCell(float target)
{
	if (!TerrainConst::RefineOnLoad) { return; }
	if (target <= 0.0f) { return; }

	// もう細かい。粗くはしない。
	// 粗くすると彫った形が丸められて消える
	if (m_cellSize <= target * 1.001f) { return; }

	const float worldW = (m_sizeX - 1) * m_cellSize;
	const float worldD = (m_sizeZ - 1) * m_cellSize;

	const int nx = static_cast<int>(worldW / target + 0.5f) + 1;
	const int nz = static_cast<int>(worldD / target + 0.5f) + 1;

	if (nx < 2 || nz < 2) { return; }

	// 確保だけで固まらないよう上限を見る
	const long long count = static_cast<long long>(nx) * nz;
	if (count > TerrainConst::RefineMaxCells) { return; }

	std::vector<float> dst(static_cast<size_t>(count));

	// 新しい点の位置を、元の格子の座標へ直して4点から混ぜる
	const float sx = worldW / static_cast<float>(nx - 1) / m_cellSize;
	const float sz = worldD / static_cast<float>(nz - 1) / m_cellSize;

	for (int z = 0; z < nz; ++z)
	{
		const float fz = z * sz;
		const int   z0 = std::min(static_cast<int>(fz), m_sizeZ - 2);
		const float tz = fz - z0;

		for (int x = 0; x < nx; ++x)
		{
			const float fx = x * sx;
			const int   x0 = std::min(static_cast<int>(fx), m_sizeX - 2);
			const float tx = fx - x0;

			const float h00 = m_height[static_cast<size_t>(z0)     * m_sizeX + x0];
			const float h10 = m_height[static_cast<size_t>(z0)     * m_sizeX + x0 + 1];
			const float h01 = m_height[static_cast<size_t>(z0 + 1) * m_sizeX + x0];
			const float h11 = m_height[static_cast<size_t>(z0 + 1) * m_sizeX + x0 + 1];

			const float a = h00 + (h10 - h00) * tx;
			const float b = h01 + (h11 - h01) * tx;

			dst[static_cast<size_t>(z) * nx + x] = a + (b - a) * tz;
		}
	}

	m_sizeX    = nx;
	m_sizeZ    = nz;
	m_cellSize = target;
	m_height   = std::move(dst);
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
	m_base = m_height;
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

	// 描かれている面と同じ三角で引く。
	//
	// 双一次で混ぜると、実際に描かれる三角の面とは別の高さになる。
	// ずれは最大 |h00 + h11 - h10 - h01| / 4。
	// セルが5mあるので、彫った斜面では数十cmになる。
	//
	// 裾の外端をここに合わせているので、ずれるとそのまま
	// 地形へ埋まるか浮く。当たり判定も同じだけずれる。
	//
	// 地形メッシュは (x+1,z)-(x,z+1) の対角で2枚に割っている
	if (fx + fz <= 1.0f)
	{
		// 手前の三角。(0,0) (1,0) (0,1)
		return h00 + (h10 - h00) * fx + (h01 - h00) * fz;
	}

	// 奥の三角。(1,1) (1,0) (0,1)
	return h11 + (h01 - h11) * (1.0f - fx) + (h10 - h11) * (1.0f - fz);
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

//----------------------------------------------------------
// 素地の高さ
//
// 手で彫った形そのもの。道はここから削り直す
//----------------------------------------------------------
float HjHeightField::BaseAtCell(int ix, int iz) const
{
	if (m_base.empty()) { return HeightAtCell(ix, iz); }

	ix = std::clamp(ix, 0, m_sizeX - 1);
	iz = std::clamp(iz, 0, m_sizeZ - 1);
	return m_base[static_cast<size_t>(iz) * m_sizeX + ix];
}

void HjHeightField::SetBaseAtCell(int ix, int iz, float h)
{
	if (ix < 0 || iz < 0 || ix >= m_sizeX || iz >= m_sizeZ) { return; }
	if (m_base.size() != m_height.size()) { m_base = m_height; }

	m_base[static_cast<size_t>(iz) * m_sizeX + ix] = h;
}

//----------------------------------------------------------
// 仕上がりを素地へ戻す
//
// 道が削り直す前に呼ぶ。
// 削った跡の上へさらに削ると、掘り進んでいく
//----------------------------------------------------------
void HjHeightField::RestoreCell(int ix, int iz)
{
	if (ix < 0 || iz < 0 || ix >= m_sizeX || iz >= m_sizeZ) { return; }
	if (m_base.size() != m_height.size()) { return; }

	const size_t k = static_cast<size_t>(iz) * m_sizeX + ix;
	m_height[k] = m_base[k];
}

//----------------------------------------------------------
// 素地を書き出す
//
// 書き出すのは素地。仕上がりを書くと、道の削りが焼き込まれて
// 次に道を動かしたときに二重に削れる
//----------------------------------------------------------
bool HjHeightField::SaveToFile(const std::string& path) const
{
	if (!IsValid()) { return false; }

	std::ofstream f(path, std::ios::binary);
	if (!f) { return false; }

	const std::vector<float>& src = m_base.empty() ? m_height : m_base;

	f.write(reinterpret_cast<const char*>(&m_sizeX), sizeof(int));
	f.write(reinterpret_cast<const char*>(&m_sizeZ), sizeof(int));
	f.write(reinterpret_cast<const char*>(&m_cellSize), sizeof(float));
	f.write(reinterpret_cast<const char*>(src.data()),
	        static_cast<std::streamsize>(src.size() * sizeof(float)));

	return f.good();
}
