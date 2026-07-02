#pragma once

class KdPostProcessShader
{
public:
	KdPostProcessShader() {}
	~KdPostProcessShader()
	{
		Release();
	}

	void SetNearClippingDistance(float distance) { m_cb0_DoFInfo.Work().NearClippingDistance = distance; }
	void SetFarClippingDistance(float distance) { m_cb0_DoFInfo.Work().FarClippingDistance = distance; }
	void SetFocusDistance(float distance) { m_cb0_DoFInfo.Work().FocusDistance = distance; }
	void SetFocusRange(float fore, float back) { m_cb0_DoFInfo.Work().FocusForeRange = fore; m_cb0_DoFInfo.Work().FocusBackRange = back; }

	void SetBrightThreshold(float threshold) { m_cb0_BrightInfo.Work().Threshold = threshold; }

	struct Vertex
	{
		Math::Vector3 Pos;
		Math::Vector2 UV;
	};

	bool Init();

	void Release();

	void Draw();

	// シーン描画先のRT（SRV付き）。エディタのゲームビューポート表示に使う
	const std::shared_ptr<KdTexture>& GetSceneRT() const { return m_postEffectRTPack.m_RTTexture; }

	void BeginBright();
	void EndBright();

	void PostEffectProcess();

	// 不透明シーン描画の直後・エフェクト描画の前に呼ぶ：画面エッジ検出のアウトラインを
	// 不透明シーンにだけ適用する（この後に描くエフェクトには線が乗らない）。
	void ApplySceneOutline();

	// 画面エッジ検出アウトライン(トゥーン輪郭)の ON/OFF と調整用アクセサ
	void SetSceneOutlineEnabled(bool enable) { m_sceneOutlineEnabled = enable; }
	bool IsSceneOutlineEnabled() const { return m_sceneOutlineEnabled; }
	float& WorkOutlineThickness()       { return m_cb0_OutlineInfo.Work().Thickness; }
	float& WorkOutlineDepthThreshold()  { return m_cb0_OutlineInfo.Work().DepthThreshold; }
	float& WorkOutlineNormalThreshold() { return m_cb0_OutlineInfo.Work().NormalThreshold; }
	float& WorkOutlineEdgeStrength()    { return m_cb0_OutlineInfo.Work().EdgeStrength; }
	Math::Vector4& WorkOutlineColor()   { return m_cb0_OutlineInfo.Work().Color; }

	// モーションブラー用：毎フレームカメラ位置を渡す
	void SetCameraPositionForMotionBlur(const Math::Vector3& pos) { m_currentCamPos = pos; m_camPosSet = true; }

	// モーションブラーの ON/OFF（既定ON）
	void SetMotionBlurEnabled(bool enable) { m_motionBlurEnabled = enable; }

	// 被ダメ赤フラッシュをトリガー
	void TriggerDamageFlash() { m_damageFlashTimer = 1.0f; }

	// DrawSprite内から呼ぶ：赤フラッシュビネットを描画（Begin〜End内で呼ぶこと）
	void DrawDamageFlash();

	void GenerateBlurTexture(std::shared_ptr<KdTexture>& spSrcTex, std::shared_ptr<KdTexture>& spDstTex, D3D11_VIEWPORT& VP, int blurRadius);

private:
	void GenerateMotionBlurTexture(std::shared_ptr<KdTexture>& spSrcTex, std::shared_ptr<KdTexture>& spDstTex, D3D11_VIEWPORT& VP, int blurRadius, const Math::Vector2& dir);

	void BlurProcess();
	void LightBloomProcess();
	void DepthOfFieldProcess();

	void CreateBlurOffsetList(std::vector<Math::Vector3>& dstInfo, const std::shared_ptr<KdTexture>& spSrcTex, int samplingSize, const Math::Vector2& dir);

	void DrawTexture(std::shared_ptr<KdTexture>* spSrcTex, int srcTexSize, std::shared_ptr<KdTexture> spDstTex, D3D11_VIEWPORT* pVP);

	void SetBlurInfo(const std::shared_ptr<KdTexture>& spSrcTex, int samplingSize, const Math::Vector2& dir);
	void SetBlurInfo(const std::vector<Math::Vector3>& srcInfo);

