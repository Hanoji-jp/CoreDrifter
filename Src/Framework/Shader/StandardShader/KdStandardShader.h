#pragma once
//============================================================
//
// 基本シェーダー
//
//============================================================
// KdShaderManager.h より先に include されるため前方宣言
enum class KdDepthStencilState;
enum class KdRasterizerState;

class KdStandardShader
{
public:
	// スキンメッシュ対応
	static const int maxBoneBufferSize = 300;

	// 定数バッファ(オブジェクト単位更新)
	struct cbObject
	{
		// UV操作
		Math::Vector2	UVOffset = { 0.0f, 0.0f };
		Math::Vector2	UVTiling = { 1.0f, 1.0f };

		// フォグ有効
		int				FogEnable = 1;

		// エミッシブのみの描画
		int				OnlyEmissie = 0;

		// スキンメッシュオブジェクトかどうか(スキンメッシュ対応)
		int				IsSkinMeshObj = 0;

		// ディゾルブ関連
		float			DissolveThreshold = 0.0f;	// 0 ～ 1
		float			DissolveEdgeRange = 0.03f;	// 0 ～ 1

		Math::Vector3	DissolveEmissive = { 0.0f, 1.0f, 1.0f };

		// トリプレーナーUV
		int				UseTriplanar   = 0;		// 有効フラグ
		float			TriplanarScale = 0.1f;	// ワールド座標スケール
		int				SphereNormal   = 0;		// 球法線を中心方向で解析計算するフラグ
		float			_tripad        = 0.0f;	// パディング

		// 芝生ブレンド
		int				UseGrass            = 0;    // 芝生ブレンド有効フラグ
		float			GrassBlendSharpness = 4.0f; // ブレンドの鋭さ
		float			GrassTriplanarScale = 0.1f; // 芝テクスチャスケール
		int				UseGrassNormal      = 0;    // 草法線マップ有効フラグ（盛り上がり）
		Math::Vector3	GravityUpDir        = { 0.0f, 1.0f, 0.0f }; // 重力の上方向
		float			_grasspad2          = 0.0f;

		// エッジ（境目）テクスチャ
		int				UseGrassEdge      = 0;    // エッジブレンド有効フラグ
		float			GrassEdgeWidth    = 0.3f; // エッジ帯域の幅（upDot 空間）
		float			GrassEdgeTexScale = 0.15f;// エッジテクスチャのスケール
		float			FullEdgeStrength  = 0.0f; // 全面エッジブレンド強度（0=無効 1=フル）
	};

	// 定数バッファ(メッシュ単位更新)
	struct cbMesh
	{
		Math::Matrix	mW;
	};

	// 定数バッファ(マテリアル単位更新)
	struct cbMaterial
	{
		Math::Vector4	BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		Math::Vector3	Emissive = { 1.0f, 1.0f, 1.0f };
		float			Metallic = 0.0f;

		float			Roughness = 1.0f;
		float			_blank[3] = { 0.0f, 0.0f ,0.0f };
	};

	// 定数バッファ(ボーン単位更新：スキンメッシュ対応)
	struct cbBone {
		Math::Matrix mBones[300];
	};

	// 定数バッファ(アウトライン：原神式背面押し出し)
	struct cbOutline {
		float			Width = 0.04f;    // ワールド単位の押し出し幅（遠いほど画面上は細くなる）
		// 深度を少しだけ奥へずらす量(NDC)。NDCバイアスは遠距離で効きすぎる（遠くで
		// 輪郭が消える）ため既定は0。継ぎ目対策はポスト処理側で行う。
		float			DepthBias = 0.0f;
		float			_opad[2] = { 0.0f, 0.0f };
	};

	//================================================
	// 設定・取得
	//================================================

	// トリプレーナーUV 設定（scale: ワールド座標に掛ける値。小さいほど細かくタイリング）
	void SetTriplanarUV(bool enable, float scale = 0.1f)
	{
		auto& cb = m_cb0_Obj.Work();
		cb.UseTriplanar   = enable ? 1 : 0;
		cb.TriplanarScale = scale;
		m_dirtyCBObj = true;
	}

