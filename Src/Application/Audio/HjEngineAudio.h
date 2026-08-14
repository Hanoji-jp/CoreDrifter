#pragma once

#include "../Const/EngineAudioConst.h"
#include "HjEngineSim.h"       // 圧力を解いて音を出す物理シミュレーション
#include "HjEngineSampler.h"   // 録音素材のクロスフェード(CarXと同じ構造)
#include "HjExhaustResonator.h" // 排気系の共鳴(モードの並列)

//==========================================================
// HjEngineAudio
//   エンジン音を波形から組み立てて鳴らす。録音素材は使わない。
//
//   人間がエンジン音だと認識しているのは「倍音の並び方」なので、
//   その倍音列を直接鳴らす(エンジンオーダー加算合成)。
//
//       基本周波数 f0 = RPM / 120      … 4ストロークのクランク1サイクル
//       出力 = Σ A(k) × sin(2π × f0 × k × t)
//
//   どの次数(オーダー)が強いかが気筒配置そのもの。
//   直4はk=4、V8はk=8が点火倍音になり、その間に挟まる中間オーダーが
//   ざらつきとうねりを作る。V8クロスプレーンの「ドロドロ」がこれ。
//
//   さらに、
//     ・吸排気の乱流としてノイズを足す
//     ・マフラーをレゾナンス付きローパスで近似する
//       (アクセルを踏むとカットオフが上がる＝音が開く)
//
//   XAudio2のソースボイスへ自分でPCMを書き込む。
//   KdAudioManagerのPlay()は再生ごとにインスタンスを作る作りなので、
//   毎サンプル計算するこの種の合成には使えない。
//
//   使い方(所有者=CarBase):
//     Init()                          … 起動時に1回
//     Update(dt, rpm, throttle, max)  … 毎フレーム
//==========================================================
class HjEngineAudio
{
public:
	// 積んでいるエンジン。気筒配置・マフラー・ターボまで一式で切り替わる。
	enum class Engine
	{
		Sr20Det,    // 直4ターボ(S15純正)。ザラつきがあり低めに唸る
		Rb26Dett,   // 直6ツインターボ。滑らかで甲高く、吸気がよく鳴く
		Jz2Gte,     // 直6ツインターボ。RBより太く低い
		V8Cross,    // V8クロスプレーン(参考。日本車ではない)
	};

	HjEngineAudio() = default;
	~HjEngineAudio() { Stop(); }

	void Init();
	// rpm=エンジン回転数 / throttle=アクセル開度(0〜1) / maxRpm=レッドゾーン
	void Update(float dt, float rpm, float throttle, float maxRpm);
	void Stop();

	// 音作り用パネル。鳴らしながら詰められないと音作りは終わらない
	void DrawImGui();
	// 保存/読込用。CarBaseのチューニングファイルへ相乗りする
	void CollectTuneParams(std::vector<std::pair<const char*, float*>>& out);

	void SetMasterVolume(float v) { m_master = v; }
	bool IsValid() const { return m_voice != nullptr; }

private:
	// 選んだエンジンの設定を一式読み込む
	void ApplyEnginePreset();
	// 気筒配置から点火角を作り、シミュレーションへ渡す
	void ConfigureSim();

	// 音源の切り替え。
	//   Sample  … 録音素材のクロスフェード(CarXと同じ構造)。素材があればこれが本命
	//   Physics … 圧力を解く物理シミュレーション
	//   Synth   … 倍音を並べる合成
	// 素材が1本も読めていない時は自動でSynthへ落ちるので、無音にはならない。
	enum class Source { Sample, Physics, Synth };
	Source m_source = Source::Sample;

	HjEngineSim     m_sim;
	HjEngineSampler m_sampler;

	// 空いているぶんだけPCMを作ってボイスへ流し込む
	void SubmitPending();
	// 1ブロックぶんの波形を書き出す
	void RenderBlock(std::vector<float>& out);
	// 出力段：マフラー・吸気の共鳴・ターボを通して1サンプル書き出す。
	// 音源(物理シミュレーション / 倍音合成)のどちらから来ても共通の処理。
	void RenderTail(float& dst, float v, float svfF, float svfQ,
	                float fmtF, float fmtQ, float spoolTarget,
	                float spoolStep, float rpmN);

	float Rand01();

	IXAudio2SourceVoice* m_voice = nullptr;

	// 送信済みブロックの保持先。XAudio2は再生し終わるまでメモリを参照するので、
	// 送ったバッファは解放せずに持ち続ける必要がある。
	std::vector<std::vector<float>> m_blocks;
	int m_nextBlock = 0;

	// 倍音ごとの状態。位相を連続させることで、回転が変わっても音が途切れない。
	struct Harmonic
	{
		float amp    = 0.0f;   // 今の振幅(目標へ滑らかに寄せる)
		// 揺らぎ。正弦波で揺らすとその揺れ自体が規則的で電子音になるので、
		// ゆっくりしたランダム(なました乱数)で不規則に揺らす。
		float wobVal = 0.0f;
	};

