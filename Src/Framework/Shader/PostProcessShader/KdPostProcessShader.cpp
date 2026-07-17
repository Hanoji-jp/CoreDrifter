#include "KdPostProcessShader.h"
#include "../../../Application/Const/PostProcessConst.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// シェーダー本体の生成、定数バッファの生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool KdPostProcessShader::Init()
{
	// VS と InputLayout作成
	{
#include "KdPostProcessShader_VS.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS))) {
			assert(0 && "頂点シェーダー作成失敗");
			Release();
			return false;
		}

		std::vector<D3D11_INPUT_ELEMENT_DESC> layout = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,		0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,			0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateInputLayout(
			&layout[0], (UINT)layout.size(), compiledBuffer,
			sizeof(compiledBuffer), &m_inputLayout)) ) 
		{
			assert(0 && "CreateInputLayout失敗");
			Release();
			return false;
		}
	}

	// PS 作成
	{
#include "KdPostProcessShader_PS_Blur.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Blur))) 
		{
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();
			
			return false;
		}

	}

	{
#include "KdPostProcessShader_PS_DoF.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_DoF))) 
		{
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();

			return false;
		}
	}

	{
#include "KdPostProcessShader_PS_Bright.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Bright)))
		{
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();

			return false;
		}
	}

	// アウトライン（画面エッジ検出）PS
	{
#include "KdPostProcessShader_PS_Outline.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Outline)))
		{
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();
			return false;
		}
	}

	// 煙シルエット輪郭 PS
	{
#include "KdPostProcessShader_PS_SmokeOutline.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_SmokeOutline)))
		{
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();
			return false;
		}
	}

	// 文字流体化 PS
	{
#include "KdPostProcessShader_PS_TextFluid.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_TextFluid)))
		{
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();
			return false;
		}
	}

	m_cb0_BlurInfo.Create();

	m_cb0_DoFInfo.Create();

	m_cb0_BrightInfo.Create();

	m_cb0_OutlineInfo.Create();

	m_cb0_SmokeOutline.Create();

	m_cb0_TextFluid.Create();

	const std::shared_ptr<KdTexture>& backBuffer = KdDirect3D::Instance().GetBackBuffer();
	
	// ポストプロセス用のシーンの全描画用画像
	m_postEffectRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight(), true);

	// ぼかし画像
	m_blurRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());
	m_strongBlurRTPack.CreateRenderTarget(backBuffer->GetWidth() / 2, backBuffer->GetHeight() / 2);
	m_motionBlurRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	// 被写界深度画像
	m_depthOfFieldRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	// アウトライン合成画像
	m_outlineRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	// 煙専用の描画先(色+アルファ)。深度は既存シーンの物を流用するのでここでは色のみ。
	m_smokeRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	// 煙をガウスぼかしした版(縁を柔らかく＋消える粒のポップを目立たなくする)
	m_smokeBlurRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	// 文字流体化：文字画像は初回描画時に遅延読み込みする(Init段階だと読めない場合があるため)

	m_brightEffectRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	int lightBloomWidth = m_brightEffectRTPack.m_RTTexture->GetWidth();
	int lightBloomHeight = m_brightEffectRTPack.m_RTTexture->GetHeight();

	// 光源ぼかし画像
	for (int i = 0; i < kLightBloomNum; ++i)
	{
		m_lightBloomRTPack[i].CreateRenderTarget(lightBloomWidth, lightBloomHeight);

		lightBloomWidth /= 2;
		lightBloomHeight /= 2;
	}

	// 画面全体に書き込む用の頂点情報
	m_screenVert[0] = { {-1,-1,0}, {0, 1} };
	m_screenVert[1] = { {-1, 1,0}, {0, 0} };
	m_screenVert[2] = { { 1,-1,0}, {1, 1} };
	m_screenVert[3] = { { 1, 1,0}, {1, 0} };

	SetBrightThreshold( 1.2f );

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// シェーダー本体の解放、定数バッファの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::Release()
{
	KdSafeRelease(m_VS);

	KdSafeRelease(m_inputLayout);

	KdSafeRelease(m_PS_Blur);
	KdSafeRelease(m_PS_DoF);
	KdSafeRelease(m_PS_Bright);
	KdSafeRelease(m_PS_Outline);
	KdSafeRelease(m_PS_SmokeOutline);
	KdSafeRelease(m_PS_TextFluid);

	m_cb0_BlurInfo.Release();
	m_cb0_DoFInfo.Release();
	m_cb0_BrightInfo.Release();
	m_cb0_OutlineInfo.Release();
	m_cb0_SmokeOutline.Release();
	m_cb0_TextFluid.Release();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::Draw()
{
	// ポストエフェクトテクスチャの描画クリア
	m_postEffectRTPack.ClearTexture();

	// 光源描画テクスチャの描画クリア
	m_brightEffectRTPack.ClearTexture(kBlackColor);

	// レンダーターゲット変更
	if (!m_postEffectRTChanger.ChangeRenderTarget(m_postEffectRTPack))
	{
		// 失敗したらUndo
		m_postEffectRTChanger.UndoRenderTarget();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::BeginBright()
{
	if (!m_brightRTChanger.ChangeRenderTarget(m_brightEffectRTPack.m_RTTexture, m_postEffectRTPack.m_ZBuffer, &m_brightEffectRTPack.m_viewPort))
	{
		m_brightRTChanger.UndoRenderTarget();
	}

	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);

	KdShaderManager::Instance().ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
}

void KdPostProcessShader::EndBright()
{
	KdShaderManager::Instance().UndoDepthStencilState();

	KdShaderManager::Instance().UndoBlendState();

	m_brightRTChanger.UndoRenderTarget();
}

void KdPostProcessShader::PostEffectProcess()
{
	m_postEffectRTChanger.UndoRenderTarget();

	LightBloomProcess();

	BlurProcess();

	DepthOfFieldProcess();

	// ---- モーションブラー（デバッグ: 常時強制ブラーで動作確認）----
	{
		const auto& camCB = KdShaderManager::Instance().GetCameraCB();
		const Math::Vector3 camPos = camCB.CamPos;
		m_camPosSet = false;

		bool useMotionBlur = false;

		if (m_motionBlurEnabled && m_prevCamPosValid)
		{
			const Math::Vector3 delta = camPos - m_prevCamPos;
			const float speed = delta.Length();

			if (speed >= PostProcessConst::MotionBlurSpeedThreshold)
			{
				// ワールド空間の移動ベクトルをView行列の回転成分（上3x3）でスクリーン方向に変換
				// ※ カメラ位置自体をVPで投影すると現在位置は常にNDC(0,0)になるため使えない
				const Math::Vector3 deltaDir = delta / speed; // 正規化方向

				// Viewの回転成分のみ適用（位置は無視）
				const Math::Matrix& mView = camCB.mView;
				Math::Vector2 screenDir = {
					deltaDir.x * mView._11 + deltaDir.y * mView._21 + deltaDir.z * mView._31,
					deltaDir.x * mView._12 + deltaDir.y * mView._22 + deltaDir.z * mView._32
				};
				// UV空間はY反転
				screenDir.y = -screenDir.y;

				const float strength = std::min(speed * PostProcessConst::MotionBlurScale,
												PostProcessConst::MotionBlurMaxStrength);
				Math::Vector2 blurDir = screenDir;
				blurDir.Normalize();
				blurDir *= strength;

				const float blurLen = blurDir.Length();
				if (blurLen > 0.00001f)
				{
					const float strength = std::min(blurLen * PostProcessConst::MotionBlurScale,
													PostProcessConst::MotionBlurMaxStrength);
					blurDir.Normalize();
					blurDir *= strength;

					GenerateMotionBlurTexture(
						m_depthOfFieldRTPack.m_RTTexture,
						m_motionBlurRTPack.m_RTTexture,
						m_motionBlurRTPack.m_viewPort,
						PostProcessConst::MotionBlurSamplingRadius,
						blurDir);

					useMotionBlur = true;
				}
			}
		}
		else
		{
			// 初回フレームは通常描画
			m_prevCamPosValid = true;
		}

		m_prevCamPos = camPos;

		// 最終画像（DoF or モーションブラー）をバックバッファへ
		// ※アウトラインは ApplySceneOutline()（不透明シーン直後）で適用済み
		std::shared_ptr<KdTexture> finalColor =
			useMotionBlur ? m_motionBlurRTPack.m_RTTexture : m_depthOfFieldRTPack.m_RTTexture;
		KdShaderManager::Instance().m_spriteShader.DrawTex(finalColor.get(), 0, 0);
	}
}

// 不透明シーンにだけアウトラインを適用（この後にエフェクトが上描きされる）
void KdPostProcessShader::ApplySceneOutline()
{
	if (!m_sceneOutlineEnabled) { return; }

	// 不透明シーン色＋深度からエッジ検出 → m_outlineRTPack へ
	OutlineProcess(m_postEffectRTPack.m_RTTexture);
	// 結果を現在のRT（=シーンRT m_postEffectRTPack）へ書き戻す
	KdShaderManager::Instance().m_spriteShader.DrawTex(m_outlineRTPack.m_RTTexture.get(), 0, 0);
}

// 煙を専用RTへ描き始める。背景は透明でクリアし、シーン深度を流用して遮蔽(地形隠れ)を維持する。
void KdPostProcessShader::BeginSmoke()
{
	if (!m_smokeOutlineEnabled) { return; }

	// 透明でクリア（アルファ0＝カバレッジ0）
	m_smokeRTPack.ClearTexture(Math::Color(0.0f, 0.0f, 0.0f, 0.0f));

	// 描画先を煙RTへ。深度はシーンの物を流用（地形に隠れる遮蔽を維持）。
	if (!m_smokeRTChanger.ChangeRenderTarget(m_smokeRTPack.m_RTTexture,
		m_postEffectRTPack.m_ZBuffer, &m_smokeRTPack.m_viewPort))
	{
		m_smokeRTChanger.UndoRenderTarget();
	}
}

// 煙RTを閉じ、塊全体のシルエット外周に輪郭を乗せてシーンへアルファ合成する。
void KdPostProcessShader::EndSmokeAndComposite()
{
	if (!m_smokeOutlineEnabled) { return; }

	// 描画先をシーンRT（m_postEffectRTPack）へ戻す
	m_smokeRTChanger.UndoRenderTarget();

	KdShaderManager& mgr = KdShaderManager::Instance();

	SetSmokeOutlineToDevice();
	mgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);
	mgr.ChangeBlendState(KdBlendState::Alpha);                     // 煙＋輪郭を「over」合成
	mgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	ID3D11DeviceContext* dc = KdDirect3D::Instance().WorkDevContext();
	dc->PSSetShaderResources(0, 1, m_smokeRTPack.m_RTTexture->WorkSRViewAddress());

	// 全画面クアッドで合成（描画先＝現在のシーンRT）
	KdDirect3D::Instance().DrawVertices(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4,
		&m_screenVert[0], sizeof(Vertex));

	ID3D11ShaderResourceView* nullSRV = nullptr;
	dc->PSSetShaderResources(0, 1, &nullSRV);

	mgr.UndoDepthStencilState();
	mgr.UndoBlendState();
	mgr.UndoSamplerState();
}

void KdPostProcessShader::SetSmokeOutlineToDevice()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	// 画面のテクセルサイズを反映
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	m_cb0_SmokeOutline.Work().TexelX = 1.0f / static_cast<float>(bb->GetWidth());
	m_cb0_SmokeOutline.Work().TexelY = 1.0f / static_cast<float>(bb->GetHeight());
	m_cb0_SmokeOutline.Write();
	DevCon->PSSetConstantBuffers(0, 1, m_cb0_SmokeOutline.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}
	shaderMgr.SetPixelShader(m_PS_SmokeOutline);
}

