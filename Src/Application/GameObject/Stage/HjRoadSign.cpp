#include "HjRoadSign.h"

#include "HjRoad.h"

#include "../../Const/RoadSignConst.h"
#include "../../Util/HjProfiler.h"
#include "../../Util/HjMeshUtil.h"

namespace RS = RoadSignConst;

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

	//======================================================
	// 板の上の平らな多角形を1枚足す
	//
	// 板の面の中で (u, v) で位置を指定する。
	// 板ごとに座標を組み直さなくて済むので、図柄が書きやすい
	//
	// 表と裏の両方を張る。標識の裏は実際には灰色の板だが、
	// ここでは同じ色で閉じておく(裏から見る場面が無い)
	//======================================================
	void AddFlat(std::vector<KdMeshVertex>& verts, std::vector<KdMeshFace>& faces,
	             const Math::Vector3& origin,
	             const Math::Vector3& axU, const Math::Vector3& axV,
	             const Math::Vector3& normal,
	             const float* uv, int count, unsigned int col)
	{
		if (count < 3) { return; }

		const UINT base = static_cast<UINT>(verts.size());

		for (int i = 0; i < count; ++i)
		{
			KdMeshVertex v;
			v.Pos     = origin + axU * uv[i * 2] + axV * uv[i * 2 + 1];
			v.Normal  = normal;
			v.Tangent = axU;
			v.Color   = col;
			v.UV      = Math::Vector2(uv[i * 2], uv[i * 2 + 1]);
			verts.push_back(v);
		}

		// 扇に割る。凸な形だけを渡す前提
		for (int i = 1; i + 1 < count; ++i)
		{
			faces.push_back({ { base,
			                    base + static_cast<UINT>(i),
			                    base + static_cast<UINT>(i + 1) } });

			// 裏面。片面だと、光の当たり方で真っ黒に見えることがある
			faces.push_back({ { base,
			                    base + static_cast<UINT>(i + 1),
			                    base + static_cast<UINT>(i) } });
		}
	}

	// 箱を1つ。柱に使う
	void AddBox(std::vector<KdMeshVertex>& verts, std::vector<KdMeshFace>& faces,
	            const Math::Vector3& center,
	            const Math::Vector3& ax, const Math::Vector3& ay, const Math::Vector3& az,
	            unsigned int col)
	{
		Math::Vector3 p[8];
		for (int i = 0; i < 8; ++i)
		{
			p[i] = center
			     + ax * ((i & 1) ? 1.0f : -1.0f)
			     + ay * ((i & 2) ? 1.0f : -1.0f)
			     + az * ((i & 4) ? 1.0f : -1.0f);
		}

		static const int face[6][4] = {
			{ 0, 2, 3, 1 }, { 5, 7, 6, 4 },
			{ 4, 6, 2, 0 }, { 1, 3, 7, 5 },
			{ 0, 1, 5, 4 }, { 6, 7, 3, 2 },
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
				v.UV      = Math::Vector2(0.0f, 0.0f);
				verts.push_back(v);
			}

			faces.push_back({ { base, base + 1, base + 2 } });
			faces.push_back({ { base, base + 2, base + 3 } });
		}
	}
}

//----------------------------------------------------------
// 1本ぶん
//
// 柱 → 黒縁の菱形 → 黄色の菱形 → 黒い矢印 の順に重ねる。
// 後に足したものが前に来るよう、少しずつ手前へ出す
//----------------------------------------------------------
void HjRoadSign::AddSign(const Math::Vector3& base,
                         const Math::Vector3& face, const Math::Vector3& right,
                         bool toRight,
                         std::vector<KdMeshVertex>& verts,
                         std::vector<KdMeshFace>& faces)
{
	const unsigned int ink  = PackColor(RS::InkR,    RS::InkG,    RS::InkB);
	const unsigned int yell = PackColor(RS::PlateR_, RS::PlateG_, RS::PlateB_);
	const unsigned int pole = PackColor(RS::PoleR,   RS::PoleG,   RS::PoleB);

	//===== 柱 =====
	{
		const float h = RS::PoleH + RS::PoleSink;

		const Math::Vector3 c = base
		                      + Math::Vector3::Up * (h * 0.5f - RS::PoleSink);

		AddBox(verts, faces, c,
		       right * RS::PoleHalf,
		       Math::Vector3::Up * (h * 0.5f),
		       face * RS::PoleHalf,
		       pole);
	}

	// 板の中心。柱の上端に菱形の下角が来るように置く
	const Math::Vector3 center = base
	                           + Math::Vector3::Up * (RS::PoleH + RS::PlateR);

	// 板の面の軸。u=横 v=縦、法線は上流へ
	const Math::Vector3 axU = right;
	const Math::Vector3 axV = Math::Vector3::Up;

	//===== 黒縁の菱形 =====
	{
		const float r = RS::PlateR;
		const float d[8] = { 0.0f, r,  r, 0.0f,  0.0f, -r,  -r, 0.0f };

		AddFlat(verts, faces, center, axU, axV, face, d, 4, ink);
	}

	//===== 黄色の菱形 =====
	{
		const float r = RS::PlateR - RS::PlateBorder;
		const float d[8] = { 0.0f, r,  r, 0.0f,  0.0f, -r,  -r, 0.0f };

		AddFlat(verts, faces,
		        center + face * RS::PlateLift, axU, axV, face, d, 4, yell);
	}

	//===== 黒い矢印 =====
	// 下から上がってきて、横へ折れる形。
	// 右カーブなら右へ、左カーブなら左へ折る
	{
		const float s = toRight ? 1.0f : -1.0f;

		const float w  = RS::ArrowW * 0.5f;
		const float dn = RS::ArrowDown;
		const float sd = RS::ArrowSide;
		const float hd = RS::ArrowHead;

		const Math::Vector3 org = center + face * (RS::PlateLift * 2.0f);

		// 縦棒
		{
			const float d[8] = { -w, -dn,  w, -dn,  w, w,  -w, w };
			AddFlat(verts, faces, org, axU, axV, face, d, 4, ink);
		}

		// 横棒。折れ目で切れないよう、縦棒と重ねて始める
		{
			const float x0 = -w * s;
			const float x1 = (sd) * s;

			const float d[8] = { x0, -w,  x1, -w,  x1, w,  x0, w };
			AddFlat(verts, faces, org, axU, axV, face, d, 4, ink);
		}

		// 頭の三角
		{
			const float x0 = sd * s;

			const float d[6] = { x0, -hd,  x0, hd,  (sd + hd) * s, 0.0f };
			AddFlat(verts, faces, org, axU, axV, face, d, 3, ink);
		}
	}
}

