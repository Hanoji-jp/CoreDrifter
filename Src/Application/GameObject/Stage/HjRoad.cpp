#include "HjRoad.h"

#include "HjHeightField.h"
#include "../../Util/HjProfiler.h"

#include <unordered_map>

namespace RC = RoadConst;

//----------------------------------------------------------
void HjRoad::Init(HjHeightField* field)
{
	m_drawType = eDrawTypeLit;

	// 制御点。無ければ仮の道を引く。
	// データ待ちで形を確かめられない、という状態を作らない
	if (!m_spline.LoadFromFile(RC::PathFile))
	{
		float len = 600.0f;
		float amp = 40.0f;

		// 地形があるなら、その広さに収める
		if (field && field->IsValid())
		{
			len = field->GetWorldD() * 0.8f;
			amp = std::min(field->GetWorldW() * 0.15f, 60.0f);
		}
		m_spline.BuildTestPath(len, amp);
	}

	m_materials.resize(1);
	m_materials[0].m_name = "road";

	// 仮の色。地形と見分けが付く程度に暗くする
	m_materials[0].m_baseColorRate =
		Math::Vector4(RC::SurfaceR, RC::SurfaceG, RC::SurfaceB, 1.0f);

	// エプロンは地形寄りの色。
	// 路面と同じ色だと道が太って見え、地形と同じ色だと
	// 路肩の外側が唐突に切れて見える。その中間に置く
	m_apronMaterials.resize(1);
	m_apronMaterials[0].m_name = "road_apron";
	m_apronMaterials[0].m_baseColorRate = Math::Vector4(
		(RC::SurfaceR + TerrainConst::GroundR) * 0.5f,
		(RC::SurfaceG + TerrainConst::GroundG) * 0.5f,
		(RC::SurfaceB + TerrainConst::GroundB) * 0.5f, 1.0f);

	// 地形を控えておく。
	//
	// 道を動かすたびに削り直すが、削った跡の上へさらに削ると
	// 掘り進んでいく。毎回この形から始める
	// 地形は借りたまま持っておく。
	//
	// 控えは丸ごと取らない。削ったマス目だけを覚えておけば、
	// 戻すのも道の周りの数万マスで済む
	m_pField = field;

	ResolveHeights(field);

	// 道の周りの地形を、道の高さへ寄せる。
	// 先にメッシュを組むと、寄せる前の地形で法線を作ってしまう
	DeformTerrain(field);

	// 削ったあとに均す。
	// 寄せる強さの細かい上下が、道に沿った波として残る
	SmoothDeformed(field);

	BuildMesh();
	BuildApron(field);
}

//----------------------------------------------------------
// 中心線の高さを地形へ沿わせる
//
// スプラインの制御点は真上から見た線なので、高さを持っていない。
// 地形の高さをそのまま拾うと、細かい凹凸で道が波打つので均す
//----------------------------------------------------------
void HjRoad::ResolveHeights(const HjHeightField* field)
{
	m_centerY.clear();
	if (!m_spline.IsValid()) { return; }

	// 刻みを決める。曲がりのきつい所は細かくなる
	BuildStations();

	const int steps = StepCount();
	if (steps < 1) { return; }

	m_centerY.resize(static_cast<size_t>(steps) + 1, 0.0f);

	// 地形から下敷きを作る。
	//
	// ※これは「地形に沿わせる」ためではなく、
	//   どのあたりの標高を通るかの当たりを付けるため。
	//   このあと均して勾配に収めるので、地形の起伏は残らない
	for (int i = 0; i <= steps; ++i)
	{
		const float s = StationS(i);
		const Math::Vector3 p = m_spline.PositionAt(s);

		float h = 0.0f;
		if (field && field->IsValid())
		{
			h = field->HeightAt(p.x, p.z);
			if (h <= TerrainConst::OutsideHeight) { h = 0.0f; }
		}
		m_centerY[i] = h;
	}

	// 細かい凹凸を落とす
	for (int pass = 0; pass < RC::SeedSmoothPasses; ++pass)
	{
		std::vector<float> next = m_centerY;
		for (size_t i = 1; i + 1 < m_centerY.size(); ++i)
		{
			next[i] = (m_centerY[i - 1] + m_centerY[i] * 2.0f + m_centerY[i + 1]) * 0.25f;
		}
		m_centerY = next;
	}

	LimitGrade();

	// 道ごと持ち上げる。
	//
	// 地形の削りも、路面も、当たり判定も、全部この値を見るので、
	// 上げても道と地形の関係は崩れない。
	// 上げるほど盛り土が増え、切り通しが減る
	// 道全体の持ち上げ
	if (m_lift != 0.0f)
	{
		for (auto& y : m_centerY) { y += m_lift; }
	}

	// 制御点ごとの持ち上げ。
	//
	// 自動で決めた形に対する上下として足す。
	// 絶対の高さにすると、勾配の上限が効かなくなって
	// 崖のような坂が作れてしまう
	for (int i = 0; i <= steps; ++i)
	{
		m_centerY[i] += m_spline.LiftAt(StationS(i));
	}

	// 左右の傾き。高さが決まってから
	ResolveBank(field);
}

