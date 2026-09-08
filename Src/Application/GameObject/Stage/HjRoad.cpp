#include "HjRoad.h"
#include "../../Const/SplatConst.h"
#include "HjTerrain.h"

#include "HjHeightField.h"
#include "../../Util/HjProfiler.h"

#include <unordered_map>
#include <fstream>

namespace RC = RoadConst;

namespace
{
	//======================================================
	// 断面を並べて作る面の、折り返した所を直す
	//
	// ■ 何が起きるか
	// 曲がりの半径より外へ出した点は、前後の断面で追い越し合う。
	// 半径5mのコーナーで中心から6m外へ出せば、その点は
	// 曲がりの中心を通り越して、進む向きと逆へ動く。
	// 面が裏返り、実測で180度の折れになっていた。
	//
	// ■ 幅を絞ってはいけない
	// 半径から幅を決めて絞る手も試したが、道の幅が変わってしまう。
	// 走る道の幅は設計値であって、地形の都合で細くしてよいものではない。
	//
	// ■ やること
	// 飛び出した頂点を落とす。面が切れて穴が開くので、
	// 残った縁どうしを繋いで塞ぐ
	//======================================================

	//--------------------------------------------------
	// 折り返している頂点を探す
	//
	// その点の前後を結んだ向きが、中心線の進む向きと逆を向く。
	// 内積が負なら追い越している。
	//
	// 曲率を見積もる必要がない。見積もりは窓の取り方で値が変わり、
	// 実測では本当の半径2.6mを5.6mと誤って見ていた
	//--------------------------------------------------
	void MarkFolded(const std::vector<KdMeshVertex>& verts,
	                const std::vector<Math::Vector3>& fwd,
	                const std::vector<float>& advance,
	                int rows, int stride, int colBegin, int colCount,
	                std::vector<bool>& valid)
	{
		for (int c = 0; c < colCount; ++c)
		{
			for (int r = 0; r < rows; ++r)
			{
				const int idx = r * stride + colBegin + c;

				const int a = (r > 0)        ? (idx - stride) : idx;
				const int b = (r < rows - 1) ? (idx + stride) : idx;
				if (a == b) { continue; }

				const Math::Vector3 d = verts[b].Pos - verts[a].Pos;

				// 中心線に比べてどれだけ進んだか。
				//
				// 「逆を向いたら」だけで見ると、進み方が中心線の1%といった
				// ほぼ潰れた面が残る。細長い棘になって法線も出ない
				if (d.Dot(fwd[r]) <= advance[r] * RC::MinAdvance)
				{
					valid[idx] = false;
				}
			}
		}
	}

	//--------------------------------------------------
	// 中心線までの本当の距離を引く
	//
	// ■ 何のために要るか
	// 断面は曲がりの中心へ向かって集まる。ヘアピンでは、
	// 入口の断面と出口の断面が、遠く離れているのに交差する。
	//
	// 前後の断面だけを見る判定では、その遠い交差を捉えられない。
	// 実測で、1307個の頂点が見逃されていた。
	//
	// ■ 距離で分かる理由
	// 交差していない点は、自分の断面の足が最短になる。
	// そのとき中心線までの距離は、中心からのずれにちょうど等しい。
	//
	// 別の区間のほうが近ければ、そこは他人の場所へ入り込んでいる。
	// 曲がりの中心を通り越した点も、これで一緒に捉えられる。
	//
	// ■ 総当たりにしない
	// 2150区間 x 66000頂点 で1.4億回になる。
	// 区間をマス目へ配って、近くのマス目だけを見る
	//--------------------------------------------------
	class CenterGrid
	{
	public:
		// cell は道の外端の幅。これだけあれば、探すのは前後2マスで足りる
		void Build(const std::vector<Math::Vector3>& pts, float cell)
		{
			m_pts  = pts;
			m_cell = std::max(cell, 1.0f);
			m_cells.clear();

			if (m_pts.size() < 2) { return; }

			m_minX = m_minZ = 1e18f;
			float maxX = -1e18f, maxZ = -1e18f;
			for (const auto& p : m_pts)
			{
				m_minX = std::min(m_minX, p.x);  maxX = std::max(maxX, p.x);
				m_minZ = std::min(m_minZ, p.z);  maxZ = std::max(maxZ, p.z);
			}

			m_nx = static_cast<int>((maxX - m_minX) / m_cell) + 3;
			m_nz = static_cast<int>((maxZ - m_minZ) / m_cell) + 3;
			m_cells.resize(static_cast<size_t>(m_nx) * m_nz);

			// 区間を、始点の入るマス目へ入れる。
			// 区間は刻みの長さ(最大3m)しかないので、
			// 前後2マス(23m)を探せば取りこぼさない
			for (int i = 0; i + 1 < static_cast<int>(m_pts.size()); ++i)
			{
				const int gx = CellX(m_pts[i].x);
				const int gz = CellZ(m_pts[i].z);
				m_cells[static_cast<size_t>(gz) * m_nx + gx].push_back(i);
			}
		}

		// 真上から見た距離。高さは見ない。
		// 見ると、坂で下を通っている道に吸い寄せられる
		float Distance(float x, float z) const
		{
			if (m_cells.empty()) { return 1e18f; }

			const int gx = CellX(x);
			const int gz = CellZ(z);

			float best = 1e18f;

			for (int dz = -2; dz <= 2; ++dz)
			{
				for (int dx = -2; dx <= 2; ++dx)
				{
					const int cx = gx + dx;
					const int cz = gz + dz;
					if (cx < 0 || cz < 0 || cx >= m_nx || cz >= m_nz) { continue; }

					for (int i : m_cells[static_cast<size_t>(cz) * m_nx + cx])
					{
						best = std::min(best, SegDist(x, z, i));
					}
				}
			}
			return best;
		}

	private:
		int CellX(float x) const
		{
			return std::clamp(static_cast<int>((x - m_minX) / m_cell), 0, m_nx - 1);
		}
		int CellZ(float z) const
		{
			return std::clamp(static_cast<int>((z - m_minZ) / m_cell), 0, m_nz - 1);
		}

		float SegDist(float x, float z, int i) const
		{
			const float ax = m_pts[i].x,     az = m_pts[i].z;
			const float vx = m_pts[i + 1].x - ax, vz = m_pts[i + 1].z - az;

			const float len2 = vx * vx + vz * vz;

			float t = 0.0f;
			if (len2 > 1e-9f)
			{
				t = std::clamp(((x - ax) * vx + (z - az) * vz) / len2, 0.0f, 1.0f);
			}

			const float dx = x - (ax + vx * t);
			const float dz = z - (az + vz * t);
			return sqrtf(dx * dx + dz * dz);
		}

		std::vector<Math::Vector3>     m_pts;
		std::vector<std::vector<int>>  m_cells;

		float m_cell = 1.0f;
		float m_minX = 0.0f;
		float m_minZ = 0.0f;
		int   m_nx = 0;
		int   m_nz = 0;
	};

