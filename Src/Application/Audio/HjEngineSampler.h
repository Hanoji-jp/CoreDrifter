#pragma once

#include "../Const/EngineSamplerConst.h"

//==========================================================
// HjEngineSampler
//   録音素材によるエンジン音。CarXと同じ構造。
//
//   RPM帯ごとに録ったループ素材を最初から全部鳴らしっぱなしにしておき、
//   毎フレーム「ピッチ」と「音量」だけを書き換える。
//   再生を開始し直さないので、回転が変わっても音が途切れない。
//
//     ピッチ … 現在のRPM ÷ 素材の基準RPM
//     音量   … (RPMの重み) × (アクセルのオン/オフの重み) の掛け算
//
//   素材が用意できていないものは黙って無視するので、
//   1本だけでも動くし、揃えるほど回転域ごとの質感が出る。
//
//   使い方(所有者=HjEngineAudio):
//     Init()                     … 起動時に1回
//     Update(dt, rpm, throttle)  … 毎フレーム
//==========================================================
class HjEngineSampler
{
public:
	void Init();
	void Update(float dt, float rpm, float throttle);
	void Stop();

	// 素材が1本でも読めているか。読めていなければ合成へ切り替える判断に使う
	bool HasAnyLayer() const { return m_hasAny; }

	void DrawImGui();
	void CollectTuneParams(std::vector<std::pair<const char*, float*>>& out);

private:
	// 1本ぶんの素材。鳴らしっぱなしにして音量とピッチだけ動かす
	struct Layer
	{
		std::shared_ptr<KdSoundInstance> inst;
		float baseRpm = 2000.0f;   // この素材を録音した時の回転数
		float volume  = 1.0f;      // 音量バランス
	};

	// 素材を読み込んでループ再生を始める(音量0で開始)。失敗したらfalse
	bool AddLayer(Layer& out, const char* path, float* baseRpm, float* volume);
	// 音量とピッチを反映する
	void ApplyLayer(Layer& layer, float rpm, float gain);

	// 等パワークロスフェード。単純な線形だと交差点で音量が凹む
	static void CrossFade(float value, float start, float end,
	                      float& outHigh, float& outLow);

	// アクセルを戻した瞬間のバックファイア。同じ音の繰り返しを避けて複数から選ぶ
	void UpdateBackfire(float dt, float rpm, float throttleRaw);

	Layer m_onLow, m_onHigh, m_offLow, m_offHigh;
	bool  m_hasAny = false;

	std::vector<std::string> m_backfirePaths;
	float m_backfireTimer = 0.0f;   // 連続で鳴らさないための待ち
	float m_prevThrottle  = 0.0f;
	unsigned int m_rng = 2463534242u;

	// 調整値
	float m_baseRpmOnLow   = EngineSamplerConst::BaseRpmOnLow;
	float m_baseRpmOnHigh  = EngineSamplerConst::BaseRpmOnHigh;
	float m_baseRpmOffLow  = EngineSamplerConst::BaseRpmOffLow;
	float m_baseRpmOffHigh = EngineSamplerConst::BaseRpmOffHigh;

	float m_volOnLow   = EngineSamplerConst::VolOnLow;
	float m_volOnHigh  = EngineSamplerConst::VolOnHigh;
	float m_volOffLow  = EngineSamplerConst::VolOffLow;
	float m_volOffHigh = EngineSamplerConst::VolOffHigh;

	float m_crossStart = EngineSamplerConst::CrossRpmStart;
	float m_crossEnd   = EngineSamplerConst::CrossRpmEnd;
	float m_master     = EngineSamplerConst::MasterVolume;
	float m_backfireVolume = EngineSamplerConst::BackfireVolume;

	// 表示・平滑化用
	float m_rpm      = 0.0f;
	float m_throttle = 0.0f;
};
