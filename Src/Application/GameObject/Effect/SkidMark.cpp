#include "SkidMark.h"

using namespace SkidMarkConst;

void SkidMark::Init()
{
	if (m_ready) { return; }   // マップは1枚だけ。二重に作らない

	m_trails.clear();
	m_pending.clear();
	m_pending.reserve(TrailCount * 6 * 4);   // 1フレームに増える区間はごく僅か

	// コースを真上から見た痕の蓄積先。ここへ書いたものはクリアせず残し続ける。
	// 濃さしか持たないので1チャンネルで足りる(RGBA8の1/4のメモリで済む)。
	// 窓を動かす時に内容をずらして写すので、同じものを2枚用意して交互に使う。
	m_map.CreateRenderTarget(MapResolution, MapResolution, false, DXGI_FORMAT_R8_UNORM);
	m_mapPrev.CreateRenderTarget(MapResolution, MapResolution, false, DXGI_FORMAT_R8_UNORM);
	m_map.ClearTexture(Math::Color(0.0f, 0.0f, 0.0f, 1.0f));       // 黒＝痕なし
	m_mapPrev.ClearTexture(Math::Color(0.0f, 0.0f, 0.0f, 1.0f));
	m_ready = true;
}

//----------------------------------------------------------
// 1台ぶんの枠を確保する。0,1=後輪 / 2,3=前輪(前輪は接地幅がやや狭い)
//----------------------------------------------------------
int SkidMark::AllocTrails()
{
	const int base = static_cast<int>(m_trails.size());
	for (int i = 0; i < TrailCount; ++i)
	{
		Trail t;
		t.widthMul = (i >= 2) ? FrontWidthMul : 1.0f;
		m_trails.push_back(t);
	}
	return base;
}

//----------------------------------------------------------
// 接地点を渡す。前の点から MinSegLen 以上離れたときだけ新しい区間を作る。
// (毎フレーム作ると極小の四角形を大量に焼くことになり、無駄に重い)
//----------------------------------------------------------
void SkidMark::Emit(int trail, const Math::Vector3& pos, const Math::Vector3& right, float strength)
{
	if (trail < 0 || trail >= static_cast<int>(m_trails.size())) { return; }

	// スリップが弱いうちは痕を残さない。次に濃くなった時は新しい痕として始める
	if (strength < SlipMinFor) { Cut(trail); return; }

	Trail& t = m_trails[trail];
	const float str = std::min(strength, 1.0f);

	if (!t.hasLast)
	{
		// 痕の始まり。次の点が来てはじめて区間になる
		t.lastPos   = pos;
		t.lastRight = right;
		t.lastStr   = str;
		t.hasLast   = true;
		return;
	}

	const Math::Vector3 d = pos - t.lastPos;
	if (d.LengthSquared() < MinSegLen * MinSegLen) { return; }

	// 解像度が足りないと、実寸のままではドットの隙間に落ちて点線になる。
	// マップ1ドットの実寸を測り、最低でも数ドットぶんの太さを確保する。
	const float texel = MapCoverage / static_cast<float>(MapResolution);
	const float halfW = std::max(Width * 0.5f * t.widthMul,
	                             texel * MapMinTexelWidth * 0.5f);

	PushSegment(t.lastPos, pos, t.lastRight, right, halfW, t.lastStr, str);

	t.lastPos   = pos;
	t.lastRight = right;
	t.lastStr   = str;
}

void SkidMark::Cut(int trail)
{
	if (trail < 0 || trail >= static_cast<int>(m_trails.size())) { return; }
	m_trails[trail].hasLast = false;
}

//----------------------------------------------------------
// 1区間ぶんの帯をマップ用の頂点として積む。
// 位置はワールドXZを、マップが覆う範囲の中での 0〜1 に直したもの。
// 焼き込み時は正射影で真上から描くので、高さ(Y)は使わない。
//----------------------------------------------------------
void SkidMark::PushSegment(const Math::Vector3& a, const Math::Vector3& b,
                           const Math::Vector3& rightA, const Math::Vector3& rightB,
                           float halfW, float strengthA, float strengthB)
{
	const Math::Vector3 a0 = a - rightA * halfW;
	const Math::Vector3 a1 = a + rightA * halfW;
	const Math::Vector3 b0 = b - rightB * halfW;
	const Math::Vector3 b1 = b + rightB * halfW;

	// 濃さは頂点カラーのアルファへ。焼き込みは通常合成なので、
	// 同じ場所を何度も通ると少しずつ濃くなっていく。
	auto pack = [](float s)
	{
		const unsigned a8 = static_cast<unsigned>(std::clamp(s * AlphaMax, 0.0f, 1.0f) * 255.0f + 0.5f);
		return (a8 << 24) | 0x00FFFFFFu;   // RGBは白。実際の色は描画時のcolRateで付ける
	};
	const unsigned int ca = pack(strengthA);
	const unsigned int cb = pack(strengthB);

	KdPolygon::Vertex v;
	v.normal = Math::Vector3(0.0f, 0.0f, -1.0f);
	auto add = [&](const Math::Vector3& p, unsigned int c)
	{
		v.pos   = Math::Vector3(p.x, p.z, 0.0f);   // 真上から見るのでXZをそのまま平面へ
		v.color = c;
		v.UV    = { 0.0f, 0.0f };
		m_pending.push_back(v);
	};

	add(a0, ca); add(a1, ca); add(b1, cb);
	add(a0, ca); add(b1, cb); add(b0, cb);
}