//----------------------------------------------------------
// 路面の高さ
//
// メッシュも当たり判定も地形の削りもエプロンも、全部ここを通す。
// 別々に計算すると、傾きを足したときに片方だけ直すことになる
//----------------------------------------------------------
float HjRoad::SurfaceY(int step, float offset) const
{
	if (m_centerY.empty()) { return 0.0f; }

	const int i = std::clamp(step, 0, static_cast<int>(m_centerY.size()) - 1);

	float y = m_centerY[i] + CrossHeight(offset);

	// 左右の傾き。中心から離れるほど効く
	if (i < static_cast<int>(m_centerRoll.size()))
	{
		y += offset * m_centerRoll[i];
	}
	return y;
}

//----------------------------------------------------------
// 刻みの位置を決める
//
// 刻みがそのまま折り目になるので、曲がりのきつい所は細かくしたい。
// かといって全部を細かくすると、直線に無駄な頂点が並ぶ。
//
// 「1区間で何度まで曲がってよいか」から刻みを逆算する
//----------------------------------------------------------
void HjRoad::BuildStations()
{
	m_stationS.clear();
	if (!m_spline.IsValid()) { return; }

	const float total = m_spline.TotalLength();
	const float maxTurn = RC::StepMaxTurnDeg * (3.14159265f / 180.0f);

	float s = 0.0f;
	m_stationS.push_back(0.0f);

	// 念のための上限。曲がりが極端だと刻みが下限に張り付いて、
	// 点が際限なく増える
	const int hardMax = static_cast<int>(total / RC::StepMin) + 16;

	while (s < total && static_cast<int>(m_stationS.size()) < hardMax)
	{
		// この地点の曲がり(1メートルあたりのラジアン)
		const float k = m_spline.CurvatureAt(s);

		// 許した角度ぶん進める距離。
		// 曲がりが0なら割れないので、上限で止める
		float step = (k > 1e-5f) ? (maxTurn / k) : RC::StepMax;
		step = std::clamp(step, RC::StepMin, RC::StepMax);

		s += step;
		m_stationS.push_back(std::min(s, total));
	}

	// 端をきっちり合わせる。
	// 合わせないと、道の末端が少し足りないか行き過ぎる
	if (m_stationS.size() >= 2) { m_stationS.back() = total; }
}

//----------------------------------------------------------
// 道のりから、何番目の刻みかを引く
//
// 刻みが一定でないので、割り算では出せない
//----------------------------------------------------------
void HjRoad::FindStation(float s, int& outIndex, float& outT) const
{
	outIndex = 0;
	outT = 0.0f;

	const int n = static_cast<int>(m_stationS.size());
	if (n < 2) { return; }

	s = std::clamp(s, 0.0f, m_stationS.back());

	// 半分ずつ狭めて探す。
	// 総当たりだと、点が数千あるときに1回の問い合わせが重くなる
	int lo = 0, hi = n - 1;
	while (hi - lo > 1)
	{
		const int mid = (lo + hi) / 2;
		if (m_stationS[mid] <= s) { lo = mid; } else { hi = mid; }
	}

	const float a = m_stationS[lo];
	const float b = m_stationS[hi];
	const float span = b - a;

	outIndex = lo;
	outT = (span > 1e-6f) ? ((s - a) / span) : 0.0f;
}

//----------------------------------------------------------
// この地点で内側へ使ってよい幅
//
// ヘアピンでは、曲がりの半径が道幅より小さくなる。
// そのまま断面を並べると、内側が前後で追い越して面が折り返す。
//
// 半径より内へ収めれば折り返さない。
// 道が少し細くなるが、実際の峠もヘアピンの内側は詰まって見える
//----------------------------------------------------------
float HjRoad::InnerLimit(int step) const
{
	const float outer = RC::HalfWidth + RC::ShoulderWidth + RC::ApronWidth;

	const float s = StationS(step);
	const float k = m_spline.CurvatureAt(s);

	// 緩い曲がりでは絞らない
	if (k < RC::CurveIgnore) { return outer; }

	const float radius = 1.0f / k;
	return std::min(outer, radius * RC::InnerLimitRatio);
}

//----------------------------------------------------------
// 左右の傾き
//
// 縦断で坂を扱うのと同じことを左右にもやる。
// 地形の傾きを拾い、均して、上限に収める
//----------------------------------------------------------
void HjRoad::ResolveBank(const HjHeightField* field)
{
	m_centerRoll.assign(m_centerY.size(), 0.0f);
	if (!field || !field->IsValid() || m_centerY.empty()) { return; }

	const int steps = StepCount();

	// 地形の傾きを拾う
	for (int i = 0; i <= steps; ++i)
	{
		const float s = StationS(i);

		const Math::Vector3 center = m_spline.PositionAt(s);
		const Math::Vector3 fwd    = m_spline.TangentAt(s);
		const Math::Vector3 right  = Math::Vector3::Up.Cross(fwd);

		// 路肩の外まで取る。
		// 近い所だけ見ると、削った跡の平らな所を測ることになる
		const float d = RC::BankProbe;

		const float hl = field->HeightAt(center.x - right.x * d, center.z - right.z * d);
		const float hr = field->HeightAt(center.x + right.x * d, center.z + right.z * d);

		if (hl <= TerrainConst::OutsideHeight || hr <= TerrainConst::OutsideHeight)
		{
			continue;
		}

		m_centerRoll[i] = ((hr - hl) / (d * 2.0f)) * m_bankFollow;
	}

	// 均す。
	// 地形の細かい凹凸を拾うと、路面が小刻みに捻れる
	for (int pass = 0; pass < RC::BankSmoothPasses; ++pass)
	{
		std::vector<float> next = m_centerRoll;
		for (size_t i = 1; i + 1 < m_centerRoll.size(); ++i)
		{
			next[i] = (m_centerRoll[i - 1] + m_centerRoll[i] * 2.0f
			         + m_centerRoll[i + 1]) * 0.25f;
		}
		m_centerRoll = next;
	}

	// 上限に収める。
	// 拾ったままだと崖に沿う所で路面が立つ
	for (auto& r : m_centerRoll)
	{
		r = std::clamp(r, -m_maxBank, m_maxBank);
	}
}

