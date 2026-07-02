#include "Framework/KdFramework.h"

#include "KdSpriteShader.h"

bool KdSpriteShader::Init()
{
	Release();

	//-------------------------------------
	// 頂点シェーダ
	//-------------------------------------
	{
		// コンパイル済みのシェーダーヘッダーファイルをインクルード
		#include "KdSpriteShader_VS.shaderInc"

		// 頂点シェーダー作成
		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS))) {
			assert(0 && "頂点シェーダー作成失敗");
			Release();
			return false;
		}

		// １頂点の詳細な情報
		std::vector<D3D11_INPUT_ELEMENT_DESC> layout = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,		0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,			0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		// 頂点インプットレイアウト作成
		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateInputLayout(
			&layout[0],
			(UINT)layout.size(),
			compiledBuffer,
			sizeof(compiledBuffer),
			&m_VLayout))
		){
			assert(0 && "CreateInputLayout失敗");
			Release();
			return false;
		}
	}

	//-------------------------------------
	// ピクセルシェーダ
	//-------------------------------------
	{
		// コンパイル済みのシェーダーヘッダーファイルをインクルード
		#include "KdSpriteShader_PS.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS))) {
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();
			return false;
		}
	}

	//-------------------------------------
	// 定数バッファ作成
	//-------------------------------------
	m_cb0.Create();
	m_cb1.Create();

	return true;
}

void KdSpriteShader::Release()
{
	KdSafeRelease(m_VS);
	KdSafeRelease(m_PS);
	KdSafeRelease(m_VLayout);
	m_cb0.Release();
	m_cb1.Release();
}

void KdSpriteShader::Begin(bool linear, bool disableZBuffer)
{
	// 既にBeginしている
	if (m_isBegin)return;
	m_isBegin = true;

	//---------------------------------------
	// 2D用正射影行列作成
	//---------------------------------------
	UINT pNumVierports = 1;
	D3D11_VIEWPORT vp;
	KdDirect3D::Instance().WorkDevContext()->RSGetViewports(&pNumVierports, &vp);
	m_mProj2D = DirectX::XMMatrixOrthographicLH(vp.Width, vp.Height, 0, 1);

	// 定数バッファ書き込み
	m_cb1.Work().mProj = m_mProj2D;
	m_cb1.Write();

	//---------------------------------------
	// 使用するステートをセット
	//---------------------------------------
	// Z判定、Z書き込み無効のステートをセット
	if (disableZBuffer) {
		KdShaderManager::Instance().ChangeDepthStencilState(KdDepthStencilState::ZDisable);
	}
	// Samplerステートをセット
	if (linear) {
		KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Linear_Clamp);
	}
	else {
		KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Point_Clamp);
	}
	// Rasterizerステートをセット
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);

	//---------------------------------------
	// シェーダ
	//---------------------------------------

	// シェーダをセット
	KdDirect3D::Instance().WorkDevContext()->VSSetShader(m_VS, 0, 0);
	KdDirect3D::Instance().WorkDevContext()->PSSetShader(m_PS, 0, 0);

	// 頂点レイアウトセット
	KdDirect3D::Instance().WorkDevContext()->IASetInputLayout(m_VLayout);

	// 定数バッファセット
	KdDirect3D::Instance().WorkDevContext()->VSSetConstantBuffers(0, 1, m_cb0.GetAddress());
	KdDirect3D::Instance().WorkDevContext()->PSSetConstantBuffers(0, 1, m_cb0.GetAddress());

	KdDirect3D::Instance().WorkDevContext()->VSSetConstantBuffers(1, 1, m_cb1.GetAddress());
	KdDirect3D::Instance().WorkDevContext()->PSSetConstantBuffers(1, 1, m_cb1.GetAddress());
}