// 空ならデフォルトの文字エフェクトを1つ用意(初回表示用)。
void KdPostProcessShader::EnsureFluidItems()
{
	if (m_fluidItems.empty())
	{
		m_fluidItems.push_back(std::make_shared<FluidTextItem>());
		m_fluidSelected = 0;
	}
}

// 選択中オブジェクトの文字を差し替え(スコア表示などから呼ぶ)。
void KdPostProcessShader::SetFluidText(const char* str)
{
	if (!str) { return; }
	EnsureFluidItems();
	if (m_fluidSelected < 0 || m_fluidSelected >= static_cast<int>(m_fluidItems.size())) { return; }
	auto& it = *m_fluidItems[m_fluidSelected];
	if (it.str != str)
	{
		it.str = str;
		// ImGui入力欄も同期(はみ出し安全)
		strncpy_s(it.editBuf, sizeof(it.editBuf), str, _TRUNCATE);
		it.dirty = true;
	}
}

// 選択中オブジェクトの効果強さ(0-1)。
void KdPostProcessShader::SetFluidTextIntensity(float v)
{
	if (m_fluidSelected < 0 || m_fluidSelected >= static_cast<int>(m_fluidItems.size())) { return; }
	m_fluidItems[m_fluidSelected]->params.Intensity = v;
}