//----------------------------------------------------------
// 勾配を上限に収める
//
// 均しただけでは、長い坂の傾きは残る。
// 実際の道は勾配に制限があり、きつい所は山を削って緩めてある。
//
// 隣り合う点の差が上限を超えていたら、両方を寄せて減らす。
// 片方だけ動かすと、道全体が一方向へずれていく
//----------------------------------------------------------
void HjRoad::LimitGrade()
{
	if (m_centerY.size() < 2) { return; }

	for (int pass = 0; pass < RC::GradePasses; ++pass)
	{
		float worst = 0.0f;

		for (size_t i = 1; i < m_centerY.size(); ++i)
		{
			// 刻みが一定でないので、区間ごとに許す量を出す。
			// 一定として扱うと、細かい所で勾配を厳しく見すぎる
			const int  ii  = static_cast<int>(i);
			const float len = std::max(StationS(ii) - StationS(ii - 1), 0.01f);
			const float maxRise = m_maxGrade * len;

			const float d = m_centerY[i] - m_centerY[i - 1];
			const float over = fabsf(d) - maxRise;
			if (over <= 0.0f) { continue; }

			worst = std::max(worst, over);

			// 超えたぶんの半分ずつを、両側から詰める
			const float fix = (d > 0.0f) ? (over * 0.5f) : (-over * 0.5f);
			m_centerY[i]     -= fix;
			m_centerY[i - 1] += fix;
		}

		// 全部収まったら終わり。無駄に回さない
		if (worst <= 0.0f) { break; }
	}
}