void KdSpriteShader::End()
{
	if (!m_isBegin) { return; }
	m_isBegin = false;

	//---------------------------------------
	// 記憶してたステートに戻す
	//---------------------------------------
	KdShaderManager::Instance().UndoDepthStencilState();
	KdShaderManager::Instance().UndoSamplerState();
	KdShaderManager::Instance().UndoRasterizerState();
}

void KdSpriteShader::DrawTex(const KdTexture* tex, int x, int y, int w, int h, const Math::Rectangle* srcRect, const Math::Color* color, const Math::Vector2& pivot)
{
	if (tex == nullptr)return;

	// もし開始していない場合は開始する(最後にEnd())
	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// テクスチャ(ShaderResourceView)セット
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, tex->WorkSRViewAddress());

	// 色
	if (color) {
		m_cb0.Work().Color = *color;
	}
	m_cb0.Write();

	// UV
	Math::Vector2 uvMin = { 0, 0 };
	Math::Vector2 uvMax = { 1, 1 };
	if (srcRect)
	{
		uvMin.x = srcRect->x / (float)tex->GetInfo().Width;
		uvMin.y = srcRect->y / (float)tex->GetInfo().Height;

		uvMax.x = (srcRect->x + srcRect->width) / (float)tex->GetInfo().Width;
		uvMax.y = (srcRect->y + srcRect->height) / (float)tex->GetInfo().Height;
	}

	// 頂点作成
	float x1 = (float)x;
	float y1 = (float)y;
	float x2 = (float)(x + w);
	float y2 = (float)(y + h);

	// 基準点(Pivot)ぶんずらす
	x1 -= pivot.x * w;
	x2 -= pivot.x * w;
	y1 -= pivot.y * h;
	y2 -= pivot.y * h;

	Vertex vertex[] = {
		{ {x1, y1, 0},	{uvMin.x, uvMax.y} },
		{ {x1, y2, 0},	{uvMin.x, uvMin.y} },
		{ {x2, y1, 0},	{uvMax.x, uvMax.y} },
		{ {x2, y2, 0},	{uvMax.x, uvMin.y} }

	};

	// 描画
	KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4, vertex, sizeof(Vertex));

	// セットしたテクスチャを解除しておく
	ID3D11ShaderResourceView* srv = nullptr;
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, &srv);

	// この関数でBeginした場合は、Endしておく
	if (!bBgn)End();
}

void KdSpriteShader::DrawTexVertices(const KdTexture* tex, const std::vector<Vertex>& vertices,
	D3D_PRIMITIVE_TOPOLOGY topology, const Math::Color* color)
{
	if (tex == nullptr || vertices.size() < 3) { return; }

	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// テクスチャ(ShaderResourceView)セット
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, tex->WorkSRViewAddress());

	if (color) { m_cb0.Work().Color = *color; }
	m_cb0.Write();

	KdDirect3D::Instance().DrawVertices(topology, static_cast<int>(vertices.size()),
		vertices.data(), sizeof(Vertex));

	// テクスチャ解除
	ID3D11ShaderResourceView* srv = nullptr;
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, &srv);

	if (!bBgn)End();
}

void KdSpriteShader::DrawPoint(int x, int y, const Math::Color* color)
{
	// もし開始していない場合は開始する(最後にEnd())
	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// 白テクスチャ
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, KdDirect3D::Instance().GetWhiteTex()->WorkSRViewAddress());

	// 色
	if (color) {
		m_cb0.Work().Color = *color;
	}
	m_cb0.Write();


	// 描画
	Vertex vertex[] = {
		{ {(float)x, (float)y, 0},	{0, 0} },
	};
	KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_POINTLIST, 1, vertex, sizeof(Vertex));

	// この関数でBeginした場合は、Endしておく
	if (!bBgn)End();
}

