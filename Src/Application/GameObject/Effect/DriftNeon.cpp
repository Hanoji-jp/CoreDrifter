#include "DriftNeon.h"

using namespace NeonFxConst;

//----------------------------------------------------------
// 初期化：プール確保＋線分用の板ポリ(テクスチャ無し=白)
//----------------------------------------------------------
void DriftNeon::Init()
{
	m_particles.assign(MaxParticles, Particle{});
	m_next = 0;

	m_poly = std::make_shared<KdSquarePolygon>();
	m_poly->SetScale(1.0f);
	m_poly->Set2DObject(false);

	// 丸い粒用の円盤(半径1, XY平面)を作る。テクスチャを使わないので三角形で円を近似する。
	m_discVerts.clear();
	m_discVerts.reserve(DotSegments * 3);
	for (int i = 0; i < DotSegments; ++i)
	{
		const float a0 = 6.2831853f * static_cast<float>(i)     / DotSegments;
		const float a1 = 6.2831853f * static_cast<float>(i + 1) / DotSegments;

		KdPolygon::Vertex v;
		v.normal = Math::Vector3(0.0f, 0.0f, -1.0f);
		v.color  = 0xFFFFFFFFu;           // 白。色は描画時のcolRateで付ける
		v.UV     = { 0.5f, 0.5f };
		v.pos = Math::Vector3(0.0f, 0.0f, 0.0f);                 m_discVerts.push_back(v);
		v.pos = Math::Vector3(cosf(a0), sinf(a0), 0.0f);          m_discVerts.push_back(v);
		v.pos = Math::Vector3(cosf(a1), sinf(a1), 0.0f);          m_discVerts.push_back(v);
	}

	// 線用の紡錘形(菱形)。Xが長さ方向、Yが太さ方向で、両端が尖る。
	// 長方形だと"棒"に見えてしまうが、両端を尖らせると走る火花・筆跡らしくなる。
	{
		KdPolygon::Vertex v;
		v.normal = Math::Vector3(0.0f, 0.0f, -1.0f);
		v.color  = 0xFFFFFFFFu;
		v.UV     = { 0.5f, 0.5f };

		const Math::Vector3 a(-1.0f,  0.0f, 0.0f);   // 後端(尖り)
		const Math::Vector3 b( 0.0f, -1.0f, 0.0f);   // 下の膨らみ
		const Math::Vector3 c( 1.0f,  0.0f, 0.0f);   // 前端(尖り)
		const Math::Vector3 d( 0.0f,  1.0f, 0.0f);   // 上の膨らみ

		m_streakVerts.clear();
		m_streakVerts.reserve(6);
		v.pos = a; m_streakVerts.push_back(v);
		v.pos = b; m_streakVerts.push_back(v);
		v.pos = c; m_streakVerts.push_back(v);
		v.pos = a; m_streakVerts.push_back(v);
		v.pos = c; m_streakVerts.push_back(v);
		v.pos = d; m_streakVerts.push_back(v);
	}
}

