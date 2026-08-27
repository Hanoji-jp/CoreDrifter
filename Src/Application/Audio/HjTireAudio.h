#pragma once

#include "../Const/TireAudioConst.h"

//==========================================================
// HjTireAudio
//   タイヤのスキール音とブレーキ鳴きを合成で鳴らす。
//
//   タイヤの「キーッ」は、路面に食いついては滑るのを高速で繰り返す
//   スティックスリップ振動。だから
//     ・ほぼ音程のある「鳴き」(共鳴させたノイズ)
//     ・路面を削る「擦れ」(広い帯域のノイズ)
//   の2つでできている。滑りが激しいほど鳴きが強く、高くなる。
//
//   ブレーキ鳴きはパッドの振動で、タイヤよりずっと高く細い。
//   低速で強く踏んだ時にだけ出る。
//
//   どちらもノイズ系なので、エンジン音と違って合成が素直に効く。
//
//   使い方(所有者=CarBase):
//     Init()                                  … 起動時に1回
//     Update(dt, slip01, speed, brake, ground) … 毎フレーム
//==========================================================
class HjTireAudio
{
public:
	HjTireAudio() = default;
	~HjTireAudio() { Stop(); }

	void Init();
	// slip01 = 後輪の滑り量(0〜1) / speed = 車速(m/s)
	// brake01 = ブレーキの踏み具合(0〜1) / onGround = 接地しているか
	void Update(float dt, float slip01, float speed, float brake01, bool onGround);
	void Stop();

	// 音源の位置を反映する(距離減衰・定位・遠くの音の鈍り)
	void Apply3D(const Math::Vector3& worldPos);

	void DrawImGui();
	void CollectTuneParams(std::vector<std::pair<const char*, float*>>& out);

	bool IsValid() const { return m_voice != nullptr; }

private:
	void SubmitPending();
	void RenderBlock(std::vector<float>& out);
	float Rand01();

	// 2極の共鳴器。ノイズを通すと、その周波数だけが鳴き出す。
	// 鋭さ(Q)を上げるほど細く「キーッ」に近づく。
	struct Resonator
	{
		float y1 = 0.0f, y2 = 0.0f;
		// 周波数と鋭さから係数を作って1サンプル通す
		float Process(float in, float freqHz, float q, float sampleRate);
	};

	IXAudio2SourceVoice* m_voice = nullptr;
	std::vector<std::vector<float>> m_blocks;
	int m_nextBlock = 0;

	Resonator m_sq1, m_sq2, m_sq3, m_brake;
	float m_scrubLp = 0.0f;
	float m_wobVal  = 0.0f;   // 鳴きの高さの揺らぎ(不規則)

	// スティックスリップ：路面に食いついては滑るのを高速で繰り返す振動。
	// これがタイヤの「キーッ」の正体で、ノイズを共鳴させただけでは出ない。
	// 食いつき→解放のたびに衝撃が出るので、波形は鋸のような繰り返しになる。
	float m_stickPhase = 0.0f;
	float m_stickJit   = 0.0f;   // 周期のばらつき(完全に一定だとブザーになる)

	// 音を作る側が読む値。急に変えるとブツッと鳴るので滑らかに追う
	float m_squealGain = 0.0f;
	float m_brakeGain  = 0.0f;
	float m_slip       = 0.0f;
	float m_speed      = 0.0f;

	unsigned int m_rng = 987654321u;

	// 調整値
	float m_squealBaseHz = TireAudioConst::SquealBaseHz;
	float m_squealSlipHz = TireAudioConst::SquealSlipHz;
	float m_squealQ      = TireAudioConst::SquealQ;
	float m_squealLevel  = TireAudioConst::SquealLevel;
	float m_scrubLevel   = TireAudioConst::ScrubLevel;
	float m_scrubTone    = TireAudioConst::ScrubTone;
	float m_brakeHz      = TireAudioConst::BrakeSquealHz;
	float m_brakeLevel   = TireAudioConst::BrakeSquealLevel;
	float m_master       = TireAudioConst::MasterVolume;

	// ボイスを作ったときのXAudio2エンジン。
	//
	// KdAudioManager::Release() はエンジンごと破棄し、ぶら下がっている
	// ボイスもそこで全て解放される。こちらが持っているのは生ポインタなので、
	// その後に触ると解放済みメモリへのアクセスになって落ちる。
	// 静的オブジェクトの破棄はエンジンの破棄より後に走ることがあるため、
	// デストラクタから解放するだけでは防げない。
	//
	// 毎回「今のエンジンが当時と同じか」を確かめ、違えば
	// ボイスは既に解放済みとみなしてポインタを捨てるだけにする。
	IXAudio2* m_xa = nullptr;
	bool IsVoiceAlive() const;

	HjTireAudio(const HjTireAudio&) = delete;
	void operator=(const HjTireAudio&) = delete;
};
