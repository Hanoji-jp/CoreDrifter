#pragma once

// ドリフトスモーク(トゥーン煙)の定数
namespace SmokeConst
{
	// パーティクルプールの上限(リングバッファで古いものから上書き)
	// 生存中の粒を上書きすると"ぱっと消える"ので、放出数×最大寿命より十分多く確保する。
	//   50枚/秒 × 4クラスタ × 2輪 × 1.4秒 ≒ 560 が最大生存数 → 余裕を持って確保
	// 描画コストは生存数依存なのでプールを大きくしても重くならない。
	constexpr int MaxParticles = 1500;

	// 寿命(秒)：生成時に Min〜Max のランダム
	constexpr float LifeMin = 0.70f;
	constexpr float LifeMax = 1.40f;

	// 板ポリのサイズ(m)：生成時→消滅時にかけて膨らむ(③膨張)
	constexpr float SizeStart = 0.55f;
	constexpr float SizeEnd   = 2.30f;   // 消え際に巨大な円にならないよう控えめ
	// 粒ごとのサイズばらつき(倍率の範囲)。均一だと卵の列に見える
	constexpr float SizeVarMin = 0.65f;
	constexpr float SizeVarMax = 1.45f;

	// 初速(密集させて重ねる＝もこもこ。散らしすぎない・ゆっくり昇る)
	constexpr float RiseSpeed   = 0.55f;  // 上昇速度(m/s)
	constexpr float SpreadSpeed = 0.55f;  // 水平の散り(m/s) ※小さいほど塊になる
	constexpr float TrailFactor = 0.15f;  // 車速の逆向きに引きずる割合
	constexpr float Drag        = 2.60f;  // 速度減衰(1/s) 大きいほど早く漂って溜まる

	// 1回の放出で近接して撒く粒数(⑥小粒を重ねて密度を稼ぐ＝薄い粒でも繋がる)
	constexpr int   ClusterCount    = 4;
	constexpr float ClusterRadius   = 0.24f; // クラスタ内のばらけ半径(m)

	// ④乱流：ゆっくり渦を巻くように揺らして規則性を壊す(消え際を散らす)
	constexpr float TurbStrength = 0.9f;   // 揺らしの強さ
	constexpr float TurbFreq     = 3.0f;   // 揺らしの周波数

	// ①ディゾルブ：消え際に雲アルファの薄い所から穴が開いて崩れる(不透明度フェードと併用)
	constexpr float ErodeStrength = 0.6f;  // 穴あきの強さ(控えめ。フェードを主役にする)
	constexpr float ErodeEdge     = 0.45f; // 穴の縁の柔らかさ(大きいほど滑らか)

	// 放出制御(密な塊をたくさん重ねる)
	constexpr float SlipThreshold = 2.0f;  // これ以上の後輪スリップ量で煙が出始める
	constexpr float SlipFull      = 8.0f;  // この横滑りで放出レート最大
	constexpr float SpawnPerSec   = 50.0f; // 全開スリップ時の毎秒放出数(1輪あたり。⑥密度増)
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

	// 不透明度(柔らかい煙アルファを半透明で重ねて"繋がった塊"にする。
	// 1粒は薄く、重なりで密度を積み上げる＝つなぎ目が消える)
	constexpr float AlphaPeak    = 0.40f;  // 最大不透明度(薄め＝濃すぎ回避・重なりで繋がる)
	constexpr float FadeInRatio  = 0.12f;  // 寿命比これまでで立ち上がる
	constexpr float FadeOutRatio = 0.45f;  // 寿命比これぶん残して消える(長めで滑らかに散る)

	// スモークテクスチャ
	//   ToonSmoke.png … 輪郭焼き込みの硬いトゥーン(重ねると輪郭線が汚い)
	//   SoftSmoke.png … 縁フェードの柔らかい煙アルファ(重なりが繋がる。本家寄り)
	constexpr const char* TexturePath = "Asset/Textures/System/SoftSmoke.png";
}
