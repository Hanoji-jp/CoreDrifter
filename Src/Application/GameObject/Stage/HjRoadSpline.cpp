#include "HjRoadSpline.h"

#include <fstream>
#include <sstream>

namespace RC = RoadConst;

namespace
{
	// 道のりを測るときの刻み。細かいほど正確だが、作るのが遅くなる。
	// 起動時に1回しか通らないので、細かめでよい
	constexpr int LengthSamples = 24;
}

//----------------------------------------------------------
void HjRoadSpline::SetPoints(const std::vector<Math::Vector3>& points)
{
	m_points = points;
	SyncApron();
	BuildLengthTable();
}

//----------------------------------------------------------
bool HjRoadSpline::LoadFromFile(const std::string& path)
{
	KdAssetIStream ifs(path);
	if (!ifs) { return false; }

	std::vector<Math::Vector3> pts;
	std::vector<float> apronL, apronR;
	std::vector<float> flatL, flatR;
	std::string line;

	while (std::getline(ifs, line))
	{
		// 空行と覚え書きは飛ばす。手で編集する前提なので、
		// 説明を書き込めるようにしておく
		if (line.empty() || line[0] == '#') { continue; }

		std::istringstream ss(line);
		float x = 0.0f, y = 0.0f, z = 0.0f;
		if (!(ss >> x >> y >> z)) { continue; }

		// 4つめ以降があれば、左右それぞれの裾と平場の幅。
		// 片方だけなら両側に使う。無い行は既定にする。
		//
		// ※ C++11 以降、>> は失敗すると値に 0 を書く。
		//   既定値を入れておいても上書きされるので、
		//   成功したときだけ受け取る
		auto Next = [&ss](float& out) -> bool
		{
			float v = 0.0f;
			if (!(ss >> v)) { return false; }
			out = v;
			return true;
		};

		float wl = RC::ApronWidth;
		float wr = RC::ApronWidth;
		if (Next(wl)) { wr = wl; Next(wr); }

		float fl = RC::ApronFlat;
		float fr = RC::ApronFlat;
		if (Next(fl)) { fr = fl; Next(fr); }

		pts.push_back(Math::Vector3(x, y, z));
		apronL.push_back(wl);
		apronR.push_back(wr);
		flatL.push_back(fl);
		flatR.push_back(fr);
	}

	if (static_cast<int>(pts.size()) < RC::MinPoints) { return false; }

	SetPoints(pts);
	m_apron[0] = apronL;
	m_apron[1] = apronR;
	m_flat[0] = flatL;
	m_flat[1] = flatR;
	SyncApron();
	return true;
}

//----------------------------------------------------------
// 仮の道
//
// 実データが無いうちに、道の形と当たり判定を確かめるためのもの。
// S字が続く線にしておくと、振り出しと切り返しの両方を試せる
//----------------------------------------------------------
void HjRoadSpline::BuildTestPath(float length, float width)
{
	std::vector<Math::Vector3> pts;

	// 何点置くか。曲がりが表現できる程度に
	constexpr int Count = 24;
	constexpr float Waves = 3.0f;   // S字の回数

	for (int i = 0; i < Count; ++i)
	{
		const float t = static_cast<float>(i) / (Count - 1);

		// 奥へ真っ直ぐ進みながら、左右に振る
		const float z = (t - 0.5f) * length;
		const float x = sinf(t * Waves * 6.28318530f) * width;

		// 高さは地形へ沿わせる工程で決めるので、ここでは0
		pts.push_back(Math::Vector3(x, 0.0f, z));
	}

	SetPoints(pts);
}