	// 芝生テクスチャを設定（t4スロットに即セット）
	void SetGrassTexture(std::shared_ptr<KdTexture> _tex)
	{
		m_grassTex = _tex;
		ID3D11ShaderResourceView* srv = m_grassTex
			? m_grassTex->WorkSRView()
			: KdDirect3D::Instance().GetWhiteTex()->WorkSRView();
		KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(4, 1, &srv);
	}

	// 草法線マップを設定（t5スロットに即セット）
	void SetGrassNormalTexture(std::shared_ptr<KdTexture> _tex)
	{
		m_grassNormalTex = _tex;
		ID3D11ShaderResourceView* srv = m_grassNormalTex
			? m_grassNormalTex->WorkSRView()
			: KdDirect3D::Instance().GetNormalTex()->WorkSRView();
		KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(5, 1, &srv);
		m_cb0_Obj.Work().UseGrassNormal = (_tex != nullptr) ? 1 : 0;
		m_dirtyCBObj = true;
	}

	// エッジ（境目）テクスチャを設定（t6スロットに即セット）
	// width : エッジ帯域の幅（upDot 空間, 0.1〜0.5 推奨）
	// texScale : トリプレーナースケール
	void SetGrassEdgeTexture(std::shared_ptr<KdTexture> _tex,
							 float width    = 0.3f,
							 float texScale = 0.15f)
	{
		m_grassEdgeTex = _tex;
		ID3D11ShaderResourceView* srv = m_grassEdgeTex
			? m_grassEdgeTex->WorkSRView()
			: KdDirect3D::Instance().GetWhiteTex()->WorkSRView();
		KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(6, 1, &srv);
		auto& cb = m_cb0_Obj.Work();
		cb.UseGrassEdge      = (_tex != nullptr) ? 1 : 0;
		cb.GrassEdgeWidth    = width;
		cb.GrassEdgeTexScale = texScale;
		m_dirtyCBObj = true;
	}
	// 全面エッジブレンド強度を設定（0=無効 1=フル）
	void SetFullEdgeStrength(float strength)
	{
		m_cb0_Obj.Work().FullEdgeStrength = strength;
		m_dirtyCBObj = true;
	}

	// upDir   : 重力の「上」方向（この方向を向いている面に芝が生える）
	// sharpness: ブレンド境界の鋭さ（2〜8推奨）
	// grassScale: 芝テクスチャのトリプレーナースケール
	void SetGrassBlend(bool enable,
					   const Math::Vector3& upDir       = { 0.0f, 1.0f, 0.0f },
					   float sharpness                  = 4.0f,
					   float grassScale                 = 0.15f)
	{
		auto& cb = m_cb0_Obj.Work();
		cb.UseGrass            = enable ? 1 : 0;
		cb.GravityUpDir        = upDir;
		cb.GrassBlendSharpness = sharpness;
		cb.GrassTriplanarScale = grassScale;
		m_dirtyCBObj = true;
	}

	// 球法線解析計算 設定（ローポリ球のシェーディング段差を消す）
	void SetSphereNormal(bool enable)
	{
		m_cb0_Obj.Work().SphereNormal = enable ? 1 : 0;
		m_dirtyCBObj = true;
	}

	// UVタイリング設定
	void SetUVTiling(const Math::Vector2& tiling)
	{
		m_cb0_Obj.Work().UVTiling = tiling;

		m_dirtyCBObj = true;
	}

	// UVオフセット設定
	void SetUVOffset(const Math::Vector2& offset)
	{
		m_cb0_Obj.Work().UVOffset = offset;

		m_dirtyCBObj = true;
	}

	// フォグ有効/無効
	void SetFogEnable(bool enable)
	{
		m_cb0_Obj.Work().FogEnable = enable;

		m_dirtyCBObj = true;
	}

	// ディゾルブ設定
	void SetDissolve(float threshold, const float* range = nullptr, const Math::Vector3* edgeColor = nullptr)
	{
		auto& cbObj = m_cb0_Obj.Work();

		cbObj.DissolveThreshold = threshold;

		if (range)
		{
			cbObj.DissolveEdgeRange = *range;
		}

		if (edgeColor)
		{
			cbObj.DissolveEmissive = *edgeColor;
		}

		m_dirtyCBObj = true;
	}

