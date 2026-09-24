#include "HjDelineator.h"

#include "HjRoad.h"

#include "../../Const/DelineatorConst.h"
#include "../../Util/HjProfiler.h"
#include "../../Util/HjMeshUtil.h"

namespace DC = DelineatorConst;

namespace
{
	unsigned int PackColor(float r, float g, float b)
	{
		auto to8 = [](float v) -> unsigned int
		{
			const float c = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
			return static_cast<unsigned int>(c * 255.0f + 0.5f);
		};

		return 0xFF000000u | (to8(b) << 16) | (to8(g) << 8) | to8(r);
	}

	// 箱を1つ足す。
	//
	// 柱も反射板も箱なので、これ1つで済む。
	// 法線は面から出して、6面それぞれで平らに塗り分かれるようにする
	void AddBox(std::vector<KdMeshVertex>& verts, std::vector<KdMeshFace>& faces,
	            const Math::Vector3& center,
	            const Math::Vector3& ax, const Math::Vector3& ay, const Math::Vector3& az,
	            unsigned int col)
	{
		// 8隅
		Math::Vector3 p[8];
		for (int i = 0; i < 8; ++i)
		{
			p[i] = center
			     + ax * ((i & 1) ? 1.0f : -1.0f)
			     + ay * ((i & 2) ? 1.0f : -1.0f)
			     + az * ((i & 4) ? 1.0f : -1.0f);
		}

		// 面ごとの4隅(反時計回り)
		static const int face[6][4] = {
			{ 0, 2, 3, 1 },   // -Z
			{ 5, 7, 6, 4 },   // +Z
			{ 4, 6, 2, 0 },   // -X
			{ 1, 3, 7, 5 },   // +X
			{ 0, 1, 5, 4 },   // -Y
			{ 6, 7, 3, 2 },   // +Y
		};

		for (const auto& f : face)
		{
			Math::Vector3 n = (p[f[1]] - p[f[0]]).Cross(p[f[2]] - p[f[0]]);
			if (n.LengthSquared() < 1e-12f) { continue; }
			n.Normalize();

			const UINT base = static_cast<UINT>(verts.size());

			for (int k = 0; k < 4; ++k)
			{
				KdMeshVertex v;
				v.Pos     = p[f[k]];
				v.Normal  = n;
				// 法線と平行だと接空間が潰れて面が真っ黒になる。
				// 箱の上下面がちょうどそれに当たる
				v.Tangent = HjMeshUtil::TangentFor(n);
				v.Color   = col;
				v.UV      = Math::Vector2(static_cast<float>(k & 1),
				                          static_cast<float>((k >> 1) & 1));
				verts.push_back(v);
			}

			faces.push_back({ { base, base + 1, base + 2 } });
			faces.push_back({ { base, base + 2, base + 3 } });
		}
	}
}

//----------------------------------------------------------
// 1本ぶん
//----------------------------------------------------------
void HjDelineator::AddPost(const Math::Vector3& base, const Math::Vector3& right,
                           float side,
                           std::vector<KdMeshVertex>& verts,
                           std::vector<KdMeshFace>& faces)
{
	// 柱の向き。断面の向きに合わせる
	const Math::Vector3 ax = right * DC::PostHalf;
	const Math::Vector3 az = Math::Vector3(-right.z, 0.0f, right.x) * DC::PostHalf;

	//===== 柱 =====
	{
		const float h = DC::PostH + DC::PostSink;

		const Math::Vector3 c = base
		                      + Math::Vector3::Up * (h * 0.5f - DC::PostSink);

		AddBox(verts, faces, c, ax, Math::Vector3::Up * (h * 0.5f), az,
		       PackColor(DC::PostR, DC::PostG, DC::PostB));
	}

	//===== 反射板 =====
	// 進む向きに対して右が橙、左が白。
	// 夜にどちら側の路肩を見ているのかが色で分かる
	{
		const bool isRight = (side > 0.0f);

		const unsigned int col = isRight
			? PackColor(DC::RightR, DC::RightG, DC::RightB)
			: PackColor(DC::LeftR,  DC::LeftG,  DC::LeftB);

		// 道側へ少し出す。柱に埋もれると光って見えない
		const Math::Vector3 c = base
		                      + Math::Vector3::Up * DC::PlateY
		                      - right * side * (DC::PostHalf + DC::PlateOut);

		AddBox(verts, faces, c,
		       right * (DC::PlateOut * 0.5f),
		       Math::Vector3::Up * (DC::PlateH * 0.5f),
		       Math::Vector3(-right.z, 0.0f, right.x) * (DC::PlateW * 0.5f),
		       col);
	}
}

//----------------------------------------------------------
void HjDelineator::Build(const HjRoad& road)
{
	HjScopedTimer _t(U8("視線誘導標を組む"));

	m_drawType = eDrawTypeLit;
	m_spMesh.reset();
	m_posts = 0;

	if (m_materials.empty())
	{
		m_materials.resize(1);
		m_materials[0].m_name = "delineator";
		m_materials[0].m_baseColorRate = Math::Vector4::One;

	}

	const int n = road.StationCount();
	if (n < 3) { return; }

	std::vector<KdMeshVertex> verts;
	std::vector<KdMeshFace>   faces;

	// 前に置いた道のり。間隔を測るのに使う
	float lastS = -1e9f;

	for (int i = 0; i < n; ++i)
	{
		if (m_posts >= DC::MaxPosts) { break; }

		// 向きの決め方は道が持っている。
		// 写して持つと、符号を取り違えたときに直す所が散らばる
		const float turn = road.TurnAt(i);
		const float mag  = fabsf(turn);

		// 直線には置かない。並べても意味が無いうえ、数が増える
		if (mag < DC::TurnMin) { continue; }

		// 曲がりがきついほど詰める。実際の道路もそうしている
		const float t = std::clamp((mag - DC::TurnMin)
		                         / std::max(DC::TurnTight - DC::TurnMin, 1e-5f),
		                           0.0f, 1.0f);

		const float spacing = DC::SpacingStraight
		                    + (DC::SpacingTight - DC::SpacingStraight) * t;

		const float s = road.StationAt(i);
		if (s - lastS < spacing) { continue; }

		lastS = s;

		// カーブの外側。右へ曲がっているなら左が外
		const float side = (turn > 0.0f) ? -1.0f : 1.0f;

		Math::Vector3 center, right;
		float miter = 1.0f;
		road.CrossAt(i, center, right, miter);

		const float off = DC::Offset * side;
		// 路肩の上に立つが、平場を詰めた区間では路肩も削られる。
		// 断面式ではなく実際の接地面を引く
		const float y   = road.GroundAt(i, off);

		const float wo = off * miter;

		const Math::Vector3 base(center.x + right.x * wo, y, center.z + right.z * wo);

		AddPost(base, right, side, verts, faces);
		++m_posts;
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
void HjDelineator::DrawLit()
{
	if (!m_visible || !m_spMesh) { return; }

	HjScopedTimer _t(U8(" 視線誘導標の描画"));

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}

//----------------------------------------------------------
void HjDelineator::GenerateDepthMapFromLight()
{
	if (!m_visible || !m_spMesh) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}