void KdSpriteShader::DrawLine(int x1, int y1, int x2, int y2, const Math::Color* color)
{
	// もし開始していない場合は開始する(最後にEnd())
	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// 白テクスチャ
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, KdDirect3D::Instance().GetWhiteTex()->WorkSRViewAddress());

	// 色
	if (color) {
		m_cb0.Work().Color = *color;
	}
	m_cb0.Write();


	// 描画
	Vertex vertex[] = {
		{ {(float)x1, (float)y1, 0},	{0, 0} },
		{ {(float)x2, (float)y2, 0},	{1, 0} },
	};
	KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP, 2, vertex, sizeof(Vertex));

	// この関数でBeginした場合は、Endしておく
	if (!bBgn)End();
}

void KdSpriteShader::DrawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, const Math::Color* color, bool fill)
{
	// もし開始していない場合は開始する(最後にEnd())
	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// 白テクスチャ
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, KdDirect3D::Instance().GetWhiteTex()->WorkSRViewAddress());

	// 色
	if (color) {
		m_cb0.Work().Color = *color;
	}
	m_cb0.Write();


	// 描画
	Vertex vertex[] = {
		{ {(float)x1, (float)y1, 0},	{0, 0} },
		{ {(float)x2, (float)y2, 0},	{1, 0} },
		{ {(float)x3, (float)y3, 0},	{0, 0} },
		{ {(float)x1, (float)y1, 0},	{1, 0} },
	};
	KdDirect3D::Instance().DrawVertices(
		fill ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP : D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
		4, vertex, sizeof(Vertex));


	// この関数でBeginした場合は、Endしておく
	if (!bBgn)End();
}

void KdSpriteShader::DrawCircle(int x, int y, int radius, const Math::Color* color, bool fill)
{
	if (radius <= 0)return;

	// もし開始していない場合は開始する(最後にEnd())
	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// 白テクスチャ
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, KdDirect3D::Instance().GetWhiteTex()->WorkSRViewAddress());

	// 色
	if (color) {
		m_cb0.Work().Color = *color;
	}
	m_cb0.Write();

	// 頂点
	if (fill)
	{
		int segments = radius + 1;
		if (segments > 300)segments = 300;
		if (segments < 3)  segments = 3;
		const float step = 360.0f / (float)segments;	// segments で割ってちょうど一周で閉じる
		std::vector<Vertex> vertex(segments * 3);		// 半径により頂点数を調整

		// 描画（中心＋外周2点のトライアングルファンを TRIANGLELIST で構築）
		for (int i = 0; i < segments; i++)
		{
			const int idx = i * 3;
			const float a0 = DirectX::XMConvertToRadians(i       * step);
			const float a1 = DirectX::XMConvertToRadians((i + 1) * step);

			// 中心（z まで明示初期化。未設定だと深度破綻でビーム状に伸びることがある）
			vertex[idx].Pos     = { (float)x, (float)y, 0.0f };
			vertex[idx + 1].Pos = { x + cosf(a0) * (float)radius, y + sinf(a0) * (float)radius, 0.0f };
			vertex[idx + 2].Pos = { x + cosf(a1) * (float)radius, y + sinf(a1) * (float)radius, 0.0f };
		}

		KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, (int)vertex.size(), &vertex[0], sizeof(Vertex));
	}
	else
	{
		int numVertex = radius + 1;
		if (numVertex > 300)numVertex = 300;
		std::vector<Vertex> vertex(numVertex);		// 半径により頂点数を調整

		// 描画
		for (int i = 0; i < numVertex; i++)
		{
			vertex[i].Pos.x = x + cos(DirectX::XMConvertToRadians(i * (360.0f / (numVertex - 1)))) * (float)radius;
			vertex[i].Pos.y = y + sin(DirectX::XMConvertToRadians(i * (360.0f / (numVertex - 1)))) * (float)radius;
			vertex[i].Pos.z = 0;
		}

		KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP, numVertex, &vertex[0], sizeof(Vertex));
	}

	// この関数でBeginした場合は、Endしておく
	if (!bBgn)End();
}

