#pragma once

#include "../../Const/RetainWallConst.h"

class HjRoad;
class HjHeightField;

//==========================================================
// HjRetainWall
//   山側の擁壁(コンクリートの壁)。自作なので Hj 接頭辞。
//
//   ■ どこに立てるかは制御点が決める
//   ガードレールは地形の落ち方から自動で置いたが、擁壁は違う。
//   「ここは壁が欲しい、ここは岩肌のまま見せたい」は作る側の判断で、
//   地形の傾きからは出てこない。
//
//   裾や平場と同じく、制御点ごとに左右の高さ(m)を持つ。0 なら壁なし。
//   地形から一括で当てはめるのは、手で直す下敷きとして用意する。
//
//   ■ 形は道と同じ仕掛け
//   断面を刻みごとに並べて押し出す。道の刻みの決め方と
//   ミター接合をそのまま借りるので、ヘアピンでも破綻しない。
//
//   ■ 当たり判定は持たない
//   将来すべて剛体で処理する方針。ここで簡易な壁を入れると二重になる。
//==========================================================
class HjRetainWall : public KdGameObject
{
public:
	// 道から組む。道を作り直したら、こちらも呼ぶ
	void Build(const HjRoad& road);

	// 地形の起伏から高さを当てはめる。
	//
	// 手で置く前の下敷き。切り通しになっている所へ壁を入れて、
	// そこから要らない区間を 0 にしていく使い方を想定している
	static void AutoFill(HjRoad& road, const HjHeightField* field);

	void DrawLit() override;
	void GenerateDepthMapFromLight() override;

	// 道と同じで数百メートルにわたるので、球では収まらない
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

	void SetVisible(bool on) { m_visible = on; }
	bool IsVisible() const { return m_visible; }

	float GetLength() const { return m_length; }

private:
	// 断面の1点。道の縁からの張り出しと、路面からの高さ
	struct Node
	{
		float out;
		float y;
	};

	// この刻み・この側の断面を作る。戻り値=点の数(固定)
	static int BuildProfile(float h, Node* out);

	void BuildSide(const HjRoad& road, int side,
	               std::vector<KdMeshVertex>& verts,
	               std::vector<KdMeshFace>& faces);

	std::shared_ptr<KdMesh>  m_spMesh;
	std::vector<KdMaterial>  m_materials;

	bool  m_visible = true;
	float m_length  = 0.0f;
};