// 選択中オブジェクトのスタイルを次へ回して番号を返す。
int KdPostProcessShader::CycleFluidStyle()
{
	EnsureFluidItems();
	if (m_fluidSelected < 0 || m_fluidSelected >= static_cast<int>(m_fluidItems.size())) { return 0; }
	auto& s = m_fluidItems[m_fluidSelected]->params.Style;
	int n = ((int)(s + 0.5f) + 1) % kFluidStyleCount;
	s = static_cast<float>(n);
	return n;
}

// UTF-8(ImGui)→Shift-JIS(フォント側が期待)へ変換。数字/英字はそのまま、日本語の化けを防ぐ。
static std::string Utf8ToSjis(const std::string& u8)
{
	if (u8.empty()) { return ""; }
	int wlen = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
	if (wlen <= 0) { return u8; }
	std::wstring w(wlen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, &w[0], wlen);
	int slen = WideCharToMultiByte(932, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (slen <= 0) { return u8; }
	std::string s(slen, '\0');
	WideCharToMultiByte(932, 0, w.c_str(), -1, &s[0], slen, nullptr, nullptr);
	if (!s.empty() && s.back() == '\0') { s.pop_back(); }
	return s;
}

// item.str を DrawFont で専用RTへ焼く。RTは"文字の実寸"で作り直す(幅いっぱいに文字が入る)。
void KdPostProcessShader::BakeFluidText(FluidTextItem& item)
{
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	// 大フォント(No.1)でグリフ生成。UTF-8→SJIS変換して渡す。初回空ならdirtyのまま再試行。
	auto fs = KdFontManager::Instance().CreateFontTexture(1, Utf8ToSjis(item.str), 0);
	if (!fs || fs->GetTexList().empty()) { return; }

	// グリフ幅を実測(DrawFontはこの幅ぶんX前進する)＝テキスト全体の幅・高さ＋グリフ数。
	// 各グリフ幅も控えておく(全角/半角で幅が違う＝桁ごとグラデのエッジ計算に使う)。
	int tw = 0, th = 1, glyphs = 0;
	std::vector<int> glyphWidths;
	for (auto& d : fs->GetTexList())
	{
		if (!d || !d->FontTex) { continue; }        // null安全
		if (d->Code == '\n') { continue; }
		const int gw = static_cast<int>(d->FontTex->GetInfo().Width);
		glyphWidths.push_back(gw);
		tw += gw;
		th = std::max<int>(th, static_cast<int>(d->FontTex->GetInfo().Height));
		++glyphs;
	}
	item.glyphCount = std::max<int>(glyphs, 1);   // 桁ごとグラデの実文字数
	tw = std::max<int>(tw, 1);
	// 左右はほぼ余白なし(文字がRT幅ぴったり=桁ごとグラデが数字と揃う)。上下だけ少し余白。
	const int hpad = 2;
	const int vpad = std::max<int>(th / 8, 2);
	const int rtW  = tw + hpad * 2;

	// 各グリフの左右エッジをRT幅で正規化してparamsへ(可変幅=全角/半角混在に対応)。
	// [0]=最初のグリフ左端、[i+1]=i番目グリフの右端。最大31文字(=32エッジ)。
	{
		float* ep = &item.params.GlyphEdges[0].x;   // float4×8=連続32float
		int cursor = hpad;
		ep[0] = static_cast<float>(cursor) / static_cast<float>(rtW);
		int e = 1;
		for (int i = 0; i < static_cast<int>(glyphWidths.size()) && e < 32; ++i, ++e)
		{
			cursor += glyphWidths[i];
			ep[e] = static_cast<float>(cursor) / static_cast<float>(rtW);
		}
		for (; e < 32; ++e) { ep[e] = 1.0f; }   // 余りは右端で埋める(安全)
	}

	// テキストの実寸でRTを作り直す
	item.rt.CreateRenderTarget(rtW, th + vpad * 2);
	item.rt.ClearTexture(Math::Color(0.0f, 0.0f, 0.0f, 1.0f));   // 黒地

	if (!m_textFluidRTChanger.ChangeRenderTarget(item.rt.m_RTTexture, nullptr, &item.rt.m_viewPort))
	{
		m_textFluidRTChanger.UndoRenderTarget();
		return;
	}
	sp.Begin(true);   // リニア。中心原点の正射影が張られる
	const Math::Color white(1.0f, 1.0f, 1.0f, 1.0f);
	// 中央寄せ：中心原点系なので左下を (-幅/2, -高さ/2) に
	sp.DrawFont(fs, Math::Vector2(-tw * 0.5f, -th * 0.5f), &white, 0);
	sp.End();
	m_textFluidRTChanger.UndoRenderTarget();

	item.dirty = false;
}

// 全オブジェクトを各スタイルで加工し、レイヤー順(先頭=奥→末尾=手前)に合成する。
void KdPostProcessShader::DrawFluidText(float dt)
{
	EnsureFluidItems();

	ID3D11DeviceContext* dc = KdDirect3D::Instance().WorkDevContext();
	if (!dc) { return; }
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	if (!bb) { return; }

	KdShaderManager& mgr = KdShaderManager::Instance();
	bool stateSet = false;

	for (auto& sp : m_fluidItems)
	{
		if (!sp || !sp->enabled) { continue; }
		if (sp->dirty) { BakeFluidText(*sp); }
		if (!sp->rt.m_RTTexture) { continue; }   // まだ焼けてない

		sp->params.Time += dt;

		// このオブジェクトのparamsを共用cbufferへ写し、毎フレーム値を上書きしてアップロード
		cbTextFluid& w = m_cb0_TextFluid.Work();
		w = sp->params;
		w.TexelX      = 1.0f / static_cast<float>(bb->GetWidth());
		w.TexelY      = 1.0f / static_cast<float>(bb->GetHeight());
		w.LetterCount = static_cast<float>(sp->glyphCount);
		m_cb0_TextFluid.Write();
		dc->PSSetConstantBuffers(0, 1, m_cb0_TextFluid.GetAddress());

		if (mgr.SetVertexShader(m_VS)) { dc->IASetInputLayout(m_inputLayout); }
		mgr.SetPixelShader(m_PS_TextFluid);

		if (!stateSet)
		{
			mgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);
			mgr.ChangeBlendState(KdBlendState::Alpha);
			mgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
			stateSet = true;
		}

		dc->PSSetShaderResources(0, 1, sp->rt.m_RTTexture->WorkSRViewAddress());
		KdDirect3D::Instance().DrawVertices(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4,
			&m_screenVert[0], sizeof(Vertex));

		ID3D11ShaderResourceView* nullSRV = nullptr;
		dc->PSSetShaderResources(0, 1, &nullSRV);
	}

	if (stateSet)
	{
		mgr.UndoDepthStencilState();
		mgr.UndoBlendState();
		mgr.UndoSamplerState();
	}
}