void KdSpriteShader::DrawBox(int x, int y, int extentX, int extentY, const Math::Color* color, bool fill)
{
	// もし開始していない場合は開始する(最後にEnd())
	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// 白テクスチャ
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, KdDirect3D::Instance().GetWhiteTex()->WorkSRViewAddress());

	// 色
	if (color) {
		m_cb0.Work().Color = *color;
	}
	m_cb0.Write();

	Math::Vector3 p1 = { (float)x - extentX, (float)y - extentY, 0 };
	Math::Vector3 p2 = { (float)x - extentX, (float)y + extentY, 0 };
	Math::Vector3 p3 = { (float)x + extentX, (float)y + extentY, 0 };
	Math::Vector3 p4 = { (float)x + extentX, (float)y - extentY, 0 };

	// 描画
	if (fill)
	{
		Vertex vertex[] = {
			{ p1, {0,0}},
			{ p2, {0,0}},
			{ p4, {0,0}},
			{ p3, {0,0}}
		};

		KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4, vertex, sizeof(Vertex));
	}
	else
	{
		Vertex vertex[] = {
			{ p1, {0,0}},
			{ p2, {0,0}},
			{ p3, {0,0}},
			{ p4, {0,0}},
			{ p1, {0,0}}
		};

		KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP, 5, vertex, sizeof(Vertex));
	}

	// この関数でBeginした場合は、Endしておく
	if (!bBgn)End();
}

// 角丸の塗りつぶしボックス
void KdSpriteShader::DrawRoundedBox(int x, int y, int extentX, int extentY, float radius,
	const Math::Color* color, int cornerSegs)
{
	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// 白テクスチャ
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, KdDirect3D::Instance().GetWhiteTex()->WorkSRViewAddress());

	if (color) { m_cb0.Work().Color = *color; }
	m_cb0.Write();

	const float ex = static_cast<float>(extentX);
	const float ey = static_cast<float>(extentY);
	float r = radius;
	const float maxR = (ex < ey) ? ex : ey;
	if (r > maxR) { r = maxR; }
	if (r < 0.0f) { r = 0.0f; }
	if (cornerSegs < 1) { cornerSegs = 1; }

	const float cx = static_cast<float>(x);
	const float cy = static_cast<float>(y);
	const float kQuarter = 1.57079632679f;

	// 4つの角の中心と、その角の開始角度（反時計回り）
	const float ccx[4] = { cx + ex - r, cx - ex + r, cx - ex + r, cx + ex - r };
	const float ccy[4] = { cy + ey - r, cy + ey - r, cy - ey + r, cy - ey + r };
	const float a0 [4] = { 0.0f, kQuarter, kQuarter * 2.0f, kQuarter * 3.0f };

	// 外周点を作る（角ごとに弧を分割）
	std::vector<Math::Vector3> peri;
	peri.reserve(static_cast<size_t>(cornerSegs + 1) * 4);
	for (int c = 0; c < 4; ++c)
	{
		for (int s = 0; s <= cornerSegs; ++s)
		{
			const float ang = a0[c] + kQuarter * (static_cast<float>(s) / static_cast<float>(cornerSegs));
			peri.push_back({ ccx[c] + std::cosf(ang) * r, ccy[c] + std::sinf(ang) * r, 0.0f });
		}
	}

	// 中心からの三角ファン → TRIANGLELIST
	const Math::Vector3 center{ cx, cy, 0.0f };
	const size_t n = peri.size();
	std::vector<Vertex> verts;
	verts.reserve(n * 3);
	for (size_t i = 0; i < n; ++i)
	{
		verts.push_back({ center,          {0, 0} });
		verts.push_back({ peri[i],         {0, 0} });
		verts.push_back({ peri[(i + 1) % n], {0, 0} });
	}

	KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		static_cast<int>(verts.size()), verts.data(), sizeof(Vertex));

	if (!bBgn)End();
}