//----------------------------------------------------------
// 道の周りの地形を、道の高さへ寄せる
//
// 中心線は地形の凹凸を均してあるので、そのままだと
// 下げた区間で道が地形の中へ潜り、上げた区間で宙に浮く。
//
// 実際の峠も切り土と盛り土で作られているので、これが本来の形。
// 山を削って道を通し、谷を埋めて道を渡す
//----------------------------------------------------------
void HjRoad::DeformTerrain(HjHeightField* field)
{
	if (!field || !field->IsValid()) { return; }
	if (!m_spline.IsValid() || m_centerY.empty()) { return; }

	const float cs = field->GetCellSize();
	const int steps = StepCount();
	const int nx = field->GetSizeX();
	const int nz = field->GetSizeZ();

	// 触ったマス目ごとに、「寄せたい高さ」と「どれだけ寄せるか」を溜める。
	//
	// 1マスを複数の区間が触る。道が折り返す所や、
	// 隣り合う刻みの範囲が重なる所では必ず起きる。
	//
	// その場で書き換えると、最後に触った区間の値が残り、
	// 手前の区間の切り通しが埋め戻される。
	// 全部集めてから、一番強く寄せたい区間の値を採る
	struct Cell
	{
		float target = 0.0f;   // 寄せたい高さ
		float weight = 0.0f;   // どれだけ寄せるか(0〜1)
	};
	std::unordered_map<int, Cell> touched;

	for (int i = 0; i <= steps; ++i)
	{
		const float s = StationS(i);
		const Math::Vector3 center = m_spline.PositionAt(s);

		float ccx = 0.0f, ccz = 0.0f;
		field->WorldToCell(center.x, center.z, ccx, ccz);

		const int reach = static_cast<int>(ceilf(m_deformOuter / cs)) + 1;
		const int cx = static_cast<int>(ccx);
		const int cz = static_cast<int>(ccz);

		for (int dz = -reach; dz <= reach; ++dz)
		{
			for (int dx = -reach; dx <= reach; ++dx)
			{
				const int ix = cx + dx;
				const int iz = cz + dz;
				if (ix < 0 || iz < 0 || ix >= nx || iz >= nz) { continue; }

				const float wx = (ix * cs) - field->GetWorldW() * 0.5f;
				const float wz = (iz * cs) - field->GetWorldD() * 0.5f;

				const float ddx = wx - center.x;
				const float ddz = wz - center.z;
				const float dist = sqrtf(ddx * ddx + ddz * ddz);
				if (dist > m_deformOuter) { continue; }

				// 内側は道の高さへ、外側へ向けて元の地形へ戻す。
				// weight=1 が「道の高さそのもの」
				float w = 1.0f;
				if (dist > m_deformInner)
				{
					const float t = (dist - m_deformInner)
					              / std::max(m_deformOuter - m_deformInner, 0.01f);
					// 端で折れないよう滑らかに
					w = 1.0f - t * t * (3.0f - 2.0f * t);
				}

				// 左右どちら側か。傾きは符号で効く
				const Math::Vector3 rightW = Math::Vector3::Up.Cross(
					m_spline.TangentAt(s));
				const float signedOff =
					(ddx * rightW.x + ddz * rightW.z) >= 0.0f ? dist : -dist;

				// 地形の高さを、路面の断面に合わせる。
				//
				// 平らな床の上に道を置くと、道の縁に段差ができる。
				// 地形のマス目の間は直線で繋がるので、その段差から
				// 地形が顔を出して路面を突き抜ける。
				//
				//   内側 … 路面の形をなぞって、その下へ掘る
				//   縁   … 路面と同じ高さ(段差を作らない)
				//   外側 … 縁の高さから元の地形へ戻す
				float bedY;
				if (dist <= RC::MatchEdge)
				{
					bedY = SurfaceY(i, signedOff) - m_bedDrop;
				}
				else
				{
					// 縁の高さ。傾いていれば左右で違う
					const float edge = (signedOff >= 0.0f)
						? RC::MatchEdge : -RC::MatchEdge;
					bedY = SurfaceY(i, edge);
				}

				const int key = iz * nx + ix;
				auto it = touched.find(key);

				// より強く寄せたい区間が勝つ。
				// 中心線に近い区間の言い分を採ることになる
				if (it == touched.end())
				{
					Cell c;
					c.target = bedY;
					c.weight = w;
					touched[key] = c;
				}
				else
				{
					if (w > it->second.weight)
					{
						it->second.target = bedY;
						it->second.weight = w;
					}
				}
			}
		}
	}

	// 集めたものを地形へ書く
	m_hasDirtyArea = false;

	for (const auto& kv : touched)
	{
		const int ix = kv.first % nx;
		const int iz = kv.first / nx;

		const float orig = field->HeightAtCell(ix, iz);

		// 削る前の高さを控える。
		// 次に動かしたとき、ここから戻す。
		// 控えずに削り続けると、動かすたびに掘り進んでいく
		m_deformedOriginal.emplace(kv.first, orig);

		// 組み直す範囲を広げる
		const float wx = (ix * cs) - field->GetWorldW() * 0.5f;
		const float wz = (iz * cs) - field->GetWorldD() * 0.5f;
		if (!m_hasDirtyArea)
		{
			m_dirtyMin = Math::Vector3(wx, 0.0f, wz);
			m_dirtyMax = m_dirtyMin;
			m_hasDirtyArea = true;
		}
		else
		{
			m_dirtyMin.x = std::min(m_dirtyMin.x, wx);
			m_dirtyMin.z = std::min(m_dirtyMin.z, wz);
			m_dirtyMax.x = std::max(m_dirtyMax.x, wx);
			m_dirtyMax.z = std::max(m_dirtyMax.z, wz);
		}

		// 寄せた結果
		const float w = kv.second.weight;
		float out = orig + (kv.second.target - orig) * w;

		// 削る・盛るの上限。
		//
		// ■ 切り替えにしてはいけない
		// 「道の真下だけ上限なし」にすると、道の縁で上限が切り替わり、
		// 隣り合うマス目の間に十数メートルの段差ができる。
		// それが棘になって地形から突き出す。
		//
		// 上限そのものを、寄せる強さに応じて緩める。
		// 道に近いほど際限なく掘れて、離れるほど元の値へ戻る。
		// 途中に切れ目が無いので段差ができない。
		const float slack = std::max(1.0f - w, 0.02f);
		const float maxCut  = m_cutMax  / slack;
		const float maxFill = m_fillMax / slack;

		out = std::clamp(out, orig - maxCut, orig + maxFill);

		field->SetHeightAtCell(ix, iz, out);
	}
}

//----------------------------------------------------------
// 断面の形
//
// 中心が少し高く、路肩で落ちる。
// 平らな板にすると、路面に見えない
//----------------------------------------------------------
float HjRoad::CrossHeight(float offset)
{
	const float a = fabsf(offset);

	// 路面の中。中央が高い
	if (a <= RC::HalfWidth)
	{
		const float t = a / std::max(RC::HalfWidth, 0.01f);
		return RC::Crown * (1.0f - t * t);
	}

	// 路肩。外へ向かって落ちる
	const float t = std::clamp((a - RC::HalfWidth)
	                           / std::max(RC::ShoulderWidth, 0.01f), 0.0f, 1.0f);
	return -RC::ShoulderDrop * t;
}

