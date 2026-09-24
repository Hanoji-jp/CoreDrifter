#include "HjGuardRail.h"

#include "HjRoad.h"
#include "../../Util/HjMeshUtil.h"
#include "HjHeightField.h"

namespace GR = GuardRailConst;

//----------------------------------------------------------
// この地点のこちら側に柵が要るか
//
// 実際の峠は、どこにでもレールがあるわけではない。
// 外側で、かつ谷になっている所だけ。
//
// 道の縁から少し外の地形が、路面よりどれだけ落ちているかで決める
//----------------------------------------------------------
bool HjGuardRail::NeedRail(const HjRoad& road, const HjHeightField* field,
                           int step, float side) const
{
	(void)field;

	// 置く場所は制御点が決める。
	//
	// ■ なぜ地形から自動で出さなくなったか
	// 落差だけで決めると、柵が要らない所にも立ち、欲しい所に
	// 立たない。どこを守るかは作る側の判断で、地形の数字からは
	// 出てこない。擁壁と同じ扱いにした。
	//
	// 地形から当てはめる道具は AutoFill として残してある
	return road.RailAtStep(step, (side > 0.0f) ? 1 : 0);
}

//----------------------------------------------------------
// 地形の落ち方から当てはめる
//
// 手で置く前の下敷き。
// 道の縁の外の地形が落ちている所へ柵を入れる
//----------------------------------------------------------
void HjGuardRail::AutoFill(HjRoad& road, const HjHeightField* field)
{
	if (!field || !field->IsValid()) { return; }

	const int pts = road.PointCount();

	for (int i = 0; i < pts; ++i)
	{
		const int step = road.StepOfPoint(i);

		for (int side = 0; side < 2; ++side)
		{
			const float sign = (side == 1) ? 1.0f : -1.0f;

			Math::Vector3 center, right;
			float miter = 1.0f;
			road.CrossAt(step, center, right, miter);

			// 路面の縁の高さ
			const float edgeY = road.HeightAtOffset(step, GR::Offset * sign);

			// そこから少し外の地形
			const float off = (GR::Offset + GR::LookOut) * sign * miter;
			const float wx = center.x + right.x * off;
			const float wz = center.z + right.z * off;

			if (!field->Contains(wx, wz))
			{
				road.SetRailAt(i, side, 0.0f);
			continue;
			}

			const float drop = edgeY - field->HeightAt(wx, wz);

			road.SetRailAt(i, side, (drop > GR::DropThreshold) ? 1.0f : 0.0f);
		}
	}
}

//----------------------------------------------------------
// 短すぎる区間を消し、短い切れ目を繋ぐ
//
// 地形のわずかな起伏で、柵が数メートルおきに切れる。
// 数メートルだけの柵は現実にも無いし、見た目も悪い
//----------------------------------------------------------
void HjGuardRail::TidyRuns(const HjRoad& road, std::vector<bool>& on) const
{
	const int n = static_cast<int>(on.size());
	if (n < 2) { return; }

	// 先に切れ目を埋める。
	// 短い区間を消すのを先にすると、消した跡がまた切れ目になる
	for (int i = 0; i < n; )
	{
		if (on[i]) { ++i; continue; }

		int j = i;
		while (j < n && !on[j]) { ++j; }

		// 両端が柵で、間が短ければ繋ぐ
		const bool inside = (i > 0) && (j < n);
		if (inside && (road.StationAt(j) - road.StationAt(i)) < GR::MaxGap)
		{
			for (int k = i; k < j; ++k) { on[k] = true; }
		}
		i = j;
	}

	// 短すぎる区間を消す
	for (int i = 0; i < n; )
	{
		if (!on[i]) { ++i; continue; }

		int j = i;
		while (j < n && on[j]) { ++j; }

		if ((road.StationAt(j - 1) - road.StationAt(i)) < GR::MinRun)
		{
			for (int k = i; k < j; ++k) { on[k] = false; }
		}
		i = j;
	}
}

