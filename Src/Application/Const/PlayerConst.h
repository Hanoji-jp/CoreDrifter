#pragma once

// プレイヤーの累計データ（レベル・累計スコア・走行回数）の定数。
namespace PlayerConst
{
	// 保存先。CarTune_*.txt / StageConfig.txt と同じ「key value」の簡易テキスト。
	constexpr const char* ProfilePath = "Asset/Data/PlayerProfile.txt";

	// 表示名。将来ガレージ側で変更できるようにする想定なので、
	// ここは「まだ名前を決めていないときの既定値」の位置づけ。
	constexpr const char* DefaultName = "DRIVER01";
	constexpr int MaxNameLen = 12;   // バッジの幅に収まる長さ

	//===== レベル =====
	// 累計スコアからレベルを決める。
	// 必要量を一定にすると、続けるほど上がりにくさが感じられなくなるので、
	// レベルが上がるごとに必要量を増やす（等差数列）。
	//   Lv1→2 に BaseExp、Lv2→3 に BaseExp+StepExp … と増えていく。
	constexpr double BaseExp = 20000.0;
	constexpr double StepExp = 8000.0;
	constexpr int    MaxLevel = 99;

	// マルチで自分の車を見分けるための色。
	// アウトラインと煙に使う。既定は緑(Silviaの煙の色に合わせてある)
	constexpr float DefaultColorR = 0.55f;
	constexpr float DefaultColorG = 1.00f;
	constexpr float DefaultColorB = 0.30f;
}
