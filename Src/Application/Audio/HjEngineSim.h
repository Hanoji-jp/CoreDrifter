#pragma once

#include "../Const/EngineSimConst.h"

//==========================================================
// HjEngineSim
//   エンジンの中の圧力を解いて、そこから出る音を作る。
//
//   ■ 何を「音」として取り出すか
//   耳に届くのは、マフラーの出口から吹き出す流れが起こす空気の疎密。
//   弁が閉じている間、流れはゼロになる。つまり音は
//   「鋭いパルス」と「静けさ」の繰り返しであって、鳴りっぱなしではない。
//
//   集合部の圧力を音として使うと、集合部は積分器なので圧力が
//   休符まで戻らず、パルスの間が埋まって一本調子の唸りになる。
//   だから音源は圧力ではなく「弁を通る流れ」から作る。
//
//   ■ 管は1本ずつ
//   気筒ごとに管を持たせると、6気筒なら排気と吸気で12本が同時に鳴り、
//   混ざって濁る。実際に耳へ届くのは集合部から先の1本と、
//   吸気の入口までの1本。響きはそこで決まる。
//
//   ■ 音の出口は3つ。実車と同じように別々に作って混ぜる
//     ① 排気  … 弁を通る流れ → 排気管 → 大気
//     ② 吸気  … 弁を通る流れ → 吸気管 → 大気(吸い込む側の音)
//     ③ 本体  … 弁が座面へ当たる音(クランク角に同期した機械音)
//   排気だけだと「マフラーの音」しか鳴らない。
//
//   クランク・ピストンの剛体シミュレーションは持たない。
//   車の駆動系(CarBase)から回転数を貰えるので、トルクを解く必要がない。
//
//   使い方:
//     Configure(気筒数, 点火角)
//     Reset()
//     float s = Step(dtSec, rpm, throttle)   … 毎サンプル呼ぶ
//==========================================================
class HjEngineSim
{
public:
	// 気筒数と点火角を設定する。点火角はクランク角(0〜720)で、
	// これがそのまま点火順序になる。
	void Configure(int cylinderCount, const float* firingAnglesDeg);

	void Reset();

	// 出力1サンプルぶん進めて音を返す。
	// 中では OverSample 倍の細かさで解き、ローパスを通してから落とす。
	// 排気の流れの変化は鋭く、出力レートのまま解くと折り返し雑音になるため。
	//   dt       … 出力1サンプルの時間(秒)
	//   rpm      … エンジン回転数
	//   throttle … アクセル開度(0〜1)。吸入量になる
	float Step(float dt, float rpm, float throttle);

private:
	// 内部の1ステップ(オーバーサンプリングされた細かい時間刻み)
	float StepInner(float dt, float rpm, float throttle);
public:

	// 調整値(音作りパネルから触る)
	float m_compressionRatio = EngineSimConst::CompressionRatio;
	float m_combustionHeat   = EngineSimConst::CombustionHeat;
	float m_ignitionDeg      = EngineSimConst::IgnitionDeg;
	float m_burnDurationDeg  = EngineSimConst::BurnDurationDeg;
	float m_exhaustOpenDeg   = EngineSimConst::ExhaustOpenDeg;
	float m_exhaustFlowRate  = EngineSimConst::ExhaustFlowRate;
	float m_plenumVolume     = EngineSimConst::PlenumVolume;
	float m_plenumOutflow    = EngineSimConst::PlenumOutflow;
	float m_runnerLength     = EngineSimConst::ExhaustPipeLength;
	float m_runnerReflect    = EngineSimConst::RunnerReflect;
	float m_runnerDamp       = EngineSimConst::RunnerDamp;
	float m_intakeLength     = EngineSimConst::IntakePipeLength;
	float m_intakeReflect    = EngineSimConst::IntakeRunnerReflect;
	float m_intakeLevel      = EngineSimConst::IntakeLevel;
	float m_mechLevel        = EngineSimConst::MechLevel;
	float m_mechHz           = EngineSimConst::MechHz;
	float m_rodRatio         = EngineSimConst::RodRatio;
	float m_outputGain       = EngineSimConst::OutputGain;

