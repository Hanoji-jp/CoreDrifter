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

	// 煙のシルエット輪郭：エフェクト(煙)を専用RTに描き、塊全体のシルエット外周にだけ
	// 輪郭線を乗せてシーンへ合成する。使い方：
	//   BeginSmoke() → (UnLitで煙をDraw) → EndSmokeAndComposite()
	void BeginSmoke();
	void EndSmokeAndComposite();
	// 煙シルエット輪郭の ON/OFF と調整用アクセサ
	void SetSmokeOutlineEnabled(bool enable) { m_smokeOutlineEnabled = enable; }
	bool IsSmokeOutlineEnabled() const { return m_smokeOutlineEnabled; }
	float& WorkSmokeOutlineThickness()      { return m_cb0_SmokeOutline.Work().Thickness; }
	float& WorkSmokeOutlineAlphaThreshold() { return m_cb0_SmokeOutline.Work().AlphaThreshold; }
	float& WorkSmokeOutlineEdgeStrength()   { return m_cb0_SmokeOutline.Work().EdgeStrength; }
	Math::Vector4& WorkSmokeOutlineColor()  { return m_cb0_SmokeOutline.Work().Color; }

	// 文字流体化(ペルソナ/ドリフト演出)：文字を焼いたRTを入力にドメインワープで
	// 輪郭を流体のようにウネらせて画面へ合成する。スプライトEndの後(バックバッファ)に呼ぶ。
	//   SetFluidText()で選択中オブジェクトの内容を差し替え → 毎フレーム DrawFluidText(dt) を呼ぶだけ。
	void SetFluidText(const char* str);            // 選択中オブジェクトの文字を差し替え
	void DrawFluidText(float dt);                  // 全オブジェクトをレイヤー順に描画
	void DrawFluidTextImGui();                     // Hierarchy/Inspectorパネル(ImGui)
	void SetFluidTextIntensity(float v);           // 選択中オブジェクトの効果強さ(0-1)
	int  CycleFluidStyle();                        // 選択中オブジェクトのスタイルを次へ回し番号を返す

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
	// 煙シルエット輪郭のPS＋定数バッファをデバイスへセット
	void SetSmokeOutlineToDevice();

	ID3D11VertexShader* m_VS = nullptr;
	ID3D11InputLayout* m_inputLayout = nullptr;

	ID3D11PixelShader* m_PS_Blur = nullptr;
	ID3D11PixelShader* m_PS_DoF = nullptr;
	ID3D11PixelShader* m_PS_Bright = nullptr;
	ID3D11PixelShader* m_PS_Outline = nullptr;
	ID3D11PixelShader* m_PS_SmokeOutline = nullptr;   // 煙シルエット輪郭
	ID3D11PixelShader* m_PS_TextFluid    = nullptr;   // 文字流体化

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

	// 煙シルエット輪郭パラメータ
	struct cbSmokeOutline
	{
		float TexelX = 0.0f;
		float TexelY = 0.0f;
		float Thickness      = 2.0f;   // 線の太さ（テクセル）
		float AlphaThreshold = 0.25f;  // シルエット判定のアルファしきい値

		float EdgeStrength   = 1.0f;   // 線の濃さ
		float BlurRadius     = 2.0f;   // 煙のぼかし半径（テクセル。0で無効）
		float _soPad[2]      = { 0.0f, 0.0f };

		Math::Vector4 Color = { 0.0f, 0.0f, 0.0f, 1.0f };  // 線の色（黒）
	};
	KdConstantBuffer<cbSmokeOutline>	m_cb0_SmokeOutline;

	// 文字流体化パラメータ(既定はグラフィックデザイン風の暖色2トーン)
	struct cbTextFluid
	{
		float TexelX = 0.0f;
		float TexelY = 0.0f;
		float Time      = 0.0f;
		float WarpAmp   = 0.3f;    // 中の模様のコントラスト(0=均一/大=くっきり)

		float WarpFreq  = 4.0f;    // 中の模様の細かさ(スケール)
		float FlowSpeed = 0.4f;    // 中の模様が流れる速さ
		float Dilate    = 0.05f;   // ロングシャドウの長さ(短め=文字際の細い帯。トレイル系兼用)
		float Intensity = 1.0f;    // 0-1 効果の強さ(全体不透明度など)

		// 主色(上)/副色(下)。スタイルにより意味が変わる。
		Math::Vector4 CoreColor  = { 1.0f, 0.95f, 0.55f, 1.0f };
		Math::Vector4 FluidColor = { 1.0f, 0.45f, 0.10f, 1.0f };

		float Style = 3.0f;      // 0=グラデ 1=虹スモーク 2=炎 3=ドット 4=縞 5=色収差 6=3D 7=ワープ
		float LetterCount = 5.0f; // 文字数(ハーフトーンの桁ごとグラデ用)
		float RectCX = 0.5f;      // 表示位置(中心X, 画面比)
		float RectCY = 0.745f;    // 表示位置(中心Y)

		float RectW = 0.76f;      // 表示幅(画面比)
		float RectH = 0.49f;      // 表示高さ
		float ShadowAngle = 45.0f; // ロングシャドウの角度(度。0=右, 45=右下)
		float ShadowLevel = 0.28f; // (予備)

		// ロングシャドウの色(rgb=色, a=不透明度)
		Math::Vector4 ShadowColor = { 0.20f, 0.18f, 0.10f, 1.0f };

		// 各グリフの左右エッジ(RT幅で正規化)。可変幅(全角/半角混在)の桁ごとグラデ用。
		// [i]=グリフ左端 / [i+1]=右端。最大31文字(=32エッジ)ぶんをfloat4×8で保持。
		Math::Vector4 GlyphEdges[8] = {};
	};
	KdConstantBuffer<cbTextFluid>	m_cb0_TextFluid;   // GPUアップロード共用(各オブジェクトのparamsを都度コピー)
	static const int kFluidStyleCount = 8;

	// 文字エフェクトの1オブジェクト。複数を追加/削除/レイヤー並び替えできる。
	struct FluidTextItem
	{
		cbTextFluid        params;              // このオブジェクトのシェーダーパラメータ(色/位置/影など)
		KdRenderTargetPack rt;                  // 文字を焼いたRT
		std::string        str      = "12340";  // 表示文字列
		char               editBuf[64] = "12340"; // ImGui入力用(選択物ごとに保持)
		int                glyphCount = 1;      // 実グリフ数(桁ごとグラデ用)
		bool               dirty    = true;     // テキスト変更で再焼き
		bool               enabled  = true;     // 表示ON/OFF
	};

	// 煙ぼかし半径の調整用アクセサ
