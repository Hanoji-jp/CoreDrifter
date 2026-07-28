#include "DriftSmoke.h"

using namespace SmokeConst;

namespace
{
	// 法線(-1〜1)を頂点カラー(R8G8B8A8)へ格納する。
	// シェーダ側でこれを取り出し、実際の平行光と照らし合わせて陰影を計算する
	// (CPUで陰影を焼き込むと、シーンの光の向き・色が変わっても追従できないため)。
	unsigned int PackNormal(const Math::Vector3& n)
	{
		auto enc = [](float v) -> unsigned int
		{
			const float t = std::clamp(v * 0.5f + 0.5f, 0.0f, 1.0f);
			return static_cast<unsigned int>(t * 255.0f + 0.5f);
		};
		// R8G8B8A8_UNORM は下位バイトからR,G,B,A
		return 0xFF000000u | (enc(n.z) << 16) | (enc(n.y) << 8) | enc(n.x);
	}
}

//----------------------------------------------------------
// 初期化：プール確保＋塊メッシュを種類ぶん生成
//----------------------------------------------------------
void DriftSmoke::Init()
{
	m_particles.assign(MeshMaxParticles, Particle{});
	m_next = 0;

	m_meshes.resize(MeshVariants);
	for (int i = 0; i < MeshVariants; ++i)
	{
		BuildBlobMesh(m_meshes[i], 1469598103u + static_cast<unsigned int>(i) * 2654435761u);
	}
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
// 塊メッシュ生成
//   球を三角関数のうねりで変形して"もこもこの瘤"にする。
//   面ごとの法線で平行光のトゥーン陰影を求め、頂点カラーへ焼き込む
//   (UnLitパスは頂点カラーを乗算するので、これだけで立体に光が当たって見える)。
//   面法線をそのまま使う＝フラットシェーディング＝硬い面が出てUnbound風になる。
//----------------------------------------------------------
void DriftSmoke::BuildBlobMesh(std::vector<KdPolygon::Vertex>& out, unsigned int seed)
{
	// 形のばらつきを決める位相・周波数(種類ごとに違う塊になる)
	auto frac = [](unsigned int v) { return static_cast<float>(v & 0xFFFFu) / 65535.0f; };
	const float s1 = frac(seed)       * 6.2831853f;
	const float s2 = frac(seed >> 3)  * 6.2831853f;
	const float s3 = frac(seed >> 7)  * 6.2831853f;
	const float s4 = frac(seed >> 11) * 6.2831853f;
	const float s5 = frac(seed >> 15) * 6.2831853f;
	const float s6 = frac(seed >> 21) * 6.2831853f;
	const float f1 = 1.5f + frac(seed >> 5)  * 1.5f;
	const float f2 = 1.5f + frac(seed >> 9)  * 1.5f;
	const float f3 = 2.5f + frac(seed >> 13) * 2.0f;
	const float f4 = 2.5f + frac(seed >> 17) * 2.0f;
	const float f5 = 5.0f + frac(seed >> 25) * 3.0f;   // 高周波(細かい岩肌)
	const float f6 = 5.0f + frac(seed >> 27) * 3.0f;
	const float a1 = MeshLump1 * (0.7f + 0.6f * frac(seed >> 19));
	const float a2 = MeshLump2 * (0.7f + 0.6f * frac(seed >> 23));
	const float a3 = MeshLump3 * (0.7f + 0.6f * frac(seed >> 29));

	// 粒ごとの縦横比(球っぽさを崩して不揃いな塊にする)
	const float ax = MeshAspectMin + (MeshAspectMax - MeshAspectMin) * frac(seed >> 2);
	const float ay = MeshAspectMin + (MeshAspectMax - MeshAspectMin) * frac(seed >> 6);
	const float az = MeshAspectMin + (MeshAspectMax - MeshAspectMin) * frac(seed >> 10);

	// 方向だけで決まる滑らかなうねり。球面座標を使わないので極の破綻が起きない。
	// 同じ方向なら必ず同じ値＝頂点を共有する面どうしで裂け目ができない。
	auto radiusAt = [&](const Math::Vector3& d) -> float
	{
		// 周波数の違う3段を重ねる＝大きなうねり + 瘤 + 細かい岩肌
		float r = 1.0f;
		r += a1 * sinf(f1 * d.x + s1) * cosf(f2 * d.y + s2);
		r += a2 * sinf(f3 * d.z + s3) * cosf(f4 * d.x + s4);
		r += a3 * sinf(f5 * d.y + s5) * cosf(f6 * d.z + s6);
		return r;
	};
	// 方向 → 変形後の頂点位置
	auto toPos = [&](const Math::Vector3& dir) -> Math::Vector3
	{
		Math::Vector3 d = dir; d.Normalize();
		const float r = radiusAt(d);
		return Math::Vector3(d.x * r * ax, d.y * r * ay, d.z * r * az);
	};

	//----- 正二十面体(20面) -----
	const float t = 1.618034f;   // 黄金比
	Math::Vector3 base[12] =
	{
		{ -1,  t,  0 }, {  1,  t,  0 }, { -1, -t,  0 }, {  1, -t,  0 },
		{  0, -1,  t }, {  0,  1,  t }, {  0, -1, -t }, {  0,  1, -t },
		{  t,  0, -1 }, {  t,  0,  1 }, { -t,  0, -1 }, { -t,  0,  1 },
	};
	for (auto& v : base) { v.Normalize(); }

	static const int kFaces[20][3] =
	{
		{ 0,11, 5}, { 0, 5, 1}, { 0, 1, 7}, { 0, 7,10}, { 0,10,11},
		{ 1, 5, 9}, { 5,11, 4}, {11,10, 2}, {10, 7, 6}, { 7, 1, 8},
		{ 3, 9, 4}, { 3, 4, 2}, { 3, 2, 6}, { 3, 6, 8}, { 3, 8, 9},
		{ 4, 9, 5}, { 2, 4,11}, { 6, 2,10}, { 8, 6, 7}, { 9, 8, 1},
	};

	out.clear();

	// 方向から表面の滑らかな法線を求める(変形後の面の傾きを微分で拾う)。
	// 面法線(フラットシェーディング)だと面ごとに法線が切り替わり、ポリゴン数を上げても
	// 面の境目が見えて"ポリポリ"する。頂点ごとの滑らかな法線なら、
	// 明暗の境目はピクセル単位の量子化でハッキリ出したまま、滑らかな曲線になる。
	auto normalAtDir = [&](const Math::Vector3& dir) -> Math::Vector3
	{
		Math::Vector3 d = dir; d.Normalize();
		// dに直交する2軸を作る
		Math::Vector3 t1 = (fabsf(d.y) < 0.9f) ? Math::Vector3::Up.Cross(d)
		                                       : Math::Vector3(1.0f, 0.0f, 0.0f).Cross(d);
		if (t1.LengthSquared() < 1e-8f) { t1 = Math::Vector3(1.0f, 0.0f, 0.0f); }
		t1.Normalize();
		const Math::Vector3 t2 = d.Cross(t1);

		const float e = 0.05f;
		const Math::Vector3 pa = toPos(d);
		const Math::Vector3 pb = toPos(d + t1 * e);
		const Math::Vector3 pc = toPos(d + t2 * e);

		Math::Vector3 n = (pb - pa).Cross(pc - pa);
		if (n.LengthSquared() < 1e-12f) { return d; }
		n.Normalize();
		if (n.Dot(pa) < 0.0f) { n = -n; }   // 外向きに揃える
		return n;
	};

	// 三角形を1枚追加する。頂点ごとに滑らかな法線を入れる。
	auto addTri = [&](const Math::Vector3& da, const Math::Vector3& db, const Math::Vector3& dc)
	{
		const Math::Vector3 dirs[3] = { da, db, dc };
		for (const auto& d : dirs)
		{
			const Math::Vector3 n = normalAtDir(d);
			KdPolygon::Vertex v;
			v.pos    = toPos(d);
			v.normal = n;
			v.color  = PackNormal(n);    // 法線を格納(シェーダで復元して照明に使う)
			v.UV     = { 0.5f, 0.5f };   // テクスチャは使わない(白)
			out.push_back(v);
		}
	};

	// 面を再帰的に細分割して、最後に変形をかけた頂点で三角形を作る
	std::function<void(const Math::Vector3&, const Math::Vector3&, const Math::Vector3&, int)> subdiv =
		[&](const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c, int depth)
	{
		if (depth <= 0)
		{
			addTri(a, b, c);   // 方向を渡す(頂点位置と滑らかな法線はaddTri側で作る)
			return;
		}
		// 各辺の中点を球面へ戻す(方向ベクトルのまま扱う)
		Math::Vector3 ab = a + b; ab.Normalize();
		Math::Vector3 bc = b + c; bc.Normalize();
		Math::Vector3 ca = c + a; ca.Normalize();
		subdiv(a,  ab, ca, depth - 1);
		subdiv(ab, b,  bc, depth - 1);
		subdiv(ca, bc, c,  depth - 1);
		subdiv(ab, bc, ca, depth - 1);
	};

	for (const auto& f : kFaces)
	{
		subdiv(base[f[0]], base[f[1]], base[f[2]], MeshSubdiv);
	}
}

//----------------------------------------------------------
// 放出：指定位置から count 個の塊をリングバッファへ書き込む
//----------------------------------------------------------
void DriftSmoke::Emit(const Math::Vector3& pos, const Math::Vector3& baseVel,
                      const Math::Vector3& outward, int count, float sizeScale)
{
	if (m_particles.empty()) { return; }

	// 煙の発生源(全粒で共有する陰影・グラデーションの基準)。急に変わらないよう滑らかに追う。
	m_baseY   += (pos.y - m_baseY) * 0.25f;
	m_originX += (pos.x - m_originX) * 0.25f;
	m_originZ += (pos.z - m_originZ) * 0.25f;

	for (int i = 0; i < count; ++i)
	{
		for (int k = 0; k < MeshClusterCount; ++k)
		{
			Particle& p = m_particles[m_next];
			m_next = (m_next + 1) % static_cast<int>(m_particles.size());

			p.alive = true;
			p.age   = 0.0f;
			p.life  = LifeMin + (LifeMax - LifeMin) * Rand01();
			p.variant = static_cast<int>(Rand01() * static_cast<float>(MeshVariants)) % MeshVariants;
			p.sizeMul = (SizeVarMin + (SizeVarMax - SizeVarMin) * Rand01()) * sizeScale;
			p.phase   = Rand01() * 6.2831853f;
			// 同じメッシュでも別の塊に見せる：Y軸回転＋非一様スケール。
			// 光はほぼ真上から当てているので、Y軸回転なら焼き込んだ陰影がほぼ崩れない。
			p.yaw     = Rand01() * 6.2831853f;
			p.squashX = 0.80f + 0.45f * Rand01();
			p.squashY = 0.70f + 0.50f * Rand01();
			p.squashZ = p.squashX;

			// クラスタ内で密に散らす
			const Math::Vector3 jitter(
				(Rand01() * 2.0f - 1.0f) * ClusterRadius,
				 Rand01() * ClusterRadius * 0.5f,
				(Rand01() * 2.0f - 1.0f) * ClusterRadius);
			p.pos = pos + jitter;

			// 車の外側(横方向)へ強く吹き出す。これが無いと発生点の周りに留まって
			// 車体にまとわりつき、本家のように左右へ張り出さない。
			const float outSpd = OutwardSpeed
			                   * (1.0f + (Rand01() * 2.0f - 1.0f) * OutwardJitter);

			// 加えて水平にランダムな散り＋上昇＋車速の逆向きの引きずり
			const float ang = Rand01() * 6.2831853f;
			const float spd = SpreadSpeed * Rand01();
			p.vel = outward * outSpd
			      + Math::Vector3(cosf(ang) * spd,
			                      RiseSpeed * (0.6f + 0.8f * Rand01()),
			                      sinf(ang) * spd) + baseVel;
		}
	}
}

//----------------------------------------------------------
// 更新：寿命・移動・乱流
//----------------------------------------------------------
void DriftSmoke::Update(float dt)
{
	if (dt <= 0.0f) { return; }

	const float dragK = std::min(Drag * dt, 1.0f);

	for (auto& p : m_particles)
	{
		if (!p.alive) { continue; }

		p.age += dt;
		if (p.age >= p.life) { p.alive = false; continue; }

		// 水平は強めに減衰させて漂わせる。上下は弱く＝立ち上る勢いを残す。
		p.vel.x -= p.vel.x * dragK;
		p.vel.z -= p.vel.z * dragK;
		p.vel.y -= p.vel.y * dragK * VertDragMul;

		// 浮力：煙は温かいので上へ伸び続ける(上限で頭打ち)
		p.vel.y = std::min(p.vel.y + Buoyancy * dt, MaxRiseSpeed);

		// 乱流：粒ごとに位相をずらしてゆっくり渦を巻くように揺らす
		p.vel.x += sinf(p.age * TurbFreq + p.phase) * TurbStrength * dt;
		p.vel.z += cosf(p.age * TurbFreq * 0.85f + p.phase * 1.3f) * TurbStrength * dt;

		p.pos += p.vel * dt;
	}
}

//----------------------------------------------------------
// 描画：塊メッシュ(3D頂点)をUnLitで描く
//   頂点カラーに陰影が焼き込んであり、粒の色(colRate)を掛けて着色する。
//   色は寿命に沿って A(根元) → B(先端) へグラデーションする。
//----------------------------------------------------------
void DriftSmoke::DrawEffect()
{
	if (m_meshes.empty()) { return; }

	auto& shaderMgr = KdShaderManager::Instance();
	auto& shader    = shaderMgr.m_StandardShader;

	m_batch.clear();   // 容量は保持されるので毎フレームの確保は起きない
	m_cull.Update();   // 今フレームの視錐台

	for (const auto& p : m_particles)
	{
		if (!p.alive) { continue; }

		const float f = p.age / p.life;   // 寿命比 0〜1

		// 大きさ：出た直後に膨らみ、ピークを過ぎたらしぼんで消える
		float radius;
		if (f < MeshSizePeakAt)
		{
			const float g = f / MeshSizePeakAt;                       // 0→1
			radius = MeshSizeStart + (MeshSizePeak - MeshSizeStart) * g;
		}
		else
		{
			const float g = (f - MeshSizePeakAt) / std::max(1.0f - MeshSizePeakAt, 1e-4f);
			radius = MeshSizePeak * (1.0f - (1.0f - MeshSizeEndScale) * g);
		}
		const float size = radius * p.sizeMul;

		// 画面に映らない粒はここで捨てる。以降のワールド変換(頂点数ぶんのループ)と
		// 頂点転送が丸ごと不要になるので、粒数が多いほど効く。
		if (CullingConst::SmokeCull)
		{
			// 潰し(squash)で膨らむぶんを見込んで、大きい方の軸で判定する
			const float r = size * std::max(p.squashX, p.squashY)
			              + CullingConst::SmokeCullMargin;
			if (!m_cull.IsVisible(p.pos, r, CullingConst::SmokeCullDist)) { continue; }
		}

		// 不透明度。本家はフェードアウトせず寿命が尽きた瞬間にパッと消える(セル画的)。
		// 出現時だけごく短く立ち上げ、あとは寿命いっぱいまで一定の濃さを保つ。
		float presence = 1.0f;
		if (PopOut)
		{
			presence = (PopFadeInEnd > 0.0f) ? std::min(f / PopFadeInEnd, 1.0f) : 1.0f;
		}
		else
		{
			const float fadeIn  = std::min(f / FadeInRatio, 1.0f);
			const float fadeOut = std::min((1.0f - f) / FadeOutRatio, 1.0f);
			presence = std::min(fadeIn, fadeOut);
		}

		// 色は全粒とも同じ(色A)。奥へのグラデーションはシェーダ側で、発生源からの距離という
		// 全粒共通の関数として掛ける＝粒の境目が出ずに滑らかに繋がる。
		// (粒ごとに色を変えると、少しずつ違う色の塊が並んでパッチワークに見える)
		const Math::Vector3 tint = m_tint;

		// ディゾルブ：寿命の終盤で穴が広がって崩れながら消える。
		// 粒ごとに違う値なので、頂点のUV.xに載せてシェーダーへ運ぶ。
		const float dissolve = std::clamp(
			(f - DissolveStart) / std::max(1.0f - DissolveStart, 1e-4f), 0.0f, 1.0f);

		// 濃さは頂点カラーのアルファへ。0だとアルファテストで消えるので下限を設ける
		const float alpha = std::max(MeshAlphaPeak * presence, 0.06f);
		const unsigned int packedA = static_cast<unsigned>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f) << 24;

		// 非一様スケール＋Y軸回転で、同じメッシュでも別の塊に見せる。
		// 行列を作ってGPUに渡すのではなく、ここでワールド空間へ変換して1本に繋げる。
		const float sx = size * p.squashX;
		const float sy = size * p.squashY;
		const float sz = size * p.squashZ;   // Xと変えると細長い筋になる
		const float cy = cosf(p.yaw);
		const float sn = sinf(p.yaw);

		// 法線は逆転置＝各軸のスケールで割ってから回す。
		// (非一様スケールのまま回すと法線が歪んで陰影がバラバラになる)
		const float inx = 1.0f / std::max(sx, 1e-4f);
		const float iny = 1.0f / std::max(sy, 1e-4f);
		const float inz = 1.0f / std::max(sz, 1e-4f);

		const auto& mesh = m_meshes[p.variant];
		for (const auto& sv : mesh)
		{
			KdPolygon::Vertex v;

			// 位置：スケール → Y回転 → 平行移動
			const float px = sv.pos.x * sx, py = sv.pos.y * sy, pz = sv.pos.z * sz;
			v.pos = Math::Vector3(px * cy + pz * sn + p.pos.x,
			                      py                + p.pos.y,
			                      -px * sn + pz * cy + p.pos.z);

			// 法線：頂点カラーRGBに詰めてあるオブジェクト空間法線を復元してワールドへ
			const float nox = (static_cast<float>( sv.color        & 0xFF) / 255.0f) * 2.0f - 1.0f;
			const float noy = (static_cast<float>((sv.color >> 8)  & 0xFF) / 255.0f) * 2.0f - 1.0f;
			const float noz = (static_cast<float>((sv.color >> 16) & 0xFF) / 255.0f) * 2.0f - 1.0f;
			float wx = (nox * inx) * cy + (noz * inz) * sn;
			float wy = (noy * iny);
			float wz = -(nox * inx) * sn + (noz * inz) * cy;
			const float len = std::max(sqrtf(wx * wx + wy * wy + wz * wz), 1e-6f);
			wx /= len; wy /= len; wz /= len;

			auto enc = [](float t) { return static_cast<unsigned>(std::clamp(t * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f); };
			v.color  = packedA | (enc(wz) << 16) | (enc(wy) << 8) | enc(wx);
			v.normal = Math::Vector3(wx, wy, wz);
			v.UV     = { dissolve, 0.0f };   // UV.x=消え具合
			m_batch.push_back(v);
		}
	}

	if (m_batch.empty()) { return; }

	// トゥーン量子化の設定は1回で済む(粒ごとに変わる値はすべて頂点へ載せたため)
	shader.SetSmokeLit(true, ToonDark, ToonWhite, ToonMidThr, ToonHiThr, ToonUpBias,
	                   SmokeMerge, m_baseY, SmokePlumeH,
	                   SmokePatScale, SmokePatStrength, SmokePatDarkBias,
	                   SmokeWobAmp, SmokeWobFreq,
	                   m_originX, m_originZ, m_gradDist, m_tintB, SmokeLocalY,
	                   0.0f, DissolveScale, m_hiColor, SmokeViewLight, SmokeFillLight);

	// 深度：ZEnable(書き込みあり)。粒は不透明な立体なので、深度で前後を決めさせる。
	// ZWriteDisableだと深度が書かれず"描画順=重なり順"になり、後から出た煙が
	// 手前に見えてしまう(奥にあるのに上に乗る)。
	// 位置はすでにワールド空間なのでワールド行列は単位行列。
	shader.DrawVertices(m_batch, Math::Matrix::Identity,
	                    Math::Color(m_tint.x, m_tint.y, m_tint.z, 1.0f),
	                    KdDepthStencilState::ZEnable,
	                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	                    KdRasterizerState::CullNone);

	// 他のUnLit描画に波及させない
	shader.SetSmokeLit(false);
}
