#include "DriftSmoke.h"

//----------------------------------------------------------
// 初期化：プール確保＋共有ビルボード板ポリの用意
//----------------------------------------------------------
void DriftSmoke::Init()
{
	m_particles.assign(SmokeConst::MaxParticles, Particle{});
	m_next = 0;

	// 単位サイズ・中央原点の板ポリに瘤状ブロブのアトラスを貼る。
	// 3D空間配置なので2Dフラグは外す(法線の2D変換を無効化)。
	m_poly = std::make_shared<KdSquarePolygon>(SmokeConst::TexturePath);
	m_poly->SetScale(1.0f);
	m_poly->Set2DObject(false);
	m_poly->SetSplit(SmokeConst::SplitX, SmokeConst::SplitY);
}

//----------------------------------------------------------
// xorshift32 による軽量乱数(0.0〜1.0)
//----------------------------------------------------------
float DriftSmoke::Rand01()
{
	m_rng ^= m_rng << 13;
	m_rng ^= m_rng >> 17;
	m_rng ^= m_rng << 5;
	return static_cast<float>(m_rng & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

//----------------------------------------------------------
// 放出：指定位置から count 枚のスモークをリングバッファへ書き込む
//----------------------------------------------------------
void DriftSmoke::Emit(const Math::Vector3& pos, const Math::Vector3& baseVel, int count)
{
	if (m_particles.empty()) { return; }

	// 要求数ぶん、それぞれを小さな瘤のクラスタ(密集した数粒)として撒く。
	// 近接して重なることで1つのもこもこした塊に見せる。
	for (int i = 0; i < count; ++i)
	{
		for (int k = 0; k < SmokeConst::ClusterCount; ++k)
		{
			Particle& p = m_particles[m_next];
			m_next = (m_next + 1) % static_cast<int>(m_particles.size());

			p.alive = true;
			p.age   = 0.0f;
			p.life  = SmokeConst::LifeMin + (SmokeConst::LifeMax - SmokeConst::LifeMin) * Rand01();

			// 形・サイズを粒ごとに変える(均一だと卵の列に見える)
			p.variant = static_cast<int>(Rand01() * static_cast<float>(SmokeConst::VariantCount))
			          % SmokeConst::VariantCount;
			p.sizeMul = SmokeConst::SizeVarMin
			          + (SmokeConst::SizeVarMax - SmokeConst::SizeVarMin) * Rand01();

			// クラスタ内で密に散らす(半径小さめ＝塊になる)
			const Math::Vector3 jitter(
				(Rand01() * 2.0f - 1.0f) * SmokeConst::ClusterRadius,
				 Rand01() * SmokeConst::ClusterRadius * 0.5f,
				(Rand01() * 2.0f - 1.0f) * SmokeConst::ClusterRadius);
			p.pos = pos + jitter;

			// 水平方向へわずかに散り、上昇成分を足す
			Math::Vector3 spread(Rand01() * 2.0f - 1.0f, 0.0f, Rand01() * 2.0f - 1.0f);
			if (spread.LengthSquared() > 1e-4f) { spread.Normalize(); }
			const float up = SmokeConst::RiseSpeed * (0.5f + Rand01() * 0.5f);
			p.vel = baseVel
			      + spread * (SmokeConst::SpreadSpeed * Rand01())
			      + Math::Vector3(0.0f, up, 0.0f);

			p.rot    = Rand01() * 6.2831853f;
			p.rotVel = (Rand01() * 2.0f - 1.0f) * SmokeConst::SpinMax;
		}
	}
}

//----------------------------------------------------------
// 更新：寿命・減衰・移動
//----------------------------------------------------------
void DriftSmoke::Update(float dt)
{
	if (dt <= 0.0f) { return; }

	const float dragK = std::min(SmokeConst::Drag * dt, 1.0f);

	for (auto& p : m_particles)
	{
		if (!p.alive) { continue; }

		p.age += dt;
		if (p.age >= p.life) { p.alive = false; continue; }

		p.vel -= p.vel * dragK;   // 速度減衰(だんだん漂う)

		// ④乱流：粒ごとに位相をずらしてゆっくり渦を巻くように揺らす(規則性を壊して散らす)
		p.vel.x += sinf(p.age * SmokeConst::TurbFreq + p.rot) * SmokeConst::TurbStrength * dt;
		p.vel.z += cosf(p.age * SmokeConst::TurbFreq * 0.85f + p.rot * 1.3f) * SmokeConst::TurbStrength * dt;

		p.pos += p.vel * dt;
		p.rot += p.rotVel * dt;
	}
}

//----------------------------------------------------------
// 描画：カメラ正対ビルボードをUnLitで描く
//   ・陰影/輪郭はテクスチャに焼き込み済み(3段トーン＋フチ)
//   ・粒ごとにアトラスからブロブ種類を選ぶ
//   ・べったり不透明に近いアルファで塊感を出す
//   ・深度書き込みOFF(粒どうしの硬い交差線を防ぐ)
//----------------------------------------------------------
void DriftSmoke::DrawEffect()
{
	if (!m_poly) { return; }

	auto& shaderMgr = KdShaderManager::Instance();
	auto& shader    = shaderMgr.m_StandardShader;

	// カメラのワールド軸(ビュー行列の逆)からビルボード基底を得る
	const Math::Matrix camWorld = shaderMgr.GetCameraCB().mView.Invert();
	Math::Vector3 camR = camWorld.Right();    camR.Normalize();
	Math::Vector3 camU = camWorld.Up();       camU.Normalize();
	Math::Vector3 camF = camWorld.Backward(); camF.Normalize();

	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	// スモーク専用ライティング＋ディゾルブON：球ドーム法線でトゥーン陰影＋消え際は縁からちぎれて溶ける
	shader.SetSmokeLit(true, static_cast<float>(SmokeConst::SplitX),
	                         static_cast<float>(SmokeConst::SplitY),
	                         SmokeConst::AlphaPeak,
	                         SmokeConst::ErodeStrength,
	                         SmokeConst::ErodeEdge);

	for (const auto& p : m_particles)
	{
		if (!p.alive) { continue; }

		const float f = p.age / p.life;   // 寿命比 0〜1

		// サイズ：小さく生まれて膨らむ(粒ごとの倍率つき)
		const float size = (SmokeConst::SizeStart
		                 + (SmokeConst::SizeEnd - SmokeConst::SizeStart) * f) * p.sizeMul;

		// 面内回転を効かせたビルボード軸
		const float cs = cosf(p.rot);
		const float sn = sinf(p.rot);
		const Math::Vector3 axisR = (camR * cs + camU * sn) * size;
		const Math::Vector3 axisU = (camR * -sn + camU * cs) * size;

		Math::Matrix world;
		world.Right(axisR);
		world.Up(axisU);
		world.Backward(camF);
		world.Translation(p.pos);

		// 生存率(presence 0〜1)：立ち上がり(FadeIn)と消え際(FadeOut)。
		// これを頂点色αで渡し、シェーダ側がエロージョン(ディゾルブ)の閾値に使う。
		// ＝均一に薄くせず、縁からちぎれて溶ける。最大不透明度はシェーダ側で掛ける。
		const float fadeIn   = std::min(f / SmokeConst::FadeInRatio, 1.0f);
		const float fadeOut  = std::min((1.0f - f) / SmokeConst::FadeOutRatio, 1.0f);
		const float presence = std::min(fadeIn, fadeOut);

		// ⑤消え際は色を暗く落とす(明るいグロー円が残らないように)
		const float darken = 0.55f + 0.45f * fadeOut;

		// ブロブ種類を選んで描画
		m_poly->SetUVRect(static_cast<UINT>(p.variant));
		const Math::Color col(m_tint.x * darken, m_tint.y * darken, m_tint.z * darken, presence);
		shader.DrawPolygon(*m_poly, world, col);
	}

	// スモーク専用ライティングOFF（他のUnLit描画に波及させない）
	shader.SetSmokeLit(false);

	shaderMgr.UndoDepthStencilState();
}
