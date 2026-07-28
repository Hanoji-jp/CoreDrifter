#pragma once

#include "../../Const/SkidMarkConst.h"
#include "HjCullView.h"   // 画面に映らない区間を頂点に積む前に捨てる

//==========================================================
// SkidMark
//   路面に残るタイヤ痕(スキッドマーク)。
//   後輪の接地点を追いかけて一定距離ごとに点を打ち、
//   隣り合う点どうしを帯(四角形2枚)で繋いで1本の連続した痕にする。
//   点は寿命で薄れて消え、古いものから捨てられる。
//
//   使い方(所有者=CarBase):
//     Init()                              … 起動時に1回
//     Emit(trail, pos, right, strength)   … 毎フレーム、後輪ごとに接地点を渡す
//     Cut(trail)                          … スリップが止まった/離陸した等で痕を切る
//     Update(dt)                          … 毎フレーム
//     DrawEffect()                        … Litパス内で描画
//==========================================================
class SkidMark
{
public:
	void Init();

	// trail=痕の番号(0..TrailCount-1) / pos=接地点 / right=タイヤの右方向(帯の幅方向)
	// strength=スリップの強さ(0〜1)。SlipMinFor未満なら痕を残さず自動で切る
	void Emit(int trail, const Math::Vector3& pos, const Math::Vector3& right, float strength);
	// 痕を途切れさせる(次の点と線で繋がないようにする)
	void Cut(int trail);

	void Update(float dt);
	void DrawEffect();

	void SetColor(const Math::Vector3& c) { m_color = c; }

private:
	struct Point
	{
		Math::Vector3 pos      = Math::Vector3::Zero;
		Math::Vector3 right    = Math::Vector3::Right;  // 帯の幅方向
		float         age      = 0.0f;
		float         strength = 1.0f;   // 打った時のスリップの強さ＝濃さ
		bool          joined   = false;  // 直前の点と繋ぐか(falseで痕の始まり)
	};

	struct Trail
	{
		std::vector<Point> pts;   // 先頭が最も古い
		bool               cut = true;   // 次に打つ点を「始まり」にするか
		float              widthMul = 1.0f;  // 痕の幅の倍率(前輪は少し細い)
	};

	std::vector<Trail>             m_trails;
	std::vector<KdPolygon::Vertex> m_verts;   // 毎フレーム組み立てる帯の頂点
	HjCullView m_cull;   // 画面に映る区間だけを積むための判定器
	Math::Vector3 m_color = { SkidMarkConst::ColorR, SkidMarkConst::ColorG, SkidMarkConst::ColorB };
};
