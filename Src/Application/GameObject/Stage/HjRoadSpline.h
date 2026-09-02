#pragma once

#include "../../Const/RoadConst.h"

//==========================================================
// HjRoadSpline
//   道の中心線。自作なので Hj 接頭辞。
//
//   ■ 何を持つか
//   制御点と、そこから引いた滑らかな線。それだけ。
//   メッシュも当たり判定も持たない。
//
//   ■ なぜCatmull-Romか
//   制御点をそのまま通る。
//   通らない曲線だと、地図から拾った座標を置いても
//   道がその位置を通らず、置き直しの繰り返しになる。
//
//   ■ 距離で引けるようにする
//   曲線の媒介変数は、進んだ距離と比例しない。
//   コーナーでは同じ刻みでも進む距離が短くなる。
//
//   そのまま等間隔で切ると、コーナーだけ密になり、
//   直線が粗くなる。道のりを測っておいて、
//   「何メートル地点」で引けるようにしておく。
//==========================================================
class HjRoadSpline
{
public:
	// 制御点を入れる。4点以上ないと線にならない
	void SetPoints(const std::vector<Math::Vector3>& points);

	// ファイルから読む。1行に "x y z"
	bool LoadFromFile(const std::string& path);

	// 仮の道を作る。実データが無いうちに形を確かめるためのもの。
	// 地形の谷に沿った、S字の続く線
	void BuildTestPath(float length, float width);

	bool IsValid() const { return m_points.size() >= RoadConst::MinPoints; }

	// 全長(m)
	float TotalLength() const { return m_total; }

	//===== 引く =====
	// 距離 s 地点の位置
	Math::Vector3 PositionAt(float s) const;
	// 距離 s 地点の進む向き(正規化済み)
	Math::Vector3 TangentAt(float s) const;

	// 位置を線へ投影する。
	//
	// 「いまどこを走っているか」を出すのに使う。
	// 時刻ではなく道のりで追えるようになるので、
	// 押し出されても目標が逃げていかない
	//
	// outS   … 何メートル地点か
	// outOff … 中心線からの横のずれ(右が正)
	bool Project(const Math::Vector3& pos, float& outS, float& outOff) const;

	const std::vector<Math::Vector3>& GetPoints() const { return m_points; }

	//===== 編集 =====
	// 道は触って決めるもの。座標を手で打つのでは道具にならない。
	// 点を動かすたびに道のりを測り直す

	int  PointCount() const { return static_cast<int>(m_points.size()); }
	bool MovePoint(int index, const Math::Vector3& pos);

	// 制御点の高さは「持ち上げ」として使う。
	//
	// 道の高さは地形から拾って勾配の上限に収めるので、
	// 絶対の高さを持たせても上書きされる。
	// 自動で決めた形に対する、その地点での上下として扱う。
	//
	// 距離 s 地点での持ち上げ。点と点の間は繋ぐ
	// 制御点そのものの道のり(m)。
	//
	// 制御点はスプラインの節なので、道のりは探すまでもなく決まっている。
	// 一番近い所を探して出すと、ヘアピンで反対側の脚に吸い寄せられる
	float StationOfPoint(int i) const;

	float LiftAt(float s) const;

	// 曲がりの強さ(1メートルあたりのラジアン)。半径の逆数。
	//
	// ヘアピンでは半径が道幅より小さくなることがある。
	// そうなると内側の断面が前後で追い越して、面が折り返す
	float CurvatureAt(float s) const;

	// index の「次」へ点を足す。
	// 前後の中間に置くので、押しただけで形が崩れない
	bool InsertAfter(int index);

	// 末尾へ足す。
	//
	// 地図の上で線を引くときは、押した所へそのまま伸ばしたい。
	// 中間へ入れる InsertAfter とは用途が違う
	void AppendPoint(const Math::Vector3& pos);

	// 端は消せる。ただし最低数は割らない
	bool ErasePoint(int index);

	// ファイルへ書く。1行に "x y z"
	bool SaveToFile(const std::string& path) const;

private:
	// 制御点の間を滑らかに繋ぐ。t は 0〜1
	Math::Vector3 Interpolate(int seg, float t) const;

	// 道のりの表を作る。曲線の媒介変数と距離は比例しないので、
	// 引くたびに積分し直さなくて済むよう、先に測っておく
	void BuildLengthTable();

	// 距離 s から、どの区間のどこかを求める
	void ToSegment(float s, int& outSeg, float& outT) const;

	std::vector<Math::Vector3> m_points;

	// 区間ごとの、始点までの累積距離
	std::vector<float> m_accum;
	float m_total = 0.0f;
};
