// 定数バッファ(オブジェクト単位)
cbuffer cbObject : register(b0)
{
	float2	g_UVOffset;
	float2	g_UVTiling;

	int g_FogEnable;	// フォグ有効
	int g_OnlyEmissie;	// エミッシブの描画だけにするかどうか
	int g_IsSkinMeshObj;// スキンメッシュオブジェクトかどうか(スキンメッシュ対応)

	float g_dissolveValue;		// ディゾルブの閾値
	float g_dissolveEdgeRange;	// ディゾルブの境界線の太さ
	float3 g_dissolveEmissive;	// 境界の色

	int   g_UseTriplanar;		// トリプレーナーUV 有効フラグ
	float g_TriplanarScale;		// トリプレーナーUV スケール（ワールド座標に掛ける値）
	int   g_SphereNormal;		// 球法線を中心方向で解析計算するフラグ
	float g__tripad;			// パディング

	// 芝生ブレンド
	int   g_UseGrass;			// 芝生ブレンド有効フラグ
	float g_GrassBlendSharpness;// ブレンドの鋭さ（大きいほど境界がくっきり）
	float g_GrassTriplanarScale;// 芝テクスチャのスケール
	int   g_UseGrassNormal;		// 芝生法線マップ有効フラグ（盛り上がり表現）
	float3 g_GravityUpDir;		// 重力の「上」方向（芝が生える面の判定に使う）
	float g__grasspad2;			// パディング

	// エッジ（境目）テクスチャブレンド
	int   g_UseGrassEdge;		// エッジブレンド有効フラグ
	float g_GrassEdgeWidth;		// エッジ帯域の幅（0.1〜0.5推奨。upDot のどの範囲をエッジとするか）
	float g_GrassEdgeTexScale;	// エッジテクスチャのトリプレーナースケール
	float g_FullEdgeStrength;	// 全面エッジテクスチャブレンド強度（0=無効 1=フル上書き）

	// スモーク専用ライティング＋ディゾルブ（板ポリを球ドーム法線でトゥーン陰影＋溶けて消す）
	// メッシュ煙のトゥーン陰影(3段：ハイライト(白) / 標準色 / 暗い色)
	int   g_SmokeLit;			// 有効フラグ
	float g_ToonDark;			// 暗い面：基準色に掛ける倍率
	float g_ToonWhite;			// 明るい面：白へ寄せる量
	float g_ToonMidThr;			// これ以上の受光で「標準色」

	float g_ToonHiThr;			// これ以上の受光で「ハイライト(白)」
	float g_ToonUpBias;			// 光を真上へ寄せる量(1=完全に真上から)
	float g_SmokeMerge;			// 共有の陰影を混ぜる割合(0=粒ごと 1=高さのみ)
	float g_SmokeBaseY;			// 煙の根元のワールド高さ

	float g_SmokePlumeH;		// 根元から上端までの高さ(m)
	// スクリーン空間のハーフトーン模様（印刷物っぽい質感）
	float g_SmokePatScale;		// 模様の周期(px)
	float g_SmokePatStrength;	// 模様の濃さ(0=無効)
	float g_SmokePatDarkBias;	// 暗い面ほど模様を強く出す量

	// 明暗の境界のうねり（水平一直線に切れて見えるのを防ぐ）
	float g_SmokeWobAmp;		// 境界の揺れ幅(m)
	float g_SmokeWobFreq;		// 揺れの細かさ(1/m)
	// 発生源から離れるほど色を変える「後方グラデーション」(全粒で共有＝境目が出ない)
	float g_SmokeOriginX;		// 発生源のワールドX
	float g_SmokeOriginZ;		// 発生源のワールドZ

	float g_SmokeGradDist;		// この距離(m)で色Bになりきる
	float g_SmokeColorBR;		// 遠方の色
	float g_SmokeColorBG;
	float g_SmokeColorBB;

	// 陰影の決め方の配合(0=法線 1=粒ローカル高さ)
	float g_SmokeLocalY;
	// ディゾルブ：消え際に穴が広がって崩れる
	float g_SmokeDissolve;		// 進行度(0=無傷 1=完全消滅)
	float g_SmokeDissolveScale;	// 崩れる粒の細かさ(セル/m)
	// ハイライトの色（白固定ではなく好きな色にできる）
	float g_SmokeHiR;

	float g_SmokeHiG;
	float g_SmokeHiB;
	// アクセントカラー塗り（車体などを指定色1色で塗り潰す。陰影はそのまま残る）
	float g_TintAmount;			// 0=元の色 1=完全にアクセントカラー
	float g_TintR;

	float g_TintG;
	float g_TintB;
	float g_SmokeViewLight;		// 煙の光をカメラ基準にする割合(1=カメラを回すと影も回る)
	float g_SmokeFillLight;		// 2つ目の光(横上から)の強さ。横向きの面にもハイライトを乗せる

	// タイヤ痕の焼き付けマップ。コースを真上から見た1枚に痕を書き溜めてあり、
	// 路面はワールドXZから引いて色を暗くする(痕そのものは描かない＝何本あっても軽い)
	float g_MarkMapEnable;		// 0=無効
	float g_MarkMapOriginX;		// 覆う範囲の隅(ワールドX)
	float g_MarkMapOriginZ;		// 覆う範囲の隅(ワールドZ)
	float g_MarkMapInvSize;		// 1÷覆う一辺(m)

	float g_MarkMapDarken;		// 最大でどこまで暗くするか(1=真っ黒)
	float g_MarkPad0;
	float g_MarkPad1;
	float g_MarkPad2;
};

// 定数バッファ(メッシュ単位)
cbuffer cbMesh : register(b1)
{
	// オブジェクト情報
	row_major float4x4 g_mWorld; // ワールド変換行列
};

cbuffer cbMaterial : register(b2)
{
	float4	g_BaseColor; // ベース色
	float3	g_Emissive;  // 自己発光色
	float	g_Metallic;	 // 金属度
	float	g_Roughness; // 粗さ
};

// ボーン行列配列(スキンメッシュ対応)
cbuffer cbBones : register(b3)
{
	row_major float4x4 g_mBones[300];
};

// 頂点シェーダから出力するデータ
struct VSOutput
{
	float4 Pos	 : SV_Position;	// 射影座標
	float3 wPos  : TEXCOORD0;	// ワールド3D座標

	float2 UV	 : TEXCOORD1;	// UV座標
	float4 Color : TEXCOORD2;	// 色

	float3 wN	 : TEXCOORD3;	// ワールド法線
	float3 wT	 : TEXCOORD4;	// ワールド接線
	float3 wB	 : TEXCOORD5;	// ワールド従法線
};

struct VSOutputNoLighting
{
	float4 Pos	 : SV_Position; // 射影座標
	float3 wPos  : TEXCOORD0;	// ワールド3D座標

	float2 UV	 : TEXCOORD1;	// UV座標
	float4 Color : TEXCOORD2;	// 色
};

struct VSOutputGenShadow
{
	float4 Pos	 : SV_Position;	// 射影座標
	float4 pPos  : TEXCOORD0;	// 射影座標（VP変換無し

	float2 UV	 : TEXCOORD1;	// UV座標
	float4 Color : TEXCOORD2;	// 色
};