void KdSpriteShader::DrawRoundedTex(const KdTexture* tex, int x, int y, int extentX, int extentY, float radius,
	const Math::Color* color, int cornerSegs)
{
	if (tex == nullptr) { return; }

	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// テクスチャ(ShaderResourceView)セット
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, tex->WorkSRViewAddress());

	if (color) { m_cb0.Work().Color = *color; }
	m_cb0.Write();

	const float ex = static_cast<float>(extentX);
	const float ey = static_cast<float>(extentY);
	float r = radius;
	const float maxR = (ex < ey) ? ex : ey;
	if (r > maxR) { r = maxR; }
	if (r < 0.0f) { r = 0.0f; }
	if (cornerSegs < 1) { cornerSegs = 1; }

	const float cx = static_cast<float>(x);
	const float cy = static_cast<float>(y);
	const float kQuarter = 1.57079632679f;

	// 4つの角の中心と、その角の開始角度（反時計回り）
	const float ccx[4] = { cx + ex - r, cx - ex + r, cx - ex + r, cx + ex - r };
	const float ccy[4] = { cy + ey - r, cy + ey - r, cy - ey + r, cy - ey + r };
	const float a0 [4] = { 0.0f, kQuarter, kQuarter * 2.0f, kQuarter * 3.0f };

	// アスペクト比を無視して全面を埋める（cover：縦横比を保ったまま中央クロップ）。
	// 画像と枠の比が違っても歪まず、はみ出した分だけ切り取られる。
	float uScale = 1.0f, vScale = 1.0f;   // 0〜1 のうち実際に使うUV幅（残りはクロップ）
	{
		const float tw = static_cast<float>(tex->GetInfo().Width);
		const float th = static_cast<float>(tex->GetInfo().Height);
		if (tw > 0.0f && th > 0.0f && ex > 0.0f && ey > 0.0f)
		{
			const float boxAspect = ex / ey;   // 枠の横/縦
			const float texAspect = tw / th;   // 画像の横/縦
			if (texAspect > boxAspect) { uScale = boxAspect / texAspect; }  // 画像が横長→左右をクロップ
			else                       { vScale = texAspect / boxAspect; }  // 画像が縦長→上下をクロップ
		}
	}
	const float uMin = 0.5f - uScale * 0.5f;
	const float vMin = 0.5f - vScale * 0.5f;

	// 位置→UV（枠全体を基準に0〜1 → coverのサブ矩形へ写像。+Yが上なので上端でV=0）
	const float left = cx - ex;
	const float top  = cy + ey;
	auto toUV = [&](const Math::Vector3& p) -> Math::Vector2
	{
		const float u01 = (p.x - left) / (2.0f * ex);
		const float v01 = (top - p.y) / (2.0f * ey);
		return { uMin + u01 * uScale, vMin + v01 * vScale };
	};

	// 外周点を作る（角ごとに弧を分割）
	std::vector<Math::Vector3> peri;
	peri.reserve(static_cast<size_t>(cornerSegs + 1) * 4);
	for (int c = 0; c < 4; ++c)
	{
		for (int s = 0; s <= cornerSegs; ++s)
		{
			const float ang = a0[c] + kQuarter * (static_cast<float>(s) / static_cast<float>(cornerSegs));
			peri.push_back({ ccx[c] + std::cosf(ang) * r, ccy[c] + std::sinf(ang) * r, 0.0f });
		}
	}

	// 中心からの三角ファン → TRIANGLELIST（各頂点にUVを付与）
	const Math::Vector3 center{ cx, cy, 0.0f };
	const Math::Vector2 centerUV{ 0.5f, 0.5f };
	const size_t n = peri.size();
	std::vector<Vertex> verts;
	verts.reserve(n * 3);
	for (size_t i = 0; i < n; ++i)
	{
		const Math::Vector3& p0 = peri[i];
		const Math::Vector3& p1 = peri[(i + 1) % n];
		verts.push_back({ center, centerUV });
		verts.push_back({ p0,     toUV(p0) });
		verts.push_back({ p1,     toUV(p1) });
	}

	KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		static_cast<int>(verts.size()), verts.data(), sizeof(Vertex));

	// セットしたテクスチャを解除しておく
	ID3D11ShaderResourceView* srv = nullptr;
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, &srv);

	if (!bBgn)End();
}