	void SetBlurToDevice();
	void SetDoFToDevice();
	void SetBrightToDevice();
	void SetOutlineToDevice();
	// アウトライン（画面エッジ検出）：srcColor を入力に、輪郭を乗せて m_outlineRTPack へ出力
	void OutlineProcess(const std::shared_ptr<KdTexture>& srcColor);

	ID3D11VertexShader* m_VS = nullptr;
	ID3D11InputLayout* m_inputLayout = nullptr;

	ID3D11PixelShader* m_PS_Blur = nullptr;
	ID3D11PixelShader* m_PS_DoF = nullptr;
	ID3D11PixelShader* m_PS_Bright = nullptr;
	ID3D11PixelShader* m_PS_Outline = nullptr;

	static const int kBlurSamplingRadius = 8;
	static const int kLightBloomSamplingRadius = 4;

	static const int kMaxSampling = 31;
	struct cbBlur
	{
		Math::Vector4 Info[kMaxSampling];
	
		int SamplingNum = 0;
		int _blank[3] = { 0, 0 ,0 };
	};
	KdConstantBuffer<cbBlur>	m_cb0_BlurInfo;

	struct cbDepthOfField
	{
		float NearClippingDistance = 0.0f;
		float FarClippingDistance = 1000.0f;

		float FocusDistance = 0.0f;
		float FocusForeRange = 0.0f;
		float FocusBackRange = 1000.0f;
		int   _blank[3] = { 0, 0, 0 };
	};
	KdConstantBuffer<cbDepthOfField>	m_cb0_DoFInfo;

	struct cbBrightFilter
	{
		float Threshold = 0.0f;
		int _blank[3] = { 0, 0, 0 };
	};
	KdConstantBuffer<cbBrightFilter>	m_cb0_BrightInfo;

	// アウトライン（画面エッジ検出）パラメータ
	struct cbOutlineInfo
	{
		float TexelX = 0.0f;
		float TexelY = 0.0f;
		float Thickness       = 1.6f;   // 線の太さ（テクセル）
		float DepthThreshold  = 0.35f;  // 深度エッジ（シルエット）しきい値

		float NormalThreshold = 0.2f;   // 法線エッジ（角）しきい値
		float EdgeStrength    = 1.0f;   // 線の濃さ
		float _opad[2] = { 0.0f, 0.0f };

		Math::Vector4 Color = { 0.0f, 0.0f, 0.0f, 1.0f };  // 線の色
	};
	KdConstantBuffer<cbOutlineInfo>		m_cb0_OutlineInfo;

	KdRenderTargetPack	m_postEffectRTPack;

	KdRenderTargetPack	m_blurRTPack;
	KdRenderTargetPack	m_strongBlurRTPack;
	KdRenderTargetPack	m_motionBlurRTPack;   // モーションブラー合成用

	// 前フレームのカメラワールド座標（モーションブラー用）
	Math::Vector3 m_prevCamPos     = { 0.0f, 0.0f, 0.0f };
	Math::Vector3 m_currentCamPos  = { 0.0f, 0.0f, 0.0f };
	bool          m_prevCamPosValid = false;
	bool          m_motionBlurEnabled = true;   // モーションブラーON/OFF
	bool          m_camPosSet       = false;
	bool          m_sceneOutlineEnabled = true; // 画面エッジ検出アウトライン(トゥーン輪郭)ON/OFF

	// 被ダメ赤フラッシュ（0=消灯 〜 1=最大）
	float         m_damageFlashTimer = 0.0f;

	KdRenderTargetPack	m_depthOfFieldRTPack;
	KdRenderTargetPack	m_outlineRTPack;   // アウトライン合成結果

	KdRenderTargetPack	m_brightEffectRTPack;
	static const int	kLightBloomNum = 4;
	KdRenderTargetPack	m_lightBloomRTPack[kLightBloomNum];

	KdRenderTargetChanger m_postEffectRTChanger;
	KdRenderTargetChanger m_brightRTChanger;

	Vertex m_screenVert[4];
};
