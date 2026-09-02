#pragma once

#include "HjRoadSpline.h"

#include <unordered_map>

class HjHeightField;

//==========================================================
// HjRoad
//   スプラインから作る道。自作なので Hj 接頭辞。
//
//   ■ 当たり判定はメッシュを見ない
//   位置をスプラインへ投影すれば、道のどこを走っているかが出る。
//   そこから断面を評価すれば、高さと法線が厳密に求まる。
//
//   面を1枚ずつ調べるより速く、しかも正確。
//   高さマップに焼くとカント(傾き)が潰れるが、こちらは残る。
//   ドリフトではカントが効くので、そこは落とせない。
//
//   ■ 高さは地形から借りる
//   スプラインの制御点は真上から見た線として置く。
//   高さは地形に沿わせて決めるので、地形を渡す。
//   (工程4で「地形を道に沿わせる」ときは逆向きになるが、
//    まずは道を地形へ乗せるほうが先に形が見える)
//==========================================================
class HjRoad : public KdGameObject
{
public:
	// 地形に沿わせて作る。地形が無ければ高さ0の平面に敷く
	// 地形に沿わせて作る。地形が無ければ高さ0の平面に敷く。
	//
	// field を書き換える。中心線は地形の凹凸を均してあるので、
	// そのままだと下げた区間で道が地形へ潜る。
	// 道の周りの地形を、道の高さへ寄せる必要がある
	void Init(HjHeightField* field);

	void DrawLit() override;
	void GenerateDepthMapFromLight() override;

	// 場面側のオブジェクト単位のカリングから外す。
	// 道は数百メートルにわたるので、原点中心の球では収まらない
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

	//===== 当たり判定 =====
	// この位置が道の上か。上なら高さと法線を返す。
	//
	// 地形より先にここを聞く。道の上なら道の面が正しく、
	// 地形の高さを使うと路肩や盛り上がりが消える
	bool SampleAt(float x, float z, float& outHeight, Math::Vector3& outNormal) const;

	const HjRoadSpline& Spline() const { return m_spline; }

	//===== 編集 =====
	// 点を動かしたら、道と地形を作り直す。
	//
	// 地形は道に合わせて削るので、元の形を控えておかないと
	// 削った跡の上へさらに削ることになり、掘り進んでいく
	void Rebuild();

	int  PointCount() const { return m_spline.PointCount(); }
	Math::Vector3 GetPoint(int i) const;

	// 制御点を画面に出す位置。
	//
	// 制御点の高さは「持ち上げ」なので、そのまま置くと
	// 道から離れた所に印が出る。道の実際の高さへ乗せる
	Math::Vector3 GetPointDisplayPos(int i) const;
	void MovePoint(int i, const Math::Vector3& pos);
	void InsertAfter(int i);
	void AppendPoint(const Math::Vector3& pos);
	void ErasePoint(int i);

	bool SavePath() const;

	// 触りながら決める値。ビルドし直さずに変えられないと道具にならない
	void DrawEditImGui();

	// 車を置く場所。道の始点
	Math::Vector3 GetStartPos() const;
	float         GetStartYaw() const;

private:
	// 断面の形。中心からの横のずれに対する、高さの差
	static float CrossHeight(float offset);

	// スプラインからメッシュを組む
	void BuildMesh();

	// 地形へ溶ける裾。
	//
	// 地形の格子は道の縁に沿えないので、地形だけで境目を作ると
	// 階段状になる。道から生やせば、境目がスプラインの縁になる
	void BuildApron(const HjHeightField* field);

	// 中心線の高さを、地形へ沿わせて決める
	// 道の高さを決める。
	//
	// 地形からは取らない。地形をなぞると、道が地面の起伏を
	// そのまま拾って上下し、道に見えなくなる。
	//
	// 制御点に高さが書いてあればそれを使う。
	// 無ければ地形から下敷きを作り、均して勾配の上限に収める
	void ResolveHeights(const HjHeightField* field);

	// 勾配を上限に収める。
	// 隣り合う点の高さの差を、少しずつ寄せて減らす
	void LimitGrade();

	// 左右の傾きを決める。
	// 地形の傾きを拾い、均して、上限に収める
	void ResolveBank(const HjHeightField* field);

	// 路面の高さ。中心線からの横のずれ(右が正)を渡す。
	//
	// メッシュも当たり判定も地形の削りもエプロンも、全部ここを通す。
	// 別々に計算すると、傾きを足したときに片方だけ直すことになる
	float SurfaceY(int step, float offset) const;

