#pragma once

//==========================================================
// HjProfiler
//   1フレームの時間がどこで使われているかを測る。
//
//   ■ なぜ必要か
//   このゲームはFPSが60で頭打ちになっている(1フレームが早く終わっても
//   次まで待つ)。そのため、速くなったかどうかがFPSでは分からない。
//   遅くなって初めて数字が落ちるので、「重い」と気づいた時点では
//   もう手遅れで、しかもどこが原因かも分からない。
//
//   区間ごとの実時間(ミリ秒)が見えれば、
//     ・16.6ミリ秒のうち何に使っているか
//     ・直した結果、その区間が本当に減ったか
//   の両方が分かる。
//
//   ■ 平均して出す
//   1フレームの値はばらつくので、そのまま出すと数字が踊って読めない。
//   少しずつ均した値を表示する。
//
//   使い方:
//     区間を測る   HjScopedTimer t(U8("物理"));   … 波括弧を抜けるまで
//     フレーム頭で HjProfiler::Instance().BeginFrame();
//     表示         HjProfiler::Instance().DrawImGui();
//==========================================================
class HjProfiler
{
public:
	static HjProfiler& Instance()
	{
		static HjProfiler inst;
		return inst;
	}

	// このフレームの計測を始める。前フレームの値は平均へ入れて捨てる
	void BeginFrame();

	// 区間の結果を足す。同じ名前を複数回呼べば合算される
	// (1フレームに何度も通る処理をまとめて測れる)
	void Add(const char* name, double ms);

	// 計測結果の表示
	void DrawImGui();

	// 計測を止める。止めている間は Add を無視するので、
	// 計測そのものの負荷も無くなる
	void SetEnabled(bool enable) { m_enabled = enable; }
	bool IsEnabled() const { return m_enabled; }

private:
	HjProfiler() = default;
	HjProfiler(const HjProfiler&) = delete;
	void operator=(const HjProfiler&) = delete;

	// 1区間ぶん
	struct Section
	{
		std::string name;
		double thisFrame = 0.0;   // このフレームの合計
		double average   = 0.0;   // 均した値(表示用)
		double peak      = 0.0;   // 直近の最大値。たまに重い所を見つける
	};

	std::vector<Section> m_sections;
	bool m_enabled = true;

	// フレーム全体の時間。区間の合計と比べて、
	// 測っていない部分がどれだけあるかを見る
	double m_frameMs = 0.0;
	long long m_frameStart = 0;
};

//==========================================================
// HjScopedTimer
//   波括弧を抜けた時点で、その区間にかかった時間を記録する。
//
//   開始と終了を手で書くと、途中で return したときに
//   終了を通らず、そこだけ測り漏れる。
//==========================================================
class HjScopedTimer
{
public:
	explicit HjScopedTimer(const char* name);
	~HjScopedTimer();

private:
	const char* m_name = nullptr;
	long long   m_start = 0;

	HjScopedTimer(const HjScopedTimer&) = delete;
	void operator=(const HjScopedTimer&) = delete;
};
