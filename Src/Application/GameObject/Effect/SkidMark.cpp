#include "SkidMark.h"

using namespace SkidMarkConst;

void SkidMark::Init()
{
	m_trails.assign(TrailCount, Trail{});
	for (int i = 0; i < TrailCount; ++i)
	{
		m_trails[i].pts.reserve(MaxPoints);
		// 0,1=後輪 / 2,3=前輪。前輪は接地幅がやや狭い
		m_trails[i].widthMul = (i >= 2) ? FrontWidthMul : 1.0f;
	}
	m_verts.clear();
	m_verts.reserve(TrailCount * MaxPoints * 6);
}

//----------------------------------------------------------
// 接地点を渡す。前の点から MinSegLen 以上離れたときだけ新しい点を打つ。
// (毎フレーム打つと低速時に点が密集し、上限をすぐ食い潰して痕が短くなる)
//----------------------------------------------------------
void SkidMark::Emit(int trail, const Math::Vector3& pos, const Math::Vector3& right, float strength)
{
	if (trail < 0 || trail >= static_cast<int>(m_trails.size())) { return; }

	// スリップが弱いうちは痕を残さない。次に濃くなった時は新しい痕として始める
	if (strength < SlipMinFor) { Cut(trail); return; }

	Trail& t = m_trails[trail];

	if (!t.pts.empty())
	{
		const Math::Vector3 d = pos - t.pts.back().pos;
		if (d.LengthSquared() < MinSegLen * MinSegLen) { return; }
	}

	// 上限に達したら最も古い点を捨てる。捨てた結果できた先頭は痕の始まりになる
	if (static_cast<int>(t.pts.size()) >= MaxPoints)
	{
		t.pts.erase(t.pts.begin());
		if (!t.pts.empty()) { t.pts.front().joined = false; }
	}

	Point p;
	p.pos      = pos + Math::Vector3(0.0f, GroundOffset, 0.0f);   // 路面へのめり込み防止
	p.right    = right;
	p.age      = 0.0f;
	p.strength = std::min(strength, 1.0f);
	p.joined   = !t.cut;      // 直前が切れていなければ繋ぐ
	t.pts.push_back(p);
	t.cut = false;
}

void SkidMark::Cut(int trail)
{
	if (trail < 0 || trail >= static_cast<int>(m_trails.size())) { return; }
	m_trails[trail].cut = true;
}

void SkidMark::Update(float dt)
{
	for (auto& t : m_trails)
	{
		for (auto& p : t.pts) { p.age += dt; }

		// 先頭(最も古い)から寿命切れを捨てる。点は古い順に並んでいるので前から見れば足りる
		size_t dead = 0;
		while (dead < t.pts.size() && t.pts[dead].age >= Lifetime) { ++dead; }
		if (dead > 0)
		{
			t.pts.erase(t.pts.begin(), t.pts.begin() + dead);
			if (!t.pts.empty()) { t.pts.front().joined = false; }
		}
	}
}

//----------------------------------------------------------
// 描画：隣り合う点の間を四角形(三角形2枚)で繋ぎ、1本ぶんをまとめて1回で描く。
// 濃さは頂点カラーのアルファに焼き込む(UnLit/Litとも頂点カラーが色に掛かる)。
// これで点ごとに描画を分けずに済み、痕が長くても描画回数は本数ぶんで済む。
//----------------------------------------------------------
void SkidMark::DrawEffect()
{
	auto& shaderMgr = KdShaderManager::Instance();
	auto& shader    = shaderMgr.m_StandardShader;

	m_verts.clear();
	m_cull.Update();   // 今フレームの視錐台

	for (const auto& t : m_trails)
	{
		const float halfW = Width * 0.5f * t.widthMul;

		for (size_t i = 1; i < t.pts.size(); ++i)
		{
			const Point& a = t.pts[i - 1];
			const Point& b = t.pts[i];
			if (!b.joined) { continue; }   // 痕の切れ目は繋がない

			// 画面に映らない区間はここで捨てる。痕は長く伸びるので、
			// 走っている間は大半が画面外に残り続ける。
			if (CullingConst::SkidCull)
			{
				const Math::Vector3 mid = (a.pos + b.pos) * 0.5f;
				// 区間をすっぽり包む球＝中点から端までの距離＋幅ぶんの余裕
				const float r = (b.pos - a.pos).Length() * 0.5f + halfW
				              + CullingConst::SkidCullMargin;
				if (!m_cull.IsVisible(mid, r, CullingConst::SkidCullDist)) { continue; }
			}

			// 寿命の後半だけ薄れて消える
			auto alphaOf = [&](const Point& p)
			{
				const float f = p.age / Lifetime;
				const float fade = (f < FadeStart)
					? 1.0f
					: 1.0f - (f - FadeStart) / std::max(1.0f - FadeStart, 1e-4f);
				return std::clamp(fade, 0.0f, 1.0f) * p.strength * AlphaMax;
			};

			auto packCol = [&](float alpha)
			{
				auto to8 = [](float v) { return static_cast<unsigned>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
				return (to8(alpha) << 24) | (to8(m_color.z) << 16) | (to8(m_color.y) << 8) | to8(m_color.x);
			};

			const unsigned int ca = packCol(alphaOf(a));
			const unsigned int cb = packCol(alphaOf(b));

			const Math::Vector3 a0 = a.pos - a.right * halfW;
			const Math::Vector3 a1 = a.pos + a.right * halfW;
			const Math::Vector3 b0 = b.pos - b.right * halfW;
			const Math::Vector3 b1 = b.pos + b.right * halfW;

			KdPolygon::Vertex v;
			v.normal = Math::Vector3(0.0f, 1.0f, 0.0f);   // 路面なので真上

			auto push = [&](const Math::Vector3& p, unsigned int c, float u, float vv)
			{
				v.pos   = p;
				v.color = c;
				v.UV    = { u, vv };
				m_verts.push_back(v);
			};

			// 四角形を三角形2枚に分割
			push(a0, ca, 0.0f, 0.0f); push(a1, ca, 1.0f, 0.0f); push(b1, cb, 1.0f, 1.0f);
			push(a0, ca, 0.0f, 0.0f); push(b1, cb, 1.0f, 1.0f); push(b0, cb, 0.0f, 1.0f);
		}
	}

	if (m_verts.empty()) { return; }

	// 深度は「テストのみ」。書き込むと痕の上に乗る煙やエフェクトが弾かれる。
	// 表裏カリング無効＝坂で裏返っても消えない。
	shaderMgr.ChangeBlendState(KdBlendState::Alpha);
	shader.DrawVertices(m_verts, Math::Matrix::Identity, Math::Color(1, 1, 1, 1),
	                    KdDepthStencilState::ZWriteDisable,
	                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	                    KdRasterizerState::CullNone);
	shaderMgr.UndoBlendState();
}
