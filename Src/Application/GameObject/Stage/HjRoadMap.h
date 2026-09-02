#pragma once

#include "../../Const/RoadMapConst.h"

class HjRoad;
class HjHeightField;

//==========================================================
// HjRoadMap
//   道を上から見て引く。自作なので Hj 接頭辞。
//
//   ■ なぜ地図で引くか
//   3Dで点を掴む形は、狙いが合わずに使い物にならなかった。
//   視線を軸へ落とす計算、画面の中でのマウス位置、
//   カメラからの距離——どれか1つずれると掴めない。
//
//   上から見た2次元の絵の上でなら、マウスの位置がそのまま
//   地図の座標になる。ずれようがない。
//
//   道は「地図の上で線を引く」ものなので、操作としても素直。
//
//   ■ 高さは触らない
//   地図には高さが無い。道の高さは地形から拾って
//   勾配の上限に収めるので、平面の位置だけ決めれば足りる。
//==========================================================
class HjRoadMap
{
public:
	// 地形から陰影の絵を作る。地形が決まったときに1回
	void Build(const HjHeightField& field);

	// 地図を出して、線を引く。ImGui の中で呼ぶ
	void DrawGui(HjRoad& road, const HjHeightField& field);

	// 車の位置を地図へ出す。どこを走っているか分かる
	void SetCarPos(const Math::Vector3& pos) { m_carPos = pos; }

	int  GetSelected() const { return m_selected; }

private:
	// 地図の中の位置(0〜1) ↔ ワールド
	Math::Vector3 MapToWorld(float u, float v, const HjHeightField& field) const;
	void WorldToMap(const Math::Vector3& w, const HjHeightField& field,
	                float& outU, float& outV) const;

	// 陰影の絵。地形の格子を間引いて作る
	std::shared_ptr<KdTexture> m_spTex;

	int m_selected = -1;

	// 掴んでいる点。地図の上で引きずる
	int m_dragging = -1;

	// 線を継ぎ足す途中か。
	// 押すたびに末尾へ足していくと、道を一気に引ける
	bool m_appending = false;

	Math::Vector3 m_carPos = Math::Vector3::Zero;
};