//----------------------------------------------------------
// メッシュを組む
//----------------------------------------------------------
void HjRoad::BuildMesh()
{
	m_spMesh = nullptr;
	if (!m_spline.IsValid() || m_centerY.empty()) { return; }

	const int steps = StepCount();
	const int cols  = RC::WidthSegments * 2 + 1;   // 幅方向の頂点数

	const float outer = RC::HalfWidth + RC::ShoulderWidth;

	std::vector<KdMeshVertex> verts;
	verts.reserve(static_cast<size_t>(steps + 1) * cols);

	for (int i = 0; i <= steps; ++i)
	{
		const float s = StationS(i);

		const Math::Vector3 center = m_spline.PositionAt(s);
		const Math::Vector3 fwd    = m_spline.TangentAt(s);
		const Math::Vector3 right  = Math::Vector3::Up.Cross(fwd);

		// 曲がりの内側はどちらか。
		//
		// 進む向きの変化から出す。ヘアピンでは内側だけを絞る。
		// 両側を絞ると、外側まで細くなって道が痩せる
		const float curv = m_spline.CurvatureAt(s);
		const float lim  = InnerLimit(i);

		// 内側の符号。曲がっていく方向が内側
		float innerSign = 0.0f;
		if (curv >= RC::CurveIgnore)
		{
			const Math::Vector3 t0 = m_spline.TangentAt(std::max(s - 2.0f, 0.0f));
			const Math::Vector3 t1 = m_spline.TangentAt(
				std::min(s + 2.0f, m_spline.TotalLength()));

			// 右へ曲がっていれば右が内側
			innerSign = (Math::Vector3::Up.Cross(t0).Dot(t1) >= 0.0f) ? 1.0f : -1.0f;
		}

		for (int c = 0; c < cols; ++c)
		{
			// -outer 〜 +outer
			const float t   = static_cast<float>(c) / (cols - 1);
			float off = (t * 2.0f - 1.0f) * outer;

			// 内側だけ、半径より内へ収める
			if (innerSign != 0.0f && off * innerSign > 0.0f)
			{
				off = std::clamp(off, -lim, lim);
			}

			const Math::Vector3 p(
				center.x + right.x * off,
				SurfaceY(i, off),
				center.z + right.z * off);

			KdMeshVertex v;
			v.Pos     = p;
			v.Normal  = Math::Vector3::Up;   // あとで面から求め直す
			v.Tangent = right;
			v.Color   = 0xFFFFFFFF;

			// 進んだ距離でUVを送る。素材の繰り返しが道に沿う
			v.UV = Math::Vector2(t, s / std::max(RC::UvLength, 0.01f));

			verts.push_back(v);
		}
	}

	std::vector<KdMeshFace> faces;
	faces.reserve(static_cast<size_t>(steps) * (cols - 1) * 2);

	for (int i = 0; i < steps; ++i)
	{
		for (int c = 0; c < cols - 1; ++c)
		{
			const UINT i0 = static_cast<UINT>(i * cols + c);
			const UINT i1 = i0 + 1;
			const UINT i2 = i0 + cols;
			const UINT i3 = i2 + 1;

			faces.push_back({ { i0, i2, i1 } });
			faces.push_back({ { i1, i2, i3 } });
		}
	}

	// 法線を面から求め直す。
	// 上向きのままだと、坂で光の当たり方がおかしくなる
	std::vector<Math::Vector3> normals(verts.size(), Math::Vector3::Zero);
	for (const auto& f : faces)
	{
		const Math::Vector3& a = verts[f.Idx[0]].Pos;
		const Math::Vector3& b = verts[f.Idx[1]].Pos;
		const Math::Vector3& c = verts[f.Idx[2]].Pos;

		Math::Vector3 n = (b - a).Cross(c - a);
		normals[f.Idx[0]] += n;
		normals[f.Idx[1]] += n;
		normals[f.Idx[2]] += n;
	}
	for (size_t i = 0; i < verts.size(); ++i)
	{
		if (normals[i].LengthSquared() < 1e-8f) { continue; }
		normals[i].Normalize();
		verts[i].Normal = normals[i];
	}

	std::vector<KdMeshSubset> subsets(1);
	subsets[0].MaterialNo = 0;
	subsets[0].FaceStart  = 0;
	subsets[0].FaceCount  = static_cast<UINT>(faces.size());

	m_spMesh = std::make_shared<KdMesh>();
	if (!m_spMesh->Create(verts, faces, subsets, false)) { m_spMesh = nullptr; }
}

//----------------------------------------------------------
// 削ったあとの地形を均す
//
// 寄せる強さは中心線からの距離で決まるが、その距離は
// 刻みごとの点までの距離で測っている。点と点の間では
// 距離がわずかに大きくなるので、寄せる強さが刻みの周期で上下する。
//
// 道に沿って細かい波が乗るのはこれが理由。削ってから均せば消える。
// 元の地形まで均すと山が丸くなるので、触った所だけにする
//----------------------------------------------------------
void HjRoad::SmoothDeformed(HjHeightField* field)
{
	if (!field || !field->IsValid()) { return; }
	if (m_deformedOriginal.empty()) { return; }

	const int nx = field->GetSizeX();
	const int nz = field->GetSizeZ();

	// 均す対象。削ったマス目のうち、中心寄りだけ。
	//
	// 外側まで均すと、削っていない地形との境目が動いて
	// そこに新しい段差ができる
	std::vector<int> keys;
	keys.reserve(m_deformedOriginal.size());
	for (const auto& kv : m_deformedOriginal) { keys.push_back(kv.first); }

	for (int pass = 0; pass < RC::DeformSmoothPasses; ++pass)
	{
		// 読みと書きを分ける。
		// その場で書き換えると、先に均した値をもとに次を均すことになり、
		// 走査の向きで結果が変わる
		std::vector<float> next;
		next.reserve(keys.size());

		for (const int key : keys)
		{
			const int ix = key % nx;
			const int iz = key / nx;

			// 上下左右と自分。中央を重くして、形を保ちながら波だけ取る
			float acc = field->HeightAtCell(ix, iz) * 4.0f;
			float cnt = 4.0f;

			for (int d = 0; d < 4; ++d)
			{
				const int jx = ix + ((d == 0) ? -1 : (d == 1) ? 1 : 0);
				const int jz = iz + ((d == 2) ? -1 : (d == 3) ? 1 : 0);
				if (jx < 0 || jz < 0 || jx >= nx || jz >= nz) { continue; }

				acc += field->HeightAtCell(jx, jz);
				cnt += 1.0f;
			}
			next.push_back(acc / cnt);
		}

		for (size_t i = 0; i < keys.size(); ++i)
		{
			field->SetHeightAtCell(keys[i] % nx, keys[i] / nx, next[i]);
		}
	}
}

