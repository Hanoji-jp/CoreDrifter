#pragma once

#include "HjRoadSpline.h"

#include <unordered_map>
#include <unordered_set>

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

	// このマス目を道がどれだけ持っているか(0〜1)。
	//
	// 筆の覆いに使う。1は路面そのもので、そこを盛ると突き抜ける。
	// 削りの端に向けて0へ落ちるので、境目に段差ができない
	// このマス目が裾に完全に覆われているか。
	//
	// 地形はここを描かない。裾と重ねて描くと、同じ高さで
	// 深度が争ってちらつく。テクスチャを貼ると模様が入れ替わる
	bool IsCoveredCell(int cellIndex) const
	{
		return m_coverCells.find(cellIndex) != m_coverCells.end();
	}

	float OwnWeightAt(int cellIndex) const
	{
		const auto it = m_deformWeight.find(cellIndex);
		return (it == m_deformWeight.end()) ? 0.0f : it->second;
	}

	//===== 道に沿って物を並べるための断面 =====
	// ガードレールなどが使う。
	//
	// 刻みの決め方(曲がりに合わせる)と断面の向き(ミター)は
	// 道で作ったものをそのまま借りる。
	// 別に作ると、ヘアピンで道と柵がずれる
	int   StationCount() const { return StepCount() + 1; }
	float StationAt(int i) const { return StationS(i); }

	// この刻みの中心・断面の向き・ミターの伸び
	void  CrossAt(int i, Math::Vector3& outCenter,
	              Math::Vector3& outRight, float& outMiter) const;

	// この刻みの、中心線から offset ずれた所の路面の高さ
	float HeightAtOffset(int i, float offset) const { return SurfaceY(i, offset); }

	//===== 道のりを指して断面を引く =====
	// 刻みの番号ではなく道のり(m)で受ける。
	//
	// ■ なぜ要るか
	// 横向きの標示(停止線・減速マーク・横断歩道)は、刻みと関係ない
	// 位置へ置きたい。一番近い刻みで代用すると、そこから直線で
	// 外挿することになり、カーブで道から外れて斜めに寝る。
	//
	// 中心はスプラインから直に取るので、刻みの粗さに影響されない
	void  FrameAtS(float s, Math::Vector3& outCenter,
	               Math::Vector3& outRight, float& outMiter) const;

	// 道のりと横位置での路面の高さ。刻みの間は繋ぐ
	float HeightAtS(float s, float offset) const;

	// 道の全長(m)
	float TotalLength() const { return StationS(StationCount() - 1); }

	//===== 路肩の外に物を置くときの高さ =====
	// 断面式(SurfaceY)は舗装の外では一定値を返す。横断勾配だけを
	// 外挿し続けるので、路面を延ばした平面の高さになる。
	//
	// 裾や地形がそこでどうなっているかは見ていないので、
	// 平場を詰めた区間や、ヘアピンで裾が削られた所では浮くか沈む。
	//
	// 高さマップには道の面を焼き戻してあるので、そちらが本当の接地面。
	// 格子の外に出たときだけ断面式へ戻す
	float GroundAt(int step, float offset) const;

	//===== どちらへどれだけ曲がっているか =====
	// 1mあたりのラジアン。符号は曲がる向きで、正なら右。
	//
	// ■ なぜ道に持たせるか
	// 標識・視線誘導標・カーブミラー・路面標示が、それぞれ
	// 同じ式を写して持っていた。写した式の符号が逆だったので、
	// 右カーブに左向きの矢印が立ち、外側のつもりで内側に物が並んだ。
	//
	// 向きの決め方は1つでよい。ここを直せば全部直る。
	//
	// ■ 断面の向きではなく中心線の位置から出す
	// 断面の向きは前後の平均(ミター)なので、隣り合う刻みでほとんど
	// 同じ値になり、その差から符号を取ると刻みの粗さに埋もれる。
	// 中心線を3点取って曲がりを見るほうが素直で、桁も稼げる
	float TurnAt(int step) const;

	//===== 編集 =====
	// 点を動かしたら、道と地形を作り直す。
	//
	// 地形は道に合わせて削るので、元の形を控えておかないと
	// 削った跡の上へさらに削ることになり、掘り進んでいく
	void Rebuild();

	// 地形の高さを制御点へ取り込む。
	//
	// 一度きりの操作。以後、道は制御点の高さで決まるので、
	// 地形を彫っても道は動かない
	void BakeHeightFromTerrain(const HjHeightField* field);

	int  PointCount() const { return m_spline.PointCount(); }

	// 制御点に一番近い刻みの番号。
	//
	// 断面の向き(ミター)は刻みでしか持っていないので、
	// 制御点ごとの値を地形と突き合わせるときに要る
	int  StepOfPoint(int i) const;
	Math::Vector3 GetPoint(int i) const;

	// 制御点を画面に出す位置。
	//
	// 制御点の高さは「持ち上げ」なので、そのまま置くと
	// 道から離れた所に印が出る。道の実際の高さへ乗せる
	Math::Vector3 GetPointDisplayPos(int i) const;
	void MovePoint(int i, const Math::Vector3& pos);

	// 制御点ごとの裾の幅。区間ごとに伸ばすために使う
	// side は 0 が左、1 が右
	float GetApronAt(int i, int side) const { return m_spline.ApronAt(i, side); }
	void  SetApronAt(int i, int side, float w);

	// 制御点ごとの平場の幅。道と平行に伸ばす部分
	float GetFlatAt(int i, int side) const { return m_spline.FlatAt(i, side); }
	void  SetFlatAt(int i, int side, float w);

	//===== 制御点ごとの擁壁の高さ(m) =====
	// 0 なら壁なし。0 と 0 でない値の境目が壁の始まりと終わりになる
	float GetWallAt(int i, int side) const { return m_spline.WallAt(i, side); }
	void  SetWallAt(int i, int side, float h);

	// 刻みの位置での擁壁の高さ。壁を組む側が引く
	float WallAtStep(int step, int side) const
	{
		return m_spline.WallAtS(StationS(step), side);
	}

	//===== 制御点ごとのガードレール =====
	// 0 なら無し、1 なら有り
	float GetRailAt(int i, int side) const { return m_spline.RailAt(i, side); }
	void  SetRailAt(int i, int side, float on) { m_spline.SetRailAt(i, side, on); }

	bool  RailAtStep(int step, int side) const
	{
		return m_spline.RailAtS(StationS(step), side) > 0.5f;
	}
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

	// 断面を置く向きと、ミターの伸び。
	//
	// 接線に直交させて置くと、角の所で隣り合う区間の平行線が
	// ぴたりと交わらず、幅がわずかに足りなくなる。
	// 二等分線の向きに 1/cos(θ/2) だけ伸ばすと交点で出会う
	void CrossFrame(int step, Math::Vector3& outRight, float& outMiter) const;

	// スプラインからメッシュを組む。
	//
	// 路面と裾を1枚で作る。別々にすると幅方向の刻みが揃わず、
	// 折り返しを削った跡の穴埋めが境目を越えられない
	void BuildMesh(const HjHeightField* field);

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
	// 道が削ったマス目と、その強さ(0〜1)。
	//
	// 次に動かすとき、ここを素地へ戻してから削り直す。
	// 戻さずに削ると、動かすたびに掘り進んでいく。
	//
	// 強さは筆の覆いにも使う。1なら道が完全に持っている場所で、
	// そこを盛ると路面を突き抜ける
	std::unordered_map<int, float> m_deformWeight;

	// 裾に完全に覆われたマス目。地形はここを描かない
	std::unordered_set<int> m_coverCells;

	// 刻みごと・左右ごとに、実際に生き残った一番外の位置(m)。
	//
	// 指定した幅ではなく、折り返しや交差の判定を通ったあとの値。
	// 指定から穴を開けると、面が落ちた所で空が見える
	std::vector<float> m_coverReach[2];

	// マス目ごとの、道の面の高さ。MarkCover で作って BakeSurface で書く
	std::unordered_map<int, float> m_surfaceY;

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

	// メッシュを組んだ結果。
	//
	// 見た目だけで原因を当てるのは無理があった。
	// 落とした数と、一点へ集まった三角の枚数を出す
	struct BuildStat
	{
		int verts   = 0;   // 頂点の数
		int dropped = 0;   // 折り返し・潰れで落とした頂点
		int faces   = 0;   // 張った面
		int   cols    = 0;     // 幅方向の列数
		int   minCols = 0;     // 一番細くなった所の生きている列数
		float minAtS  = 0.0f;  // それが起きた道のり(m)
	};
	BuildStat m_statRoad;

	// 折り返しの除去を切る。
	//
	// 切って道が戻るなら、削りすぎが原因だと確かめられる
	bool m_trimFold = false;

	// 線だけで描く。
	// 塗り潰しだと、面が切れているのか繋がっているのか分からない
	bool m_wireframe = false;

	std::shared_ptr<KdMesh>  m_spMesh;

	// 地形へ溶ける裾。路面とは別の材質で塗る
	std::vector<KdMaterial>  m_materials;

	// 中心線の高さ。刻みごとに持つ。
	// スプラインの制御点は真上から見た線なので、高さは別に持つ
	std::vector<float> m_centerY;

	// 左右の傾き(高さ÷距離)。刻みごとに持つ。
	//
	// 中心線の高さだけだと路面が常に水平になり、山肌を横切る道が
	// 不自然になる。縦断で坂を扱うのと同じことを左右にもやる
	std::vector<float> m_centerRoll;



	// 刻みごと・左右ごとの裾の幅(m)。[0]=左 [1]=右
	//
	// 制御点ごとの指定と、向かいの裾との重なりで決まる
	std::vector<float> m_apronSpan[2];

	// 刻みごと・左右ごとの平場の幅(m)。
	// 道と平行に伸ばす部分。裾と同じく重なりで詰まる
	std::vector<float> m_apronFlatSpan[2];

	// 刻みごとの裾の幅(m)。左右で別。[0]=左 [1]=右
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

	// 刻みごとの裾の幅を決める。
	//
	// 制御点ごとの指定を引いたうえで、
	// 向かい合った裾と重なるなら、互いの真ん中で止める
	void ResolveApronSpan();

	// 裾に覆われたマス目を出す。地形はそこを描かない
	// 真上から見て、道の面に覆われたマス目を出す。
	//
	// 半径では決めない。ヘアピンでは折り目で面が削られて帯に穴が開くので、
	// 半径で消すと面の無い所まで地形が消えて空白になる
	void MarkCover(const HjHeightField* field,
	               const std::vector<KdMeshVertex>& verts,
	               const std::vector<KdMeshFace>& faces);

	// 道の面の高さを高さマップへ焼き戻す。
	// 当たり判定と見た目を同じ面にするため
	void BakeSurface(HjHeightField* field);


	// 触りながら決める値。
	//
	// 定数のままだと、変えるたびにビルドし直すことになる。
	// 道は見ながら決めるものなので、それでは道具にならない。
	// 平場の幅を全部の点へ一度に入れるときの値(m)。
	//
	// 幅そのものは制御点が持つ。ここは「全部これにする」ための入れ物
	float m_apronFlat = RoadConst::ApronFlat;

	// 平場を広げたぶん、斜面の降りる先も外へ伸ばす割合
	float m_apronFollowFlat = RoadConst::ApronFollowFlat;

	// 裾が路面から離れてよい高さ(m)。
	//
	// 道のすぐ横に高い山があると、裾は山の中を通って
	// 先だけ表へ出る。そこがちらつく
	float m_apronMaxRise = RoadConst::ApronMaxRise;
	float m_apronMaxDrop = RoadConst::ApronMaxDrop;



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
