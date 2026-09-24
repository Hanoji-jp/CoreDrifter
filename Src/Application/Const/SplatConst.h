#pragma once

#include "RoadConst.h"

// 地形の塗り分け(オートマテリアル)の定数。
//
// ■ 何をするか
// 地形の頂点1つずつに「ここは岩か、土か、草か、舗装か」の重みを持たせ、
// ピクセルシェーダーで4層を混ぜる。
//
// ■ なぜ手で塗らないか
// 数キロ四方の山肌を手で塗るのは無理がある。
// 傾きと道からの距離という、既に持っている情報から出せば、
// 地形を筆で変えた瞬間に塗りも付いてくる。
//
// ■ なぜ画像を貼らないか
// 1枚の絵を繰り返すと必ず継ぎ目が見える。隠すために大きく貼ると
// 解像度が足りない。借りものの権利を1枚ずつ確かめて回ることにもなる。
// ワールド座標のノイズで作れば、どこまで行っても模様が続く。
namespace SplatConst
{
	//===== 岩(傾きで出す) =====
	// 面の法線の上向き成分。1=真っ平ら、0=垂直の壁。
	//
	// 急な所に草を生やすと、切り立った崖が芝生の壁になって
	// 高さの感覚が消える
	constexpr float RockUpFull  = 0.55f;   // これ以下は完全に岩
	constexpr float RockUpNone  = 0.82f;   // これ以上は岩なし

	//===== 土(道からの距離で出す) =====
	// 道の脇がいきなり草だと、切り貼りしたように見える。
	// 踏み荒らされた帯を挟むと道が地面に馴染む。
	//
	// ■ 距離で書く理由
	// 手掛かりにできるのは HjRoad::OwnWeightAt だが、あれは
	// 「地形をどれだけ道の高さへ寄せたか」で、1m から 23m かけて落ちる。
	// 重みの数値でしきい値を書くと、0.55 のつもりが半径11mになる。
	// 見ての通り、路肩どころか山の中腹まで土になる。
	//
	// 距離で書いて、同じ式で重みへ直す。
	// DeformInner/Outer を動かしても帯の幅は変わらない
	constexpr float DirtFullDist = 5.0f;    // ここまでは完全に土(路肩の外まで)
	constexpr float DirtNoneDist = 9.0f;    // ここから先は草

	// HjRoad::DeformTerrain と同じ落ち方。
	// 向こうを変えたらこちらも合わせること
	constexpr float DeformWeightAt(float dist)
	{
		if (dist <= RoadConst::DeformInner) { return 1.0f; }
		if (dist >= RoadConst::DeformOuter) { return 0.0f; }

		const float t = (dist - RoadConst::DeformInner)
		              / (RoadConst::DeformOuter - RoadConst::DeformInner);

		return 1.0f - t * t * (3.0f - 2.0f * t);
	}

	constexpr float DirtOwnFull = DeformWeightAt(DirtFullDist);
	constexpr float DirtOwnNone = DeformWeightAt(DirtNoneDist);

	//===== 層の色 =====
	// 彩度は落とし気味にする。原色に寄せると、車と道が背景に負ける
	constexpr float RockR  = 0.42f, RockG  = 0.41f, RockB  = 0.39f;
	constexpr float DirtR  = 0.40f, DirtG  = 0.32f, DirtB  = 0.23f;
	constexpr float GrassR = 0.24f, GrassG = 0.34f, GrassB = 0.17f;
	constexpr float RoadR  = 0.16f, RoadG  = 0.16f, RoadB  = 0.17f;

	//===== 層の粗さ =====
	// 1 に近いほど艶が無い。
	//
	// 舗装を 0.55 にしていたが、これは濡れた路面の値。
	// 乾いたアスファルトは骨材がむき出しの粗い面で、
	// 実測でも 0.85〜0.95 あたり。低くすると、曇っていても
	// 路面だけ空を映して光る
	constexpr float RockRough  = 0.95f;
	constexpr float DirtRough  = 0.92f;
	constexpr float GrassRough = 0.90f;
	constexpr float RoadRough  = 0.88f;

	//===== 模様 =====
	constexpr float Grain     = 0.09f;    // 明暗の振れ幅
	constexpr float GrainFreq = 2.5f;     // 細かい粒(1/m)
	constexpr float MacroFreq = 0.035f;   // 大きな色ムラ(1/m)

	//===== 頂点色への詰め方 =====
	// R=岩 G=土 B=草 A=舗装。ABGR の並びで1つのuintにする
	inline unsigned int Pack(float rock, float dirt, float grass, float road)
	{
		auto to8 = [](float v) -> unsigned int
		{
			const float c = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
			return static_cast<unsigned int>(c * 255.0f + 0.5f);
		};

		return (to8(road)  << 24)
		     | (to8(grass) << 16)
		     | (to8(dirt)  <<  8)
		     |  to8(rock);
	}

	//===== 傾きと道の持ち分から重みを出す =====
	// 地形も道の裾も同じ規則で塗る。別々に決めると、
	// 継ぎ目で色が飛んで、そこに面の境目があることが見えてしまう
	inline unsigned int Weights(float upDot, float roadOwn)
	{
		// 符号は捨てる。
		//
		// 面の巻き方によっては法線が下を向く。絵の側は
		// SV_IsFrontFace で反転して正しく出るが、こちらはそれを
		// 知らないので、平らな裾がまるごと「垂直の壁」と判定されて
		// 岩になる。傾きの判定に要るのは大きさだけ
		upDot = (upDot < 0.0f) ? -upDot : upDot;

		const float rock = (upDot <= RockUpFull) ? 1.0f
		                 : (upDot >= RockUpNone) ? 0.0f
		                 : (RockUpNone - upDot) / (RockUpNone - RockUpFull);

		const float dirt = (roadOwn >= DirtOwnFull) ? 1.0f
		                 : (roadOwn <= DirtOwnNone) ? 0.0f
		                 : (roadOwn - DirtOwnNone) / (DirtOwnFull - DirtOwnNone);

		// 岩と土で埋まらなかったぶんが草。
		// 3つを別々に決めて後から正規化すると、急斜面の道端が
		// 岩でも土でもない中間色になって濁る
		const float used  = (rock + dirt > 1.0f) ? 1.0f : (rock + dirt);
		const float grass = 1.0f - used;

		return Pack(rock, dirt, grass, 0.0f);
	}
}
