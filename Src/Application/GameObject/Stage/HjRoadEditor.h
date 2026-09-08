#pragma once

#include "../../Const/RoadEditConst.h"

class HjRoad;
class HjHeightField;
class KdDebugWireFrame;

//==========================================================
// HjRoadEditor
//   道の制御点を編集する。自作なので Hj 接頭辞。
//
//   ■ 前の作品のギズモをそのまま持ってきた
//   軸を掴んで引く形。自分で書き直したときに、
//   視線を軸へ落とす式の符号を間違えて、
//   軸の反対側を掴むことになっていた。
//
//   実際に使えていた実装があるなら、それを写すのが速い。
//
//   ■ 一覧と数値も残す
//   遠くの点や、重なっている点は掴みにくい。
//   数値で直接動かせる道も要る。
//==========================================================
class HjRoadEditor
{
public:
	// 画面の位置から光線を作る。
	//
	// 地形の筆でも使う。同じ計算を2つ持つと、
	// 片方だけ直したときに狙いが食い違う
	static bool ScreenRay(float u, float v,
	                      Math::Vector3& outOrigin, Math::Vector3& outDir);

	// 掴む・動かす。毎フレーム
	void Update(HjRoad& road, const HjHeightField& field);

	// ImGui。一覧と、選んだ点の数値
	void DrawGui(HjRoad& road);

	// 制御点の印と軸を積む。
	//
	// ※自分では描かない。線は KdGameObject が持つ仕組みで
	//   描画の番に出されるので、そこへ積むだけにする
	void PushMarker(const HjRoad& road, KdDebugWireFrame& wire) const;

	int  GetSelected() const { return m_selected; }
	void ClearSelection() { m_selected = -1; }

private:
	// ギズモの操作軸
	enum class Axis { None, X, Y, Z };

	// ゲーム画像内の位置からワールドの視線を作る

	// 軸ハンドルのピック
	static Axis PickAxis(const Math::Vector3& ro, const Math::Vector3& rd,
	                     const Math::Vector3& selPos);

	// 視線を地形へ当てる。
	// 高さマップは面の集まりではないので、進めながら潜ったかを見る
	static bool RayToTerrain(const Math::Vector3& origin, const Math::Vector3& dir,
	                         const HjHeightField& field, Math::Vector3& outHit);

	int  m_selected = -1;
	bool m_lmbPrev  = false;

	//===== 軸ドラッグ =====
	Axis          m_dragAxis   = Axis::None;
	Math::Vector3 m_dragStart  = Math::Vector3::Zero;   // 掴んだ瞬間の位置
	float         m_dragStartS = 0.0f;                  // 掴んだ瞬間の軸上の位置

	//===== 目盛り =====
	bool  m_snapEnabled = false;
	float m_snapSize    = RoadEditConst::DefaultSnapSize;
};
