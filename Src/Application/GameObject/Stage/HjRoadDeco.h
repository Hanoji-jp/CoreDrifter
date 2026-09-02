#pragma once

#include "../../Const/RoadDecoConst.h"

class HjRoad;

//==========================================================
// HjRoadDeco
//   道に付ける物。ガードレールと路面標示。自作なので Hj 接頭辞。
//
//   ■ なぜ道と分けるか
//   道の形が決まってから作るもので、寿命が違う。
//   道を動かすたびに作り直すが、道そのものの計算とは関係ない。
//
//   混ぜると、路面の断面を直したいだけなのに
//   ガードレールの計算まで読むことになる。
//
//   ■ スプラインに沿って置く
//   中心線から横へずらして並べるだけ。
//   曲がりにも勾配にも自動で付いてくるので、
//   道を動かせばガードレールも一緒に曲がる。
//==========================================================
class HjRoadDeco : public KdGameObject
{
public:
	// 道から形を読んで作る。道を動かしたら呼び直す
	void Build(const HjRoad& road);

	void DrawLit() override;
	void GenerateDepthMapFromLight() override;

	// 場面側のオブジェクト単位のカリングから外す。
	// 道は数百メートルにわたるので、原点中心の球では収まらない
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

private:
	// ガードレール。支柱と板をまとめて1つのメッシュにする。
	// 支柱を1本ずつ描くと、数百回の描画命令になる
	void BuildRail(const HjRoad& road);

	// 路面標示。中央線と外側線
	void BuildMarks(const HjRoad& road);

	std::shared_ptr<KdMesh> m_spRail;
	std::shared_ptr<KdMesh> m_spMark;

	std::vector<KdMaterial> m_railMaterials;
	std::vector<KdMaterial> m_markMaterials;
};
