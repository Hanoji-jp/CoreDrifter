#pragma once

#include "../../Const/TerrainConst.h"

//==========================================================
// HjHeightField
//   標高の格子。自作なので Hj 接頭辞。
//
//   ■ 何を持つか
//   マス目ごとの高さ、それだけ。描画もしないし、当たり判定も持たない。
//
//   ■ なぜ分けるか
//   ここが「地面の高さとは何か」を答える唯一の場所になる。
//
//   見えている地面と当たる地面がずれるのは、
//   描画と当たり判定が別々に高さを求めているときに起きる。
//   両方がここへ聞きに来る形にすれば、ずれようがない。
//
//   ■ 剛体化しても変わらない
//   いまは地形に沿わせるだけだが、いずれサスペンションを入れる。
//   そのときも要るのは「この位置の高さと斜面の向き」で、同じ。
//   上に乗る処理は入れ替わるが、この口は残る。
//==========================================================
class HjHeightField
{
public:
	// 標高データを読む。実数をそのまま並べた形。
	//
	// 画像にしないのは、8bitでは標高400mを256段に潰すことになり、
	// 1.5m刻みになるため。車が乗る地面としては粗すぎる。
	//
	// 読めなければ false(呼び側が仮の地形へ倒せるように)
	bool LoadFromFile(const std::string& path);

	// 仮の地形を作る。実データが無いうちに、
	// 車が地面に乗るかを確かめるためのもの
	void BuildTestTerrain();

	//===== 問い合わせ =====
	// この位置の高さ。格子の外なら TerrainConst::OutsideHeight
	float HeightAt(float x, float z) const;

	// 斜面の向き。サスペンションが押し返す向きに要る。
	// 高さと一緒に求まるので、別に計算し直す必要はない
	Math::Vector3 NormalAt(float x, float z) const;

	// 高さと向きをまとめて。
	// 別々に呼ぶと同じ場所を2回引くことになる
	void SampleAt(float x, float z, float& outHeight, Math::Vector3& outNormal) const;

	// 格子の中か
	bool Contains(float x, float z) const;

	//===== 形 =====
	int   GetSizeX() const { return m_sizeX; }
	int   GetSizeZ() const { return m_sizeZ; }
	float GetCellSize() const { return m_cellSize; }

	// 端から端までの長さ(m)
	float GetWorldW() const { return (m_sizeX - 1) * m_cellSize; }
	float GetWorldD() const { return (m_sizeZ - 1) * m_cellSize; }

	bool IsValid() const { return m_sizeX > 1 && m_sizeZ > 1; }

	// マス目の高さを直に読む。
	// メッシュを組むときに、補間なしでそのまま欲しい
	float HeightAtCell(int ix, int iz) const;

	// マス目の高さを書き換える。
	// 道を通すために、周りの地形を寄せるときに使う
	void SetHeightAtCell(int ix, int iz, float h);

	// 位置からマス目の番号へ。地形を書き換える側が範囲を求めるのに要る
	void WorldToCell(float x, float z, float& outCx, float& outCz) const
	{
		ToCell(x, z, outCx, outCz);
	}

private:
	// 位置をマス目の座標へ。
	// 地形の左手前を原点(0,0)として、そこからの距離をマス目で数える
	void ToCell(float x, float z, float& outCx, float& outCz) const;

	std::vector<float> m_height;   // 高さ(m)。[iz * m_sizeX + ix]
	int   m_sizeX = 0;
	int   m_sizeZ = 0;
	float m_cellSize = TerrainConst::CellSize;
};