//----------------------------------------------------------
// 片側ぶんを積む
//
// ビームは断面3点(下・真ん中・上)の押し出し。
// 真ん中を外へ張り出させると、本物のW断面らしく見える。
//
// 支柱は決まった間隔で箱を置く
//----------------------------------------------------------
void HjGuardRail::BuildSide(const HjRoad& road, float side,
                            std::vector<KdMeshVertex>& verts,
                            std::vector<KdMeshFace>& faces)
{
	const int n = road.StationCount();
	if (n < 2) { return; }

	// この側で柵が要る所
	std::vector<bool> on(n, false);
	for (int i = 0; i < n; ++i) { on[i] = m_needCache[i]; }

	const unsigned int colBeam = 0xFF000000u
		| (static_cast<unsigned int>(GR::BeamB * 255.0f) << 16)
		| (static_cast<unsigned int>(GR::BeamG * 255.0f) << 8)
		|  static_cast<unsigned int>(GR::BeamR * 255.0f);

	const unsigned int colPost = 0xFF000000u
		| (static_cast<unsigned int>(GR::PostColB * 255.0f) << 16)
		| (static_cast<unsigned int>(GR::PostColG * 255.0f) << 8)
		|  static_cast<unsigned int>(GR::PostColR * 255.0f);

	//===== Ｗビームの断面 =====
	// 路面からの高さと、道側への出っ張り。
	//
	// 下の縁 → 下の波 → 中央の谷 → 上の波 → 上の縁 で波が2つ。
	// 上下の縁は裏へ折り返して断面を閉じる。
	//
	// 平らな板では衝突で折れるので、実物はこの形で断面の高さを稼ぐ。
	// 見た目のためではなく、この形でないとガードレールにならない。
	//
	// 前は3点で中を膨らませただけだったので波が1つしかなく、
	// 樋を横にしたように見えていた
	static constexpr int BeamNodes = 7;

	const float mid = (GR::BeamBottom + GR::BeamTop) * 0.5f;
	const float qtr = (GR::BeamTop - GR::BeamBottom) * 0.25f;

	const float ys[BeamNodes] = {
		GR::BeamBottom,   // 折り返しの内端
		GR::BeamBottom,   // 下の縁
		mid - qtr,        // 下の波の頂点
		mid,              // 中央の谷(ここで支柱に留まる)
		mid + qtr,        // 上の波の頂点
		GR::BeamTop,      // 上の縁
		GR::BeamTop,      // 折り返しの内端
	};

	// 道側への出っ張り。負は裏へ折り返す
	const float outs[BeamNodes] = {
		-GR::BeamLip,
		 0.0f,
		 GR::BeamCrest,
		 GR::BeamCrest - GR::BeamValley,
		 GR::BeamCrest,
		 0.0f,
		-GR::BeamLip,
	};

	float nextPost = -1e9f;

	for (int i = 0; i < n; )
	{
		if (!on[i]) { ++i; continue; }

		int j = i;
		while (j < n && on[j]) { ++j; }

		//===== ビーム =====
		const UINT base = static_cast<UINT>(verts.size());

		for (int k = i; k < j; ++k)
		{
			Math::Vector3 center, right;
			float miter = 1.0f;
			road.CrossAt(k, center, right, miter);

			// 柵が立つ高さ。路肩の外なので断面式は使わない。
			// 断面式は舗装の外で一定値になるので、平場を詰めた区間では浮く
			const float roadY = road.GroundAt(k, GR::Offset * side);
			const float s     = road.StationAt(k);

			// 断面の「線分」ごとに頂点を持つ。
			//
			// 点を共有すると、波の折れ目で法線が均されて波が消える。
			// フラットに塗り分かれてこそ、波形の板に見える
			for (int sg = 0; sg + 1 < BeamNodes; ++sg)
			{
				// 断面の向きから法線を作る。面は道の側を向く
				const float tx = outs[sg + 1] - outs[sg];
				const float ty = ys[sg + 1]   - ys[sg];

				const float len = sqrtf(tx * tx + ty * ty);
				const float nx2 = (len > 1e-6f) ? ( ty / len) : 1.0f;
				const float ny2 = (len > 1e-6f) ? (-tx / len) : 0.0f;

				const Math::Vector3 nrm = right * (side * nx2)
				                        + Math::Vector3::Up * ny2;

				for (int e = 0; e < 2; ++e)
				{
					const int cn = sg + e;

					// 出っ張りは道側。中心線から見て内へ寄せる
					const float off = (GR::Offset - outs[cn]) * side * miter;

					KdMeshVertex v;
					v.Pos = Math::Vector3(center.x + right.x * off,
						                      roadY + ys[cn],
						                      center.z + right.z * off);
					v.Normal  = nrm;

					// 上向きに決め打ちしない。
					// 板の上下の折り返しは水平なので、法線も上向きになり、
					// 接線と平行になって接空間が潰れる
					v.Tangent = HjMeshUtil::TangentFor(nrm);
					v.Color   = colBeam;
					v.UV      = Math::Vector2(ys[cn], s * 0.5f);
					verts.push_back(v);
				}
			}

			m_length += (k > i) ? (road.StationAt(k) - road.StationAt(k - 1)) : 0.0f;
		}

		const int runRows = j - i;
		const int perRow  = (BeamNodes - 1) * 2;

		for (int r = 0; r + 1 < runRows; ++r)
		{
			for (int sg = 0; sg + 1 < BeamNodes; ++sg)
			{
				const UINT i0 = base + static_cast<UINT>(r * perRow + sg * 2);
				const UINT i1 = i0 + 1;
				const UINT i2 = base + static_cast<UINT>((r + 1) * perRow + sg * 2);
				const UINT i3 = i2 + 1;

				// 表と裏の両方を張る。
				// 柵は内側からも外側からも見えるので、片面だと消える
				faces.push_back({ { i0, i2, i1 } });
				faces.push_back({ { i1, i2, i3 } });
				faces.push_back({ { i0, i1, i2 } });
				faces.push_back({ { i1, i3, i2 } });
			}
		}

		//===== 支柱 =====
		for (int k = i; k < j; ++k)
		{
			const float s = road.StationAt(k);
			if (s < nextPost) { continue; }
			nextPost = s + GR::PostSpacing;

			Math::Vector3 center, right;
			float miter = 1.0f;
			road.CrossAt(k, center, right, miter);

			// 柵が立つ高さ。路肩の外なので断面式は使わない。
			// 断面式は舗装の外で一定値になるので、平場を詰めた区間では浮く
			const float roadY = road.GroundAt(k, GR::Offset * side);
			const float off   = GR::Offset * side * miter;

			const Math::Vector3 p(center.x + right.x * off, roadY,
			                      center.z + right.z * off);

			// 進む向き。支柱を板の裏へ下げるのに使う
			const Math::Vector3 fwd = Math::Vector3::Up.Cross(right) * -1.0f;

			// 支柱は板の裏。実物も板が手前で支柱が奥。
			// 同じ面に置くと、板から柱が生えているように見える
			const Math::Vector3 q = p + right * (GR::PostBack * side);

			//===== 丸パイプ =====
			// 実物はφ139.7mmの鋼管。四角い柱のガードレールは無い
			const UINT pb = static_cast<UINT>(verts.size());

			for (int sd = 0; sd < GR::PostSides; ++sd)
			{
				const float ang = 6.28318530718f * sd / GR::PostSides;

				const Math::Vector3 d = right * (cosf(ang) * GR::PostRadius)
				                      + fwd   * (sinf(ang) * GR::PostRadius);

				for (int lv = 0; lv < 2; ++lv)
				{
					const float y = (lv == 0) ? GR::PostBottom : GR::PostTop;

					KdMeshVertex v;
					v.Pos     = Math::Vector3(q.x + d.x, roadY + y, q.z + d.z);
					v.Normal  = d;
					v.Tangent = Math::Vector3::Up;
					v.Color   = colPost;
					v.UV      = Math::Vector2(static_cast<float>(sd), static_cast<float>(lv));
					verts.push_back(v);
				}
			}

			// 側面。上下の蓋は見えないので張らない
			for (int sd = 0; sd < GR::PostSides; ++sd)
			{
				const UINT a0 = pb + static_cast<UINT>(sd * 2);
				const UINT a1 = a0 + 1;
				const UINT b0 = pb + static_cast<UINT>(((sd + 1) % GR::PostSides) * 2);
				const UINT b1 = b0 + 1;

				faces.push_back({ { a0, a1, b0 } });
				faces.push_back({ { a1, b1, b0 } });
			}

			++m_posts;
		}

		i = j;
	}
}