//----------------------------------------------------------
// 地形へ溶ける裾
//
// 地形は格子(1m四方)で、道は曲がった帯。
// 格子は道の縁に沿えないので、地形だけで境目を作ると階段状になる。
//
// 道から生やせば、境目がスプラインの縁になる。曲がりに沿うので滑らか
//----------------------------------------------------------
void HjRoad::BuildApron(const HjHeightField* field)
{
	m_spApron = nullptr;
	if (!m_spline.IsValid() || m_centerY.empty()) { return; }

	const int steps = StepCount();
	// 幅方向の分割。
	// 粗いと、地形の起伏を拾いきれずに外端が浮く
	const int segs  = RC::ApronSegments;

	// 左右2枚。中央(路面)を挟むので、1枚のメッシュにはできない
	const int cols = (segs + 1) * 2;

	const float inner = RC::HalfWidth + RC::ShoulderWidth;
	const float outer = inner + RC::ApronWidth;

	std::vector<KdMeshVertex> verts;
	verts.reserve(static_cast<size_t>(steps + 1) * cols);

	for (int i = 0; i <= steps; ++i)
	{
		const float s = StationS(i);

		const Math::Vector3 center = m_spline.PositionAt(s);
		const Math::Vector3 fwd    = m_spline.TangentAt(s);
		const Math::Vector3 right  = Math::Vector3::Up.Cross(fwd);

		// 路肩の外端の高さ。ここから始めれば道と段差ができない
		// 縁の高さは左右で違う(傾いているので)。側ごとに取る

		for (int side = 0; side < 2; ++side)
		{
			const float sign = (side == 0) ? -1.0f : 1.0f;

			// この側の路肩の外端。ここから始めれば道と段差ができない
			const float edgeY = SurfaceY(i, inner * sign);

			for (int c = 0; c <= segs; ++c)
			{
				const float t   = static_cast<float>(c) / segs;
				const float off = inner + RC::ApronWidth * t;

				const float wx = center.x + right.x * off * sign;
				const float wz = center.z + right.z * off * sign;

				// 内側は路面の縁、外側は地形。間は滑らかに繋ぐ
				float groundY = edgeY;
				if (field && field->IsValid())
				{
					const float g = field->HeightAt(wx, wz);
					if (g > TerrainConst::OutsideHeight) { groundY = g; }
				}

				// 端で折れないよう滑らかに
				const float k = t * t * (3.0f - 2.0f * t);
				float y = edgeY + (groundY - edgeY) * k;

				// 外端はわずかに沈める。
				// ちょうどに合わせると、どちらが手前か決まらずちらつく
				y -= RC::ApronSink * k;

				KdMeshVertex v;
				v.Pos     = Math::Vector3(wx, y, wz);
				v.Normal  = Math::Vector3::Up;   // あとで面から求め直す
				v.Tangent = right;
				v.Color   = 0xFFFFFFFF;
				v.UV = Math::Vector2(t, s / std::max(RC::UvLength, 0.01f));

				verts.push_back(v);
			}
		}
	}

	std::vector<KdMeshFace> faces;
	faces.reserve(static_cast<size_t>(steps) * segs * 4);

	for (int i = 0; i < steps; ++i)
	{
		for (int side = 0; side < 2; ++side)
		{
			const int base = i * cols + side * (segs + 1);

			for (int c = 0; c < segs; ++c)
			{
				const UINT i0 = static_cast<UINT>(base + c);
				const UINT i1 = i0 + 1;
				const UINT i2 = static_cast<UINT>(base + cols + c);
				const UINT i3 = i2 + 1;

				// 左右で表裏が入れ替わる。
				// 同じ順番にすると、片側が裏になって消える
				if (side == 0)
				{
					faces.push_back({ { i0, i1, i2 } });
					faces.push_back({ { i1, i3, i2 } });
				}
				else
				{
					faces.push_back({ { i0, i2, i1 } });
					faces.push_back({ { i1, i2, i3 } });
				}
			}
		}
	}

	if (faces.empty()) { return; }

	// 法線を面から求め直す
	std::vector<Math::Vector3> normals(verts.size(), Math::Vector3::Zero);
	for (const auto& f : faces)
	{
		const Math::Vector3& a = verts[f.Idx[0]].Pos;
		const Math::Vector3& b = verts[f.Idx[1]].Pos;
		const Math::Vector3& c = verts[f.Idx[2]].Pos;

		const Math::Vector3 n = (b - a).Cross(c - a);
		normals[f.Idx[0]] += n;
		normals[f.Idx[1]] += n;
		normals[f.Idx[2]] += n;
	}
	for (size_t i = 0; i < verts.size(); ++i)
	{
		if (normals[i].LengthSquared() < 1e-8f) { continue; }
		normals[i].Normalize();

		// 裏を向いていたら返す。左右で面の向きが違うので、
		// そのままだと片側が暗くなる
		if (normals[i].y < 0.0f) { normals[i] = -normals[i]; }
		verts[i].Normal = normals[i];
	}

	std::vector<KdMeshSubset> subsets(1);
	subsets[0].MaterialNo = 0;
	subsets[0].FaceStart  = 0;
	subsets[0].FaceCount  = static_cast<UINT>(faces.size());

	m_spApron = std::make_shared<KdMesh>();
	if (!m_spApron->Create(verts, faces, subsets, false)) { m_spApron = nullptr; }
}

