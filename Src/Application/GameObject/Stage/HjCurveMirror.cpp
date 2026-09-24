#include "HjCurveMirror.h"

#include "HjRoad.h"

#include "../../Const/CurveMirrorConst.h"
#include "../../Util/HjProfiler.h"
#include "../../Util/HjMeshUtil.h"

namespace CM = CurveMirrorConst;

namespace
{
	constexpr float Pi2 = 6.28318530718f;
	constexpr float Deg = 3.14159265f / 180.0f;

	unsigned int PackColor(float r, float g, float b)
	{
		auto to8 = [](float v) -> unsigned int
		{
			const float c = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
			return static_cast<unsigned int>(c * 255.0f + 0.5f);
		};

		return 0xFF000000u | (to8(b) << 16) | (to8(g) << 8) | to8(r);
	}

	// 三角を1枚。法線は面から出して平らに塗り分ける
	void AddTri(std::vector<KdMeshVertex>& verts, std::vector<KdMeshFace>& faces,
	            const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c,
	            unsigned int col)
	{
		Math::Vector3 n = (b - a).Cross(c - a);
		if (n.LengthSquared() < 1e-12f) { return; }
		n.Normalize();

		const UINT base = static_cast<UINT>(verts.size());

		const Math::Vector3 p[3] = { a, b, c };
		for (int i = 0; i < 3; ++i)
		{
			KdMeshVertex v;
			v.Pos     = p[i];
			v.Normal  = n;
			v.Tangent = HjMeshUtil::TangentFor(n);
			v.Color   = col;
			v.UV      = Math::Vector2(0.0f, 0.0f);
			verts.push_back(v);
		}

		faces.push_back({ { base, base + 1, base + 2 } });
	}

	// 四角を1枚
	void AddQuad(std::vector<KdMeshVertex>& verts, std::vector<KdMeshFace>& faces,
	             const Math::Vector3& a, const Math::Vector3& b,
	             const Math::Vector3& c, const Math::Vector3& d,
	             unsigned int col)
	{
		AddTri(verts, faces, a, b, c, col);
		AddTri(verts, faces, a, c, d, col);
	}

	//======================================================
	// 筒を1本
	//
	// 柱も腕も筒。四角い柱のカーブミラーは無い
	//======================================================
	void AddTube(std::vector<KdMeshVertex>& verts, std::vector<KdMeshFace>& faces,
	             const Math::Vector3& from, const Math::Vector3& to,
	             float radius, int sides, unsigned int col)
	{
		Math::Vector3 axis = to - from;
		const float len = axis.Length();
		if (len < 1e-5f) { return; }
		axis /= len;

		// 軸と平行でない向きから、断面の2軸を作る
		const Math::Vector3 helper = (fabsf(axis.y) < 0.9f)
			? Math::Vector3::Up : Math::Vector3::Right;

		Math::Vector3 u = helper.Cross(axis);
		if (u.LengthSquared() < 1e-8f) { u = Math::Vector3::Right; }
		u.Normalize();

		Math::Vector3 w = axis.Cross(u);
		w.Normalize();

		for (int i = 0; i < sides; ++i)
		{
			const float a0 = Pi2 * i / sides;
			const float a1 = Pi2 * (i + 1) / sides;

			const Math::Vector3 d0 = u * (cosf(a0) * radius) + w * (sinf(a0) * radius);
			const Math::Vector3 d1 = u * (cosf(a1) * radius) + w * (sinf(a1) * radius);

			AddQuad(verts, faces,
			        from + d0, from + d1, to + d1, to + d0, col);
		}

		// 先の蓋。切り口が抜けて見えないように
		for (int i = 0; i < sides; ++i)
		{
			const float a0 = Pi2 * i / sides;
			const float a1 = Pi2 * (i + 1) / sides;

			const Math::Vector3 d0 = u * (cosf(a0) * radius) + w * (sinf(a0) * radius);
			const Math::Vector3 d1 = u * (cosf(a1) * radius) + w * (sinf(a1) * radius);

			AddTri(verts, faces, to, to + d0, to + d1, col);
		}
	}
}