float DriftNeon::Rand01()
{
	m_rng ^= m_rng << 13;
	m_rng ^= m_rng >> 17;
	m_rng ^= m_rng << 5;
	return static_cast<float>(m_rng & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

DriftNeon::Particle& DriftNeon::NextSlot()
{
	Particle& p = m_particles[m_next];
	m_next = (m_next + 1) % static_cast<int>(m_particles.size());
	return p;
}

//----------------------------------------------------------
// 放出：リング(タイヤを囲む輪)＋スパーク(飛び散る線)
//----------------------------------------------------------
void DriftNeon::Emit(const Math::Vector3& pos, const Math::Vector3& axis,
                     const Math::Vector3& carVel, int ringCount, int sparkCount)
{
	if (m_particles.empty()) { return; }

	// リング平面の基底：タイヤ回転軸(車の右方向)に垂直な2軸＝ホイールの面
	Math::Vector3 n = axis;
	if (n.LengthSquared() < 1e-6f) { n = Math::Vector3(1.0f, 0.0f, 0.0f); }
	n.Normalize();
	Math::Vector3 u = Math::Vector3::Up.Cross(n);
	if (u.LengthSquared() < 1e-6f) { u = Math::Vector3(0.0f, 0.0f, 1.0f); }
	u.Normalize();
	const Math::Vector3 v = n.Cross(u);

	for (int i = 0; i < ringCount; ++i)
	{
		Particle& p = NextSlot();
		p.kind   = Kind::Ring;
		p.alive  = true;
		p.age    = 0.0f;
		p.life   = RingLifeMin + (RingLifeMax - RingLifeMin) * Rand01();
		p.pos    = pos + Math::Vector3(0.0f, 0.25f, 0.0f);   // ホイール中心あたり
		p.vel    = Math::Vector3::Zero;
		p.axisU  = u;
		p.axisV  = v;
		p.radius = RingRadiusMin + (RingRadiusMax - RingRadiusMin) * Rand01();
		p.rot    = Rand01() * 6.2831853f;
		p.rotVel = (Rand01() * 2.0f - 1.0f) * RingSpinMax;
		p.mix    = Rand01();
		p.seed   = Rand01() * 10.0f;
	}

	for (int i = 0; i < sparkCount; ++i)
	{
		Particle& p = NextSlot();
		p.kind  = Kind::Spark;
		p.alive = true;
		p.age   = 0.0f;
		p.life  = SparkLifeMin + (SparkLifeMax - SparkLifeMin) * Rand01();
		p.pos   = pos + Math::Vector3(0.0f, 0.06f, 0.0f);

		// ホイール面内へ飛ばす＋車速の逆向きに少し引きずる
		const float a = Rand01() * 6.2831853f;
		const float sp = SparkSpeedMin + (SparkSpeedMax - SparkSpeedMin) * Rand01();
		p.vel = (u * cosf(a) + v * sinf(a)) * sp
		      + Math::Vector3(0.0f, SparkRise * Rand01(), 0.0f)
		      - carVel * 0.15f;
		p.mix = Rand01();
		p.radius = 1.0f;   // 通常のスパークは標準の長さ
	}
}

//----------------------------------------------------------
// ブースト発動時の爆発：全方向へ勢いよく弾けさせる(ニトロの"ぱぁん")
//----------------------------------------------------------
void DriftNeon::Burst(const Math::Vector3& pos, const Math::Vector3& carVel)
{
	if (m_particles.empty()) { return; }

	// リング：向きをばらして数枚重ねる＝どの角度から見ても輪が見える
	for (int i = 0; i < BoostBurstRings; ++i)
	{
		// ランダムな平面を作る
		Math::Vector3 n(Rand01() * 2.0f - 1.0f, Rand01() * 2.0f - 1.0f, Rand01() * 2.0f - 1.0f);
		if (n.LengthSquared() < 1e-4f) { n = Math::Vector3(0.0f, 1.0f, 0.0f); }
		n.Normalize();
		Math::Vector3 u = Math::Vector3::Up.Cross(n);
		if (u.LengthSquared() < 1e-6f) { u = Math::Vector3(1.0f, 0.0f, 0.0f); }
		u.Normalize();
		const Math::Vector3 v = n.Cross(u);

		Particle& p = NextSlot();
		p.kind   = Kind::Ring;
		p.alive  = true;
		p.age    = 0.0f;
		p.life   = RingLifeMin + (RingLifeMax - RingLifeMin) * Rand01();
		p.pos    = pos;
		p.vel    = Math::Vector3::Zero;
		p.axisU  = u;
		p.axisV  = v;
		p.radius = RingRadiusMax * (1.0f + Rand01());   // 大きめの輪
		p.rot    = Rand01() * 6.2831853f;
		p.rotVel = (Rand01() * 2.0f - 1.0f) * RingSpinMax;
		p.mix    = Rand01();
		p.seed   = Rand01() * 10.0f;
	}

	// 地面に沿って水平に放射する向きを作る(上下のばらつきはごく僅か)
	auto flatDir = [&]() -> Math::Vector3
	{
		const float a = Rand01() * 6.2831853f;
		Math::Vector3 d(cosf(a), (Rand01() * 2.0f - 1.0f) * BoostVertSpread, sinf(a));
		d.Normalize();
		return d;
	};

	// スパーク：地面に沿って水平に放射状に飛ばす
	for (int i = 0; i < BoostBurstSparks; ++i)
	{
		const Math::Vector3 dir = flatDir();

		Particle& p = NextSlot();
		p.kind  = Kind::Spark;
		p.alive = true;
		p.age   = 0.0f;
		p.life  = SparkLifeMin + (SparkLifeMax - SparkLifeMin) * Rand01();
		// 車体の中に埋もれないよう、少し外側から湧かせる(球状に広がる見た目にもなる)
		p.pos   = pos + dir * (BoostSpawnRadius * (0.7f + 0.6f * Rand01()));
		const float sp = (SparkSpeedMin + (SparkSpeedMax - SparkSpeedMin) * Rand01()) * BoostBurstSpeed;
		p.vel   = dir * sp + carVel * 0.25f;   // 車速に少し引っ張られる
		p.mix   = Rand01();
		// 線の長さをばらす(短い破片から長い尾を引くものまで混ぜる)
		p.radius = BoostLenMin + (BoostLenMax - BoostLenMin) * Rand01();
	}

	// 丸い粒：こちらは全方向(360度)へ散らす。線が地を這うのに対して、
	// 丸は上にも飛ぶことで立体感が出る。
	for (int i = 0; i < BoostBurstDots; ++i)
	{
		Math::Vector3 dir(Rand01() * 2.0f - 1.0f, Rand01() * 2.0f - 1.0f, Rand01() * 2.0f - 1.0f);
		if (dir.LengthSquared() < 1e-4f) { dir = Math::Vector3(0.0f, 1.0f, 0.0f); }
		dir.Normalize();

		Particle& p = NextSlot();
		p.kind  = Kind::Dot;
		p.alive = true;
		p.age   = 0.0f;
		p.life  = DotLifeMin + (DotLifeMax - DotLifeMin) * Rand01();
		p.pos   = pos + dir * (BoostSpawnRadius * (0.7f + 0.6f * Rand01()));
		const float sp = (SparkSpeedMin + (SparkSpeedMax - SparkSpeedMin) * Rand01()) * BoostBurstSpeed * 0.7f;
		p.vel   = dir * sp + carVel * 0.25f;
		p.mix   = Rand01();
		p.radius = DotSizeMin + (DotSizeMax - DotSizeMin) * Rand01();   // 丸の半径
	}
}

//----------------------------------------------------------
// 更新：寿命・移動
//----------------------------------------------------------
void DriftNeon::Update(float dt)
{
	for (auto& p : m_particles)
	{
		if (!p.alive) { continue; }
		p.age += dt;
		if (p.age >= p.life) { p.alive = false; continue; }

		if (p.kind == Kind::Spark)
		{
			p.pos += p.vel * dt;
			p.vel -= p.vel * std::min(SparkDrag * dt, 1.0f);   // 減速
		}
		else if (p.kind == Kind::Dot)
		{
			p.pos += p.vel * dt;
			p.vel -= p.vel * std::min(DotDrag * dt, 1.0f);     // 減速
			p.vel.y -= DotGravity * dt;                        // 落下(線より重く見せる)
		}
		else
		{
			p.rot += p.rotVel * dt;
		}
	}
}

//----------------------------------------------------------
// 描画：細い板ポリを線分として並べ、加算合成で発光させる
//----------------------------------------------------------
void DriftNeon::DrawEffect()
{
	DrawInternal(false);
}

//----------------------------------------------------------
// 輝度パス：グロー層だけを輝度RTへ描く。
// この絵はポストプロセス側で4段階のガウスぼかしを掛けてから画面へ加算されるので、
// 板ポリを重ねるだけの偽グローと違い、光が周囲へ本当に滲む(レーザーらしくなる)。
//----------------------------------------------------------
void DriftNeon::DrawBrightPass()
{
	DrawInternal(true);
}


//----------------------------------------------------------
// 全粒を1本の頂点列にまとめてから1回で描く。
// 粒ごとにDrawVerticesを呼ぶと、定数バッファ転送・状態変更・頂点転送が
// 粒数ぶん走って極端に重い(リングは線分ごとなので1個で20回描いていた)。
// 色・濃さは頂点カラーへ載せるので、まとめても粒ごとに違う色を出せる。
//----------------------------------------------------------
void DriftNeon::DrawInternal(bool bright)
{
	auto& shaderMgr = KdShaderManager::Instance();
	auto& shader    = shaderMgr.m_StandardShader;

	// カメラのワールド軸(スパーク・丸い粒のビルボード化に使う)
	const Math::Matrix camWorld = shaderMgr.GetCameraCB().mView.Invert();
	Math::Vector3 camF = camWorld.Backward(); camF.Normalize();
	Math::Vector3 camR = camWorld.Right();    camR.Normalize();
	Math::Vector3 camU = camWorld.Up();       camU.Normalize();

	m_batch.clear();   // 容量は保持されるので毎フレームの確保は起きない
	m_cull.Update();   // 今フレームの視錐台

	// 色を頂点カラー(ABGR)へ詰める
	auto pack = [](const Math::Vector3& rgb, float a)
	{
		auto to8 = [](float v) { return static_cast<unsigned>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
		return (to8(a) << 24) | (to8(rgb.z) << 16) | (to8(rgb.y) << 8) | to8(rgb.x);
	};

	// 四角形1枚を三角形2枚として積む(center=中心, ex/ey=半ベクトル)
	auto pushQuad = [&](const Math::Vector3& center, const Math::Vector3& ex,
	                    const Math::Vector3& ey, const Math::Vector3& nrm, unsigned int c)
	{
		KdPolygon::Vertex v;
		v.normal = nrm;
		v.color  = c;
		auto add = [&](const Math::Vector3& p, float u, float w)
		{
			v.pos = p; v.UV = { u, w }; m_batch.push_back(v);
		};
		const Math::Vector3 a = center - ex - ey;
		const Math::Vector3 b = center + ex - ey;
		const Math::Vector3 d = center + ex + ey;
		const Math::Vector3 e = center - ex + ey;
		add(a, 0, 0); add(b, 1, 0); add(d, 1, 1);
		add(a, 0, 0); add(d, 1, 1); add(e, 0, 1);
	};

	// 紡錘形(両端が尖った菱形)。長方形だと"棒"に見えてしまう
	auto pushSpindle = [&](const Math::Vector3& center, const Math::Vector3& ex,
	                       const Math::Vector3& ey, const Math::Vector3& nrm, unsigned int c)
	{
		KdPolygon::Vertex v;
		v.normal = nrm;
		v.color  = c;
		auto add = [&](const Math::Vector3& p, float u, float w)
		{
			v.pos = p; v.UV = { u, w }; m_batch.push_back(v);
		};
		const Math::Vector3 a = center - ex;   // 先端(左)
		const Math::Vector3 b = center - ey;   // 下
		const Math::Vector3 d = center + ex;   // 先端(右)
		const Math::Vector3 e = center + ey;   // 上
		add(a, 0, 0.5f); add(b, 0.5f, 0); add(d, 1, 0.5f);
		add(a, 0, 0.5f); add(d, 1, 0.5f); add(e, 0.5f, 1);
	};

	// 円盤(カメラに正対)。丸い粒用
	auto pushDisc = [&](const Math::Vector3& center, float rad, unsigned int c)
	{
		KdPolygon::Vertex v;
		v.normal = -camF;
		v.color  = c;
		v.UV     = { 0.5f, 0.5f };
		for (int i = 0; i < DotSegments; ++i)
		{
			const float a0 = 6.2831853f * static_cast<float>(i)     / DotSegments;
			const float a1 = 6.2831853f * static_cast<float>(i + 1) / DotSegments;
			v.pos = center;                                                  m_batch.push_back(v);
			v.pos = center + camR * (cosf(a0) * rad) + camU * (sinf(a0) * rad); m_batch.push_back(v);
			v.pos = center + camR * (cosf(a1) * rad) + camU * (sinf(a1) * rad); m_batch.push_back(v);
		}
	};

	for (const auto& p : m_particles)
	{
		if (!p.alive) { continue; }

		// 画面に映らない粒はここで捨てる。リングは20区間ぶんの頂点を積むので特に効く
		if (CullingConst::SmokeCull)
		{
			// リングは半径ぶん、線・丸は長さ/半径ぶんの広がりを見込む
			const float r = (p.kind == Kind::Ring)
				? p.radius * RingGrow + RingThickness
				: std::max(p.radius, SparkLength * p.radius);
			if (!m_cull.IsVisible(p.pos, r + CullingConst::SmokeCullMargin,
			                      CullingConst::SmokeCullDist)) { continue; }
		}

		const float f = p.age / p.life;                 // 寿命比
		// 寿命の大半は完全不透明のまま保ち、最後だけ薄れて消える
		const float fade = (f < FadeStart)
			? 1.0f
			: 1.0f - (f - FadeStart) / std::max(1.0f - FadeStart, 1e-4f);
		// 色の混色比を段階化する。ColorSteps=1なら中間色を作らず色A/色Bのどちらかになる
		// (連続補間だと中間色だらけになって2色に見えない)。
		const float mix = (ColorSteps <= 1)
			? ((p.mix < 0.5f) ? 0.0f : 1.0f)
			: (floorf(p.mix * ColorSteps + 0.5f) / static_cast<float>(ColorSteps));
		const Math::Vector3 c3 = Math::Vector3::Lerp(m_colA, m_colB, mix) * Brightness;

		// 芯は白へ寄せた明るい中心。外側は色そのまま
		const Math::Vector3 coreRgb = Math::Vector3::Lerp(c3, Math::Vector3(1.0f, 1.0f, 1.0f), CoreWhite);

		if (p.kind == Kind::Dot)
		{
			// 丸い粒：消え際に少し縮む
			const float r = p.radius * (0.55f + 0.45f * (1.0f - f));
			if (bright)
			{
				// 輝度RTへは光の芯だけ。ぼかしは後段のポストプロセスが担当する
				pushDisc(p.pos, r * GlowWidthMul * BloomWidthMul, pack(c3, fade * BloomAlpha));
				continue;
			}
			pushDisc(p.pos, r * GlowWidthMul,  pack(c3, fade * GlowAlpha));   // 色の本体
			pushDisc(p.pos, r * 0.55f,         pack(coreRgb, fade));          // 白い芯
		}
		else if (p.kind == Kind::Ring)
		{
			// 広がる輪：円周をN本の線分で描く(半径を少し揺らして手描き感)
			const float radius = p.radius * (1.0f + (RingGrow - 1.0f) * f);
			const Math::Vector3 nrm = p.axisU.Cross(p.axisV);
			const float step = 6.2831853f / static_cast<float>(RingSegments);
			const float segLen = radius * step * 0.62f;   // 半長(少し重ねて途切れ防止)
			const unsigned int c = pack(c3, fade);

			for (int i = 0; i < RingSegments; ++i)
			{
				const float a = step * static_cast<float>(i) + p.rot;
				const float wob = 1.0f + RingWobble * sinf(a * 3.0f + p.seed);
				const float r   = radius * wob;

				const Math::Vector3 radial  = p.axisU * cosf(a) + p.axisV * sinf(a);
				const Math::Vector3 tangent = p.axisU * -sinf(a) + p.axisV * cosf(a);
				pushQuad(p.pos + radial * r, tangent * segLen,
				         radial * (RingThickness * 0.5f), nrm, c);
			}
		}
		else
		{
			// スパーク：進行方向へ伸びる短い線(カメラへ正対する太さ方向)
			Math::Vector3 dir = p.vel;
			if (dir.LengthSquared() < 1e-6f) { continue; }
			dir.Normalize();
			Math::Vector3 side = dir.Cross(camF);
			if (side.LengthSquared() < 1e-6f) { side = Math::Vector3::Up; }
			side.Normalize();

			// 粒ごとの長さ倍率(radius)を掛ける＝短い破片と長い尾が混ざる
			const float len = SparkLength * p.radius * (0.4f + 0.6f * (1.0f - f));
			const float half = len * 0.5f;

			if (bright)
			{
				pushSpindle(p.pos, dir * half, side * (SparkThick * 0.5f * GlowWidthMul * BloomWidthMul),
				            -camF, pack(c3, fade * BloomAlpha));
				continue;
			}
			// ①色の本体
			pushSpindle(p.pos, dir * half, side * (SparkThick * 0.5f * GlowWidthMul),
			            -camF, pack(c3, fade * GlowAlpha));
			// ②芯：細く白く明るい中心
			pushSpindle(p.pos, dir * (half * 0.96f), side * (SparkThick * 0.5f * CoreWidthMul),
			            -camF, pack(coreRgb, fade));
		}
	}

	if (m_batch.empty()) { return; }

	// 深度：手前の物には隠れる／自分の奥は隠さない。
	// 車の後ろへ回ったパーティクルは車体に隠れる＝奥行きが出て、車から湧いて出る感じになる。
	// 輝度パスは呼び出し元(BeginBright)が加算合成にしているのでそのまま使う。
	// 通常パスは加算だと白飛びするので通常合成。
	if (!bright) { shaderMgr.ChangeBlendState(KdBlendState::Alpha); }

	shader.DrawVertices(m_batch, Math::Matrix::Identity, Math::Color(1, 1, 1, 1),
	                    KdDepthStencilState::ZWriteDisable,
	                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	                    KdRasterizerState::CullNone);

	if (!bright) { shaderMgr.UndoBlendState(); }
}
