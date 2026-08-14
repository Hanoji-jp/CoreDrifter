#pragma once

#include "../Const/EngineSimConst.h"

//==========================================================
// HjEngineSim
//   エンジンの中の圧力を解いて、排気の吹き出しを音として出す。
//
//   倍音を並べる合成と根本的に違い、波形は「その瞬間の圧力」から
//   出てくるので、回転数やアクセル開度で音の形そのものが変わる。
//   合成では作れない以下が自然に出る：
//     ・ブローダウン(排気弁が開いた瞬間の鋭い衝撃)と
//       排気行程の押し出し(緩やか)が別の音として現れる
//     ・高回転では吹き出す時間が足りず、パルスの形が変わる
//     ・アクセルを抜くと吸入量が減り、爆発が弱く音が痩せる
//     ・点火順序はカム角から自然に出る(点火表を書かなくてよい)
//
//   クランク・ピストンの剛体シミュレーションは持たない。
//   車の駆動系(CarBase)から回転数を貰えるので、トルクを解く必要がない。
//
//   使い方:
//     SetCylinderCount(n) / SetFiringAngles(...)
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
	// 排気の圧力変化は鋭く、出力レートのまま解くと折り返し雑音になるため。
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
	float m_outputGain       = EngineSimConst::OutputGain;

	float GetCrankDeg() const { return m_crankDeg; }
	int   GetCylinderCount() const { return static_cast<int>(m_cylinders.size()); }

private:
	// 1気筒ぶんの状態
	struct Cylinder
	{
		float firingDeg = 0.0f;   // この気筒が点火するクランク角(=点火順序)
		float pressure  = EngineSimConst::AtmosPressure;  // 中の圧力(大気を1とした相対値)
		float charge    = 0.0f;   // 吸い込んだ量。燃焼の強さになる
	};

	// クランク角からシリンダー容積を返す(上死点で最小、下死点で最大)
	float CylinderVolume(float deg) const;
	// 弁の開き具合(0〜1)。開き始めと閉じ際をなめらかにする
	static float ValveLift(float deg, float openDeg, float closeDeg);

	std::vector<Cylinder> m_cylinders;

	float m_crankDeg = 0.0f;      // 現在のクランク角(0〜720)
	float m_plenum   = EngineSimConst::AtmosPressure;  // 排気集合部の圧力

	// 出力用。圧力そのものではなく変化が音なので、前回との差を取る
	float m_prevPlenum = EngineSimConst::AtmosPressure;
	float m_dcState    = 0.0f;    // 直流成分を抜くフィルタの状態
	float m_dcPrevIn   = 0.0f;

	// アンチエイリアス用のローパス。細かく解いた結果を落とす前に高域を切る。
	// 一次を2段重ねて、折り返す帯域を十分に減らす。
	float m_aaLp1 = 0.0f;
	float m_aaLp2 = 0.0f;
};