// 文字エフェクトのライブ調整パネル(文字化け/例外回避のためASCIIラベル＋安全ウィジェット)
//   Unity風に Hierarchy(オブジェクトの追加/削除/レイヤー並び替え) と Inspector(選択物の編集) の2枚に分ける。
void KdPostProcessShader::DrawFluidTextImGui()
{
	EnsureFluidItems();
	const int count = static_cast<int>(m_fluidItems.size());
	if (m_fluidSelected >= count) { m_fluidSelected = count - 1; }
	if (m_fluidSelected < 0)      { m_fluidSelected = 0; }

	// --- Hierarchy：オブジェクト一覧＋追加/削除/レイヤー移動 ---
	ImGui::Begin("Hierarchy");

	if (ImGui::Button("Add"))
	{
		auto add = std::make_shared<FluidTextItem>();
		m_fluidItems.push_back(add);
		m_fluidSelected = static_cast<int>(m_fluidItems.size()) - 1;   // 追加＝末尾＝最前面
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete") && count > 0)
	{
		m_fluidItems.erase(m_fluidItems.begin() + m_fluidSelected);
		if (m_fluidSelected >= static_cast<int>(m_fluidItems.size())) { m_fluidSelected = static_cast<int>(m_fluidItems.size()) - 1; }
	}
	ImGui::SameLine();
	// レイヤー移動：Upで奥へ(描画が早い) / Downで手前へ(描画が遅い＝上に重なる)
	if (ImGui::Button("Layer Up") && m_fluidSelected > 0)
	{
		std::swap(m_fluidItems[m_fluidSelected], m_fluidItems[m_fluidSelected - 1]);
		--m_fluidSelected;
	}
	ImGui::SameLine();
	if (ImGui::Button("Layer Down") && m_fluidSelected >= 0 && m_fluidSelected < static_cast<int>(m_fluidItems.size()) - 1)
	{
		std::swap(m_fluidItems[m_fluidSelected], m_fluidItems[m_fluidSelected + 1]);
		++m_fluidSelected;
	}

	ImGui::Separator();
	ImGui::TextDisabled("top=back / bottom=front");
	// 一覧(先頭=奥→末尾=手前)。ラベルはインデックス＋文字列。IDは##で一意化。
	for (int i = 0; i < static_cast<int>(m_fluidItems.size()); ++i)
	{
		auto& it = m_fluidItems[i];
		char label[96];
		sprintf_s(label, sizeof(label), "%s[%d] %s###fluid%d",
			(it->enabled ? "" : "(off) "), i, it->str.c_str(), i);
		if (ImGui::Selectable(label, m_fluidSelected == i)) { m_fluidSelected = i; }
	}
	ImGui::End();

	// --- Inspector：Hierarchyで選んだオブジェクトのパラメータをここで編集 ---
	ImGui::Begin("Inspector");
	if (m_fluidSelected >= 0 && m_fluidSelected < static_cast<int>(m_fluidItems.size()))
	{
		auto& item = *m_fluidItems[m_fluidSelected];
		cbTextFluid& w = item.params;

		ImGui::Text("Text FX [%d]", m_fluidSelected);
		ImGui::Separator();

		ImGui::Checkbox("Enabled", &item.enabled);

		// テキスト入力。Enterで確定した時だけ反映＝編集中(特にバックスペース)の
		// 途中文字列を焼かない → 削除時のxmemory例外を回避。バッファはオブジェクトごと。
		if (ImGui::InputText("Text (Enter)", item.editBuf, sizeof(item.editBuf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			item.str = item.editBuf;
			item.dirty = true;
		}

		int st = (int)(w.Style + 0.5f);
		if (ImGui::SliderInt("Style 0-7", &st, 0, 7)) { w.Style = (float)st; }
		ImGui::TextUnformatted("0Grad 1Smoke 2Fire 3Dot 4Line 5RGB 6 3D 7Warp");

		ImGui::Separator();
		ImGui::ColorEdit4("Color1", &w.CoreColor.x);
		ImGui::ColorEdit4("Color2", &w.FluidColor.x);

		ImGui::Separator();
		ImGui::DragFloat("Freq/Density", &w.WarpFreq, 0.1f, 0.5f, 80.0f);
		ImGui::DragFloat("FlowSpeed",    &w.FlowSpeed, 0.02f, 0.0f, 5.0f);
		ImGui::DragFloat("Amount",       &w.WarpAmp, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("Intensity",    &w.Intensity, 0.01f, 0.0f, 1.0f);

		// ロングシャドウ(Style 3 ハーフトーン)：長さ/角度/色
		ImGui::Separator();
		ImGui::TextUnformatted("Long Shadow (Dot style)");
		ImGui::DragFloat("ShadowLen",   &w.Dilate, 0.02f, 0.0f, 5.0f);
		ImGui::DragFloat("ShadowAngle", &w.ShadowAngle, 1.0f, 0.0f, 360.0f);
		ImGui::ColorEdit4("ShadowColor", &w.ShadowColor.x);   // rgb=色 / a=不透明度

		ImGui::Separator();
		ImGui::DragFloat("PosX",   &w.RectCX, 0.005f, -0.5f, 1.5f);
		ImGui::DragFloat("PosY",   &w.RectCY, 0.005f, -0.5f, 1.5f);
		ImGui::DragFloat("Width",  &w.RectW, 0.005f, 0.05f, 2.0f);
		ImGui::DragFloat("Height", &w.RectH, 0.005f, 0.05f, 2.0f);
	}
	else
	{
		ImGui::TextUnformatted("Add an object in Hierarchy.");
	}
	ImGui::End();
}

void KdPostProcessShader::DrawDamageFlash()
{
	if (m_damageFlashTimer <= 0.0f) { return; }

	constexpr float kDt = 1.0f / 60.0f;
	m_damageFlashTimer -= kDt / PostProcessConst::DamageFlashDuration;
	m_damageFlashTimer = std::max(m_damageFlashTimer, 0.0f);

	// イーズアウトで強度を計算
	const float t     = m_damageFlashTimer * m_damageFlashTimer;
	const float alpha = t * PostProcessConst::DamageFlashIntensity;

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int w = static_cast<int>(bb->GetWidth());
	const int h = static_cast<int>(bb->GetHeight());
	const Math::Color col = { 1.0f, 0.0f, 0.0f, alpha };
	KdShaderManager::Instance().m_spriteShader.DrawBox(w / 2, h / 2, w / 2, h / 2, &col, true);
}

void KdPostProcessShader::LightBloomProcess()
{
	SetBrightToDevice();

	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);

	// 高輝度抽出
	DrawTexture(&m_postEffectRTPack.m_RTTexture, 1, m_brightEffectRTPack.m_RTTexture, &m_brightEffectRTPack.m_viewPort);

	KdShaderManager::Instance().UndoBlendState();

	// LightBloom画像の作成
	SetBlurToDevice();

	std::shared_ptr<KdTexture> srcRTTex = m_brightEffectRTPack.m_RTTexture;

	for (int i = 0; i < kLightBloomNum; ++i)
	{
		GenerateBlurTexture(srcRTTex, m_lightBloomRTPack[i].m_RTTexture, m_lightBloomRTPack[i].m_viewPort, kBlurSamplingRadius);
			
		srcRTTex = m_lightBloomRTPack[i].m_RTTexture;
	}

	KdRenderTargetChanger RTChanger;
	RTChanger.ChangeRenderTarget(m_postEffectRTPack);

	KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Linear_Clamp);

	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);

	// 光源ぼかし画像の合成
	for (int i = 0; i < kLightBloomNum; ++i)
	{
		KdShaderManager::Instance().m_spriteShader.DrawTex(m_lightBloomRTPack[i].m_RTTexture.get(), 0, 0, m_postEffectRTPack.m_RTTexture->GetWidth(), m_postEffectRTPack.m_RTTexture->GetHeight());
	}

	RTChanger.UndoRenderTarget();

	KdShaderManager::Instance().UndoBlendState();

	KdShaderManager::Instance().UndoSamplerState();
}

void KdPostProcessShader::BlurProcess()
{
	SetBlurToDevice();

	GenerateBlurTexture(m_postEffectRTPack.m_RTTexture, m_blurRTPack.m_RTTexture, m_blurRTPack.m_viewPort, kBlurSamplingRadius);

	GenerateBlurTexture(m_blurRTPack.m_RTTexture, m_strongBlurRTPack.m_RTTexture, m_strongBlurRTPack.m_viewPort, kBlurSamplingRadius);
}

void KdPostProcessShader::DepthOfFieldProcess()
{
	SetDoFToDevice();

	std::shared_ptr<KdTexture> srcTexList[4] =
	{
		m_postEffectRTPack.m_RTTexture,
		m_blurRTPack.m_RTTexture,
		m_strongBlurRTPack.m_RTTexture,
		m_postEffectRTPack.m_ZBuffer
	};

	DrawTexture(srcTexList, 4, m_depthOfFieldRTPack.m_RTTexture, &m_depthOfFieldRTPack.m_viewPort);
}

void KdPostProcessShader::CreateBlurOffsetList(std::vector<Math::Vector3>& dstInfo, const std::shared_ptr<KdTexture>& spSrcTex, int samplingRadius, const Math::Vector2& dir)
{
	Math::Vector2 blurDir = dir;
	blurDir.Normalize();

	// 両サイドのサンプリング回数 ＋ サンプル開始中央のピクセル
	int totalSamplingNum = samplingRadius * 2 + 1;

	// サンプリングするテクセルのオフセット値
	Math::Vector2 texelSize;
	texelSize.x = 1.0f / spSrcTex->GetWidth();
	texelSize.y = 1.0f / spSrcTex->GetHeight();

	dstInfo.resize(totalSamplingNum);

	float totalWeight = 0;
	for (int i = 0; i < totalSamplingNum; ++i)
	{
		int samplingOffset = i - samplingRadius;
		dstInfo[i].x = blurDir.x * (samplingOffset * texelSize.x);
		dstInfo[i].y = blurDir.y * (samplingOffset * texelSize.y);

		// 中心のピクセルのウェイトが大きくなる計算
		float weight = exp(-(samplingOffset * samplingOffset) / 18.0f);

		// サンプリングする各ピクセルに重みをつける
		dstInfo[i].z = weight;
		totalWeight += weight;
	}

	// ウェイトを全体のウェイトから割り算し、各ピクセルのウェイトの意味を割合に置き換える
	// 全部足して1になるように数値を調整する
	for (int i = 0; i < totalSamplingNum; ++i)
	{
		dstInfo[i].z /= totalWeight;
	}
}

void KdPostProcessShader::GenerateMotionBlurTexture(
	std::shared_ptr<KdTexture>& spSrcTex,
	std::shared_ptr<KdTexture>& spDstTex,
	D3D11_VIEWPORT& VP,
	int blurRadius,
	const Math::Vector2& dir)
{
	SetBlurToDevice();
	KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Linear_Clamp);

	// UV空間の方向をそのままオフセットとして使う1パスの方向ブラー
	std::vector<Math::Vector3> blurInfo;
	const int totalSamples = blurRadius * 2 + 1;
	blurInfo.resize(totalSamples);

	float totalWeight = 0.0f;
	for (int i = 0; i < totalSamples; ++i)
	{
		const int offset = i - blurRadius;
		const float t = static_cast<float>(offset) / static_cast<float>(blurRadius);
		blurInfo[i].x = dir.x * t;
		blurInfo[i].y = dir.y * t;
		const float weight = expf(-(t * t) / 0.5f);
		blurInfo[i].z = weight;
		totalWeight += weight;
	}
	for (int i = 0; i < totalSamples; ++i)
	{
		blurInfo[i].z /= totalWeight;
	}

	SetBlurInfo(blurInfo);
	DrawTexture(&spSrcTex, 1, spDstTex, &VP);

	KdShaderManager::Instance().UndoSamplerState();
}

