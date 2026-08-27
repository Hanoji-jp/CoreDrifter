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
	// revCut=レブリミッターの効き具合(0〜1)。点火カットとして音に出す
	void Update(float dt, float rpm, float throttle, float maxRpm, float revCut = 0.0f);
	void Stop();

	// 音源の位置を反映する(距離減衰・定位・遠くの音の鈍り)。
	// 音が常に耳元で同じ大きさで鳴っていると距離と方向の手がかりが無く、
	// どれだけ波形を作り込んでも実在しない音に聞こえる。
	void Apply3D(const Math::Vector3& worldPos);

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
	// cutScale … 点火直後だけカットオフを開く倍率(1発の中のアタック＝パンチ)
	void RenderTail(float& dst, float v, float svfF, float svfQ,
	                float fmtF, float fmtQ, float spoolTarget,
	                float spoolStep, float rpmN, float cutScale);

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

	// 気筒ごとの燃焼の強さ。実機は気筒ごとにわずかに違い、その差が
	// クランク1サイクルごとの振幅の揺れになる＝あの「ざらついた回り方」。
	// 全部同じだと純粋な倍音列になり、ブザーやノコギリ波と同じ構造になる。
	float m_cylGain[EngineAudioConst::MaxHarmonics] = {};
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
	// 低域を切るフィルタの状態。排気管の最低モードは非常に低く、
	// そのままだと「ボー」という濁りになる
	float m_hpState = 0.0f;
	float m_highPassHz = EngineAudioConst::HighPassHz;

	// ノイズを整えるための一次フィルタ
	float m_noiseLp = 0.0f;
	float m_bovLp   = 0.0f;

	// 回転のゆらぎ。燃焼のばらつきとクランクのねじれで、実機の回転は常に揺れている。
	// 全倍音がまとめて同じ比率で揺れるので、生き物っぽさが出る。
	float m_rpmJitter = 0.0f;
	// ノイズを点火に合わせて脈打たせるための位相
	float m_firePhase = 0.0f;

	// レブリミッター。実物は点火をカットするので、その気筒は燃えずに
	// 生ガスが排気管で爆ぜる。あの「ババババッ」の正体。
	// 音量を絞るだけでは「当たっている感じ」が出ない。
	float m_revCut     = 0.0f;   // 効き具合(0〜1)

	// 飛ばす点火を均等に散らすための繰り上がり。
	// 確率で毎回独立に決めると、カットが連続して音が大きく途切れる。
	// 実物のECUも失火が偏らないよう均等に分散させる。
	float m_cutCarry = 0.0f;
	// 今の点火・前の点火が飛んでいるか(0=燃えた 1=カット)。
	// 点火の切れ目でステップ状に切り替えると波形が不連続になりクリックが出るので、
	// 2つ持って点火位相で補間する。
	float m_cutNow  = 0.0f;
	float m_cutPrev = 0.0f;

	// 排気管で爆ぜる音。飛んだ生ガスは排気管へ流れてから着火するので、
	// 「飛んだ瞬間」ではなく少し遅れて鳴る。
	float m_popEnv     = 0.0f;   // 減衰
	float m_popAttack  = 0.0f;   // 立ち上がり(一瞬でジャンプさせない)
	float m_popPending = 0.0f;   // 遅れて鳴らす予約の強さ
	// 破裂音の音量はイベントごとに違う(レブのカットとアフターファイアでは別物)。
	// 共通の係数で一律に掛けると、片方に合わせるともう片方が聞こえなくなる。
	float m_popLevel   = 0.0f;
	float m_popPendingLevel = 0.0f;
	float m_popDelay   = 0.0f;   // 残り時間(秒)
	float m_popLp      = 0.0f;

	// アフターファイア。高回転で一気に閉じた時、混合気が排気管で
	// まとめて燃える「パパパンッ」。常時パラパラ鳴るオーバーランとは別物。
	int   m_afterfireLeft = 0;      // 残り発数
	float m_afterfireNext = 0.0f;   // 次の1発までの時間(秒)
	float m_afterfireLevel = EngineAudioConst::AfterfireLevel;
	// 深く切りすぎると音がブツブツに途切れ、破裂音も大きいと暴れすぎる。
	// 「当たった」と分かる程度に留めるための調整値。
	float m_cutDepth       = EngineAudioConst::CutDepth;
	float m_cutPopLevel    = EngineAudioConst::CutPopLevel;
	float m_overrunCrackle = EngineAudioConst::OverrunCrackle;

	// ターボ。タービンは排気で回るので、回転とアクセルに遅れて追従する。
	// この遅れが「踏んですぐには過給されない」ターボらしさになる。
	float m_spool     = 0.0f;   // 過給の立ち上がり具合(0〜1)
	float m_whinePhase = 0.0f;  // スプール音の位相
	// ブローオフバルブ。アクセルを閉じた瞬間の「プシュー」
	float m_bovEnv    = 0.0f;   // 鳴っている量(発動時に1へ)
	float m_prevThrottleForBov = 0.0f;

	// コンプレッサーサージ(タービンのフラッター)。
	// 逃がし弁が無い/閉じている時、過給空気がコンプレッサーを逆流して羽根を叩く。
	// 圧力が抜けるにつれて周期が延びる＝「ストゥトゥトゥ…トゥ…」と遅くなる。
	float m_surgeEnv   = 0.0f;   // 鳴っている量
	float m_surgePhase = 0.0f;   // 逆流の周期
	float m_surgeLp    = 0.0f;
	float m_surgeR1 = 0.0f, m_surgeR2 = 0.0f;   // 羽根を叩く音の共鳴
	float m_surgeLevel = EngineAudioConst::SurgeLevel;

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
	// 吸気の鳴きが回転で上がる量。ここがエンジンの「立ち上がり方」を決める。
	// 大きい＝6連スロットルのように回転と一緒に鳴き上がる(RB26)
	// 小さい＝大きなプレナムでモワッと立ち上がる(R35のVR38など)
	float m_formantRpmGain = EngineAudioConst::PresetRb26Dett.formantRpmGain;

	float m_rolloff      = EngineAudioConst::Rolloff;
	float m_pulseWidthMs = EngineAudioConst::PulseWidthMs;      // 排気パルスの長さ
	float m_cylImbalance = EngineAudioConst::CylinderImbalance; // 気筒ごとの強さの差
	float m_firingBoost  = EngineAudioConst::FiringBoost;
	float m_offRolloffAdd = EngineAudioConst::OffRolloffAdd;
	float m_wobble       = EngineAudioConst::HarmonicWobble;
	// 1発の中のアタック(パンチ)。倍音が一定のままでは作れない要素。
	float m_pulseAttack      = EngineAudioConst::PulseAttack;
	float m_pulseAttackSharp = EngineAudioConst::PulseAttackSharp;
	float m_pulsePunch       = EngineAudioConst::PulsePunch;

	float m_noiseLevel   = EngineAudioConst::NoiseLevel;
	float m_noiseTone    = EngineAudioConst::NoiseTone;

	float m_cutoffBase    = EngineAudioConst::CutoffBase;
	float m_cutoffRpmGain = EngineAudioConst::CutoffRpmGain;
	float m_cutoffThrGain = EngineAudioConst::CutoffThrGain;
	float m_resonance     = EngineAudioConst::Resonance;

	float m_offVolume  = EngineAudioConst::OffThrottleVolume;
	float m_rpmVolGain = EngineAudioConst::RpmVolumeGain;
	float m_master     = EngineAudioConst::MasterVolume;

	// 診断用のピーク値。耳だけで詰めると、飽和しているのか小さすぎるのか
	// 判別できない。特に物理シミュレーションは出力の大きさを机上で決めたので、
	// 目で見えないと調整が手探りになる。
	float m_peakSource = 0.0f;   // 音源(合成 or シミュレーション)の生の大きさ
	float m_peakOut    = 0.0f;   // 出力段を通した後

	unsigned int m_rng = 22695477u;   // ノイズ用

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

	HjEngineAudio(const HjEngineAudio&) = delete;
	void operator=(const HjEngineAudio&) = delete;
};