//----------------------------------------------------------
// この位置が道の上か
//
// メッシュを見ない。スプラインへ投影して、断面を評価するだけ。
// 面を1枚ずつ調べるより速く、しかもカントが厳密に出る
//----------------------------------------------------------
bool HjRoad::SampleAt(float x, float z, float& outHeight, Math::Vector3& outNormal) const
{
	if (!m_spline.IsValid() || m_centerY.empty()) { return false; }

	float s = 0.0f, off = 0.0f;
	if (!m_spline.Project(Math::Vector3(x, 0.0f, z), s, off)) { return false; }

	// 道の外
	const float outer = RC::HalfWidth + RC::ShoulderWidth + RC::EdgeMargin;
	if (fabsf(off) > outer) { return false; }

	// 中心線の高さ。刻みの間は繋ぐ
	// 何番目の刻みか。刻みが一定でないので、割り算では出せない
	int   i0 = 0;
	float ft = 0.0f;
	FindStation(s, i0, ft);

	i0 = std::clamp(i0, 0, static_cast<int>(m_centerY.size()) - 1);
	const int i1 = std::min(i0 + 1, static_cast<int>(m_centerY.size()) - 1);

	// 刻みの間は繋ぐ。傾きも一緒に混ざる
	outHeight = SurfaceY(i0, off) + (SurfaceY(i1, off) - SurfaceY(i0, off)) * ft;

	// 法線。
	// 前後の高さの差で坂の傾きを、断面の差で横の傾きを出す。
	// 高さマップに焼くと潰れるカントが、ここでは残る
	// 前後の刻みの長さ。一定でないので実際の距離を使う
	const float ds = std::max(StationS(i1 + 1) - StationS(i0 - 1), 0.02f) * 0.5f;
	const float hb = (i0 > 0) ? m_centerY[i0 - 1] : m_centerY[i0];
	const float hf = (i1 + 1 < static_cast<int>(m_centerY.size()))
		? m_centerY[i1 + 1] : m_centerY[i1];

	const float slopeAlong = (hf - hb) / std::max(ds * 2.0f, 0.01f);

	const float d = 0.25f;
	// 断面の形に、左右の傾きを足す。
	// 足さないと、傾いた路面で法線が上を向いたままになり、
	// 車が坂を感じない
	const float slopeAcross =
		(SurfaceY(i0, off + d) - SurfaceY(i0, off - d)) / (d * 2.0f);

	const Math::Vector3 fwd   = m_spline.TangentAt(s);
	const Math::Vector3 right = Math::Vector3::Up.Cross(fwd);

	// 傾きの分だけ、上向きから倒す
	Math::Vector3 n = Math::Vector3::Up
	                - fwd   * slopeAlong
	                - right * slopeAcross;
	n.Normalize();
	outNormal = n;

	return true;
}

//----------------------------------------------------------
void HjRoad::DrawLit()
{
	if (!m_spMesh) { return; }

	HjScopedTimer _t(U8(" └ 道の描画"));

	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 裾を先に描く。路面を先に描くと、重なった所で裾が上に乗る
	if (m_spApron)
	{
		shader.DrawMesh(m_spApron.get(), Math::Matrix::Identity,
		                m_apronMaterials, kWhiteColor, Math::Vector3::Zero);
	}

	shader.DrawMesh(m_spMesh.get(), Math::Matrix::Identity,
	                m_materials, kWhiteColor, Math::Vector3::Zero);
}

void HjRoad::GenerateDepthMapFromLight()
{
	if (!m_spMesh) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}

//----------------------------------------------------------
Math::Vector3 HjRoad::GetStartPos() const
{
	if (!m_spline.IsValid() || m_centerY.empty()) { return Math::Vector3::Zero; }

	const Math::Vector3 p = m_spline.PositionAt(0.0f);
	return Math::Vector3(p.x, SurfaceY(0, 0.0f), p.z);
}

float HjRoad::GetStartYaw() const
{
	if (!m_spline.IsValid()) { return 0.0f; }

	const Math::Vector3 fwd = m_spline.TangentAt(0.0f);
	return atan2f(fwd.x, fwd.z);
}

//----------------------------------------------------------
// 編集
//----------------------------------------------------------
Math::Vector3 HjRoad::GetPoint(int i) const
{
	const auto& pts = m_spline.GetPoints();
	if (i < 0 || i >= static_cast<int>(pts.size())) { return Math::Vector3::Zero; }
	return pts[i];
}

void HjRoad::MovePoint(int i, const Math::Vector3& pos)
{
	if (m_spline.MovePoint(i, pos)) { Rebuild(); }
}

void HjRoad::InsertAfter(int i)
{
	if (m_spline.InsertAfter(i)) { Rebuild(); }
}

void HjRoad::AppendPoint(const Math::Vector3& pos)
{
	m_spline.AppendPoint(pos);
	Rebuild();
}

void HjRoad::ErasePoint(int i)
{
	if (m_spline.ErasePoint(i)) { Rebuild(); }
}