//----------------------------------------------------------
// 鏡1枚
//
// 凸の鏡面 → 橙の縁 → 裏の椀 → 縁に沿った庇。
//
// ■ 平らな円板では鏡に見えない
// カーブミラーが広い範囲を映せるのは面が凸だから。
// 平らにすると、鏡ではなく白い丸看板になる。
//
// ■ 庇は曲面
// 縁の上側に沿って回り込む。平らな箱を乗せると、
// 鏡に板を立てかけたようにしか見えない
//----------------------------------------------------------
void HjCurveMirror::AddFace(const Math::Vector3& center, const Math::Vector3& normal,
                            std::vector<KdMeshVertex>& verts,
                            std::vector<KdMeshFace>& faces)
{
	// 鏡の面の中での横と縦。縦は上向きに揃える
	Math::Vector3 axU = Math::Vector3::Up.Cross(normal);
	if (axU.LengthSquared() < 1e-8f) { axU = Math::Vector3::Right; }
	axU.Normalize();

	Math::Vector3 axV = normal.Cross(axU);
	axV.Normalize();

	const unsigned int colGlass = PackColor(CM::GlassR, CM::GlassG, CM::GlassB);
	const unsigned int colFrame = PackColor(CM::FrameR, CM::FrameG, CM::FrameB);
	const unsigned int colPole  = PackColor(CM::PoleColR, CM::PoleColG, CM::PoleColB);

	const int  N = CM::MirrorSides;
	const float rGlass = CM::MirrorR - CM::MirrorFrame;

	// 面の上の点。半径 r、角 a、手前へ d
	auto at = [&](float r, float a, float d) -> Math::Vector3
	{
		return center
		     + axU * (cosf(a) * r)
		     + axV * (sinf(a) * r)
		     + normal * d;
	};

	// 凸の深さ。中心がもっとも手前へ出る
	auto bulgeAt = [&](float r) -> float
	{
		const float t = std::clamp(r / std::max(rGlass, 1e-4f), 0.0f, 1.0f);
		return CM::Bulge * (1.0f - t * t);
	};

	//===== 鏡面(凸) =====
	// 内側にもう1輪入れて、丸みが陰影に出るようにする。
	// 中心から縁へ一気に張ると、面が1枚の円錐になって光り方が均一になる
	{
		const float rMid = rGlass * 0.55f;

		for (int i = 0; i < N; ++i)
		{
			const float a0 = Pi2 * i / N;
			const float a1 = Pi2 * (i + 1) / N;

			AddTri(verts, faces,
			       at(0.0f, 0.0f, bulgeAt(0.0f)),
			       at(rMid, a0, bulgeAt(rMid)),
			       at(rMid, a1, bulgeAt(rMid)), colGlass);

			AddQuad(verts, faces,
			        at(rMid,   a0, bulgeAt(rMid)),
			        at(rGlass, a0, bulgeAt(rGlass)),
			        at(rGlass, a1, bulgeAt(rGlass)),
			        at(rMid,   a1, bulgeAt(rMid)), colGlass);
		}
	}

	//===== 縁 =====
	// 鏡面より少しだけ手前。実物も縁が一段立っている
	{
		for (int i = 0; i < N; ++i)
		{
			const float a0 = Pi2 * i / N;
			const float a1 = Pi2 * (i + 1) / N;

			AddQuad(verts, faces,
			        at(rGlass,      a0, bulgeAt(rGlass)),
			        at(CM::MirrorR, a0, 0.0f),
			        at(CM::MirrorR, a1, 0.0f),
			        at(rGlass,      a1, bulgeAt(rGlass)), colFrame);
		}
	}

	//===== 裏の椀 =====
	// 実物の裏は平らではなく丸く膨らんでいる。
	// 平らに閉じると、真横から見たときに紙のように見える
	{
		const Math::Vector3 tip = center - normal * CM::BackDepth;

		for (int i = 0; i < N; ++i)
		{
			const float a0 = Pi2 * i / N;
			const float a1 = Pi2 * (i + 1) / N;

			AddTri(verts, faces,
			       tip,
			       at(CM::MirrorR, a1, 0.0f),
			       at(CM::MirrorR, a0, 0.0f), colPole);
		}
	}

	//===== 庇 =====
	// 縁の上側に沿って回り込む曲面。
	// 前へ出しつつ、少し外へ開く
	{
		const float half = CM::HoodArcDeg * 0.5f * Deg;

		// 上を 90度 として、そこから左右へ広げる
		const float a0 = 1.57079633f - half;
		const float a1 = 1.57079633f + half;

		const int seg = std::max(N / 2, 4);

		for (int i = 0; i < seg; ++i)
		{
			const float t0 = a0 + (a1 - a0) * i / seg;
			const float t1 = a0 + (a1 - a0) * (i + 1) / seg;

			// 縁の上と、そこから前へ出した先
			const Math::Vector3 e0 = at(CM::MirrorR, t0, 0.0f);
			const Math::Vector3 e1 = at(CM::MirrorR, t1, 0.0f);

			const Math::Vector3 f0 = at(CM::MirrorR + CM::HoodRise, t0, CM::HoodDepth);
			const Math::Vector3 f1 = at(CM::MirrorR + CM::HoodRise, t1, CM::HoodDepth);

			// 表と裏の両方。庇は下から見上げる場面がある
			AddQuad(verts, faces, e0, f0, f1, e1, colPole);
			AddQuad(verts, faces, e1, f1, f0, e0, colPole);
		}
	}
}