void KdPostProcessShader::GenerateBlurTexture(std::shared_ptr<KdTexture>& spSrcTex, std::shared_ptr<KdTexture>& spDstTex, D3D11_VIEWPORT& VP, int blurRadius)
{
	// ブラー用シェーダ（VS/入力レイアウト/PS）をセット。
	// 外部から単独で呼んでも動くよう自己完結させる（内部の二重呼び出しは無害）。
	SetBlurToDevice();

	KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Linear_Clamp);

	KdRenderTargetPack tmpBlurRTPack;
	tmpBlurRTPack.CreateRenderTarget(spDstTex->GetWidth(), spDstTex->GetHeight());

	// 横にぼかす
	std::vector<Math::Vector3> horizontalBlurInfo;
	CreateBlurOffsetList(horizontalBlurInfo, spDstTex, blurRadius, { 1.0f, 0 });
	SetBlurInfo(horizontalBlurInfo);

	DrawTexture(&spSrcTex, 1, tmpBlurRTPack.m_RTTexture, &tmpBlurRTPack.m_viewPort);

	// 横にぼかした画像を更に縦にぼかす
	std::vector<Math::Vector3> verticalBlurInfo;
	CreateBlurOffsetList(verticalBlurInfo, spDstTex, blurRadius, { 0, 1.0f });
	SetBlurInfo(verticalBlurInfo);

	DrawTexture(&tmpBlurRTPack.m_RTTexture, 1, spDstTex, &VP);

	KdShaderManager::Instance().UndoSamplerState();
}