//----------------------------------------------------------
void HjRoadSign::Build(const HjRoad& road)
{
	HjScopedTimer _t(U8("標識を組む"));

	m_drawType = eDrawTypeLit;
	m_spMesh.reset();
	m_signs = 0;

	if (m_materials.empty())
	{
		m_materials.resize(1);
		m_materials[0].m_name = "roadsign";
		m_materials[0].m_baseColorRate = Math::Vector4::One;
	}

	const int n = road.StationCount();
	if (n < 3) { return; }

	//===== カーブの入口を拾う =====
	// 曲がりが閾値を超えた瞬間を入口とする。
	// 中で何度も立てないよう、抜けるまでは次を探さない
	std::vector<KdMeshVertex> verts;
	std::vector<KdMeshFace>   faces;

	float lastS = -1e9f;
	bool  inCurve = false;
	int   runStart = 0;
	float peak = 0.0f;

	// 区間が確定してから、その頂点の向きで立てる。
	//
	// 入口の曲がりだけで決めると、そこはまだ曲がりが弱いので
	// 符号が安定せず、S字の入りでは向きを取り違える
	auto place = [&]()
	{
		if (m_signs >= RS::MaxSigns) { return; }

		// 入口から手前へ下げる。
		// 標識はこれから起きることを伝えるもので、
		// カーブの中に置いても読む時間が無い
		const float wantS = road.StationAt(runStart) - RS::LeadIn;
		if (wantS < 0.0f) { return; }
		if (wantS - lastS < RS::MinGap) { return; }

		// その道のりに一番近い刻みを探す
		int   at = runStart;
		float bestD = 1e18f;
		for (int k = 0; k < n; ++k)
		{
			const float d = fabsf(road.StationAt(k) - wantS);
			if (d < bestD) { bestD = d; at = k; }
		}

		lastS = wantS;

		Math::Vector3 center, right;
		float miter = 1.0f;
		road.CrossAt(at, center, right, miter);

		// 日本は左側通行。標識は進行方向の左に立つ。
		// 右に立てると対向車線へ向けた標識になる
		const float side = -1.0f;

		const float off = RS::Offset * side;
		const float y   = road.GroundAt(at, off);

		const float wo = off * miter;

		const Math::Vector3 pos(center.x + right.x * wo, y, center.z + right.z * wo);

		// 板は上流(来る方向)を向く。
		// 断面の向きと直交するのが進む向きなので、その逆
		const Math::Vector3 fwd(-right.z, 0.0f, right.x);

		AddSign(pos, -fwd, right, (peak > 0.0f), verts, faces);
		++m_signs;
	};

	for (int i = 1; i < n; ++i)
	{
		// 向きの決め方は道が持っている。
		// 写して持つと、符号を取り違えたときに直す所が散らばる
		const float turn = road.TurnAt(i);

		if (fabsf(turn) >= RS::TurnMin)
		{
			if (!inCurve)
			{
				inCurve  = true;
				runStart = i;
				peak     = turn;
			}
			else if (fabsf(turn) > fabsf(peak))
			{
				peak = turn;
			}
			continue;
		}

		if (!inCurve) { continue; }

		inCurve = false;
		place();
	}

	// 道の終わりがカーブの途中だった場合
	if (inCurve) { place(); }

	if (faces.empty()) { return; }

	std::vector<KdMeshSubset> subsets(1);
	subsets[0].MaterialNo = 0;
	subsets[0].FaceStart  = 0;
	subsets[0].FaceCount  = static_cast<UINT>(faces.size());

	m_spMesh = std::make_shared<KdMesh>();
	if (!m_spMesh->Create(verts, faces, subsets, false)) { m_spMesh = nullptr; }
}

//----------------------------------------------------------
void HjRoadSign::DrawLit()
{
	if (!m_visible || !m_spMesh) { return; }

	HjScopedTimer _t(U8(" 標識の描画"));

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}

//----------------------------------------------------------
void HjRoadSign::GenerateDepthMapFromLight()
{
	if (!m_visible || !m_spMesh) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}
