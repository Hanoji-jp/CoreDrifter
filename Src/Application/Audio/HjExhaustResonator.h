#pragma once

#include "../Const/ExhaustConst.h"

//==========================================================
// HjExhaustResonator
//   排気系(管・集合部・マフラー)の共鳴を、共鳴モードの並列で作る。
//
//   共鳴フィルタ1個でマフラーを近似すると、倍音の豊かな音に
//   鋭い共鳴が1つ立つ構造になり、これは人間の母音とまったく同じ。
//   結果、エンジンではなく「唸り声」に聞こえてしまう。
//
//   実物の排気系は共鳴が何十個もある。片側が開いた管の奇数次モード、
//   集合部や膨張室、サイレンサー内部の反射。それらが重なることで
//   はじめて「管を通った音」になる。
//
//   engine-simはインパルス応答の畳み込みでこれを行っているが、
//   ここでは共鳴モードを並列に置いて近似する(モーダル合成)。
//   畳み込みより軽く、管の長さから周波数を決められるので調整もしやすい。
//
//   使い方:
//     SetPipeLength(m) …長さを変えたら呼ぶ(モードを組み直す)
//     float y = Process(x)   … 毎サンプル
//==========================================================
class HjExhaustResonator
{
public:
	void Init(float sampleRate);
	// 管の長さから各モードの周波数を組み直す
	void Rebuild();

	// 1サンプル通す
	float Process(float in);

	void Reset();

	// 調整値(音作りパネルから触る)
	float m_pipeLength   = ExhaustConst::PipeLength;
	float m_bandwidth    = ExhaustConst::ModeBandwidth;
	float m_bandwidthRise = ExhaustConst::ModeBandwidthRise;
	float m_rolloff      = ExhaustConst::ModeRolloff;
	float m_detune       = ExhaustConst::ModeDetune;
	float m_lowCut       = ExhaustConst::LowModeCut;   // 最低次のモードを抑える強さ
	float m_mix          = ExhaustConst::ResonanceMix;
	float m_trim         = ExhaustConst::OutputTrim;

private:
	// 共鳴モード1つ。2極の共振器。
	//   y[n] = a1*y[n-1] + a2*y[n-2] + gain*x[n]
	// 極を単位円の近くに置くことで、その周波数だけが長く響く。
	struct Mode
	{
		float a1 = 0.0f, a2 = 0.0f;   // 係数(周波数と減衰から決まる)
		float gain = 0.0f;
		float y1 = 0.0f, y2 = 0.0f;   // 過去の出力
	};

	Mode  m_modes[ExhaustConst::ModeCount];
	float m_sampleRate = 44100.0f;
	unsigned int m_rng = 1234567u;    // モードのずらし方を決める(毎回同じ形にする)
};
