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

	//===== 制御点ごとの裾の幅 =====
	// 区間ごとに裾を伸ばしたいので、制御点に持たせる。
	//
	// 道のりの範囲で持つやり方もあるが、制御点を動かすと
	// 全長が変わって、指定した範囲がずれていく。
	// 制御点に持たせれば、点と一緒に動く
	// side は 0 が左、1 が右。進む向きに対しての左右
	float ApronAt(int index, int side) const;
	void  SetApronAt(int index, int side, float w);

	// 道のりから引く。制御点の間はなめらかに繋ぐ
	float ApronAtS(float s, int side) const;

	//===== 制御点ごとの平場の幅 =====
	// 路肩の外に、道と平行に伸ばす幅。
	// 裾と同じく区間ごとに変えたいので、制御点に持たせる
	float FlatAt(int index, int side) const;
	void  SetFlatAt(int index, int side, float w);
	float FlatAtS(float s, int side) const;

	//===== 制御点ごとの擁壁の高さ(m) =====
	// 0 なら壁なし。裾や平場と同じで区間ごとに変えたい。
	//
	// 高さで持つ理由は、有無だけだと「ここは低い壁でいい」が言えないため。
	// 0 と 0 でない値の境目が、そのまま壁の始まりと終わりになる
	float WallAt(int index, int side) const;
	void  SetWallAt(int index, int side, float h);
	float WallAtS(float s, int side) const;

	//===== 制御点ごとのガードレール =====
	// 0 なら無し、1 なら有り。間は繋ぐので、0.5 を境に切り替わる。
	//
	// 有無だけなので bool でよさそうだが、裾や擁壁と同じ float で持つ。
	// 保存も補間も同じ仕組みに乗るので、片方だけ別扱いにしない
	float RailAt(int index, int side) const;
	void  SetRailAt(int index, int side, float on);
	float RailAtS(float s, int side) const;
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

	// その場の曲がりの強さ。近い3点の外接円から出す。
	//
	// CurvatureAt は前後2mを平均するので、ヘアピンの入口では
	// 直線と曲線が混ざって実際より緩く出る。道を組むには
	// 平均のほうが値が暴れなくてよいが、
	// 「ここは急すぎる」と知らせるには本当の値が要る
	float LocalCurvatureAt(float s) const;

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

	// 制御点ごとの裾の幅(m)。m_points と同じ数だけ持つ。
	//
	// 点を足す・消すたびに数を合わせる必要があるので、
	// 触る所は SyncApron を通す
	std::vector<float> m_apron[2];

	// 制御点ごとの平場の幅(m)。同じく左が0、右が1
	std::vector<float> m_flat[2];

	// 制御点ごとの擁壁の高さ(m)。0=壁なし
	std::vector<float> m_wall[2];

	// 制御点ごとのガードレール。0=無し 1=有り
	std::vector<float> m_rail[2];

	// 裾の幅の数を、制御点の数へ合わせる
	void SyncApron();

	// 区間ごとの、始点までの累積距離
	std::vector<float> m_accum;
	float m_total = 0.0f;
};