void KdPostProcessShader::DrawTexture(std::shared_ptr<KdTexture>* spSrcTex, int srcTexSize, std::shared_ptr<KdTexture> spDstTex, D3D11_VIEWPORT* pVP)
{
	if (!spSrcTex) { return; }

	KdRenderTargetChanger RTChanger;

	if (spDstTex)
	{
		RTChanger.ChangeRenderTarget(spDstTex, nullptr, pVP);
	}

	ID3D11DeviceContext* pDevCon = KdDirect3D::Instance().WorkDevContext();

	// SRVのセット
	for (int i = 0; i < srcTexSize; ++i)
	{
		pDevCon->PSSetShaderResources(i, 1, spSrcTex[i]->WorkSRViewAddress());
	}

	// テクスチャーの描画
	KdDirect3D::Instance().DrawVertices(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4, &m_screenVert[0], sizeof(Vertex));

	// SRVの解放
	ID3D11ShaderResourceView* nullSRV = nullptr;

	for (int i = 0; i < srcTexSize; ++i)
	{
		pDevCon->PSSetShaderResources(i, 1, &nullSRV);
	}

	RTChanger.UndoRenderTarget();
}

void KdPostProcessShader::SetBlurInfo(const std::shared_ptr<KdTexture>& spSrcTex, int samplingRadius, const Math::Vector2& dir)
{
	std::vector<Math::Vector3> blurOffsetList;

	CreateBlurOffsetList(blurOffsetList, spSrcTex, samplingRadius, dir);

	SetBlurInfo(blurOffsetList);
}

