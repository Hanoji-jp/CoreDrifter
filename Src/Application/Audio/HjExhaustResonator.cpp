#include "HjExhaustResonator.h"

using namespace ExhaustConst;

void HjExhaustResonator::Init(float sampleRate)
{
	m_sampleRate = std::max(sampleRate, 8000.0f);
	Reset();
	Rebuild();
}

void HjExhaustResonator::Reset()
{
	for (Mode& m : m_modes) { m.y1 = 0.0f; m.y2 = 0.0f; }
}

//----------------------------------------------------------
// 管の長さから各モードの周波数を組み直す。
//
// 排気管はエンジン側がほぼ閉じ、出口が開いた管なので、
// 共鳴は奇数次に立つ。
//     f(n) = (2n-1) × 音速 ÷ (4 × 管長)
//
// ただし綺麗な奇数比のまま並べると倍音列と同じ構造になり、
// 音程が立って楽器のように聞こえてしまう。
// 実物は管も部屋も形が不揃いで比が綺麗にならないので、少しずらす。
//----------------------------------------------------------
void HjExhaustResonator::Rebuild()
{
	const float len = std::max(m_pipeLength, 0.15f);
	const float f1  = SpeedOfSound / (4.0f * len);   // 1次モード

	// 毎回同じ形になるよう、乱数の種を固定してから作る
	m_rng = 1234567u;
	auto rnd = [&]()
	{
		m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5;
		return static_cast<float>(m_rng & 0xFFFF) / 65535.0f;   // 0〜1
	};

	const float nyquist = m_sampleRate * 0.45f;

	for (int i = 0; i < ModeCount; ++i)
	{
		Mode& m = m_modes[i];
		const int n = i + 1;

		// 奇数次(1,3,5...)＋不揃いのずらし
		const float detune = 1.0f + (rnd() * 2.0f - 1.0f) * m_detune;
		float f = f1 * static_cast<float>(2 * n - 1) * detune;

		if (f >= nyquist)
		{
			// 鳴らせない高さのモードは黙らせる(折り返し雑音になるため)
			m.a1 = m.a2 = m.gain = 0.0f;
			continue;
		}

		// 高いモードほど速く減衰する(高音ほど早く失われる実際の性質)
		const float bw = m_bandwidth + m_bandwidthRise * static_cast<float>(i);

		// 2極の共振器。極の半径rが1に近いほど長く響く
		const float r = expf(-3.14159265f * bw / m_sampleRate);
		const float w = 6.2831853f * f / m_sampleRate;

		m.a1 = 2.0f * r * cosf(w);
		m.a2 = -r * r;

		// 高いモードほど弱く。共振の利得(1/(1-r))で割って高さを揃える
		float lvl = powf(1.0f / static_cast<float>(n), m_rolloff);

		// 最低次のモードを抑える。1/nで並べると1次が一番強くなるが、
		// 実物の管では最低次ほど強く減衰する。そのままだと
		// 100〜180Hzが張り出して「ドコドコ」＝トラクターのような音になる。
		if (m_lowCut > 0.001f)
		{
			const float x = f / LowModeCutHz;
			lvl *= 1.0f - m_lowCut / (1.0f + x * x * x);   // 低いほど強く削る
		}

		m.gain = lvl * (1.0f - r);
	}
}

//----------------------------------------------------------
// 1サンプル通す。
// 全モードの出力を足す＝管の中で音が何十通りにも反射している状態。
// 共鳴が多いので、どれか1つが突出して「母音」に聞こえることがない。
//----------------------------------------------------------
float HjExhaustResonator::Process(float in)
{
	float sum = 0.0f;
	for (Mode& m : m_modes)
	{
		if (m.gain == 0.0f) { continue; }

		const float y = m.a1 * m.y1 + m.a2 * m.y2 + m.gain * in;
		m.y2 = m.y1;
		m.y1 = y;
		sum += y;
	}

	// 素の波形と混ぜる。0にすると管を通す前の音になる
	const float mix = std::clamp(m_mix, 0.0f, 1.0f);
	return (in * (1.0f - mix) + sum * mix) * m_trim;
}