void KdSpriteShader::DrawRoundedBubble(int x, int y, int extentX, int extentY, float radius,
	int tailW, int tailH, const Math::Color* color, int cornerSegs)
{
	// 角丸ボックス本体
	DrawRoundedBox(x, y, extentX, extentY, radius, color, cornerSegs);

	// 下向きの尻尾（三角）
	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, KdDirect3D::Instance().GetWhiteTex()->WorkSRViewAddress());
	if (color) { m_cb0.Work().Color = *color; }
	m_cb0.Write();

	const float fx = static_cast<float>(x);
	const float fy = static_cast<float>(y);
	const float baseY = fy - static_cast<float>(extentY) + 1.0f;        // ボックス下端（少し内側）
	const float apexY = fy - static_cast<float>(extentY) - static_cast<float>(tailH); // 下へ伸びる先端

	// center-origin・+Yが上。左根元 → 先端 → 右根元
	Vertex v[3] =
	{
		{ { fx - static_cast<float>(tailW), baseY, 0.0f }, { 0, 0 } },
		{ { fx,                              apexY, 0.0f }, { 0, 0 } },
		{ { fx + static_cast<float>(tailW), baseY, 0.0f }, { 0, 0 } },
	};
	KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, 3, v, sizeof(Vertex));

	if (!bBgn)End();
}

void KdSpriteShader::DrawRoundedBubbleTo(int x, int y, int extentX, int extentY, float radius,
	float tailTargetX, float tailTargetY, int tailHalfW, float maxTailLen,
	const Math::Color* color, int cornerSegs)
{
	DrawRoundedBox(x, y, extentX, extentY, radius, color, cornerSegs);

	const float cx = static_cast<float>(x);
	const float cy = static_cast<float>(y);
	float dx = tailTargetX - cx;
	float dy = tailTargetY - cy;
	float len = std::sqrtf(dx * dx + dy * dy);
	if (len < 1e-3f) { return; }   // 同じ位置なら尻尾なし
	dx /= len; dy /= len;          // 方向（正規化）

	// ボックス縁上の尻尾の根元中心（矩形境界へ）
	const float ex = static_cast<float>(extentX);
	const float ey = static_cast<float>(extentY);
	const float sX = (std::fabs(dx) > 1e-4f) ? ex / std::fabs(dx) : 1e9f;
	const float sY = (std::fabs(dy) > 1e-4f) ? ey / std::fabs(dy) : 1e9f;
	const float s  = (sX < sY) ? sX : sY;
	const float baseX = cx + dx * s;
	const float baseY = cy + dy * s;

	// 尻尾の長さ（対象まで。遠ければ maxTailLen でクランプ）
	float tailLen = len - s;
	if (tailLen < 6.0f) { tailLen = 6.0f; }
	if (tailLen > maxTailLen) { tailLen = maxTailLen; }
	const float apexX = baseX + dx * tailLen;
	const float apexY = baseY + dy * tailLen;

	// 根元の左右（方向に垂直）
	const float px = -dy, py = dx;
	const float hw = static_cast<float>(tailHalfW);

	bool bBgn = m_isBegin;
	if (!bBgn)Begin();
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, KdDirect3D::Instance().GetWhiteTex()->WorkSRViewAddress());
	if (color) { m_cb0.Work().Color = *color; }
	m_cb0.Write();
	Vertex v[3] =
	{
		{ { baseX + px * hw, baseY + py * hw, 0.0f }, { 0, 0 } },
		{ { apexX,           apexY,           0.0f }, { 0, 0 } },
		{ { baseX - px * hw, baseY - py * hw, 0.0f }, { 0, 0 } },
	};
	KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, 3, v, sizeof(Vertex));
	if (!bBgn)End();
}