void KdPostProcessShader::SetBlurInfo(const std::vector<Math::Vector3>& srcInfo)
{
	KdPostProcessShader::cbBlur& blurInfo = m_cb0_BlurInfo.Work();

	blurInfo.SamplingNum = (signed)srcInfo.size();

	if (blurInfo.SamplingNum > kMaxSampling)
	{
		assert(0 && "サンプリング指定回数が上限を超えています。");

		blurInfo.SamplingNum = 0;

		return;
	}

	for (int i = 0; i < blurInfo.SamplingNum; ++i)
	{
		blurInfo.Info[i].x = srcInfo[i].x;
		blurInfo.Info[i].y = srcInfo[i].y;
		blurInfo.Info[i].z = srcInfo[i].z;
	}

	m_cb0_BlurInfo.Write();
}

void KdPostProcessShader::SetBlurToDevice()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	m_cb0_BlurInfo.Write();

	KdDirect3D::Instance().WorkDevContext()->PSSetConstantBuffers(0, 1, m_cb0_BlurInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}

	shaderMgr.SetPixelShader(m_PS_Blur);
}

void KdPostProcessShader::SetDoFToDevice()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	m_cb0_DoFInfo.Write();

	KdDirect3D::Instance().WorkDevContext()->PSSetConstantBuffers(0, 1, m_cb0_DoFInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}

	shaderMgr.SetPixelShader(m_PS_DoF);
}