//----------------------------------------------------------
// 窓(マップが覆う範囲)をカメラに追わせる。
// 窓を動かす時は、前の内容を「ずれたぶんだけ位置をずらして」新しい方へ写す。
// 隅はテクセルの境界へスナップしてから動かすので、写す量は必ず整数テクセルになり、
// 何度写しても絵が滲まない(半端な位置で写すと繰り返すたびにボケていく)。
//----------------------------------------------------------
void SkidMark::Recenter(float camX, float camZ)
{
	const float texel = MapCoverage / static_cast<float>(MapResolution);

	// 新しい隅：カメラを中心に置き、テクセル境界へ落とす
	const float nx = floorf(camX / texel) * texel - MapCoverage * 0.5f;
	const float nz = floorf(camZ / texel) * texel - MapCoverage * 0.5f;

	if (!m_hasOrigin)
	{
		m_originX = nx;
		m_originZ = nz;
		m_hasOrigin = true;
		return;
	}

	// まだ中心付近にいるなら動かさない(毎フレーム写すと無駄が大きい)
	const float cx = m_originX + MapCoverage * 0.5f;
	const float cz = m_originZ + MapCoverage * 0.5f;
	if (fabsf(camX - cx) < MapRecenterDist && fabsf(camZ - cz) < MapRecenterDist) { return; }

	const int shiftX = static_cast<int>(lroundf((nx - m_originX) / texel));
	const int shiftZ = static_cast<int>(lroundf((nz - m_originZ) / texel));
	if (shiftX == 0 && shiftZ == 0) { return; }

	// 今の内容を「前の絵」に回し、新しい方を空にしてから写す。
	// 写した後にはみ出す側は黒のまま残る＝そこは痕なしになる。
	std::swap(m_map, m_mapPrev);
	m_map.ClearTexture(Math::Color(0.0f, 0.0f, 0.0f, 1.0f));

	{
		KdRenderTargetChanger rtc;
		rtc.ChangeRenderTarget(m_map.m_RTTexture, nullptr, &m_map.m_viewPort);

		// スプライトの座標系は「描画先の中心が原点・Yは上向き」。
		// 焼き込みの正射影も +Z(ワールド) が上向きなので、Zのずれはそのまま
		// Yのずれとして扱える。内容は進んだ向きと逆へずらす。
		auto& sp = KdShaderManager::Instance().m_spriteShader;
		const Math::Vector2 pivot(0.5f, 0.5f);
		sp.Begin(false, true);   // 点サンプリング＝整数ずらしなので一切滲まない
		sp.DrawTex(m_mapPrev.m_RTTexture.get(), -shiftX, -shiftZ,
		           MapResolution, MapResolution, nullptr, nullptr, pivot);
		sp.End();

		rtc.UndoRenderTarget();
	}

	m_originX = nx;
	m_originZ = nz;
}

//----------------------------------------------------------
// 増えたぶんをマークマップへ焼き込む。
// マップはクリアしないので、ここで書いたものはずっと残る。
// 1フレームで焼くのは進んだ数センチぶんの四角形だけなので、
// 痕がどれだけ長くなっても、何本あってもコストは変わらない。
//----------------------------------------------------------
void SkidMark::BakePending()
{
	if (!m_ready) { return; }

	auto& shaderMgr = KdShaderManager::Instance();
	auto& shader    = shaderMgr.m_StandardShader;

	// 窓はカメラを追わせる。車ではなくカメラ基準にすることで、
	// 車が何台いても(マルチプレイでも)窓は1つで済む。
	const Math::Vector3 camPos = shaderMgr.GetCameraCB().CamPos;
	Recenter(camPos.x, camPos.z);

	if (m_pending.empty()) { return; }

	// 焼き込み中だけカメラを「窓を真上から見た正射影」に差し替える。
	// 元のカメラ情報は退避しておき、終わったら必ず戻す。
	const auto  saved     = shaderMgr.GetCameraCB();
	const Math::Matrix savedCam = saved.mView.Invert();

	// XZ平面をそのまま描くので、ビューは単位行列でよい(頂点側でXZ→XYにしてある)。
	const Math::Matrix ortho = DirectX::XMMatrixOrthographicOffCenterLH(
		m_originX, m_originX + MapCoverage,
		m_originZ, m_originZ + MapCoverage,
		-1.0f, 1.0f);
	shaderMgr.WriteCBCamera(Math::Matrix::Identity, ortho);

	{
		KdRenderTargetChanger rtc;
		rtc.ChangeRenderTarget(m_map.m_RTTexture, nullptr, &m_map.m_viewPort);

		shader.BeginUnLit();
		shaderMgr.ChangeBlendState(KdBlendState::Alpha);

		// 深度は使わない(真上からの平面描画なので前後関係が無い)
		shader.DrawVertices(m_pending, Math::Matrix::Identity, Math::Color(1, 1, 1, 1),
		                    KdDepthStencilState::ZDisable,
		                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		                    KdRasterizerState::CullNone);

		shaderMgr.UndoBlendState();

		// 描画先を必ず自分で元へ戻す。
		// KdRenderTargetChangerのデストラクタは保存したビューを解放するだけで
		// 復帰はしないため、任せるとマークマップが描画先に残り、
		// この後のシーンが丸ごとマップへ描かれてしまう(画面が真っさらになる)。
		rtc.UndoRenderTarget();
	}

	// カメラを元へ戻す(戻さないと以降の描画が全部この正射影になる)
	shaderMgr.WriteCBCamera(savedCam, saved.mProj);

	m_pending.clear();
}

//----------------------------------------------------------
// 路面を描く直前に呼ぶ。マップと覆う範囲をシェーダーへ渡す。
// 路面のピクセルシェーダーがワールドXZからこのマップを引いて色を暗くする。
//----------------------------------------------------------
void SkidMark::ApplyToShader() const
{
	if (!m_ready) { return; }

	KdShaderManager::Instance().m_StandardShader.SetMarkMap(
		m_map.m_RTTexture, m_originX, m_originZ, MapCoverage, MapDarken);
}