//----------------------------------------------------------
// 1本ぶん
//
// 基礎 → 柱 → 腕 → 鏡2枚。
//
// 鏡は柱に直付けしない。腕で前へ持ち出す。
// 直付けだと柱が鏡を裏から突き上げる形になり、
// 2枚付けたときに互いが柱へめり込む
//----------------------------------------------------------
void HjCurveMirror::AddMirror(const Math::Vector3& base,
                              const Math::Vector3& fwd, const Math::Vector3& right,
                              float outSign,
                              std::vector<KdMeshVertex>& verts,
                              std::vector<KdMeshFace>& faces)
{
	const unsigned int colPole = PackColor(CM::PoleColR, CM::PoleColG, CM::PoleColB);
	const unsigned int colBase = PackColor(CM::BaseColR, CM::BaseColG, CM::BaseColB);

	//===== 基礎 =====
	// コンクリートの根巻き。
	// 無いと、柱が地面へ刺さっただけに見える
	AddTube(verts, faces,
	        base - Math::Vector3::Up * CM::BaseSink,
	        base + Math::Vector3::Up * CM::BaseH,
	        CM::BaseRadius, CM::PoleSides, colBase);

	//===== 柱 =====
	AddTube(verts, faces,
	        base + Math::Vector3::Up * (CM::BaseH - CM::PoleSink),
	        base + Math::Vector3::Up * (CM::PoleH + CM::MirrorR * 0.35f),
	        CM::PoleRadius, CM::PoleSides, colPole);

	// 道の内側。外側に立っているので、内側は外側の逆
	const Math::Vector3 inward = right * (-outSign);

	const float toe = CM::ToeInDeg * Deg;

	//===== 腕と鏡 =====
	// 片方は来る車へ、もう片方は去る車へ。
	//
	// 真後ろへ向けても、見通しを塞いでいる斜面が映るだけ。
	// カーブの先を映すには内側へ振る
	for (int d = 0; d < 2; ++d)
	{
		const float dir = (d == 0) ? 1.0f : -1.0f;

		Math::Vector3 n = fwd * (-dir) * cosf(toe) + inward * sinf(toe);
		if (n.LengthSquared() < 1e-8f) { continue; }
		n.Normalize();

		const Math::Vector3 hub = base + Math::Vector3::Up * CM::PoleH;

		// 腕は柱から鏡の裏へ。鏡の向きの逆へ伸ばす
		const Math::Vector3 head = hub - n * CM::ArmLen;

		AddTube(verts, faces, hub, head, CM::ArmRadius, CM::ArmSides, colPole);

		AddFace(head, n, verts, faces);
	}
}

//----------------------------------------------------------
void HjCurveMirror::Build(const HjRoad& road)
{
	HjScopedTimer _t(U8("カーブミラーを組む"));

	m_drawType = eDrawTypeLit;
	m_spMesh.reset();
	m_count = 0;

	if (m_materials.empty())
	{
		m_materials.resize(1);
		m_materials[0].m_name = "curvemirror";
		m_materials[0].m_baseColorRate = Math::Vector4::One;
	}

	const int n = road.StationCount();
	if (n < 3) { return; }

	std::vector<KdMeshVertex> verts;
	std::vector<KdMeshFace>   faces;

	//===== カーブの頂点を拾う =====
	// 一番きつい所へ立てる。入口だと、まだ塞がれていないので
	// 映しても見えるのは手前の斜面ばかりになる
	bool  inCurve = false;
	int   peakAt  = 0;
	float peak    = 0.0f;

	float lastS = -1e9f;

	auto place = [&]()
	{
		if (m_count >= CM::MaxMirrors) { return; }

		const float s = road.StationAt(peakAt);
		if (s - lastS < CM::MinGap) { return; }
		lastS = s;

		Math::Vector3 center, right;
		float miter = 1.0f;
		road.CrossAt(peakAt, center, right, miter);

		// 外側は曲がる向きの逆
		const float outSign = (peak > 0.0f) ? -1.0f : 1.0f;

		const float off = CM::Offset * outSign;
		// 路肩の外なので、断面式ではなく実際の接地面を引く
		const float y   = road.GroundAt(peakAt, off);

		const float wo = off * miter;

		const Math::Vector3 base(center.x + right.x * wo, y, center.z + right.z * wo);

		// 進む向き。断面の向きと直交する
		const Math::Vector3 fwd(-right.z, 0.0f, right.x);

		AddMirror(base, fwd, right, outSign, verts, faces);
		++m_count;
	};

	for (int i = 1; i < n; ++i)
	{
		float turn = 0.0f;

		if (i < n - 1)
		{
			// 向きの決め方は道が持っている。
			// 写して持つと、符号を取り違えたときに直す所が散らばる
			turn = road.TurnAt(i);
		}

		const bool tight = (fabsf(turn) >= CM::TurnMin);

		if (tight)
		{
			if (!inCurve)
			{
				inCurve = true;
				peakAt  = i;
				peak    = turn;
			}
			else if (fabsf(turn) > fabsf(peak))
			{
				peakAt = i;
				peak   = turn;
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
void HjCurveMirror::DrawLit()
{
	if (!m_visible || !m_spMesh) { return; }

	HjScopedTimer _t(U8(" カーブミラーの描画"));

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}

//----------------------------------------------------------
void HjCurveMirror::GenerateDepthMapFromLight()
{
	if (!m_visible || !m_spMesh) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}