bool HjRoad::SavePath() const
{
	return m_spline.SaveToFile(RC::PathFile);
}

//----------------------------------------------------------
// 道と地形を作り直す
//
// 点を動かすたびに呼ぶ。
// 地形は控えた形から削り直す。削った跡の上へ削ると掘り進んでいく
//----------------------------------------------------------
void HjRoad::Rebuild()
{
	// 前に削った所だけ戻す。
	//
	// 地形を丸ごと戻すと、動かすたびに数百万マスを舐めることになる。
	// 触った所だけなら、道の周りの数万マスで済む
	if (m_pField)
	{
		const int nx = m_pField->GetSizeX();
		for (const auto& kv : m_deformedOriginal)
		{
			m_pField->SetHeightAtCell(kv.first % nx, kv.first / nx, kv.second);
		}
	}
	m_deformedOriginal.clear();

	ResolveHeights(m_pField);
	DeformTerrain(m_pField);
	SmoothDeformed(m_pField);
	BuildMesh();
	BuildApron(m_pField);

	// 地形のメッシュも組み直す必要がある
	m_dirty = true;
}

//----------------------------------------------------------
// 触りながら決める値
//
// ビルドし直さないと変えられないのでは道具にならない。
// 動かした結果をその場で見られるようにする
//----------------------------------------------------------
void HjRoad::DrawEditImGui()
{
	ImGui::TextUnformatted(U8("道"));
	ImGui::Text(U8("全長 %.0f m / 制御点 %d 個"),
	            m_spline.TotalLength(), m_spline.PointCount());

	if (ImGui::Button(U8("制御点を書き出す")))
	{
		SavePath();
	}
	ImGui::SameLine();
	ImGui::TextDisabled(RC::PathFile);

	ImGui::SeparatorText(U8("縦断"));

	// 触った値をその場へ反映する。
	// 押してから反映では、行き過ぎたか戻しすぎたかが分からない
	bool changed = false;

	changed |= ImGui::DragFloat(U8("道の持ち上げ"), &m_lift, 0.05f, -30.0f, 60.0f);
	ImGui::SetItemTooltip(U8("道ごと上下する。上げるほど盛り土が増え、切り通しが減る"));

	changed |= ImGui::DragFloat(U8("勾配の上限"), &m_maxGrade, 0.002f, 0.01f, 0.40f);
	ImGui::SetItemTooltip(U8("1進んだときに上下できる量。0.12で約7度"));

	ImGui::SeparatorText(U8("横断(左右の傾き)"));

	changed |= ImGui::DragFloat(U8("地形への追従"), &m_bankFollow, 0.01f, 0.0f, 1.5f);
	ImGui::SetItemTooltip(U8("0=常に水平 / 1=地形の傾きをそのまま拾う"));

	changed |= ImGui::DragFloat(U8("傾きの上限"), &m_maxBank, 0.005f, 0.0f, 0.60f);
	ImGui::SetItemTooltip(U8("高さ÷距離。0.18で約10度"));

	ImGui::SeparatorText(U8("地形の削り"));

	changed |= ImGui::DragFloat(U8("そのまま合わせる幅"), &m_deformInner, 0.1f, 1.0f, 40.0f);
	changed |= ImGui::DragFloat(U8("戻していく幅"),       &m_deformOuter, 0.5f, 2.0f, 120.0f);
	changed |= ImGui::DragFloat(U8("削る深さの上限"),     &m_cutMax,      0.5f, 1.0f, 120.0f);
	changed |= ImGui::DragFloat(U8("盛る高さの上限"),     &m_fillMax,     0.5f, 1.0f, 120.0f);

	ImGui::SeparatorText(U8("路面の下"));

	changed |= ImGui::DragFloat(U8("路面より下に敷く"), &m_bedDrop, 0.01f, 0.0f, 2.0f);
	ImGui::SetItemTooltip(U8("見えない所を掘る深さ。深いほど地形が路面を突き抜けにくい"));

	if (changed) { Rebuild(); }
}

//----------------------------------------------------------
// 制御点を画面に出す位置
//
// 制御点の高さは「持ち上げ」なので、そのまま置くと
// 道から離れた所に印が出る。道の実際の高さへ乗せる
//----------------------------------------------------------
Math::Vector3 HjRoad::GetPointDisplayPos(int i) const
{
	const Math::Vector3 p = GetPoint(i);
	if (m_centerY.empty()) { return p; }

	// 制御点はスプラインの節なので、道のりは決まっている。
	// 一番近い所を探すと、ヘアピンで反対側の脚に吸い寄せられる
	const float s = m_spline.StationOfPoint(i);

	// 何番目の刻みか。刻みが一定でないので、割り算では出せない
	int   i0 = 0;
	float t  = 0.0f;
	FindStation(s, i0, t);

	const int last = static_cast<int>(m_centerY.size()) - 1;
	i0 = std::clamp(i0, 0, last);
	const int i1 = std::min(i0 + 1, last);

	// 刻みの間を繋ぐ。
	//
	// 手前の刻みの高さをそのまま使うと、直線では刻みが3mまで空くので、
	// 坂の途中で印が路面から浮いたり沈んだりする
	const float y = SurfaceY(i0, 0.0f) * (1.0f - t) + SurfaceY(i1, 0.0f) * t;

	return Math::Vector3(p.x, y, p.z);
}