	// ディゾルブテクスチャ設定
	void SetDissolveTexture(KdTexture& dissolveMask)
	{
		KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(11, 1, dissolveMask.WorkSRViewAddress());
	}

	// デフォルトディゾルブテクスチャ設定
	void SetDefaultDissolveTexture(std::shared_ptr<KdTexture>& spDissolveMask)
	{
		if (!spDissolveMask) { return; }

		m_dissolveTex = spDissolveMask;

		SetDissolveTexture(*spDissolveMask);
	}

	// デフォルトのディゾルブテクスチャに戻す
	void ResetDissolveTexture()
	{
		if (!m_dissolveTex) { return; }

		SetDissolveTexture(*m_dissolveTex);
	}

	//================================================
	// 各定数バッファの取得
	//================================================
	const cbObject& GetObjectCB() const { return m_cb0_Obj.Get(); }

	const cbMesh& MeshCB() const { return m_cb1_Mesh.Get(); }

	const cbMaterial& WorkMaterialCB() const { return m_cb2_Material.Get(); }

	// スキンメッシュ対応
	const cbBone& WorkBoneCB() const { return m_cb3_Bone.Get(); }

	//================================================
	// 描画準備
	//================================================
	// 陰影をつけるオブジェクト等を描画する前後に行う
	void BeginLit();
	void EndLit();

	// 陰影をつけないオブジェクト等を描画する前後に行う
	void BeginUnLit();
	void EndUnLit();

	// 最も初めに行う、光を遮るオブジェクトを描画する前後に行う
	void BeginGenerateDepthMapFromLight();
	void EndGenerateDepthMapFromLight();

	// アウトライン（原神式：法線方向に背面を押し出して単色描画）を描く前後に行う。
	// この間に DrawModel() でモデルを描くと輪郭線になる。colRate に暗めの色を渡すと
	// その色（=ベース色×colRate）で輪郭が出る。
	void BeginOutline();
	void EndOutline();
	// 輪郭の太さ（画面上のおおよその割合。FOVにほぼ依存しない）
	void SetOutlineWidth(float width)
	{
		m_cb_Outline.Work().Width = width;
		m_cb_Outline.Write();
	}

	//================================================
	// 描画関数
	//================================================
	// メッシュ描画
	void DrawMesh(const KdMesh* mesh, const Math::Matrix& mWorld, const std::vector<KdMaterial>& materials,
		const Math::Vector4& col, const Math::Vector3& emissive);

	// モデルデータ描画：アニメーションに非対応
	void DrawModel(const KdModelData& rModel, const Math::Matrix& mWorld = Math::Matrix::Identity, 
		const Math::Color& colRate = kWhiteColor, const Math::Vector3& emissive = Math::Vector3::Zero);

	// モデルワーク描画：アニメーションに対応
	void DrawModel(KdModelWork& rModel, const Math::Matrix& mWorld = Math::Matrix::Identity,
		const Math::Color& colRate = kWhiteColor, const Math::Vector3& emissive = Math::Vector3::Zero);

	// 任意の頂点群からなるポリゴン描画
	void DrawPolygon(const KdPolygon& poly, const Math::Matrix& mWorld = Math::Matrix::Identity,
		const Math::Color& colRate = kWhiteColor, const Math::Vector3& emissive = Math::Vector3::Zero);

	// 任意の頂点群からなるポリゴンライン描画
	void DrawVertices(const std::vector<KdPolygon::Vertex>& vertices, const Math::Matrix& mWorld = Math::Matrix::Identity,
		const Math::Color& colRate = kWhiteColor,
		KdDepthStencilState depthState = static_cast<KdDepthStencilState>(2),
		D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
		KdRasterizerState raster = static_cast<KdRasterizerState>(0)); // 0 = CullNone

	//================================================
	// 初期化・解放
	//================================================

