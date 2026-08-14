#pragma once

#include "../Const/TireAudioConst.h"

//==========================================================
// 1輪ぶんの滑り具合。
// 物理側(CarBase::StepTireForces)が毎フレーム書き込み、音側が読むだけ。
// 音のために物理を変えることはしない。
//==========================================================
struct HjTireSlipState
{
	// 接地面が路面に対して滑っている速さ(m/s)。
	// 横滑り＋駆動スリップに、摩擦円で切られたぶん(掴みきれず流れている量)を足したもの。
	float slipSpeed = 0.0f;
	// 接地荷重(0.25＝4輪均等の基準)。荷重が抜けた輪は鳴りが細くなる
	float load = 0.0f;
};

//==========================================================
// HjTireAudio
//   タイヤの鳴き(スキール音)を波形から組み立てて鳴らす。
//
//   タイヤが鳴く仕組みは「スティックスリップ」。接地面のゴムブロックが
//   路面に貼り付いて→ねじれて→限界で滑って戻る、を高速で繰り返す。
//   この自励振動はゴムとカーカスの共振周波数で起こるので、
//   鳴きの高さは車速ではなく「タイヤの構造」で決まる。
//   (実際、引きずったスキール音はずっと同じ高さで鳴り続ける)
//
//   よって滑る速さで動かすのは音程ではなく音量と音の荒さ。
//   滑りが深くなるとブロックが貼り付く時間が無くなり、
//   音程のある「鳴き」から音程の無い「擦れ」へ移る。
//
//     鳴き … ノイズ → 高いQの共鳴モード数本
//     擦れ … ノイズ → 広いバンドパス
//
//   共鳴は前軸・後軸で別に持つ。前後は荷重も滑りも違うので、
//   1つにまとめると1本の笛になってしまう。
//
//   エンジン音と同じくXAudio2のソースボイスへ自分でPCMを書き込む。
//
//   使い方(所有者=CarBase):
//     Init()                        … 起動時に1回
//     Update(dt, wheels, grounded)  … 毎フレーム
//==========================================================
class HjTireAudio
{
public:
	HjTireAudio() = default;
	~HjTireAudio() { Stop(); }

	void Init();
	// wheels=各輪の滑り具合 / grounded=接地しているか(滞空中は鳴らさない)。
	// 立ち上がりの平滑化はサンプル単位で行うので、フレームの経過時間は要らない。
	void Update(const HjTireSlipState (&wheels)[TireAudioConst::WheelNum], bool grounded);
	void Stop();

	// 音作り用パネル。鳴らしながら詰められないと音作りは終わらない
	void DrawImGui();
	// 保存/読込用。CarBaseのチューニングファイルへ相乗りする
	void CollectTuneParams(std::vector<std::pair<const char*, float*>>& out);

	void SetMasterVolume(float v) { m_master = v; }
	bool IsValid() const { return m_voice != nullptr; }

private:
	// 空いているぶんだけPCMを作ってボイスへ流し込む
	void SubmitPending();
	// 1ブロックぶんの波形を書き出す
	void RenderBlock(std::vector<float>& out);

	float Rand01();

	IXAudio2SourceVoice* m_voice = nullptr;

	// 送信済みブロックの保持先。XAudio2は再生し終わるまでメモリを参照するので、
	// 送ったバッファは解放せずに持ち続ける必要がある。
	std::vector<std::vector<float>> m_blocks;
	int m_nextBlock = 0;

	// 共鳴モード1つ。2極の共振器。
	//   y[n] = a1*y[n-1] + a2*y[n-2] + gain*x[n]
	// 極を単位円の近くに置くことで、その周波数だけが長く響く。
	struct Mode
	{
		float a1 = 0.0f, a2 = 0.0f;
		float gain = 0.0f;
		float y1 = 0.0f, y2 = 0.0f;
	};

	// 軸(前/後)ごとの状態。前後で荷重も滑りも違うので別々に鳴らす。
	struct Axle
	{
		// Updateが書く目標値。音量はサンプル単位で追従させるので、
		// フレーム境界で段が付かない(段が付くとプチプチとノイズが乗る)。
		float squealTarget = 0.0f;
		float roarTarget   = 0.0f;
		float hzTarget     = TireAudioConst::SquealBaseHz;

		float squeal = 0.0f;   // 鳴きの強さ(0〜1, 平滑化済み)
		float roar   = 0.0f;   // 擦れの強さ(0〜1, 平滑化済み)
		float hz     = TireAudioConst::SquealBaseHz;   // 今の共鳴周波数
		// 音程のゆらぎ。スティックスリップは不安定で常に揺れている。
		// 正弦で揺らすとビブラートになるので、なました乱数で不規則に揺らす。
		float warble = 0.0f;

		Mode  modes[TireAudioConst::ModeCount];
		// 擦れ用バンドパス(状態変数フィルタ)の状態
		float scrubLow = 0.0f, scrubBand = 0.0f;
	};

	Axle m_axle[TireAudioConst::AxleNum];

	unsigned int m_rng = 987654321u;   // ノイズ用

	//===== 音作りのパラメータ =====
	// 定数ではなくメンバに持たせて、走りながら耳で詰められるようにする
	float m_baseHz        = TireAudioConst::SquealBaseHz;
	float m_frontPitchMul = TireAudioConst::FrontPitchMul;
	float m_bandwidth     = TireAudioConst::ModeBandwidth;
	float m_bandwidthRise = TireAudioConst::ModeBandwidthRise;

	float m_pitchSlipGain = TireAudioConst::PitchSlipGain;
	float m_pitchLoadGain = TireAudioConst::PitchLoadGain;
	float m_warbleDepth   = TireAudioConst::WarbleDepth;

	float m_slipStart    = TireAudioConst::SlipStart;
	float m_slipFull     = TireAudioConst::SlipFull;
	float m_roarStart    = TireAudioConst::RoarStart;
	float m_roarFull     = TireAudioConst::RoarFull;
	float m_roarTakeover = TireAudioConst::RoarTakeover;

	float m_scrubHz = TireAudioConst::ScrubCenterHz;
	float m_scrubQ  = TireAudioConst::ScrubQ;

	float m_attack  = TireAudioConst::AttackSpeed;
	float m_release = TireAudioConst::ReleaseSpeed;

	float m_squealLevel = TireAudioConst::SquealLevel;
	float m_scrubLevel  = TireAudioConst::ScrubLevel;
	float m_master      = TireAudioConst::MasterVolume;

	HjTireAudio(const HjTireAudio&) = delete;
	void operator=(const HjTireAudio&) = delete;
};
