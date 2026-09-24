#pragma once

#include "../../Const/GuardRailConst.h"

class HjRoad;
class HjHeightField;

//==========================================================
// HjGuardRail
//   道に沿うガードレール。自作なので Hj 接頭辞。
//
//   ■ どこに置くかが本体
//   実際の峠は、どこにでもレールがあるわけではない。
//   外側で、かつ谷になっている所だけ。
//
//   高さマップがあるので自動で出せる。
//   「道の縁から少し外の地形が、これだけ落ちていたら置く」
//
//   ■ 形は道と同じ仕掛け
//   断面を刻みごとに並べて押し出す。道で作った
//   刻みの決め方(曲がりに合わせる)と断面の向き(ミター)を
//   そのまま借りるので、ヘアピンでも破綻しない。
//
//   ■ 当たり判定は持たない
//   将来すべて剛体で処理する方針。
//   ここで簡易な壁を入れると、剛体を入れるときに二重になる。
//==========================================================
class HjGuardRail : public KdGameObject
{
public:
	// 道と地形から組む。道を作り直したら、こちらも呼ぶ
	void Build(const HjRoad& road, const HjHeightField* field);

	// 地形の落ち方から当てはめる。
	//
	// 手で置く前の下敷き。落差のある所へ柵を入れて、
	// そこから要らない区間を外していく使い方を想定している
	static void AutoFill(HjRoad& road, const HjHeightField* field);

	void DrawLit() override;
	void GenerateDepthMapFromLight() override;

	// 場面側のオブジェクト単位のカリングから外す。
	// 道と同じで、数百メートルにわたるので球では収まらない
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

	// 出す・出さない
	void SetVisible(bool on) { m_visible = on; }
	bool IsVisible() const { return m_visible; }

	// 何メートルぶん立ったか。調整の目安になる
	float GetLength() const { return m_length; }
	int   GetPostCount() const { return m_posts; }

private:
	// この地点のこちら側に柵が要るか。
	//
	// 道の縁から少し外の地形が、路面よりどれだけ落ちているかで決める
	bool NeedRail(const HjRoad& road, const HjHeightField* field,
	              int step, float side) const;

	// 短すぎる区間を消し、短い切れ目を繋ぐ。
	//
	// 地形のわずかな起伏で柵が飛び飛びになるのを防ぐ。
	// 数メートルだけの柵は現実にも無い
	void TidyRuns(const HjRoad& road, std::vector<bool>& on) const;

	// 片側ぶんを積む
	void BuildSide(const HjRoad& road, float side,
	               std::vector<KdMeshVertex>& verts,
	               std::vector<KdMeshFace>& faces);

	// この側で柵が要る所。片側ずつ組むので使い回す
	std::vector<bool> m_needCache;

	std::shared_ptr<KdMesh> m_spMesh;
	std::vector<KdMaterial> m_materials;

	bool  m_visible = true;
	float m_length  = 0.0f;
	int   m_posts   = 0;
};