	// 初期化
	bool Init();
	// 解放
	void Release();

	~KdStandardShader()
	{
		Release();
	}

	std::shared_ptr<KdTexture>& GetDepthTex() { return m_depthMapFromLightRTPack.m_RTTexture; }

private:

	// マテリアルのセット
	void WriteMaterial(const KdMaterial& material, const Math::Vector4& colRate, const Math::Vector3& emiRate);

	// ポリゴンの法線情報を2Dように書き換える
	void ConvertNormalsFor2D(std::vector<KdPolygon::Vertex>& target, const Math::Matrix& mWorld);

	// 定数バッファを初期状態に戻す
	void ResetCBObject();

	// スキンメッシュ有効かどうか(スキンメッシュ対応)
	void SetIsSkinMeshObj(bool isSkinMEshObj)
	{
		if (m_cb0_Obj.Work().IsSkinMeshObj != (int)isSkinMEshObj)
		{
			m_cb0_Obj.Work().IsSkinMeshObj = isSkinMEshObj;
			m_dirtyCBObj = true;
		}
	}

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// Lit：陰影をつけるオブジェクトの描画用（不透明な物体やキャラクタの板ポリなど
	// 平行光・点光源などの影響を受け角度によって色を変化させるオブジェクトを描画するシェーダー
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// UnLit：陰影のつかないオブジェクトの描画用（エフェクトや透明な物体など
	// 光の計算を行わずマテリアルの色をそのまま出力するシェーダー
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// GenDepthFromLight：光から見たオブジェクトの距離を赤で出力用
	// Litシェーダーで影の描画を行うために必要な情報テクスチャを作成するシェーダー
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

	// 頂点シェーダー
	ID3D11VertexShader* m_VS_Lit = nullptr;					// 陰影あり
	ID3D11VertexShader* m_VS_UnLit = nullptr;				// 陰影なし
	ID3D11VertexShader* m_VS_GenDepthFromLight = nullptr;	// 光からの深度
	ID3D11VertexShader* m_VS_Outline = nullptr;				// アウトライン（背面押し出し）

	// 頂点入力レイアウト
	ID3D11InputLayout* m_inputLayout = nullptr;
	ID3D11InputLayout* m_outlineInputLayout = nullptr;	// アウトライン用（スロット1=スムース法線）
	
	// ピクセルシェーダー
	ID3D11PixelShader* m_PS_Lit = nullptr;					// 陰影あり
	ID3D11PixelShader* m_PS_UnLit = nullptr;				// 陰影なし
	ID3D11PixelShader* m_PS_GenDepthFromLight = nullptr;	// 光からの深度
	ID3D11PixelShader* m_PS_Outline = nullptr;				// アウトライン（単色）

	// テクスチャ
	std::shared_ptr<KdTexture>	m_dissolveTex    = nullptr;	// ディゾルブで使用するデフォルトテクスチャ
	std::shared_ptr<KdTexture>	m_grassTex       = nullptr;	// 芝生テクスチャ（t4スロット）
	std::shared_ptr<KdTexture>	m_grassNormalTex = nullptr;	// 芝生法線マップ（t5スロット）
	std::shared_ptr<KdTexture>	m_grassEdgeTex   = nullptr;	// 草エッジ（境目）テクスチャ（t6スロット）

	// 定数バッファ
	KdConstantBuffer<cbObject>		m_cb0_Obj;				// オブジェクト単位で更新
	KdConstantBuffer<cbMesh>		m_cb1_Mesh;				// メッシュ毎に更新
	KdConstantBuffer<cbMaterial>	m_cb2_Material;			// マテリアル毎に更新
	KdConstantBuffer<cbBone>		m_cb3_Bone;				// ボーン事に更新(スキンメッシュ対応「)
	KdConstantBuffer<cbOutline>		m_cb_Outline;			// アウトライン太さ（b4）

	KdRenderTargetPack	m_depthMapFromLightRTPack;
	KdRenderTargetChanger m_depthMapFromLightRTChanger;

	bool		m_dirtyCBObj = false;						// 定数バッファのオブジェクトに変更があったかどうか
};