void KdPostProcessShader::SetBrightToDevice()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	m_cb0_BrightInfo.Write();

	KdDirect3D::Instance().WorkDevContext()->PSSetConstantBuffers(0, 1, m_cb0_BrightInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}

	shaderMgr.SetPixelShader(m_PS_Bright);
}

void KdPostProcessShader::SetOutlineToDevice()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	// 画面のテクセルサイズを反映
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	m_cb0_OutlineInfo.Work().TexelX = 1.0f / static_cast<float>(bb->GetWidth());
	m_cb0_OutlineInfo.Work().TexelY = 1.0f / static_cast<float>(bb->GetHeight());
	m_cb0_OutlineInfo.Write();
	DevCon->PSSetConstantBuffers(0, 1, m_cb0_OutlineInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}
	shaderMgr.SetPixelShader(m_PS_Outline);
}

// 画面エッジ検出でアウトラインを乗せ、m_outlineRTPack へ出力する
void KdPostProcessShader::OutlineProcess(const std::shared_ptr<KdTexture>& srcColor)
{
	SetOutlineToDevice();
	KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Linear_Clamp);

	// t0=シーン色、t1=深度
	std::shared_ptr<KdTexture> srcList[2] = { srcColor, m_postEffectRTPack.m_ZBuffer };
	DrawTexture(srcList, 2, m_outlineRTPack.m_RTTexture, &m_outlineRTPack.m_viewPort);

	KdShaderManager::Instance().UndoSamplerState();
}
