#pragma once

// 走り出しのカウントダウン。デザイン座標(1536x864)。
namespace CountdownConst
{
	constexpr float CanvasW = 1536.0f, CanvasH = 864.0f;
	constexpr float CenterX = 768.0f;

	// 3 → 2 → 1 → GO。1つあたりの秒数
	constexpr float StepTime = 1.0f;
	constexpr int   StartCount = 3;
	// GOを出しておく時間
	constexpr float GoTime = 0.7f;

	// 数字。画面いっぱいに出す
	constexpr float NumberY = 470.0f;
	constexpr float NumberPx = 200.0f;
	// 1秒ごとに膨らませて縮める。動きがあると「今変わった」と分かる
	constexpr float PulseAmount = 0.16f;

	// 上の帯(コース名)と下の帯(合図)
	constexpr float CourseY = 236.0f;
	constexpr float StrapY = 588.0f, StrapH = 38.0f, StrapPadX = 20.0f;

	// 四隅のかぎ括弧
	constexpr float BracketInset = 40.0f, BracketLen = 110.0f;
}
