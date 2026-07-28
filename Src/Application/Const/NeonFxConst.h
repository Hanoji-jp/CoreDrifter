#pragma once

// タイヤ周りのネオン・ライン画エフェクト(NFS Unbound風の落書きグラフィック)の定数。
// リング(渦)＋スパーク(線の飛び)を加算合成で描く。
namespace NeonFxConst
{
	constexpr int MaxParticles = 620;      // プール上限(爆発で一度に大量に出るので多めに)

	// ── リング(タイヤを囲む輪。広がりながら消える) ──
	constexpr int   RingSegments  = 20;    // 円を何本の線分で描くか
	constexpr float RingLifeMin   = 0.28f;
	constexpr float RingLifeMax   = 0.55f;
	constexpr float RingRadiusMin = 0.30f; // 生成時の半径(m)
	constexpr float RingRadiusMax = 0.55f;
	constexpr float RingGrow      = 1.9f;  // 寿命いっぱいで何倍に広がるか
	constexpr float RingThickness = 0.13f; // 線の太さ(m)
	constexpr float RingSpinMax   = 6.0f;  // 面内回転速度(rad/s)
	constexpr float RingWobble    = 0.18f; // 半径の揺らぎ(手描き感。0=真円)

	// ── スパーク(接地点から飛ぶ短い線) ──
	constexpr float SparkLifeMin  = 0.32f;
	constexpr float SparkLifeMax  = 0.75f;
	constexpr float SparkSpeedMin = 3.0f;  // 初速(m/s)
	constexpr float SparkSpeedMax = 9.0f;
	constexpr float SparkLength   = 0.34f; // 線の長さ(m)
	constexpr float SparkThick    = 0.10f; // 線の太さ(m)
	constexpr float SparkDrag     = 1.3f;  // 減速(1/s)。強いと初速を上げても飛距離が伸びない
	constexpr float SparkRise     = 1.6f;  // 上向き初速(m/s)

	// ── 放出制御(スリップ量に応じる。煙と同じ条件で出す) ──
	constexpr float RingPerSec    = 7.0f;  // 全開スリップ時のリング放出数/秒(1輪)
	constexpr float SparkPerSec   = 26.0f; // 同スパーク放出数/秒(1輪)

	// ── ブースト(ニトロ)の一瞬のフラッシュ＋パーティクル爆発 ──
	// 本家は発動の瞬間だけ車体に色が乗り、同時に線画が弾け飛ぶ。
	// フェードさせず、パッと色が乗ってパッと戻す(本家はこの切り替わり方)
	constexpr float BoostFlashHold  = 0.20f;  // 色が乗っている時間(秒)
	// 本家は輪ではなく「細長い線が放射状に大量に飛ぶ」。線の長さもバラバラで、
	// 短い破片から長い尾を引くものまで混ざる。
	constexpr int   BoostBurstRings  = 1;     // 発動時に弾けるリングの数(控えめ)
	constexpr int   BoostBurstSparks = 85;    // 発動時に弾けるスパークの数
	constexpr float BoostBurstSpeed  = 5.0f;  // 爆発の勢い(通常スパークに対する倍率)
	// 車体の中に埋もれないよう、中心から少し外側で湧かせる半径(m)。
	// 深度テストを効かせているので、中心から出すと車内の粒が見えなくなる。
	constexpr float BoostSpawnRadius = 1.1f;
	// 飛ぶ向き：地面に沿って水平に放射状。上下へのばらつきはごく僅かに留める。
	// (球状に全方向へ飛ばすと"爆発"になってしまい、地を這って走る感じが出ない)
	constexpr float BoostVertSpread = 0.16f;  // 上下のばらつき(0=完全に水平)
	constexpr float BoostSpawnY     = 0.35f;  // 湧く高さ(車の足元寄り, m)
	constexpr float BoostLenMin      = 1.2f;  // 線の長さのばらつき(倍率)
	constexpr float BoostLenMax      = 5.0f;
	// 丸い粒。本家は線だけでなく丸のパーティクルも一緒に飛んでいる。
	constexpr int   BoostBurstDots = 20;      // 発動時に飛ぶ丸い粒の数
	constexpr int   DotSegments    = 10;      // 丸の分割数(多いほど滑らか)
	constexpr float DotSizeMin     = 0.05f;   // 丸の半径(m)
	constexpr float DotSizeMax     = 0.16f;
	constexpr float DotLifeMin     = 0.25f;
	constexpr float DotLifeMax     = 0.60f;
	constexpr float DotDrag        = 1.1f;    // 減速(1/s)
	constexpr float DotGravity     = 0.9f;    // 落下(m/s^2)。地を這わせるので弱め

	// 不透明度：寿命のほとんどは濃いまま保ち、最後だけ消える。
	// (寿命に比例して薄めると、ほぼ常に半透明で色が乗らない)
	constexpr float FadeStart = 0.75f;   // この寿命比までは完全不透明

	// 色の混ぜ方の段数。1=A/Bのどちらかだけ(中間色なし) / 大きいほど連続的に混ざる。
	// 本家は赤と白がはっきり分かれていて中間のピンクは少ないので、既定は2色に振り分ける。
	constexpr int ColorSteps = 1;

	// レーザー光線の見た目：色の本体＋中心の白い芯。
	// 周囲への滲みは板ポリを重ねて作らず、後段のLightBloom(実ブラー)に任せる。
	// ここで太いグロー層を重ねると線が分厚くなるだけなので、本体は実寸のままにする。
	constexpr float GlowWidthMul = 1.0f;   // 色の本体の太さ(基準に対する倍率)
	constexpr float GlowAlpha    = 1.00f;  // 本体の濃さ
	constexpr float CoreWidthMul = 0.38f;  // 芯の太さ(基準に対する倍率)
	constexpr float CoreWhite    = 0.85f;  // 芯を白へ寄せる量(1=真っ白)

	// 実際のぼかしはポストプロセスのLightBloomに任せる。
	// DrawBright()で輝度RTへ描いた絵は4段階のガウスぼかしを経て加算合成されるため、
	// 板ポリを重ねるだけの偽グローと違い、周囲へ本当に光が滲む。
	// 輝度RTの絵は4段階のぼかしを重ねて加算されるので、強く書くとすぐ白飛びする。
	// 光っていると分かる程度に留め、線そのものの形を潰さないこと。
	constexpr float BloomWidthMul = 0.8f;  // 輝度RTへ描く形の太さ(本体に対する倍率)
	constexpr float BloomAlpha    = 0.30f; // 輝度RTへの書き込み強度(強いほど光が広がる)

	// ── 色(加算合成なので明るめ) ──
	constexpr float ColorAR = 0.25f, ColorAG = 1.00f, ColorAB = 0.85f;  // ティール
	constexpr float ColorBR = 0.70f, ColorBG = 0.30f, ColorBB = 1.00f;  // 紫
	// 通常合成なので1.0が素の色。上げると白飛びする(加算合成時代の名残で1.6だった)
	constexpr float Brightness = 1.0f;
}