public:
	float& WorkSmokeBlurRadius() { return m_cb0_SmokeOutline.Work().BlurRadius; }
private:

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
	bool          m_smokeOutlineEnabled = true; // 煙シルエット輪郭 ON/OFF

	// 被ダメ赤フラッシュ（0=消灯 〜 1=最大）
	float         m_damageFlashTimer = 0.0f;

	KdRenderTargetPack	m_depthOfFieldRTPack;
	KdRenderTargetPack	m_outlineRTPack;   // アウトライン合成結果
	KdRenderTargetPack	m_smokeRTPack;     // 煙専用の描画先(色+アルファ。シルエット輪郭用)
	KdRenderTargetPack	m_smokeBlurRTPack; // 煙をガウスぼかしした版(縁を柔らかく＋ポップ目立たなく)
	KdRenderTargetChanger m_smokeRTChanger;

	// 文字エフェクト：複数オブジェクトをリストで持つ(追加/削除/レイヤー並び替え)。
	// DrawFontで文字をRTへ焼き(テキスト変更時のみ実寸で作り直す)、レイヤー順に合成する。
	KdRenderTargetChanger                       m_textFluidRTChanger;   // 焼き込み共用
	std::vector<std::shared_ptr<FluidTextItem>> m_fluidItems;           // 描画順=レイヤー(先頭=奥)
	int                                         m_fluidSelected = 0;    // Inspector対象
	void EnsureFluidItems();               // 空なら既定オブジェクトを1つ用意
	void BakeFluidText(FluidTextItem& item);  // item.str をitem.rtへDrawFontで焼く

	KdRenderTargetPack	m_brightEffectRTPack;
	static const int	kLightBloomNum = 4;
	KdRenderTargetPack	m_lightBloomRTPack[kLightBloomNum];

	KdRenderTargetChanger m_postEffectRTChanger;
	KdRenderTargetChanger m_brightRTChanger;

	Vertex m_screenVert[4];
};