	//--------------------------------------------------
	// 断面どうしが交差している頂点を探す
	//
	// offs は頂点ごとの「中心線からのずれ」。
	// 裾の幅が刻みごとに違うので、列だけでは決まらない
	//
	// 交差していない点は、中心線までの距離が
	// 中心からのずれにちょうど等しい。
	// 近ければ、他人の場所へ入り込んでいる
	//--------------------------------------------------
	void MarkCrossed(const std::vector<KdMeshVertex>& verts,
	                 const std::vector<float>& offs,
	                 const CenterGrid& grid,
	                 int rows, int cols,
	                 std::vector<bool>& valid)
	{
		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < cols; ++c)
			{
				const size_t idx = static_cast<size_t>(r) * cols + c;

				// 裾の幅は刻みごとに違うので、頂点ごとに持っている
				const float o = offs[idx];

				// 中心線のすぐそばは、刻みの粗さで誤判定になる
				if (o < RC::CrossIgnore) { continue; }

				if (!valid[idx]) { continue; }

				const float d = grid.Distance(verts[idx].Pos.x, verts[idx].Pos.z);

				if (d < o * (1.0f - RC::CrossTol)) { valid[idx] = false; }
			}
		}
	}

	//--------------------------------------------------
	// 中心線が、前後の断面の間で進む距離
	//
	// 幅方向の点がどれだけ進んだかを、これと比べる。
	// 比べる相手が無いと「潰れている」が決められない
	//--------------------------------------------------
	std::vector<float> CenterAdvance(const std::vector<Math::Vector3>& centers)
	{
		const int rows = static_cast<int>(centers.size());
		std::vector<float> out(std::max(rows, 0), 1.0f);

		for (int r = 0; r < rows; ++r)
		{
			const int a = (r > 0)        ? (r - 1) : r;
			const int b = (r < rows - 1) ? (r + 1) : r;
			out[r] = std::max((centers[b] - centers[a]).Length(), 1e-4f);
		}
		return out;
	}

	//--------------------------------------------------
	// 残った頂点で面を張り、落とした跡を塞ぐ
	//
	// 折り返しは幅の端から順に起きるので、生きている列は
	// 必ず続きになる。行ごとに「どこからどこまで生きているか」で足りる。
	//
	// 隣の行と生きている範囲が違うと、そこが穴になる。
	// はみ出したぶんを、相手の行の端へ扇状に繋いで塞ぐ
	//--------------------------------------------------
	void StripFaces(const std::vector<bool>& valid,
	                int rows, int stride, int colBegin, int colCount,
	                int seed, bool flip, std::vector<KdMeshFace>& out)
	{
		auto id = [&](int r, int c)
		{
			return static_cast<UINT>(r * stride + colBegin + c);
		};

		// 行ごとに、生きている列の範囲。
		//
		// 中心線に一番近い列から外へ広げる。
		// 両端から詰めると、途中に1つ死んだ列があったとき
		// それを跨いで面を張ってしまう
		std::vector<int> lo(rows, 1), hi(rows, 0);
		for (int r = 0; r < rows; ++r)
		{
			if (!valid[id(r, seed)]) { continue; }

			int a = seed, b = seed;
			while (a - 1 >= 0        && valid[id(r, a - 1)]) { --a; }
			while (b + 1 < colCount  && valid[id(r, b + 1)]) { ++b; }
			lo[r] = a;
			hi[r] = b;
		}

		auto tri = [&](UINT x, UINT y, UINT z)
		{
			// 潰れた三角は法線が出ないので入れない
			if (x == y || y == z || z == x) { return; }

			if (flip) { out.push_back({ { x, z, y } }); }
			else      { out.push_back({ { x, y, z } }); }
		};

		for (int r = 0; r + 1 < rows; ++r)
		{
			if (lo[r] > hi[r] || lo[r + 1] > hi[r + 1]) { continue; }

			// 両方の行で生きている範囲
			const int a = std::max(lo[r], lo[r + 1]);
			const int b = std::min(hi[r], hi[r + 1]);
			if (a > b) { continue; }

			for (int c = a; c < b; ++c)
			{
				tri(id(r, c),     id(r + 1, c), id(r, c + 1));
				tri(id(r, c + 1), id(r + 1, c), id(r + 1, c + 1));
			}

			//===== 落とした跡を塞ぐ =====
			// 片方の行にだけ残っている列を、相手の行の端へ繋ぐ
			for (int c = b; c < hi[r]; ++c)
			{
				tri(id(r, c), id(r + 1, b), id(r, c + 1));
			}
			for (int c = b; c < hi[r + 1]; ++c)
			{
				tri(id(r, b), id(r + 1, c), id(r + 1, c + 1));
			}
			for (int c = lo[r]; c < a; ++c)
			{
				tri(id(r, c), id(r + 1, a), id(r, c + 1));
			}
			for (int c = lo[r + 1]; c < a; ++c)
			{
				tri(id(r, a), id(r + 1, c), id(r + 1, c + 1));
			}
		}
	}
}

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

	// 色は頂点に持たせる。
	//
	// 路面と裾で色を変えたいが、材質を分けるとメッシュを1枚にできない。
	// 分けると境目の穴埋めが相手側へ届かず、穴が残る
	m_materials[0].m_baseColorRate = Math::Vector4::One;

	// 地形を控えておく。
	//
	// 道を動かすたびに削り直すが、削った跡の上へさらに削ると
	// 掘り進んでいく。毎回この形から始める
	// 地形は借りたまま持っておく。
	//
	// 控えは丸ごと取らない。削ったマス目だけを覚えておけば、
	// 戻すのも道の周りの数万マスで済む
	m_pField = field;

	// 作る手順は編集中と同じ。
	// 2箇所に書くと、片方にだけ工程を足して食い違う
	Rebuild();
}

//----------------------------------------------------------
// 中心線の高さを地形へ沿わせる
//
// スプラインの制御点は真上から見た線なので、高さを持っていない。
// 地形の高さをそのまま拾うと、細かい凹凸で道が波打つので均す
//----------------------------------------------------------
// 道の高さを決める
//
// ■ 高さは制御点が持つ
// 地形から下敷きを作っていたが、それだと地形を彫るたびに
// 道が上下する。道の下を盛っただけで道が持ち上がっていた。
//
// 点を置いた所の高さがそのまま道の高さ。
// 間はスプラインで繋ぐので、位置と同じ曲線になる。
//
// 地形を彫っても道は動かない。逆に、道を動かしたければ
// 制御点の高さを触る。どちらが原因かがはっきりする。
//
// ■ 地形から取り込みたいときは
// DrawEditImGui の「地形から高さを取り込む」で、
// そのときの地形の高さを制御点へ書き込む。一度きりの操作
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

	// 制御点の高さを、位置と同じ曲線で繋ぐ。
	//
	// 直線で繋ぐと、1点上げただけで三角のテントになる。
	// 位置は滑らかなのに高さだけ角ばる
	for (int i = 0; i <= steps; ++i)
	{
		m_centerY[i] = m_spline.LiftAt(StationS(i));
	}

	// 勾配の上限に収める。
	//
	// 点の高さは尊重するが、隣り合う点が離れすぎていると
	// 崖のような坂ができて走れない。そこだけ寄せる
	LimitGrade();

	// 道ごと持ち上げる。
	//
	// 地形の削りも、路面も、当たり判定も、全部この値を見るので、
	// 上げても道と地形の関係は崩れない。
	// 上げるほど盛り土が増え、切り通しが減る
	if (m_lift != 0.0f)
	{
		for (auto& y : m_centerY) { y += m_lift; }
	}

	// 左右の傾き。高さが決まってから
	ResolveBank(field);
}