//----------------------------------------------------------
// 道と地形から組む
//----------------------------------------------------------
void HjGuardRail::Build(const HjRoad& road, const HjHeightField* field)
{
	m_spMesh = nullptr;
	m_length = 0.0f;
	m_posts  = 0;

	m_drawType = eDrawTypeLit;

	if (m_materials.empty())
	{
		m_materials.resize(1);
		m_materials[0].m_name = "guardrail";

		// 色は頂点に持たせる。
		// ビームと支柱で材質を分けると、メッシュを2枚にすることになる
		m_materials[0].m_baseColorRate = Math::Vector4::One;
	}

	const int n = road.StationCount();
	if (n < 2) { return; }

	std::vector<KdMeshVertex> verts;
	std::vector<KdMeshFace>   faces;

	for (int sideIndex = 0; sideIndex < 2; ++sideIndex)
	{
		const float side = (sideIndex == 0) ? -1.0f : 1.0f;

		// この側で柵が要る所を出して、飛び飛びを整える
		m_needCache.assign(n, false);
		for (int i = 0; i < n; ++i)
		{
			m_needCache[i] = NeedRail(road, field, i, side);
		}
		TidyRuns(road, m_needCache);

		BuildSide(road, side, verts, faces);
	}

	if (faces.empty()) { return; }

	std::vector<KdMeshSubset> subsets(1);
	subsets[0].MaterialNo = 0;
	subsets[0].FaceStart  = 0;
	subsets[0].FaceCount  = static_cast<UINT>(faces.size());

	m_spMesh = std::make_shared<KdMesh>();
	if (!m_spMesh->Create(verts, faces, subsets, false)) { m_spMesh = nullptr; }
}

//----------------------------------------------------------
void HjGuardRail::DrawLit()
{
	if (!m_spMesh || !m_visible) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}

//----------------------------------------------------------
void HjGuardRail::GenerateDepthMapFromLight()
{
	if (!m_spMesh || !m_visible) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}
