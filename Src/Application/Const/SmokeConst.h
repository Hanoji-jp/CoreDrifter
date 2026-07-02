#pragma once

// ドリフトスモーク(トゥーン煙)の定数
namespace SmokeConst
{
	// パーティクルプールの上限(リングバッファで古いものから上書き)
	// クラスタ放出で数が増えるので多めに確保
	constexpr int MaxParticles = 640;

	// 寿命(秒)：生成時に Min〜Max のランダム
	constexpr float LifeMin = 0.70f;
	constexpr float LifeMax = 1.40f;

	// 板ポリのサイズ(m)：生成時→消滅時にかけて膨らむ
	constexpr float SizeStart = 0.55f;
	constexpr float SizeEnd   = 2.60f;
	// 粒ごとのサイズばらつき(倍率の範囲)。均一だと卵の列に見える
	constexpr float SizeVarMin = 0.65f;
	constexpr float SizeVarMax = 1.45f;

	// 初速(密集させて重ねる＝もこもこ。散らしすぎない・ゆっくり昇る)
	constexpr float RiseSpeed   = 0.55f;  // 上昇速度(m/s)
	constexpr float SpreadSpeed = 0.55f;  // 水平の散り(m/s) ※小さいほど塊になる
	constexpr float TrailFactor = 0.15f;  // 車速の逆向きに引きずる割合
	constexpr float Drag        = 2.60f;  // 速度減衰(1/s) 大きいほど早く漂って溜まる

	// 1回の放出で近接して撒く粒数(小さな瘤を重ねて1つの塊に見せる)
	constexpr int   ClusterCount    = 3;
	constexpr float ClusterRadius   = 0.22f; // クラスタ内のばらけ半径(m)

	// 放出制御(密な塊をたくさん重ねる)
	constexpr float SlipThreshold = 2.0f;  // これ以上の後輪スリップ量で煙が出始める
	constexpr float SlipFull      = 8.0f;  // この横滑りで放出レート最大
	constexpr float SpawnPerSec   = 38.0f; // 全開スリップ時の毎秒放出数(1輪あたり)
	constexpr float HandbrakeBoost = 4.0f; // サイド中はスリップ量に加算(常時煙)
	// これ未満の車速では煙を出さない(停止中にサイドを引いても出さない)
	constexpr float MinSpeed = 2.5f;

	// 板ポリの面内回転(rad/s)
	constexpr float SpinMax = 1.2f;

	// 放出位置の後輪ローカル配置
	constexpr float WheelGroundY = 0.06f;  // 接地点のわずかな浮き

	// テクスチャアトラス(瘤状ブロブ4種を2x2に格納。陰影・輪郭は焼き込み済み)
	constexpr int SplitX = 2;
	constexpr int SplitY = 2;
	constexpr int VariantCount = 4;

	// 不透明度(NFS Unbound風のべったり不透明な塊。フェードは最小限)
	constexpr float AlphaPeak    = 0.92f;  // 最大不透明度
	constexpr float FadeInRatio  = 0.08f;  // 寿命比これまでで立ち上がる
	constexpr float FadeOutRatio = 0.30f;  // 寿命比これぶん残して消えていく

	// スモークテクスチャ
	constexpr const char* TexturePath = "Asset/Textures/System/ToonSmoke.png";
}