//----------------------------------------------------------
// 地形の高さを制御点へ取り込む
//
// 一度きりの操作。以後、道は制御点の高さで決まる。
//
// 地形に沿った形から始めたいときに使う。
// 押したあとに地形を彫っても、道はもう動かない
//----------------------------------------------------------
void HjRoad::BakeHeightFromTerrain(const HjHeightField* field)
{
	if (!field || !field->IsValid()) { return; }

	const int n = m_spline.PointCount();
	if (n < 2) { return; }

	std::vector<float> h(n, 0.0f);

	for (int i = 0; i < n; ++i)
	{
		const Math::Vector3 p = m_spline.GetPoints()[i];

		float y = field->HeightAt(p.x, p.z);
		if (y <= TerrainConst::OutsideHeight) { y = 0.0f; }

		h[i] = y;
	}

	// 細かい凹凸を落とす。
	// 地形をそのままなぞると、道が地面の起伏を拾って上下する
	for (int pass = 0; pass < RC::SeedSmoothPasses; ++pass)
	{
		std::vector<float> next = h;
		for (int i = 1; i + 1 < n; ++i)
		{
			next[i] = (h[i - 1] + h[i] * 2.0f + h[i + 1]) * 0.25f;
		}
		h = next;
	}

	for (int i = 0; i < n; ++i)
	{
		Math::Vector3 p = m_spline.GetPoints()[i];
		p.y = h[i];
		m_spline.MovePoint(i, p);
	}

	Rebuild();
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

	const float total   = m_spline.TotalLength();
	const float maxTurn = RC::StepMaxTurnDeg * (3.14159265f / 180.0f);

	float s    = 0.0f;
	float prev = 0.0f;
	m_stationS.push_back(0.0f);

	// 念のための上限。曲がりが極端だと刻みが下限に張り付いて、
	// 点が際限なく増える
	const int hardMax = static_cast<int>(total / RC::StepMin) + 16;

	while (s < total && static_cast<int>(m_stationS.size()) < hardMax)
	{
		float step = RC::StepMax;

		//===== 先を見て決める =====
		// その場の曲率だけで前へ進めると、直線からヘアピンへ入るとき
		// 直線の刻み(3m)のままカーブへ飛び込む。
		// 出るときは細かいままなので、制御点を境に密度が食い違う。
		//
		// 刻みを決めるのに刻みが要るので、何度か回して寄せる
		for (int pass = 0; pass < RC::StepPasses; ++pass)
		{
			const float win = std::max(step, RC::StepLookAhead);

			// その窓の中で一番きつい所に合わせる
			float k = 0.0f;
			for (int i = 0; i <= RC::StepProbes; ++i)
			{
				const float t = static_cast<float>(i) / RC::StepProbes;
				k = std::max(k, m_spline.CurvatureAt(std::min(s + win * t, total)));
			}

			step = (k > 1e-5f) ? (maxTurn / k) : RC::StepMax;
			step = std::clamp(step, RC::StepMin, RC::StepMax);
		}

		// 広がるのは徐々に。狭まるのは安全なので即座に。
		// いきなり広げると、カーブを出た所で三角の形が跳ねる
		if (prev > 0.0f) { step = std::min(step, prev * RC::StepGrowth); }
		prev = step;

		s += step;
		m_stationS.push_back(std::min(s, total));
	}

	//===== 端の始末 =====
	// 最後を全長へ切り詰めるだけだと、そこに極端に短い区間ができる。
	// 短い区間は三角が潰れて、そこが弱点になる。
	// 最後の2つを均して収める
	const size_t n = m_stationS.size();
	if (n >= 3)
	{
		m_stationS[n - 2] = (m_stationS[n - 3] + total) * 0.5f;
		m_stationS[n - 1] = total;
	}
	else if (n >= 2)
	{
		m_stationS.back() = total;
	}
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
// 曲がりの内側はどちら側か
//
// 進む向きの変化から出す。内側だけを絞りたいので、側が要る。
// 両側を絞ると、外側まで細くなって道が痩せる
//----------------------------------------------------------
// 横のずれを、内側の限界に収める
//
// 路面もエプロンもここを通す。
//
// 別々に持つと、路面だけが絞られてエプロンが元の幅のまま残り、
// その間に穴が開く。実際それで内側の面が消えていた
//----------------------------------------------------------
// 絞りを決める
//
// ■ なぜ絞るか
// ヘアピンでは、曲がりの半径が道幅より小さくなることがある。
// 半径2.5mのコーナーに幅9.4mの道は通せない。
// 内側のふちの半径が負になり、面が裏返る(実測で180度の折れ)。
//
// ■ なぜその場の半径だけでは駄目か
// 半径が8m→3mと急に変わると、幅が5.0m→1.9mと飛ぶ。
// 刻みは0.25mしか離れていないので、横に3m飛んだ1区間ができる。
// 裏返りは消えるが、そこが新しい折れになる(実測で41度)。
//
// ■ 前後を見て、均す
// 前後10mで一番狭い所に合わせておけば、コーナーへ入る前から
// 細くなり始める。そのあと均せば境目の階段も消える。
// 実測で、最大の折れが180度から13度まで落ちた。
//
// 実際の峠も、ヘアピンの手前から道が狭くなっていく
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

		// 触ったマス目と、道がどれだけ持っているかを控える。
		//
		// 次に動かすとき、ここを素地へ戻してから削り直す。
		// 控えずに削り続けると、動かすたびに掘り進んでいく。
		//
		// 強さは筆の覆いにも使う。道が持っている所を盛ると突き抜ける
		m_deformWeight[kv.first] = kv.second.weight;

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
// 断面を置く向きと、ミターの伸び
//
// ■ 何が問題か
// 断面をその場の接線に直交させて置くと、角の所で
// 隣り合う区間の平行線がぴたりと交わらない。
// 交わらないぶん、道の幅がわずかに足りなくなる。
// 実測で、ずれは中央1.6mm・最大91mm。
//
// ■ ミター接合
// 前後の区間の向きの二等分線に直交する向きへ、
// 1/cos(θ/2) だけ伸ばして置く。
// こうすると、区間ごとの平行線が交点でぴたりと出会う。
// 幅のずれは0になる。
//
// ■ 上限が要る
// 角が急になるほど 1/cos(θ/2) は跳ね上がる。
// 180度に近づけば無限へ飛ぶ。線を描く道具でも同じ制限を置く
//----------------------------------------------------------
void HjRoad::CrossFrame(int step, Math::Vector3& outRight, float& outMiter) const
{
	const Math::Vector3 fwd = m_spline.TangentAt(StationS(step));

	outRight = Math::Vector3::Up.Cross(fwd);
	outMiter = 1.0f;

	const int steps = StepCount();
	if (step <= 0 || step >= steps) { return; }

	// 前後の区間の向き
	const Math::Vector3 prev = m_spline.PositionAt(StationS(step - 1));
	const Math::Vector3 here = m_spline.PositionAt(StationS(step));
	const Math::Vector3 next = m_spline.PositionAt(StationS(step + 1));

	Math::Vector3 a(here.x - prev.x, 0.0f, here.z - prev.z);
	Math::Vector3 b(next.x - here.x, 0.0f, next.z - here.z);

	if (a.LengthSquared() < 1e-8f || b.LengthSquared() < 1e-8f) { return; }
	a.Normalize();
	b.Normalize();

	// 二等分線
	Math::Vector3 bis = a + b;
	if (bis.LengthSquared() < 1e-8f) { return; }   // 真後ろへ折り返している
	bis.Normalize();

	// 二等分線と区間の角度が θ/2。その cos で割ると交点まで届く
	const float cosHalf = bis.Dot(b);
	if (cosHalf < 1e-3f) { return; }

	outRight = Math::Vector3::Up.Cross(bis);
	outMiter = std::min(1.0f / cosHalf, RC::MiterLimit);
}

//----------------------------------------------------------
void HjRoad::SmoothDeformed(HjHeightField* field)
{
	if (!field || !field->IsValid()) { return; }
	if (m_deformWeight.empty()) { return; }

	const int nx = field->GetSizeX();
	const int nz = field->GetSizeZ();

	// 均す対象。削ったマス目のうち、中心寄りだけ。
	//
	// 外側まで均すと、削っていない地形との境目が動いて
	// そこに新しい段差ができる
	std::vector<int> keys;
	keys.reserve(m_deformWeight.size());
	for (const auto& kv : m_deformWeight) { keys.push_back(kv.first); }

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
// この刻みの中心・断面の向き・ミターの伸び
//
// ガードレールなど、道に沿って物を並べるものが使う。
// 別に作ると、ヘアピンで道と柵がずれる
//----------------------------------------------------------
void HjRoad::CrossAt(int i, Math::Vector3& outCenter,
                     Math::Vector3& outRight, float& outMiter) const
{
	outCenter = m_spline.PositionAt(StationS(i));
	CrossFrame(i, outRight, outMiter);
}

//----------------------------------------------------------
// 道のりで並んだ値を、任意の道のりで引く
//
// 刻みは曲がりに合わせて変わるので、番号では引けない。
// 覚えた形を使うときは、道のりで突き合わせる
//----------------------------------------------------------
// 固定した高さを書き出す
//
// 制御点の並びとは別のファイルにする。
// 刻みは曲がりに合わせて変わるので、制御点1つにつき1つでは表せない
//----------------------------------------------------------
// 固定した高さを読む
//----------------------------------------------------------
// いまの高さで固定する
//
// 地形を彫ると道が上下するのを止める。
// 道のりで覚えるので、刻みが変わっても形は保たれる
//----------------------------------------------------------
// 刻みごとの裾の幅を決める
//
// ■ 制御点ごとに伸ばせる
// 区間によって裾を広げたい。制御点に幅を持たせて、
// 間はなめらかに繋ぐ。
//
// 道のりの範囲で持つやり方もあるが、制御点を動かすと
// 全長が変わって、指定した範囲がずれていく。
//
// ■ 向かいの裾と重なったら、真ん中で止める
// ヘアピンや九十九折りでは、行きと帰りの道が近づく。
// 裾を伸ばすほど、向かい合った裾どうしが重なる。
//
// 重なったぶんを消すと、間に地形が覗いて溝になる。
// 互いの真ん中で止めれば、そこで出会って1枚の面になる。
//
// 高さも合う。両方とも外端では地形の高さへ寄せてあるので、
// 同じ場所で終われば同じ高さになる
//----------------------------------------------------------
void HjRoad::ResolveApronSpan()
{
	const int steps = StepCount();

	m_apronSpan[0].assign(static_cast<size_t>(steps) + 1, RC::ApronWidth);
	m_apronSpan[1].assign(static_cast<size_t>(steps) + 1, RC::ApronWidth);
	m_apronFlatSpan[0].assign(static_cast<size_t>(steps) + 1, RC::ApronFlat);
	m_apronFlatSpan[1].assign(static_cast<size_t>(steps) + 1, RC::ApronFlat);

	if (steps < 1 || !m_spline.IsValid()) { return; }

	// 中心線を、相手探しのために格子へ入れる
	std::vector<Math::Vector3> centers;
	centers.reserve(static_cast<size_t>(steps) + 1);
	for (int i = 0; i <= steps; ++i)
	{
		centers.push_back(m_spline.PositionAt(StationS(i)));
	}

	const float inner = RC::HalfWidth + RC::ShoulderWidth;

	// 届く距離。これより遠い相手は、伸ばしても重ならない
	const float reach = (inner + RC::ApronWidthMax) * 2.0f;

	// マス目へ配る。総当たりだと 2150 x 2150 で 460万回になる
	const float cell = std::max(reach, 1.0f);

	float gridMinX = 1e18f, gridMinZ = 1e18f;
	float gridMaxX = -1e18f, gridMaxZ = -1e18f;
	for (const auto& p : centers)
	{
		gridMinX = std::min(gridMinX, p.x);  gridMaxX = std::max(gridMaxX, p.x);
		gridMinZ = std::min(gridMinZ, p.z);  gridMaxZ = std::max(gridMaxZ, p.z);
	}

	const int gnx = static_cast<int>((gridMaxX - gridMinX) / cell) + 2;
	const int gnz = static_cast<int>((gridMaxZ - gridMinZ) / cell) + 2;

	std::vector<std::vector<int>> buckets(static_cast<size_t>(gnx) * gnz);
	for (int i = 0; i <= steps; ++i)
	{
		const int gx = std::clamp(static_cast<int>((centers[i].x - gridMinX) / cell), 0, gnx - 1);
		const int gz = std::clamp(static_cast<int>((centers[i].z - gridMinZ) / cell), 0, gnz - 1);
		buckets[static_cast<size_t>(gz) * gnx + gx].push_back(i);
	}

	for (int i = 0; i <= steps; ++i)
	{
		const float s = StationS(i);

		// 制御点ごとの指定
		// 左右で別に持つ。谷側だけ伸ばす、という使い方をする
		for (int side = 0; side < 2; ++side)
		{
			m_apronSpan[side][i] =
				std::clamp(m_spline.ApronAtS(s, side), 0.0f, RC::ApronWidthMax);

			m_apronFlatSpan[side][i] =
				std::clamp(m_spline.FlatAtS(s, side), 0.0f, RC::ApronFlatMax);

			// 平場を広げたぶん、斜面の降りる先も外へ伸ばす。
			//
			// 平場だけ伸ばすと、降りる先はそのままなので、
			// 同じ幅でより深い所まで降りることになって斜面が立つ
			const float extra = std::max(
				m_apronFlatSpan[side][i] - RC::ApronFlat, 0.0f);

			m_apronSpan[side][i] = std::clamp(
				m_apronSpan[side][i] + extra * m_apronFollowFlat,
				0.0f, RC::ApronWidthMax);
		}

		//===== 向かいの道を探す =====
		// 自分のすぐ前後は相手ではない。
		// 見ないと、隣の断面を相手だと思って幅が潰れる
		const Math::Vector3 here = centers[i];

		Math::Vector3 right;
		float miter = 1.0f;
		CrossFrame(i, right, miter);

		float nearDist[2] = { 1e18f, 1e18f };   // [0]=左 [1]=右

		// 近くのマス目だけ見る。
		// 総当たりだと 2150 x 2150 で 460万回になり、
		// 点を掴んで動かしている間に固まる
		const int gx = static_cast<int>((here.x - gridMinX) / cell);
		const int gz = static_cast<int>((here.z - gridMinZ) / cell);

		for (int dzc = -1; dzc <= 1; ++dzc)
		{
			for (int dxc = -1; dxc <= 1; ++dxc)
			{
				const int cx = gx + dxc;
				const int cz = gz + dzc;
				if (cx < 0 || cz < 0 || cx >= gnx || cz >= gnz) { continue; }

				for (int j : buckets[static_cast<size_t>(cz) * gnx + cx])
				{
					// 自分のすぐ前後は相手ではない。
					// 見ると、隣の断面を相手だと思って幅が潰れる
					if (fabsf(StationS(j) - s) < RC::MergeIgnoreS) { continue; }

					const float dx = centers[j].x - here.x;
					const float dz = centers[j].z - here.z;
					const float d  = sqrtf(dx * dx + dz * dz);

					// 遠すぎる相手は、伸ばしても届かない
					if (d > reach) { continue; }

					// どちら側に居るか
					const int side = (dx * right.x + dz * right.z >= 0.0f) ? 1 : 0;

					nearDist[side] = std::min(nearDist[side], d);
				}
			}
		}

		//===== 真ん中で止める =====
		for (int side = 0; side < 2; ++side)
		{
			if (nearDist[side] > 1e17f) { continue; }

			// 相手との真ん中まで。路肩の外端からの距離に直す。
			//
			// 効かせるのは「平場+法面」の合計。
			// 法面だけに掛けると、平場のぶん外へはみ出して重なる
			const float half = std::max(nearDist[side] * 0.5f - inner - RC::MergeGap, 0.0f);

			// 詰めるのは斜面だけ。平場は指定どおり伸ばす。
			//
			// 平場まで制限に入れると、道どうしが近いコースでは
			// 「真ん中まで」の上限が中央値10m ほどしかないので、
			// いくら広げても押し戻されて伸びない。
			//
			// 平場は道と平行な面なので、向かいと重なっても
			// 高さが近く、破綻しにくい。
			// 斜面は降りる面なので、重なると交差して見える
			m_apronSpan[side][i] = std::clamp(half, 0.0f, m_apronSpan[side][i]);
		}
	}

	//===== なめらかにする =====
	// 相手との距離は地点ごとに跳ねる。
	// そのまま使うと裾の外端がガタつき、
	// 一点に潰れた三角の扇ができる
	for (int side = 0; side < 2; ++side)
	{
		// 法面と平場の両方を均す。
		// 片方だけだと、境目が階段になる
		std::vector<float>* arr[2] = { &m_apronSpan[side], &m_apronFlatSpan[side] };

		for (std::vector<float>* pv : arr)
		{
			std::vector<float>& v = *pv;
			std::vector<float> next = v;

			for (int pass = 0; pass < RC::ApronSmoothPasses; ++pass)
			{
				for (int i = 1; i < steps; ++i)
				{
					next[i] = (v[i - 1] + v[i] * 2.0f + v[i + 1]) * 0.25f;
				}
				v = next;
			}
		}
	}
}

//----------------------------------------------------------
// 裾に覆われたマス目を出す
//
// ■ なぜ要るか
// 裾と地形は同じ高さで広く重なる。道の周りの地形を道の高さへ
// 寄せているので当然だが、遠くでどちらが手前か決まらずちらつく。
// テクスチャを貼ると、模様が入れ替わって見えるのでもっと目立つ。
//
// 完全に隠れている地形の四角は、そもそも張らない。
//
// ■ 削りは消さない
// 切り土も盛り土も今までどおり。高さマップも触らない。
// 当たり判定もブラシも、裾が目標にする高さも変わらない。
// 消すのは「描く三角」だけ。
//
// ■ 控えめに開ける
// 裾は折り返しや交差の判定で落ちることがある。
// そこで穴を開けていると、両方無くなって空が見える。
// マス目1つぶん内側に寄せておけば、残った地形が裾の下に隠れる
//----------------------------------------------------------
void HjRoad::MarkCover(const HjHeightField* field,
                      const std::vector<KdMeshVertex>& verts,
                      const std::vector<KdMeshFace>& faces)
{
	m_coverCells.clear();
	if (!field || !field->IsValid()) { return; }

	const int   nx = field->GetSizeX();
	const int   nz = field->GetSizeZ();
	const float cs = field->GetCellSize();

	const float offX = field->GetWorldW() * 0.5f;
	const float offZ = field->GetWorldD() * 0.5f;

	//===== 実際に残った三角を、真上から見て塗る =====
	//
	// ■ なぜ半径で決めないか
	// 前は「刻みの中心から半径いくつまでが道」として消していた。
	// 直線なら合うが、ヘアピンでは折り目で内側の面が削られて
	// 帯に穴が開く。それでも外側の1点が残っていれば半径は大きいままなので、
	// 削られて面が無い所まで地形を消してしまう。そこが空白になる。
	//
	// 面から直に出せば、残った所だけが消える
	std::vector<unsigned char> inside(static_cast<size_t>(nx) * nz, 0);

	for (const auto& f : faces)
	{
		const Math::Vector3& a = verts[f.Idx[0]].Pos;
		const Math::Vector3& b = verts[f.Idx[1]].Pos;
		const Math::Vector3& cpos = verts[f.Idx[2]].Pos;

		// 真上から見た三角。縦の壁はここで潰れるが、
		// 潰れた三角は塗る面積が無いだけで害はない
		const float minX = std::min({ a.x, b.x, cpos.x });
		const float maxX = std::max({ a.x, b.x, cpos.x });
		const float minZ = std::min({ a.z, b.z, cpos.z });
		const float maxZ = std::max({ a.z, b.z, cpos.z });

		const int x0 = std::max(0,      static_cast<int>(floorf((minX + offX) / cs)));
		const int x1 = std::min(nx - 1, static_cast<int>(ceilf ((maxX + offX) / cs)));
		const int z0 = std::max(0,      static_cast<int>(floorf((minZ + offZ) / cs)));
		const int z1 = std::min(nz - 1, static_cast<int>(ceilf ((maxZ + offZ) / cs)));

		// 三角の中か。外積の符号が3辺で揃えば中
		const float e = (b.x - a.x) * (cpos.z - a.z) - (b.z - a.z) * (cpos.x - a.x);
		if (fabsf(e) < 1e-6f) { continue; }
		const float invE = 1.0f / e;

		for (int iz = z0; iz <= z1; ++iz)
		{
			const float wz = iz * cs - offZ;

			for (int ix = x0; ix <= x1; ++ix)
			{
				const float wx = ix * cs - offX;

				const float w0 = ((b.x - a.x) * (wz - a.z) - (b.z - a.z) * (wx - a.x)) * invE;
				const float w1 = ((cpos.x - b.x) * (wz - b.z) - (cpos.z - b.z) * (wx - b.x)) * invE;
				const float w2 = ((a.x - cpos.x) * (wz - cpos.z) - (a.z - cpos.z) * (wx - cpos.x)) * invE;

				if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) { continue; }

				inside[static_cast<size_t>(iz) * nx + ix] = 1;
			}
		}
	}

	//===== 縁を残す =====
	// 塗った所をそのまま消すと、道の縁とちょうど同じ所で地形が切れる。
	// 縁は面の形が細かく揺れるので、必ずどこかに隙間が出る。
	//
	// 周りが全部道になっているマス目だけ消す(内側へ削る)
	const int r = std::max(1, static_cast<int>(ceilf(RC::CoverMargin)));

	for (int iz = 0; iz < nz; ++iz)
	{
		for (int ix = 0; ix < nx; ++ix)
		{
			if (!inside[static_cast<size_t>(iz) * nx + ix]) { continue; }

			bool all = true;
			for (int dz = -r; dz <= r && all; ++dz)
			{
				const int z = iz + dz;
				if (z < 0 || z >= nz) { all = false; break; }

				for (int dx = -r; dx <= r; ++dx)
				{
					const int x = ix + dx;
					if (x < 0 || x >= nx || !inside[static_cast<size_t>(z) * nx + x])
					{
						all = false;
						break;
					}
				}
			}

			if (all) { m_coverCells.insert(iz * nx + ix); }
		}
	}
}

//----------------------------------------------------------
// メッシュを組む
//
// ■ 路面と裾を1枚で作る
// 別々に作っていたときは、境目に穴が開き続けた。
//
// 幅方向の刻みが違ったのが原因。
// 路面は9.4mを13列(0.78mごと)、裾は7.0mを15列(0.50mごと)。
// 頂点の位置は境目で一致するが、別のメッシュなので
// 折り返しを削ったあとの穴埋めが相手側へ届かない。
//
// 1枚にすれば、削った跡を端から端まで繋げる。
// 幅方向の刻みも揃うので、境目で三角の形が変わらない。
//
// ■ 色は頂点で分ける
// 路面と裾で色を変えたいが、材質を分けると1枚にできない。
// シェーダが頂点カラーを掛けているので、そちらに持たせる
//----------------------------------------------------------
void HjRoad::BuildMesh(const HjHeightField* field)
{
	m_spMesh = nullptr;
	if (!m_spline.IsValid() || m_centerY.empty()) { return; }

	const int steps    = StepCount();
	const int roadCols = RC::WidthSegments * 2 + 1;

	const float inner = RC::HalfWidth + RC::ShoulderWidth;

	// 幅方向の刻み。路面に合わせて、裾もこの間隔で割る。
	// 揃えないと境目で三角の形が変わり、そこが弱くなる
	const float lat = (inner * 2.0f) / std::max(roadCols - 1, 1);

	// 列の数は一番広い所に合わせる。
	//
	// 刻みごとに列の数を変えると、隣の断面と頂点の数が合わず、
	// 面を張れない。数は固定して、幅だけ縮める
	float widest = RC::ApronWidth;
	for (int side = 0; side < 2; ++side)
	{
		for (float w : m_apronSpan[side]) { widest = std::max(widest, w); }
	}
	float widestFlat = 0.0f;
	for (int side = 0; side < 2; ++side)
	{
		for (float w : m_apronFlatSpan[side]) { widestFlat = std::max(widestFlat, w); }
	}
	const float apronMax = std::max(widestFlat + widest, 0.5f);

	const int apronSegs = std::max(2,
		static_cast<int>(apronMax / std::max(lat, 0.01f) + 0.5f));

	// 両端に、真下へ垂らす列を1つずつ足す。
	//
	// 地形の格子は裾の縁に沿えないので、境目は必ず少しずれる。
	// 見えない壁を垂らしておけば、隙間があっても透けない
	const int cols = apronSegs * 2 + roadCols + 2;

	// 裾の列だけを数えるときの起点。0番は左の壁
	const int firstApron = 1;

	// 列の番号から、中心線からの横のずれを出す。
	//
	// 裾の幅は刻みごとに違う。制御点ごとの指定と、
	// 向かいの裾との重なりで決まるので、刻みも渡す。
	//
	// 列の数は変えない。変えると隣の断面と頂点の数が合わず、面を張れない
	auto FlatSpanAt = [&](int i, int side) -> float
	{
		const std::vector<float>& v = m_apronFlatSpan[side];
		if (v.empty()) { return m_apronFlat; }

		return v[std::clamp(i, 0, static_cast<int>(v.size()) - 1)];
	};

	auto ApronTotalAt = [&](int i, int side) -> float
	{
		const std::vector<float>& v = m_apronSpan[side];
		if (v.empty()) { return RC::ApronTotal; }

		return FlatSpanAt(i, side)
		     + v[std::clamp(i, 0, static_cast<int>(v.size()) - 1)];
	};

	auto OffsetOf = [&](int i, int c) -> float
	{
		// 両端の壁は、隣の頂点と同じ横位置。真下へ落とすだけ
		const int cc = std::clamp(c - firstApron, 0, apronSegs * 2 + roadCols - 1);

		if (cc < apronSegs)
		{
			// 左の裾。0が一番外、apronSegs-1 が路肩のすぐ隣
			const float u = static_cast<float>(apronSegs - cc)
			              / static_cast<float>(apronSegs);
			return -(inner + ApronTotalAt(i, 0) * u);
		}
		if (cc < apronSegs + roadCols)
		{
			// 路面。幅は設計値なので刻みで変えない
			const float t = static_cast<float>(cc - apronSegs)
			              / static_cast<float>(std::max(roadCols - 1, 1));
			return (t * 2.0f - 1.0f) * inner;
		}
		// 右の裾
		const float u = static_cast<float>(cc - apronSegs - roadCols + 1)
		              / static_cast<float>(apronSegs);
		return inner + ApronTotalAt(i, 1) * u;
	};

	// 頂点色は塗り分けの重み(R=岩 G=土 B=草 A=舗装)。
	//
	// 路面は舗装だけ。裾は地形と同じ規則で後から出す。
	// 裾をここで一色に決めてしまうと、地形との継ぎ目で色が飛んで、
	// そこに面の境目があることが見えてしまう
	const unsigned int colRoad = SplatConst::Pack(0.0f, 0.0f, 0.0f, 1.0f);

	std::vector<KdMeshVertex> verts;
	verts.reserve(static_cast<size_t>(steps + 1) * cols);

	// 頂点ごとの、中心線からの横のずれ。
	// 裾の幅が刻みごとに違うので、列だけでは決まらない
	std::vector<float> vertOff;
	vertOff.reserve(static_cast<size_t>(steps + 1) * cols);

	// 頂点ごとの、路面からの高さの差。
	//
	// 道のすぐ横に高い山があると、裾は山の中を通って
	// 先だけ表へ出る。離れすぎた所を落とすのに使う
	std::vector<float> vertRise;
	vertRise.reserve(static_cast<size_t>(steps + 1) * cols);

	// 進む向きと中心線。折り返した頂点を見つけるのに要る
	std::vector<Math::Vector3> fwds;
	std::vector<Math::Vector3> centers;
	fwds.reserve(static_cast<size_t>(steps) + 1);
	centers.reserve(static_cast<size_t>(steps) + 1);

	for (int i = 0; i <= steps; ++i)
	{
		const float s = StationS(i);

		const Math::Vector3 center = m_spline.PositionAt(s);

		fwds.push_back(m_spline.TangentAt(s));
		centers.push_back(center);

		// 断面を置く向きと伸び。
		// 接線に直交させると、角で幅がわずかに足りなくなる
		Math::Vector3 right;
		float miter = 1.0f;
		CrossFrame(i, right, miter);

		for (int c = 0; c < cols; ++c)
		{
			const float off  = OffsetOf(i, c);
			const float dist = fabsf(off);
			vertOff.push_back(dist);

			// 両端は壁。横は同じで、真下へ落とす
			const bool skirt = (c == 0 || c == cols - 1);

			// 平面の位置だけ伸ばす。
			// 断面の高さは「道のどこか」で決まるので、伸ばす前の値で引く
			const float wx = center.x + right.x * off * miter;
			const float wz = center.z + right.z * off * miter;

			float y = 0.0f;
			unsigned int col = colRoad;

			if (dist <= inner)
			{
				// 路面。幅は設計値なので絞らない
				y = SurfaceY(i, off);
			}
			else
			{
				const float sign = (off > 0.0f) ? 1.0f : -1.0f;

				// 路肩の外端からどれだけ出たか
				const float over = dist - inner;

				const int sideIdx = (off > 0.0f) ? 1 : 0;
				const float flat  = FlatSpanAt(i, sideIdx);

				if (over <= flat)
				{
					// 平場。道の面をそのまま伸ばす。
					//
					// SurfaceY は路肩より外でも、路肩の落ちを保ったまま
					// カントを続けるので、道と平行に伸びる。
					//
					// 縁からいきなり落とすと、道が細い板に見える
					y = SurfaceY(i, off);
				}
				else
				{
					// 法面。平場の外端から地形へ溶ける
					const float shelfY = SurfaceY(i, (inner + flat) * sign);

					float groundY = shelfY;
					if (field && field->IsValid())
					{
						const float g = field->HeightAt(wx, wz);
						if (g > TerrainConst::OutsideHeight) { groundY = g; }
					}

					// 端で折れないよう滑らかに。
					// 溶けきるまでの幅は刻みごとに違う
					const float span = std::max(
						ApronTotalAt(i, sideIdx) - flat, 0.01f);

					const float t = std::clamp((over - flat) / span, 0.0f, 1.0f);
					const float k = t * t * (3.0f - 2.0f * t);

					y = shelfY + (groundY - shelfY) * k;

					// 外端はわずかに沈める。
					// ちょうどに合わせると、どちらが手前か決まらずちらつく
					// 外端はわずかに持ち上げる。
					// 縁に残る地形の輪と同じ高さだと、どちらが手前か
					// 決まらずちらつく。沈めると地形が勝って線が出る
					y += RC::ApronLift * k;
				}

				// 裾は地形と同じ規則で塗る。
				//
				// ただし傾きが要るのに、法線はまだ面から求め直していない。
				// ここでは目印だけ置いて、法線が出てから塗る
				col = 0u;
			}

			// 壁は真下へ。上から見えないので、隙間だけを塞ぐ
			if (skirt) { y -= RC::ApronSkirt; }

			// 路面(中心線の高さ)からどれだけ離れたか。
			// 裾が山の中を通っていないかを見るのに使う
			vertRise.push_back(y - m_centerY[std::clamp(i, 0,
				static_cast<int>(m_centerY.size()) - 1)]);

			KdMeshVertex v;
			v.Pos     = Math::Vector3(wx, y, wz);
			v.Normal  = Math::Vector3::Up;   // あとで面から求め直す
			v.Tangent = right;
			v.Color   = col;

			// 進んだ距離でUVを送る。素材の繰り返しが道に沿う
			v.UV = Math::Vector2(
				(off / (inner * 2.0f)) + 0.5f,
				s / std::max(RC::UvLength, 0.01f));

			verts.push_back(v);
		}
	}

	//===== 折り返しを削って、跡を塞ぐ =====
	const std::vector<float> advance = CenterAdvance(centers);

	std::vector<bool> valid(verts.size(), true);

	//===== 路面から離れすぎた裾を落とす =====
	// 道のすぐ横に高い山があると、裾は山の斜面の高さへ向かって伸びる。
	// 途中は山の中を通り、先だけ表へ出るので、そこがちらつく。
	//
	// 落としたあとは穴埋めが縁を塞ぐので、
	// 裾は山にぶつかった所で終わる
	for (size_t k = 0; k < verts.size(); ++k)
	{
		const float rise = vertRise[k];

		if (rise > m_apronMaxRise || rise < -m_apronMaxDrop)
		{
			valid[k] = false;
		}
	}

	if (m_trimFold)
	{
		// ① 前後の断面と追い越し合っている頂点。
		//    ほぼ潰れた面もここで落とす
		MarkFolded(verts, fwds, advance, steps + 1, cols, 0, cols, valid);

		// ② 遠く離れた断面と交差している頂点。
		//    ヘアピンの入口と出口のように、隣どうしでは分からない交差
		CenterGrid grid;
		grid.Build(centers, RC::HalfWidth + RC::ShoulderWidth + RC::ApronWidthMax);

		MarkCrossed(verts, vertOff, grid, steps + 1, cols, valid);
	}

	std::vector<KdMeshFace> faces;
	faces.reserve(static_cast<size_t>(steps) * (cols - 1) * 2);

	// 中心線の列から外へ広げる。そこは必ず生きている。
	//
	// 1枚なので、路面から裾まで一続きに塞げる
	StripFaces(valid, steps + 1, cols, 0, cols,
	           apronSegs + roadCols / 2, false, faces);

	if (faces.empty()) { return; }

	//===== 実際に生き残った一番外の位置 =====
	// 地形に穴を開けるのに使う。
	// 指定した幅から開けると、面が落ちた所で空が見える
	m_coverReach[0].assign(static_cast<size_t>(steps) + 1, inner);
	m_coverReach[1].assign(static_cast<size_t>(steps) + 1, inner);

	for (int r = 0; r <= steps; ++r)
	{
		for (int cc2 = 0; cc2 < cols; ++cc2)
		{
			if (!valid[static_cast<size_t>(r) * cols + cc2]) { continue; }

			const float off = OffsetOf(r, cc2);
			const int   sd  = (off >= 0.0f) ? 1 : 0;

			m_coverReach[sd][r] = std::max(m_coverReach[sd][r], fabsf(off));
		}
	}

	//===== 組んだ結果を数で出す =====
	// 見た目だけで原因を当てようとして、何度も外した
	m_statRoad = BuildStat{};
	m_statRoad.verts   = static_cast<int>(verts.size());
	m_statRoad.faces   = static_cast<int>(faces.size());
	m_statRoad.cols    = cols;
	m_statRoad.minCols = cols;

	for (int r = 0; r <= steps; ++r)
	{
		int live = 0;
		for (int c = 0; c < cols; ++c)
		{
			if (valid[static_cast<size_t>(r) * cols + c]) { ++live; }
			else { ++m_statRoad.dropped; }
		}
		if (live < m_statRoad.minCols)
		{
			m_statRoad.minCols = live;
			m_statRoad.minAtS  = StationS(r);
		}
	}

	//===== 法線を面から求め直す =====
	// 上向きのままだと、坂で光の当たり方がおかしくなる
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
		verts[i].Normal = normals[i];
	}

	//===== 裾の塗り分け =====
	// 法線が出てから塗る。傾きが要るため。
	//
	// ■ 地形とまったく同じ入力を使う
	// 傾きは面から求めた法線、道の持ち分は OwnWeightAt。
	// 裾だけ別の式(外端からの距離など)で出すと、隣り合う地形の頂点と
	// 値が食い違って、継ぎ目にそこだけ色の段が出る
	{
		const int nx = field ? field->GetSizeX() : 0;

		for (auto& v : verts)
		{
			// 路面は舗装のまま
			if (v.Color != 0u) { continue; }

			float own = 0.0f;
			if (field && nx > 0)
			{
				float cx = 0.0f, cz = 0.0f;
				field->WorldToCell(v.Pos.x, v.Pos.z, cx, cz);

				const int gx = static_cast<int>(cx + 0.5f);
				const int gz = static_cast<int>(cz + 0.5f);

				if (gx >= 0 && gz >= 0 && gx < nx && gz < field->GetSizeZ())
				{
					own = OwnWeightAt(gz * nx + gx);
				}
			}

			v.Color = SplatConst::Weights(v.Normal.y, own);
		}
	}

	std::vector<KdMeshSubset> subsets(1);
	subsets[0].MaterialNo = 0;
	subsets[0].FaceStart  = 0;
	subsets[0].FaceCount  = static_cast<UINT>(faces.size());

	m_spMesh = std::make_shared<KdMesh>();
	if (!m_spMesh->Create(verts, faces, subsets, false)) { m_spMesh = nullptr; }

	// 覆いはここで出す。
	// 面が確定してからでないと、削られた所まで地形を消してしまう
	MarkCover(field, verts, faces);
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

	// 面の張り方を目で確かめる。
	// 塗り潰しだと、切れているのか繋がっているのか分からない
	if (m_wireframe)
	{
		KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::WireFrame);
	}

	// 地形と同じ塗り分けを掛ける。
	// 路面は舗装、裾は土から草へ。設定を地形と分けると、
	// 継ぎ目でだけ色が飛んで面の境目が見えてしまう
	//
	// DrawMesh は定数バッファに触らないので、送るのも戻すのも自分でやる
	HjTerrain::ApplySplat(shader);

	shader.DrawMesh(m_spMesh.get(), Math::Matrix::Identity,
	                m_materials, kWhiteColor, Math::Vector3::Zero);

	shader.PopCBObject();

	if (m_wireframe)
	{
		KdShaderManager::Instance().UndoRasterizerState();
	}
}