	// 倍音すべての基準になる位相。
	// 各倍音は k 倍した位相で鳴らす＝位相が揃う。
	// ※倍音ごとに独立した位相を持たせると、同じ倍音構成でも波形が均されて
	//   フルートやオルガンのような音になる。エンジンの波形は
	//   爆発のたびの鋭い圧力パルスなので、位相が揃っていないといけない。
	float m_masterPhase = 0.0f;
	Harmonic m_harm[EngineAudioConst::MaxHarmonics];

	// マフラー(レゾナンス付きローパス)の状態
	float m_svfLow  = 0.0f;
	float m_svfBand = 0.0f;
	// 排気系の共鳴。共鳴フィルタ1個だと母音になってしまうので、
	// 管・集合部・マフラーのモードを何十個も並列に置いて近似する。
	HjExhaustResonator m_exhaust;

	// 吸気の共鳴(バンドパス)の状態。狭い帯域だけを強調して金属的な鳴きを出す
	float m_fmtLow  = 0.0f;
	float m_fmtBand = 0.0f;
	// ノイズを整えるための一次フィルタ
	float m_noiseLp = 0.0f;
	float m_bovLp   = 0.0f;

	// 回転のゆらぎ。燃焼のばらつきとクランクのねじれで、実機の回転は常に揺れている。
	// 全倍音がまとめて同じ比率で揺れるので、生き物っぽさが出る。
	float m_rpmJitter = 0.0f;
	// ノイズを点火に合わせて脈打たせるための位相
	float m_firePhase = 0.0f;

	// ターボ。タービンは排気で回るので、回転とアクセルに遅れて追従する。
	// この遅れが「踏んですぐには過給されない」ターボらしさになる。
	float m_spool     = 0.0f;   // 過給の立ち上がり具合(0〜1)
	float m_whinePhase = 0.0f;  // スプール音の位相
	// ブローオフバルブ。アクセルを閉じた瞬間の「プシュー」
	float m_bovEnv    = 0.0f;   // 鳴っている量(発動時に1へ)
	float m_prevThrottleForBov = 0.0f;

	// 音を作る側が読む値。毎フレーム滑らかに追従させる
	float m_rpm      = 0.0f;
	float m_throttle = 0.0f;
	float m_maxRpm   = 8000.0f;

	//===== 音作りのパラメータ =====
	// 定数ではなくメンバに持たせて、走りながら耳で詰められるようにする
	Engine m_engine = Engine::Rb26Dett;   // S15にRB26を積むのは実際によくある組み合わせ
	float m_firingOrder = static_cast<float>(EngineAudioConst::PresetRb26Dett.firingOrder);
	float m_halfLevel   = EngineAudioConst::PresetRb26Dett.halfLevel;
	float m_otherLevel  = EngineAudioConst::PresetRb26Dett.otherLevel;

	// ターボ
	float m_turboLevel  = EngineAudioConst::PresetRb26Dett.turboLevel;
	float m_spoolUp     = EngineAudioConst::SpoolUpSpeed;
	float m_spoolDown   = EngineAudioConst::SpoolDownSpeed;
	float m_whineBase   = EngineAudioConst::WhineBaseHz;
	float m_whineGain   = EngineAudioConst::WhineGainHz;
	float m_bovLevel    = EngineAudioConst::BovLevel;

	// 吸気の共鳴
	float m_formantHz     = EngineAudioConst::PresetRb26Dett.formantHz;
	float m_formantAmount = EngineAudioConst::PresetRb26Dett.formantAmount;
	float m_formantQ      = EngineAudioConst::FormantQ;

	float m_rolloff      = EngineAudioConst::Rolloff;
	float m_firingBoost  = EngineAudioConst::FiringBoost;
	float m_offRolloffAdd = EngineAudioConst::OffRolloffAdd;
	float m_wobble       = EngineAudioConst::HarmonicWobble;

	float m_noiseLevel   = EngineAudioConst::NoiseLevel;
	float m_noiseTone    = EngineAudioConst::NoiseTone;

	float m_cutoffBase    = EngineAudioConst::CutoffBase;
	float m_cutoffRpmGain = EngineAudioConst::CutoffRpmGain;
	float m_cutoffThrGain = EngineAudioConst::CutoffThrGain;
	float m_resonance     = EngineAudioConst::Resonance;

	float m_offVolume  = EngineAudioConst::OffThrottleVolume;
	float m_rpmVolGain = EngineAudioConst::RpmVolumeGain;
	float m_master     = EngineAudioConst::MasterVolume;

	unsigned int m_rng = 22695477u;   // ノイズ用

	HjEngineAudio(const HjEngineAudio&) = delete;
	void operator=(const HjEngineAudio&) = delete;
};