// 切り抜き範囲を設定する
// ・rect			… 範囲

void KdSpriteShader::SetScissorRect(const Math::Rectangle& rect)
{
	// ラスタライザステート作成・セット
	ID3D11RasterizerState* rs = KdDirect3D::Instance().CreateRasterizerState(D3D11_CULL_BACK, D3D11_FILL_SOLID, true, true);
	KdDirect3D::Instance().WorkDevContext()->RSSetState(rs);
	rs->Release();

	D3D11_RECT rc{};
	rc.left = rect.x;
	rc.top = rect.y;
	rc.right = rect.x + rect.width;
	rc.bottom = rect.y + rect.height;
	KdDirect3D::Instance().WorkDevContext()->RSSetScissorRects(1, &rc);
}

void KdSpriteShader::DrawFont(std::shared_ptr<KdFontSprite>& fontSprite, const Math::Vector2& Pos, const Math::Color* color, const int antiAliasingFlag)
{
	if (fontSprite == nullptr)					return;
	if (fontSprite->GetTexList().size() == 0)	return;

	// もし開始していない場合は開始する(最後にEnd())
	bool bBgn = m_isBegin;
	if (!bBgn)Begin();

	// 白テクスチャ
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, KdDirect3D::Instance().GetWhiteTex()->WorkSRViewAddress());

	// 色
	if (color) {
		m_cb0.Work().Color = *color;
	}
	m_cb0.Write();

	// フォントの高さ
	float _moveY = (float)fontSprite->GetTexList()[0]->FontTex->GetInfo().Height;

	// 全ての文字テクスチャを描画する
	float			_moveX	= 0;
	Math::Vector2	_pos	= Pos;
	for (auto& data : fontSprite->GetTexList())
	{
		// 改行文字の場合は、X,Y座標を操作
		if (data->Code == '\n')
		{
			_pos.x -= _moveX;
			_pos.y -= _moveY;
			_moveX = 0;
		}
		// その他の文字
		else {
			// テクスチャをセット
			KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, data->FontTex->WorkSRViewAddress());

			// UV
			Math::Vector2 uvMin = { 0, 0 };
			Math::Vector2 uvMax = { 1, 1 };

			// 頂点作成
			float imageW = (float)data->FontTex->GetInfo().Width;
			float imageH = (float)data->FontTex->GetInfo().Height;
			float x1 = (float)_pos.x;
			float y1 = (float)_pos.y;
			float x2 = (float)(x1 + imageW);
			float y2 = (float)(y1 + imageH);

			Vertex vertex[] = {
				{ {x1, y1, 0},	{uvMin.x, uvMax.y} },
				{ {x1, y2, 0},	{uvMin.x, uvMin.y} },
				{ {x2, y1, 0},	{uvMax.x, uvMax.y} },
				{ {x2, y2, 0},	{uvMax.x, uvMin.y} }
			};
			// 描画
			KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4, vertex, sizeof(Vertex));

			// 次の文字のため、X座標を進める
			_pos.x += data->FontTex->GetInfo().Width;
			_moveX += data->FontTex->GetInfo().Width;
		}
	}

	// セットしたテクスチャを解除しておく
	ID3D11ShaderResourceView* srv = nullptr;
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, 1, &srv);

	// この関数でBeginした場合は、Endしておく
	if (!bBgn)End();
}

void KdSpriteShader::DrawFont(const Math::Vector2& Pos, const Math::Color* color, const char* format, ...)
{
	char tmpStr[128]{};
	va_list argptr;
	va_start(argptr, format);
	vsprintf_s(tmpStr, format, argptr);
	va_end(argptr);
	std::shared_ptr<KdFontSprite> fontSprite = KdFontManager::Instance().CreateFontTexture(0, tmpStr, false);

	DrawFont(fontSprite, Pos, color, 0);
}