	// この地点で内側へ使ってよい幅(m)。
	//
	// ヘアピンでは、曲がりの半径が道幅より小さくなる。
	// そのまま断面を並べると、内側が前後で追い越して面が折り返す。
	// 半径より内へ収めれば折り返さない
	float InnerLimit(int step) const;

	// 道の周りの地形を、道の高さへ寄せる。
	// これをやらないと、道が地形へ埋まったり宙に浮いたりする
	void DeformTerrain(HjHeightField* field);

	// 削ったあとの地形を、触った所だけ均す。
	//
	// 寄せる強さが刻みの周期で上下するので、道に沿って細かい波が乗る。
	// 削り終えてから均せば消える
	void SmoothDeformed(HjHeightField* field);

	HjRoadSpline m_spline;

	// 地形。作り直すたびに削り直すので、借りたまま持っておく
	HjHeightField* m_pField = nullptr;

	// 削ったマス目と、削る前の高さ。
	//
	// 道を動かすたびに削り直すが、削った跡の上へさらに削ると
	// 掘り進んでいく。毎回ここから戻す。
	//
	// 地形を丸ごと控えると、戻すだけで数百万マスを舐めることになる。
	// 触った所だけ覚えておけば、道の周りの数万マスで済む
	std::unordered_map<int, float> m_deformedOriginal;

	// 前回削った範囲(ワールド)。
	// 地形のメッシュを組み直す所を、この範囲だけに絞る
	Math::Vector3 m_dirtyMin = Math::Vector3::Zero;
	Math::Vector3 m_dirtyMax = Math::Vector3::Zero;
	bool          m_hasDirtyArea = false;

	// 作り直したことを外へ伝える。
	// 地形のメッシュも組み直す必要がある
	bool m_dirty = false;

public:
	// 地形を作り直す必要があるか。読んだら下ろす
	bool ConsumeDirty() { const bool d = m_dirty; m_dirty = false; return d; }

	// 前回削った範囲。地形のメッシュを組み直す所を絞るのに使う
	bool GetDirtyArea(Math::Vector3& outMin, Math::Vector3& outMax) const
	{
		if (!m_hasDirtyArea) { return false; }
		outMin = m_dirtyMin;
		outMax = m_dirtyMax;
		return true;
	}

private:

	std::shared_ptr<KdMesh>  m_spMesh;

	// 地形へ溶ける裾。路面とは別の材質で塗る
	std::shared_ptr<KdMesh>  m_spApron;
	std::vector<KdMaterial>  m_apronMaterials;
	std::vector<KdMaterial>  m_materials;

	// 中心線の高さ。刻みごとに持つ。
	// スプラインの制御点は真上から見た線なので、高さは別に持つ
	std::vector<float> m_centerY;

	// 左右の傾き(高さ÷距離)。刻みごとに持つ。
	//
	// 中心線の高さだけだと路面が常に水平になり、山肌を横切る道が
	// 不自然になる。縦断で坂を扱うのと同じことを左右にもやる
	std::vector<float> m_centerRoll;
	// 刻みごとの道のり(m)。
	//
	// 刻みは一定ではない。曲がりのきつい所は細かく、直線は粗くする。
	// 「何番目の刻みか」から道のりを引けるように持っておく
	std::vector<float> m_stationS;

	// 刻みの数(区間の数)。m_stationS.size() - 1
	int StepCount() const
	{
		return std::max(0, static_cast<int>(m_stationS.size()) - 1);
	}

	// 何番目の刻みの道のり。範囲外は端で止める
	float StationS(int i) const
	{
		if (m_stationS.empty()) { return 0.0f; }
		return m_stationS[std::clamp(i, 0, static_cast<int>(m_stationS.size()) - 1)];
	}

	// 道のりから、何番目の刻みかを引く。
	// 刻みが一定でないので、割り算では出せない
	void FindStation(float s, int& outIndex, float& outT) const;

	// 刻みの位置を決める。曲がりのきつい所を細かくする
	void BuildStations();

	// 触りながら決める値。
	//
	// 定数のままだと、変えるたびにビルドし直すことになる。
	// 道は見ながら決めるものなので、それでは道具にならない。
	// 初期値は RoadConst から取る
	float m_maxGrade    = RoadConst::MaxGrade;
	float m_lift        = RoadConst::Lift;
	float m_bankFollow  = RoadConst::BankFollow;
	float m_maxBank     = RoadConst::MaxBank;
	float m_deformInner = RoadConst::DeformInner;
	float m_deformOuter = RoadConst::DeformOuter;
	float m_cutMax      = RoadConst::DeformCutMax;
	float m_fillMax     = RoadConst::DeformFillMax;
	float m_bedDrop     = RoadConst::BedDrop;
};
