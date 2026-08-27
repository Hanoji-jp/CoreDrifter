#include "HjProfiler.h"

namespace
{
	// 平均の寄せ具合。小さいほど落ち着くが、変化に気づくのが遅れる
	constexpr double Smooth = 0.06;
	// この値を下回ったら最大値を忘れる。ずっと覚えていると、
	// 一度重かっただけの数字が残り続けて役に立たない
	constexpr double PeakDecay = 0.97;

	// 高精度カウンタの1秒あたりの目盛り数。最初に一度だけ調べる
	double TicksPerMs()
	{
		static double ms = []()
		{
			LARGE_INTEGER f;
			QueryPerformanceFrequency(&f);
			return static_cast<double>(f.QuadPart) / 1000.0;
		}();
		return ms;
	}

	long long NowTicks()
	{
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);
		return t.QuadPart;
	}
}

void HjProfiler::BeginFrame()
{
	if (!m_enabled) { return; }

	// 前フレームの結果を平均へ入れる
	const long long now = NowTicks();
	if (m_frameStart != 0)
	{
		const double ms = static_cast<double>(now - m_frameStart) / TicksPerMs();
		m_frameMs += (ms - m_frameMs) * Smooth;
	}
	m_frameStart = now;

	for (Section& s : m_sections)
	{
		s.average += (s.thisFrame - s.average) * Smooth;

		// 最大値は少しずつ忘れる。忘れないと、一度重かっただけの
		// 数字が残り続けて今の状態が読めない
		s.peak *= PeakDecay;
		if (s.thisFrame > s.peak) { s.peak = s.thisFrame; }

		s.thisFrame = 0.0;
	}
}

void HjProfiler::Add(const char* name, double ms)
{
	if (!m_enabled || !name) { return; }

	for (Section& s : m_sections)
	{
		if (s.name == name) { s.thisFrame += ms; return; }
	}

	Section s;
	s.name      = name;
	s.thisFrame = ms;
	m_sections.push_back(s);
}

void HjProfiler::DrawImGui()
{
	if (!ImGui::CollapsingHeader(U8("処理時間"))) { return; }

	bool on = m_enabled;
	if (ImGui::Checkbox(U8("計測する"), &on)) { SetEnabled(on); }

	if (!m_enabled)
	{
		ImGui::TextDisabled(U8("(止めています)"));
		return;
	}

	// 1フレームの持ち時間。ここを超えるとFPSが落ちる
	constexpr double Budget = 1000.0 / 60.0;
	ImGui::Text(U8("1フレーム %.2f ms  (60fpsの持ち時間 %.2f ms)"), m_frameMs, Budget);

	// 持ち時間に対する割合を棒で出す。数字だけより一目で分かる
	ImGui::ProgressBar(static_cast<float>(m_frameMs / Budget), ImVec2(-1, 0));

	ImGui::Separator();

	// 遅い順に並べる。探す手間を省く
	std::vector<const Section*> sorted;
	sorted.reserve(m_sections.size());
	for (const Section& s : m_sections) { sorted.push_back(&s); }
	std::sort(sorted.begin(), sorted.end(),
	          [](const Section* a, const Section* b) { return a->average > b->average; });

	double sum = 0.0;
	for (const Section* s : sorted)
	{
		sum += s->average;

		// 持ち時間の1/4を超える区間は色を変える。犯人はたいていここ
		const bool heavy = (s->average > Budget * 0.25);
		if (heavy) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.4f, 1.0f)); }

		ImGui::Text(U8("%-16s %6.2f ms   (最大 %5.2f)"),
		            s->name.c_str(), s->average, s->peak);

		if (heavy) { ImGui::PopStyleColor(); }
	}

	ImGui::Separator();

	// 測っていない部分。ここが大きいなら、計測箇所が足りていない
	ImGui::TextDisabled(U8("計測した合計 %.2f ms / 測っていない分 %.2f ms"),
	                    sum, m_frameMs - sum);
}

//==========================================================
// 区間の計測
//==========================================================

HjScopedTimer::HjScopedTimer(const char* name)
	: m_name(name)
{
	if (!HjProfiler::Instance().IsEnabled()) { m_name = nullptr; return; }
	m_start = NowTicks();
}

HjScopedTimer::~HjScopedTimer()
{
	if (!m_name) { return; }

	const double ms = static_cast<double>(NowTicks() - m_start) / TicksPerMs();
	HjProfiler::Instance().Add(m_name, ms);
}