	// いま自動で何倍にしているか(調整パネルの表示用)
	float GetAutoGain() const { return m_autoGain; }
	float GetCrankDeg() const { return m_crankDeg; }
	int   GetCylinderCount() const { return static_cast<int>(m_cylinders.size()); }

private:
	//------------------------------------------------------
	// デジタル導波管。管の中を走る圧力波を、行きと帰りの
	// 2本の遅延線で表す。両端で反射させることで、管の長さで
	// 決まる定在波(＝管の響き)が自然にできる。
	//------------------------------------------------------
	struct Waveguide
	{
		std::vector<float> fwd;   // 出口へ向かう波
		std::vector<float> bwd;   // 根元へ戻る波
		int   pos     = 0;
		float lpState = 0.0f;     // 壁で高域が失われる分のフィルタ状態

		// 管へ入れる前に定常成分を落とすフィルタ。
		// 音は「流れの変化」であって流れそのものではない。
		// 定常的に流れているぶんまで波として入れると、管の固有振動
		// (長さで決まる固定の音程)が回転数と無関係に鳴り続ける。
		float hpPrevIn = 0.0f;
		float hpState  = 0.0f;

		void Alloc(int n);
		void Clear();

		// 1サンプル進める。戻り値は出口から外へ抜けた量(＝音になる)
		float Step(float injected, int delay, float endReflect, float damp);
		// いま根元へ戻ってきている波(背圧に効く)
		float ReturnedAtSource(int delay) const;
	};

	// 1気筒ぶんの状態
	struct Cylinder
	{
		float firingDeg = 0.0f;   // この気筒が点火するクランク角(=点火順序)
		float pressure  = EngineSimConst::AtmosPressure;  // 中の圧力(大気を1とした相対値)
		float charge    = 0.0f;   // 吸い込んだ量。燃焼の強さになる

		// 弁が閉じた瞬間を捉えるための前回値。着座音を出すのに使う
		float prevExLift = 0.0f;
		float prevInLift = 0.0f;
	};

	// 圧力比から流量の頭打ち(チョーク)を含めた流れやすさを返す
	static float OrificeFlowFactor(float pUp, float pDown);
	// クランク角からシリンダー容積を返す(上死点で最小、下死点で最大)
	float CylinderVolume(float deg) const;
	// 管の片道の遅延サンプル数
	static int   PipeDelaySamples(float lengthM, float soundSpeed, float subDt);
	// 弁の開き具合(0〜1)。開き始めと閉じ際をなめらかにする
	static float ValveLift(float deg, float openDeg, float closeDeg);

	std::vector<Cylinder> m_cylinders;

	// 管は1本ずつ。気筒ごとに持たせると本数ぶん同時に鳴って濁る
	Waveguide m_exPipe;   // 集合部から出口まで
	Waveguide m_inPipe;   // 吸気の弁から入口まで

	float m_crankDeg = 0.0f;      // 現在のクランク角(0〜720)
	// 集合部の圧力。音としては使わず、弁が押し返される背圧としてだけ持つ
	float m_plenum   = EngineSimConst::AtmosPressure;

	// 出力用。直流成分を抜くフィルタの状態
	float m_dcState  = 0.0f;
	float m_dcPrevIn = 0.0f;

	// エンジン本体の機械音(弁の着座)を鳴らす共鳴器
	float m_mech1 = 0.0f, m_mech2 = 0.0f;

	// アンチエイリアス用のローパス。細かく解いた結果を落とす前に高域を切る
	float m_aaLp1 = 0.0f;
	float m_aaLp2 = 0.0f;

	// 自動レベル合わせ。山の高さを見て、目標へ合わせる倍率を作る
	float m_peakEnv  = 0.0f;
	float m_autoGain = 1.0f;
};
