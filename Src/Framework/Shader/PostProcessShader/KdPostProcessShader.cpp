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

	m_cb0_BlurInfo.Create();

	m_cb0_DoFInfo.Create();

	m_cb0_BrightInfo.Create();

	m_cb0_OutlineInfo.Create();

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

	m_cb0_BlurInfo.Release();
	m_cb0_DoFInfo.Release();
	m_cb0_BrightInfo.Release();
	m_cb0_OutlineInfo.Release();
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