void HjRoad::GenerateDepthMapFromLight()
{
	if (!m_spMesh) { return; }

	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 影の生成でも入れる。頂点色は色ではなく重みなので、
	// そのまま透明度として読まれると道が影を落とさなくなる
	HjTerrain::ApplySplat(shader);

	shader.DrawMesh(m_spMesh.get(), Math::Matrix::Identity,
	                m_materials, kWhiteColor, Math::Vector3::Zero);

	shader.PopCBObject();
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
// 制御点ごとの裾の幅
//
// 変えたら作り直す。押してから反映では、
// 広げすぎたか足りないかが分からない
//----------------------------------------------------------
void HjRoad::SetApronAt(int i, int side, float w)
{
	m_spline.SetApronAt(i, side, w);
	Rebuild();
}

//----------------------------------------------------------
// 制御点ごとの平場の幅
//----------------------------------------------------------
void HjRoad::SetFlatAt(int i, int side, float w)
{
	m_spline.SetFlatAt(i, side, w);
	Rebuild();
}

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
		// 素地へ戻す。
		//
		// 削る前の値を控えて戻すのではなく、素地を正とする。
		// 控えた値を戻す形だと、手で彫った所を道が上書きしたあと、
		// 彫る前の形へ戻ってしまう
		const int nx = m_pField->GetSizeX();
		for (const auto& kv : m_deformWeight)
		{
			m_pField->RestoreCell(kv.first % nx, kv.first / nx);
		}
	}
	m_deformWeight.clear();

	ResolveHeights(m_pField);

	// 刻みが決まったので、裾の幅を決める。メッシュより先でないといけない
	ResolveApronSpan();

	DeformTerrain(m_pField);
	SmoothDeformed(m_pField);
	BuildMesh(m_pField);

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

	// 面の張り方を目で確かめる。
	// 塗り潰しだと、切れているのか繋がっているのか分からない
	ImGui::Checkbox(U8("線だけで描く"), &m_wireframe);
	ImGui::SetItemTooltip(U8("ヘアピンで面を落とした跡が塞がっているか確かめる"));

	ImGui::SameLine();
	if (ImGui::Checkbox(U8("折り返しを削る"), &m_trimFold)) { Rebuild(); }
	ImGui::SetItemTooltip(U8("切って道が戻るなら、削りすぎが原因だと分かる"));

	//===== 組んだ結果 =====
	// 見た目だけで原因を当てようとして何度も外したので、数で出す
	ImGui::Text(U8("頂点%d (%d列)  落とした%d (%.1f%%)  面%d"),
		m_statRoad.verts, m_statRoad.cols, m_statRoad.dropped,
		m_statRoad.verts ? (100.0f * m_statRoad.dropped / m_statRoad.verts) : 0.0f,
		m_statRoad.faces);

	// 幅が何列まで細ったか。1なら道が線に潰れている
	if (m_statRoad.minCols < m_statRoad.cols)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
			U8("  一番細い所 %d/%d 列  (道のり %.0fm)"),
			m_statRoad.minCols, m_statRoad.cols, m_statRoad.minAtS);
	}

	ImGui::SeparatorText(U8("縦断"));

	//===== 高さの取り込み =====
	// 高さは制御点が持つ。地形を彫っても道は動かない。
	//
	// 地形に沿った形から始めたいときだけ、ここで取り込む
	ImGui::TextDisabled(U8("高さは制御点が持つ。地形を彫っても道は動かない"));

	if (ImGui::Button(U8("地形から高さを取り込む")))
	{
		BakeHeightFromTerrain(m_pField);
	}
	ImGui::SetItemTooltip(U8(
		"いまの地形の高さを制御点へ書き込む。一度きりの操作。"
		"押したあとに地形を彫っても、道はもう動かない"));



	// 触った値をその場へ反映する。
	// 押してから反映では、行き過ぎたか戻しすぎたかが分からない
	bool changed = false;

	changed |= ImGui::DragFloat(U8("道の持ち上げ"), &m_lift, 0.05f, -30.0f, 60.0f);
	ImGui::SetItemTooltip(U8("道ごと上下する。上げるほど盛り土が増え、切り通しが減る"));

	changed |= ImGui::DragFloat(U8("勾配の上限"), &m_maxGrade, 0.002f, 0.01f, 0.40f);
	ImGui::SetItemTooltip(U8("1進んだときに上下できる量。0.12で約7度"));

	// 平場の幅。動かした瞬間に全部の点へ効く。
	//
	// 幅そのものは制御点が持っているが、ここを「入れてからボタン」に
	// すると、動かしても何も起きない。実際それで詰まった
	if (ImGui::DragFloat(U8("平場の幅(m)"), &m_apronFlat, 0.05f,
	                     0.0f, RC::ApronFlatMax))
	{
		for (int i = 0; i < m_spline.PointCount(); ++i)
		{
			m_spline.SetFlatAt(i, 0, m_apronFlat);
			m_spline.SetFlatAt(i, 1, m_apronFlat);
		}
		changed = true;
	}
	ImGui::SetItemTooltip(U8(
		"路肩の外に、道と平行に伸ばす幅。全部の点へ効く。"
		"区間ごとに変えたいときは、点を選んで個別に触る"));

	changed |= ImGui::DragFloat(U8("裾が上がってよい高さ(m)"), &m_apronMaxRise, 0.2f,
	                            0.5f, RC::ApronRiseMax);
	ImGui::SetItemTooltip(U8(
		"路面よりこれ以上高い所は裾を出さない。"
		"道の横に高い山があると、裾が山の中を通って先だけ表へ出る"));

	changed |= ImGui::DragFloat(U8("裾が下がってよい高さ(m)"), &m_apronMaxDrop, 0.5f,
	                            0.5f, RC::ApronRiseMax);
	ImGui::SetItemTooltip(U8("路面よりこれ以上低い所は裾を出さない。盛り土は深くてよい"));

	changed |= ImGui::DragFloat(U8("斜面も一緒に伸ばす"), &m_apronFollowFlat, 0.02f,
	                            0.0f, RC::ApronFollowMax);
	ImGui::SetItemTooltip(U8(
		"平場を広げたぶん、斜面の降りる先も外へ伸ばす割合。"
		"1.0 で広げた量と同じだけ斜面も伸びる。0 で斜面は伸びない"));

	//===== 実際に配られた幅と、面が残ったか =====
	// 指定した幅がそのまま出るとは限らない。
	// 向かいの裾と取り合うし、曲がりの内側では面が落とされる。
	//
	// 「伸びていない」のが配り方の話か、落とされた話かを分ける
	{
		float fLo = 1e9f, fHi = -1e9f, sLo = 1e9f, sHi = -1e9f;

		for (int side = 0; side < 2; ++side)
		{
			for (size_t k = 0; k < m_apronSpan[side].size(); ++k)
			{
				const float f = m_apronFlatSpan[side][k];
				const float g = m_apronSpan[side][k];
				fLo = std::min(fLo, f); fHi = std::max(fHi, f);
				sLo = std::min(sLo, g); sHi = std::max(sHi, g);
			}
		}

		if (fHi >= fLo)
		{
			ImGui::Text(U8("配られた幅  平場 %.1f〜%.1fm   斜面 %.1f〜%.1fm"),
			            fLo, fHi, sLo, sHi);
		}

		ImGui::Text(U8("面  頂点%d (%d列)  落とした%d (%.1f%%)"),
		            m_statRoad.verts, m_statRoad.cols, m_statRoad.dropped,
		            m_statRoad.verts
		                ? (100.0f * m_statRoad.dropped / m_statRoad.verts) : 0.0f);
	}

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
