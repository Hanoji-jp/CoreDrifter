//=====================================================
//
// 全シェーダ共通
//
//=====================================================

//------------------------------
// 定数バッファ(カメラ)
//------------------------------
cbuffer cbCamera : register(b7)
{
	// カメラ情報
	row_major float4x4  g_mView;	// ビュー変換行列
	row_major float4x4  g_mProj;	// 射影変換行列
	row_major float4x4  g_mProjInv;	// 射影変換行列：逆行列
	float3              g_CamPos;	// カメラ座標
};

//------------------------------
// 定数バッファ(フォグ)
//------------------------------
cbuffer cbFog : register(b8)
{
	// 距離フォグ
	int g_DistanceFogEnable;		// 距離フォグ有効/無効フラグ
	float3 g_DistanceFogColor;		// 距離フォグ色
	float g_DistanceFogDensity;		// 距離フォグ減衰率
	
	// 高さフォグ
	float g_HeightFogTopValue;		// 高さフォグの開始の高さ
	float g_HeightFogBottomValue;	// 高さフォグの終了の高さ
	float g_HeightFogDistance;		// 高さフォグの開始の距離
	float3 g_HeightFogColor;		// 高さフォグ色
	int g_HeightFogEnable;			// 高さフォグ有効/無効フラグ
};

//------------------------------
// 定数バッファ(ライト)
//------------------------------
cbuffer cbLight : register(b9)
{
	// 環境光
	float4  g_AmbientLight;

	// 平行光
	float3  g_DL_Dir;    // 光の方向
	float3  g_DL_Color;  // 光の色
	row_major float4x4  g_DL_mLightVP;  // ビュー行列 x 射影行列
	
	//--------------
	// 環境の映り込み
	//--------------
	// まわりの景色を上・横・下の3色で持つ。
	//
	// キューブマップの代わり。車体へ映るのは
	// 「上に何があるか」「横に何があるか」でほぼ決まるので、
	// 3色を向きで混ぜるだけでも金属らしさが出る。
	//
	// 屋外なら 上=空 / 横=地平 / 下=地面、
	// 車庫なら 上=照明 / 横=壁 / 下=床
	float4  g_EnvUp;
	float4  g_EnvSide;
	float4  g_EnvDown;
	
	//--------------
	// 点光
	//--------------
	// 使用数
	int4			g_PointLightNum;   // パッキング規則が大変なので、int4のxのみを使用する
	// データ
	struct PointLight 
	{
	    float3	    Color;			// 色
	    float	    Radius;			// 半径
	    float3	    Pos;			// 座標
	    int			IsBright;		// 明度用ライトかどうか
	} g_PointLights[100];

	//--------------
	// スポット光
	//--------------
	// 点光に向きと開き角を付けたもの。
	//
	// 点光は全方向へ届くので、当てたい物を明るくすると、
	// まわりの床も壁も一緒に明るくなる。
	// 当てたい所だけを照らすには、円錐で切る必要がある。
	//
	// 並びは C++ 側の SpotLight と1バイトも違えられない
	int4			g_SpotLightNum;   // int4 の x だけを使う

	struct SpotLight
	{
		float3		Color;      // 色。強さも色の大きさで持つ
		float			Range;      // 届く距離

		float3		Pos;        // 光源の位置
		float			AngleCos;   // 円錐の外側。これより外は届かない

		float3		Dir;        // 向いている方向(正規化済み)
		float			InnerCos;   // 円錐の内側。ここまでは減らさない
	} g_SpotLights[8];
};