//----------------------------------------------------------
// 制御点の間を滑らかに繋ぐ(Catmull-Rom)
//
// 制御点をそのまま通るので、地図から拾った座標を置けば
// 道はその位置を通る。通らない曲線だと置き直しの繰り返しになる
//----------------------------------------------------------
Math::Vector3 HjRoadSpline::Interpolate(int seg, float t) const
{
	const int n = static_cast<int>(m_points.size());

	// 前後の点も要る。端では自分自身で代用する
	const int i0 = std::clamp(seg - 1, 0, n - 1);
	const int i1 = std::clamp(seg,     0, n - 1);
	const int i2 = std::clamp(seg + 1, 0, n - 1);
	const int i3 = std::clamp(seg + 2, 0, n - 1);

	const Math::Vector3& p0 = m_points[i0];
	const Math::Vector3& p1 = m_points[i1];
	const Math::Vector3& p2 = m_points[i2];
	const Math::Vector3& p3 = m_points[i3];

	const float t2 = t * t;
	const float t3 = t2 * t;

	return 0.5f * ((2.0f * p1)
	             + (-p0 + p2) * t
	             + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
	             + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

//----------------------------------------------------------
// 道のりの表
//
// 曲線の媒介変数と進んだ距離は比例しない。
// コーナーでは同じ刻みでも進む距離が短くなる。
//
// そのまま等間隔で切ると、コーナーだけ密になって直線が粗くなる。
// 先に測っておいて、距離で引けるようにする
//----------------------------------------------------------
void HjRoadSpline::BuildLengthTable()
{
	m_accum.clear();
	m_total = 0.0f;

	if (!IsValid()) { return; }

	const int segs = static_cast<int>(m_points.size()) - 1;
	m_accum.reserve(static_cast<size_t>(segs) + 1);
	m_accum.push_back(0.0f);

	for (int s = 0; s < segs; ++s)
	{
		Math::Vector3 prev = Interpolate(s, 0.0f);
		float len = 0.0f;

		for (int i = 1; i <= LengthSamples; ++i)
		{
			const Math::Vector3 cur =
				Interpolate(s, static_cast<float>(i) / LengthSamples);
			len += (cur - prev).Length();
			prev = cur;
		}

		m_total += len;
		m_accum.push_back(m_total);
	}
}

//----------------------------------------------------------
void HjRoadSpline::ToSegment(float s, int& outSeg, float& outT) const
{
	outSeg = 0;
	outT   = 0.0f;
	if (m_accum.size() < 2) { return; }

	s = std::clamp(s, 0.0f, m_total);

	// どの区間に入るかを探す。
	// 区間の数は数十なので、総当たりで足りる
	for (size_t i = 1; i < m_accum.size(); ++i)
	{
		if (s > m_accum[i]) { continue; }

		const float a = m_accum[i - 1];
		const float b = m_accum[i];
		const float span = b - a;

		outSeg = static_cast<int>(i) - 1;
		outT   = (span > 1e-6f) ? ((s - a) / span) : 0.0f;
		return;
	}

	outSeg = static_cast<int>(m_accum.size()) - 2;
	outT   = 1.0f;
}

//----------------------------------------------------------
Math::Vector3 HjRoadSpline::PositionAt(float s) const
{
	if (!IsValid()) { return Math::Vector3::Zero; }

	int seg = 0;
	float t = 0.0f;
	ToSegment(s, seg, t);
	return Interpolate(seg, t);
}

//----------------------------------------------------------
Math::Vector3 HjRoadSpline::TangentAt(float s) const
{
	if (!IsValid()) { return Math::Vector3::UnitZ; }

	// 前後の点の差から求める。
	// 微分の式を書くより、端の扱いが単純になる
	const float d = 0.5f;
	const Math::Vector3 a = PositionAt(std::max(s - d, 0.0f));
	const Math::Vector3 b = PositionAt(std::min(s + d, m_total));

	Math::Vector3 dir = b - a;
	if (dir.LengthSquared() < 1e-8f) { return Math::Vector3::UnitZ; }

	dir.Normalize();
	return dir;
}

//----------------------------------------------------------
// 位置を線へ投影する
//
// 「いまどこを走っているか」を道のりで出す。
// 時刻で追うと、押し出された瞬間に目標が先へ逃げていくが、
// 道のりなら遅れは遅れとして受け入れられる
//----------------------------------------------------------
bool HjRoadSpline::Project(const Math::Vector3& pos, float& outS, float& outOff) const
{
	if (!IsValid()) { return false; }

	const int segs = static_cast<int>(m_points.size()) - 1;

	float bestD2 = 1e18f;
	float bestS  = 0.0f;

	// 区間ごとに、いくつか点を打って一番近い所を探す。
	// 曲がっているので直線として扱うと少しずれる
	for (int s = 0; s < segs; ++s)
	{
		for (int i = 0; i <= RC::ProjectSamples; ++i)
		{
			const float t = static_cast<float>(i) / RC::ProjectSamples;
			const Math::Vector3 p = Interpolate(s, t);

			// 高さは見ない。真上から見た距離で判断する。
			// 見ると、坂で下を通っている道に吸い寄せられる
			const float dx = p.x - pos.x;
			const float dz = p.z - pos.z;
			const float d2 = dx * dx + dz * dz;

			if (d2 >= bestD2) { continue; }

			bestD2 = d2;

			// 区間の中の位置を、道のりへ直す
			const float a = m_accum[s];
			const float b = m_accum[s + 1];
			bestS = a + (b - a) * t;
		}
	}

	// 大まかな位置が出た。ここから詰める。
	//
	// 分割だけだと、全長に対して数メートルおきにしか
	// 位置が決まらない。コーナーで横のずれが狂う原因になる。
	//
	// 前後を半分ずつ狭めて、一番近い所へ寄せていく
	{
		// 探す幅。分割1つ分の長さ
		const float segLen = m_total / std::max<float>(
			static_cast<float>(segs * RC::ProjectSamples), 1.0f);

		float lo = std::max(bestS - segLen, 0.0f);
		float hi = std::min(bestS + segLen, m_total);

		// 真上から見た距離で測る。
		// 高さを含めると、坂で下を通っている道に吸い寄せられる
		auto dist2 = [&](float s)
		{
			const Math::Vector3 p = PositionAt(s);
			const float dx = p.x - pos.x;
			const float dz = p.z - pos.z;
			return dx * dx + dz * dz;
		};

		for (int i = 0; i < RC::ProjectRefine; ++i)
		{
			// 三等分して、遠いほうを捨てる
			const float a = lo + (hi - lo) / 3.0f;
			const float b = hi - (hi - lo) / 3.0f;

			if (dist2(a) < dist2(b)) { hi = b; } else { lo = a; }
		}
		bestS = (lo + hi) * 0.5f;
	}

	outS = bestS;

	// 中心線からどちら側へ、どれだけ外れているか。
	// 右が正になるよう、進む向きと外積を取る
	const Math::Vector3 center = PositionAt(bestS);
	const Math::Vector3 fwd    = TangentAt(bestS);
	const Math::Vector3 right  = Math::Vector3::Up.Cross(fwd);

	const Math::Vector3 diff(pos.x - center.x, 0.0f, pos.z - center.z);
	outOff = diff.Dot(right);

	return true;
}

//----------------------------------------------------------
// 編集
//----------------------------------------------------------
bool HjRoadSpline::MovePoint(int index, const Math::Vector3& pos)
{
	if (index < 0 || index >= static_cast<int>(m_points.size())) { return false; }

	m_points[index] = pos;

	// 動かすと道のりが変わる。測り直さないと、
	// 引く位置と実際の位置が食い違う
	BuildLengthTable();
	return true;
}

bool HjRoadSpline::InsertAfter(int index)
{
	const int n = static_cast<int>(m_points.size());
	if (n < 2) { return false; }

	index = std::clamp(index, 0, n - 1);

	// 前後の中間へ置く。
	// 同じ場所へ足すと線が折り返し、端へ足すと道が伸びる。
	// どちらも「点を増やしたい」ときに望む動きではない
	const int next = std::min(index + 1, n - 1);
	const Math::Vector3 mid = (m_points[index] + m_points[next]) * 0.5f;

	m_points.insert(m_points.begin() + index + 1, mid);

	// 裾の幅も同じ所へ入れる。入れないと以降の点の幅が1つずつずれる
	SyncApron();
	m_apron[0].insert(m_apron[0].begin() + index + 1, ApronAt(index, 0));
	m_apron[1].insert(m_apron[1].begin() + index + 1, ApronAt(index, 1));
	m_flat[0].insert(m_flat[0].begin() + index + 1, FlatAt(index, 0));
	m_flat[1].insert(m_flat[1].begin() + index + 1, FlatAt(index, 1));
	BuildLengthTable();
	return true;
}

bool HjRoadSpline::ErasePoint(int index)
{
	const int n = static_cast<int>(m_points.size());

	// 最低数を割ると線が引けなくなる。
	// 消せなくなるより、消させないほうがよい
	if (n <= RoadConst::MinPoints) { return false; }
	if (index < 0 || index >= n) { return false; }

	m_points.erase(m_points.begin() + index);

	SyncApron();
	for (int side = 0; side < 2; ++side)
	{
		if (index < static_cast<int>(m_apron[side].size()))
		{
			m_apron[side].erase(m_apron[side].begin() + index);
		}
		if (index < static_cast<int>(m_flat[side].size()))
		{
			m_flat[side].erase(m_flat[side].begin() + index);
		}
	}
	BuildLengthTable();
	return true;
}

bool HjRoadSpline::SaveToFile(const std::string& path) const
{
	std::ofstream ofs(path);
	if (!ofs) { return false; }

	ofs << "# 道の制御点。1行に x y z 左の裾 右の裾 左の平場 右の平場\n";
	ofs << "# 高さを書いておくと、その高さが道になる(地形がそれに合わせて削れる)\n";
	ofs << "# 裾の幅は省ける。1つだけなら両側に使う\n";

	for (size_t i = 0; i < m_points.size(); ++i)
	{
		const Math::Vector3& p = m_points[i];
		const int k = static_cast<int>(i);
		ofs << p.x << " " << p.y << " " << p.z << " "
		    << ApronAt(k, 0) << " " << ApronAt(k, 1) << " "
		    << FlatAt(k, 0)  << " " << FlatAt(k, 1)  << "\n";
	}
	return true;
}

//----------------------------------------------------------
// 制御点の持ち上げ
//
// 道の高さは地形から拾って勾配の上限に収めるので、
// 絶対の高さを持たせても上書きされる。
// 自動で決めた形に対する、その地点での上下として扱う
//----------------------------------------------------------
float HjRoadSpline::LiftAt(float s) const
{
	if (!IsValid()) { return 0.0f; }

	int seg = 0;
	float t = 0.0f;
	ToSegment(s, seg, t);

	// 位置と同じ曲線で繋ぐ。
	//
	// 直線で繋ぐと、1点上げただけで三角のテントになる。
	// 位置は滑らかなのに高さだけ角ばるので、
	// 坂がパキパキした階段のように見える
	return Interpolate(seg, t).y;
}

//----------------------------------------------------------
// 末尾へ足す
//----------------------------------------------------------
void HjRoadSpline::AppendPoint(const Math::Vector3& pos)
{
	m_points.push_back(pos);
	SyncApron();
	BuildLengthTable();
}

//----------------------------------------------------------
// 曲がりの強さ
//
// ヘアピンでは、曲がりの半径が道幅より小さくなることがある。
// そうなると内側の断面が前後で追い越して、面が折り返す。
//
// 半径が分かれば、そこで許される道幅も決まる
//----------------------------------------------------------
float HjRoadSpline::CurvatureAt(float s) const
{
	if (!IsValid()) { return 0.0f; }

	// 前後の進む向きが、どれだけ変わったかを見る
	const float d = 2.0f;

	const Math::Vector3 a = TangentAt(std::max(s - d, 0.0f));
	const Math::Vector3 b = TangentAt(std::min(s + d, m_total));

	const float dot = std::clamp(a.Dot(b), -1.0f, 1.0f);
	const float ang = acosf(dot);

	// 1メートル進むあたり何ラジアン曲がるか。
	// これが半径の逆数になる
	return ang / (d * 2.0f);
}

//----------------------------------------------------------
// 制御点そのものの道のり
//
// 制御点はスプラインの節なので、道のりの表にそのまま載っている。
// 一番近い所を探して出す必要はない。
//
// 探すと、ヘアピンで行きと帰りの道が数メートルまで近づいた所で、
// 自分の節ではなく反対側の脚に吸い寄せられる
//----------------------------------------------------------
float HjRoadSpline::StationOfPoint(int i) const
{
	if (m_accum.empty()) { return 0.0f; }
	return m_accum[std::clamp(i, 0, static_cast<int>(m_accum.size()) - 1)];
}

//----------------------------------------------------------
// その場の曲がりの強さ
//
// CurvatureAt は前後2mの平均なので、ヘアピンの入口では
// 直線と曲線が混ざって実際より緩く出る。
// 実測では、本当の最小半径2.6mを5.6mと見ていた。
//
// こちらは近い3点の外接円から出すので、平均で薄まらない。
// 道を組むのに使うと値が地点ごとに暴れて幅がガタつくので、
// 「ここは急すぎる」と知らせるためだけに使う
//----------------------------------------------------------
float HjRoadSpline::LocalCurvatureAt(float s) const
{
	if (!IsValid()) { return 0.0f; }

	const float d = 0.4f;

	const Math::Vector3 a = PositionAt(std::max(s - d, 0.0f));
	const Math::Vector3 b = PositionAt(s);
	const Math::Vector3 cc = PositionAt(std::min(s + d, m_total));

	// 真上から見た三角形。高さは見ない
	const float ab = sqrtf((b.x - a.x) * (b.x - a.x) + (b.z - a.z) * (b.z - a.z));
	const float bc = sqrtf((cc.x - b.x) * (cc.x - b.x) + (cc.z - b.z) * (cc.z - b.z));
	const float ca = sqrtf((cc.x - a.x) * (cc.x - a.x) + (cc.z - a.z) * (cc.z - a.z));

	// 外接円の半径は (辺の積) / (4 * 面積)
	const float area = fabsf((b.x - a.x) * (cc.z - a.z) - (b.z - a.z) * (cc.x - a.x)) * 0.5f;

	if (area < 1e-6f || ab * bc * ca < 1e-9f) { return 0.0f; }

	return 4.0f * area / (ab * bc * ca);
}

//----------------------------------------------------------
// 裾の幅の数を、制御点の数へ合わせる
//
// 点を足す・消すたびに呼ぶ。合わせないと、
// 以降の点の幅が1つずつずれる
//----------------------------------------------------------
// 裾の幅の数を、制御点の数へ合わせる
//
// 点を足す・消すたびに呼ぶ。合わせないと、
// 以降の点の幅が1つずつずれる
//----------------------------------------------------------
void HjRoadSpline::SyncApron()
{
	m_apron[0].resize(m_points.size(), RC::ApronWidth);
	m_apron[1].resize(m_points.size(), RC::ApronWidth);
	m_flat[0].resize(m_points.size(), RC::ApronFlat);
	m_flat[1].resize(m_points.size(), RC::ApronFlat);
}

//----------------------------------------------------------
// 制御点ごとの裾の幅
//
// 左右で別に持つ。
// 谷側だけ伸ばして山側は詰める、という使い方をするので、
// 1つの値だと片側に合わせるしかなくなる
//----------------------------------------------------------
float HjRoadSpline::ApronAt(int index, int side) const
{
	const std::vector<float>& v = m_apron[std::clamp(side, 0, 1)];
	if (v.empty()) { return RC::ApronWidth; }

	return v[std::clamp(index, 0, static_cast<int>(v.size()) - 1)];
}

void HjRoadSpline::SetApronAt(int index, int side, float w)
{
	SyncApron();

	std::vector<float>& v = m_apron[std::clamp(side, 0, 1)];
	if (index < 0 || index >= static_cast<int>(v.size())) { return; }

	v[index] = std::clamp(w, 0.0f, RC::ApronWidthMax);
}

//----------------------------------------------------------
// 道のりから裾の幅を引く
//
// 制御点の間はなめらかに繋ぐ。
// そのまま切り替えると、そこで裾の外端が横へ跳ねて、
// 一点に潰れた三角の扇ができる
//----------------------------------------------------------
float HjRoadSpline::ApronAtS(float s, int side) const
{
	const std::vector<float>& v = m_apron[std::clamp(side, 0, 1)];
	if (v.empty()) { return RC::ApronWidth; }

	int seg = 0;
	float t = 0.0f;
	ToSegment(s, seg, t);

	const int n = static_cast<int>(v.size());
	const int i0 = std::clamp(seg,     0, n - 1);
	const int i1 = std::clamp(seg + 1, 0, n - 1);

	// 端で折れないよう滑らかに
	const float k = t * t * (3.0f - 2.0f * t);

	return v[i0] + (v[i1] - v[i0]) * k;
}

//----------------------------------------------------------
// 制御点ごとの平場の幅
//
// 路肩の外に、道と平行に伸ばす幅。
// 裾と同じく区間ごとに変えたい
//----------------------------------------------------------
float HjRoadSpline::FlatAt(int index, int side) const
{
	const std::vector<float>& v = m_flat[std::clamp(side, 0, 1)];
	if (v.empty()) { return RC::ApronFlat; }

	return v[std::clamp(index, 0, static_cast<int>(v.size()) - 1)];
}

void HjRoadSpline::SetFlatAt(int index, int side, float w)
{
	SyncApron();

	std::vector<float>& v = m_flat[std::clamp(side, 0, 1)];
	if (index < 0 || index >= static_cast<int>(v.size())) { return; }

	v[index] = std::clamp(w, 0.0f, RC::ApronFlatMax);
}

//----------------------------------------------------------
// 道のりから平場の幅を引く
//
// 制御点の間はなめらかに繋ぐ。
// そのまま切り替えると、そこで外端が横へ跳ねる
//----------------------------------------------------------
float HjRoadSpline::FlatAtS(float s, int side) const
{
	const std::vector<float>& v = m_flat[std::clamp(side, 0, 1)];
	if (v.empty()) { return RC::ApronFlat; }

	int seg = 0;
	float t = 0.0f;
	ToSegment(s, seg, t);

	const int n = static_cast<int>(v.size());
	const int i0 = std::clamp(seg,     0, n - 1);
	const int i1 = std::clamp(seg + 1, 0, n - 1);

	// 端で折れないよう滑らかに
	const float k = t * t * (3.0f - 2.0f * t);

	return v[i0] + (v[i1] - v[i0]) * k;
}
