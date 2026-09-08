#include "HjGuardRail.h"

#include "HjRoad.h"
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
	if (!field || !field->IsValid()) { return false; }

	Math::Vector3 center, right;
	float miter = 1.0f;
	road.CrossAt(step, center, right, miter);

	// 路面の縁の高さ
	const float edgeY = road.HeightAtOffset(step, GR::Offset * side);

	// そこから少し外の地形
	const float off = (GR::Offset + GR::LookOut) * side * miter;

	const float wx = center.x + right.x * off;
	const float wz = center.z + right.z * off;

	if (!field->Contains(wx, wz)) { return false; }

	const float groundY = field->HeightAt(wx, wz);

	return (edgeY - groundY) > GR::DropThreshold;
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
		| (static_cast<unsigned int>(GR::PostB * 255.0f) << 16)
		| (static_cast<unsigned int>(GR::PostG * 255.0f) << 8)
		|  static_cast<unsigned int>(GR::PostR * 255.0f);

	// 断面の高さ(路面から)。真ん中は外へ張り出す
	const float ys[3]   = { GR::BeamLow, (GR::BeamLow + GR::BeamHigh) * 0.5f, GR::BeamHigh };
	const float bulge[3] = { 0.0f, GR::BeamBulge, 0.0f };

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

			const float roadY = road.HeightAtOffset(k, GR::Offset * side);
			const float s     = road.StationAt(k);

			for (int c = 0; c < 3; ++c)
			{
				const float off = (GR::Offset + bulge[c] * side * side) * side * miter;

				KdMeshVertex v;
				v.Pos = Math::Vector3(center.x + right.x * off,
				                      roadY + ys[c],
				                      center.z + right.z * off);
				v.Normal  = right * side;
				v.Tangent = Math::Vector3::Up;
				v.Color   = colBeam;
				v.UV      = Math::Vector2(static_cast<float>(c) * 0.5f, s * 0.5f);
				verts.push_back(v);
			}

			m_length += (k > i) ? (road.StationAt(k) - road.StationAt(k - 1)) : 0.0f;
		}

		const int runRows = j - i;
		for (int r = 0; r + 1 < runRows; ++r)
		{
			for (int c = 0; c < 2; ++c)
			{
				const UINT i0 = base + static_cast<UINT>(r * 3 + c);
				const UINT i1 = i0 + 1;
				const UINT i2 = i0 + 3;
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

			const float roadY = road.HeightAtOffset(k, GR::Offset * side);
			const float off   = GR::Offset * side * miter;

			const Math::Vector3 p(center.x + right.x * off, roadY,
			                      center.z + right.z * off);

			// 進む向き。箱の奥行きに使う
			const Math::Vector3 fwd = Math::Vector3::Up.Cross(right) * -1.0f;

			const UINT pb = static_cast<UINT>(verts.size());

			for (int corner = 0; corner < 4; ++corner)
			{
				const float sx = (corner == 0 || corner == 3) ? -1.0f : 1.0f;
				const float sz = (corner < 2) ? -1.0f : 1.0f;

				const Math::Vector3 d = right * (GR::PostHalf * sx)
				                      + fwd   * (GR::PostHalf * sz);

				for (int lv = 0; lv < 2; ++lv)
				{
					const float y = (lv == 0) ? GR::PostBottom : GR::PostTop;

					KdMeshVertex v;
					v.Pos     = Math::Vector3(p.x + d.x, roadY + y, p.z + d.z);
					v.Normal  = d;
					v.Tangent = Math::Vector3::Up;
					v.Color   = colPost;
					v.UV      = Math::Vector2(0.0f, 0.0f);
					verts.push_back(v);
				}
			}

			// 側面4枚。上下の蓋は見えないので張らない
			for (int c = 0; c < 4; ++c)
			{
				const UINT a0 = pb + static_cast<UINT>(c * 2);
				const UINT a1 = a0 + 1;
				const UINT b0 = pb + static_cast<UINT>(((c + 1) % 4) * 2);
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
