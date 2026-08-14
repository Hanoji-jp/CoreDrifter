#include "HjEngineSim.h"

using namespace EngineSimConst;

//----------------------------------------------------------
// 気筒数と点火角を設定する。点火角がそのまま点火順序になる。
//----------------------------------------------------------
void HjEngineSim::Configure(int cylinderCount, const float* firingAnglesDeg)
{
	const int n = std::clamp(cylinderCount, 1, 16);
	m_cylinders.assign(n, Cylinder{});

	for (int i = 0; i < n; ++i)
	{
		m_cylinders[i].firingDeg = firingAnglesDeg
			? firingAnglesDeg[i]
			: (CycleDeg / static_cast<float>(n)) * static_cast<float>(i);   // 等間隔
	}
	Reset();
}

void HjEngineSim::Reset()
{
	m_crankDeg = 0.0f;
	m_plenum   = AtmosPressure;
	m_prevPlenum = AtmosPressure;
	m_dcState  = 0.0f;
	m_dcPrevIn = 0.0f;

	for (auto& c : m_cylinders)
	{
		c.pressure = AtmosPressure;
		c.charge   = 0.0f;
	}
}

//----------------------------------------------------------
// クランク角からシリンダー容積を返す。
// 上死点(0度)で最小、下死点(180度)で最大。
// 圧縮比が「最大 ÷ 最小」なので、そこから逆算して形を決める。
//----------------------------------------------------------
float HjEngineSim::CylinderVolume(float deg) const
{
	const float cr = std::max(m_compressionRatio, 1.5f);
	const float vMin = 1.0f;                 // 上死点の容積を1として相対で扱う
	const float vDisp = vMin * (cr - 1.0f);  // 行程容積

	// ピストンは上死点から下死点へ向けて、余弦で往復する
	const float rad = deg * 3.14159265f / 180.0f;
	return vMin + vDisp * 0.5f * (1.0f - cosf(rad));
}

//----------------------------------------------------------
// 弁の開き具合(0〜1)。
// 角度が open〜close の間で開く。境目でいきなり全開にすると
// 段差が音になるので、開き始めと閉じ際をなめらかにする。
//----------------------------------------------------------
float HjEngineSim::ValveLift(float deg, float openDeg, float closeDeg)
{
	// open から close までの長さ(1周を跨ぐ場合がある)
	float span = closeDeg - openDeg;
	while (span <   0.0f) { span += CycleDeg; }
	while (span > CycleDeg) { span -= CycleDeg; }
	if (span < 1.0f) { return 0.0f; }

	// 現在角が open からどれだけ進んだか
	float t = deg - openDeg;
	while (t <   0.0f) { t += CycleDeg; }
	while (t > CycleDeg) { t -= CycleDeg; }
	if (t > span) { return 0.0f; }

	// 山なりに開いて閉じる(カムのリフト曲線に相当)
	const float x = t / span;
	return sinf(x * 3.14159265f);
}

//----------------------------------------------------------
// 1サンプルぶん進める。
//
// ① クランクを回す
// ② 気筒ごとに：容積変化で断熱圧縮/膨張 → 点火 → 弁から出入り
// ③ 排気集合部の圧力を更新し、その変化を音として返す
//----------------------------------------------------------
//----------------------------------------------------------
// 出力1サンプルぶん進める。
// 中では OverSample 倍の細かさで解き、ローパスを通してから落とす。
// 排気の圧力変化は非常に鋭いので、出力レートのまま解くと
// 可聴上限を超える成分が折り返して金属的なノイズになる。
// 回転が上がって波形が鋭くなるほど悪化するため、高回転で特に効く。
//----------------------------------------------------------
float HjEngineSim::Step(float dt, float rpm, float throttle)
{
	const int   n     = std::max(OverSample, 1);
	const float subDt = dt / static_cast<float>(n);

	// 細かい方のレートでのローパス係数
	const float sub_sr = 1.0f / std::max(subDt, 1e-9f);
	const float k = std::clamp(6.2831853f * AntiAliasHz / sub_sr, 0.0f, 1.0f);

	float v = 0.0f;
	for (int i = 0; i < n; ++i)
	{
		v = StepInner(subDt, rpm, throttle);

		// 一次を2段。落とす前に高域を十分に減らす
		m_aaLp1 += (v - m_aaLp1) * k;
		m_aaLp2 += (m_aaLp1 - m_aaLp2) * k;
	}
	return m_aaLp2;
}

float HjEngineSim::StepInner(float dt, float rpm, float throttle)
{
	if (m_cylinders.empty()) { return 0.0f; }

	//----- ① クランクを回す -----
	// rpm回転/分 → 1秒あたり rpm*6 度
	const float degPerSec = std::max(rpm, 1.0f) * 6.0f;
	const float prevCrank = m_crankDeg;
	m_crankDeg += degPerSec * dt;
	while (m_crankDeg >= CycleDeg) { m_crankDeg -= CycleDeg; }

	// アクセル全閉でも少しは空気が入る(完全に0だとエンジンが止まる)
	const float thr = IdleThrottle + (1.0f - IdleThrottle) * std::clamp(throttle, 0.0f, 1.0f);

	float exhaustIn = 0.0f;   // 今回、排気集合部へ入ってきた量

	for (Cylinder& c : m_cylinders)
	{
		// この気筒のクランク角。点火角のぶんだけずらす＝これが点火順序
		float deg = m_crankDeg - c.firingDeg + m_ignitionDeg;
		while (deg <   0.0f)    { deg += CycleDeg; }
		while (deg >= CycleDeg) { deg -= CycleDeg; }

		float degPrev = prevCrank - c.firingDeg + m_ignitionDeg;
		while (degPrev <   0.0f)    { degPrev += CycleDeg; }
		while (degPrev >= CycleDeg) { degPrev -= CycleDeg; }

		//----- ② 容積変化による断熱圧縮/膨張 -----
		// ピストンが上がれば圧縮されて圧力が上がり、下がれば膨張して下がる。
		//   P・V^γ = 一定
		const float vNow  = CylinderVolume(deg);
		const float vPrev = CylinderVolume(degPrev);
		if (vNow > 1e-4f)
		{
			c.pressure *= powf(vPrev / vNow, Gamma);
		}

		//----- 点火・燃焼 -----
		// 圧縮の終わりで火が入り、決められたクランク角をかけて燃える。
		// 加わる圧力は吸入量に比例するので、アクセルを抜くと爆発が弱くなる。
		{
			const float burn = std::max(m_burnDurationDeg, 1.0f);
			// 上で m_ignitionDeg ぶんずらしてあるので、点火は 360度(圧縮上死点)
			float t = deg - 360.0f;
			while (t <   0.0f)    { t += CycleDeg; }
			while (t >= CycleDeg) { t -= CycleDeg; }
			if (t < burn)
			{
				// 燃焼速度。中盤が最も速い(実際の燃え広がりと同じ形)
				const float rate = sinf(t / burn * 3.14159265f) * (3.14159265f / (2.0f * burn));
				c.pressure += m_combustionHeat * c.charge * rate * (degPerSec * dt);
			}
		}

		//----- 排気弁 -----
		// 開いている間、シリンダーの圧力は集合部の圧力へ向かって抜けていく。
		// 排気弁は下死点の手前で開くので、まだ圧力が高いまま一気に抜ける。
		// これが「ブローダウン」で、エンジン音の鋭さの正体。
		//
		// ※流量係数は「毎秒どれだけ抜けるか」で持つこと。
		//   サンプル単位で解くので、dtを掛けて初めて1サンプルぶんになる。
		//   ここを取り違えると圧力が抜けきる前に次のサイクルが来て、
		//   毎周期積み上がって飽和し、ずっと歪んだ音になる。
		{
			const float lift = ValveLift(deg, m_exhaustOpenDeg, ExhaustCloseDeg);
			if (lift > 0.001f)
			{
				// 圧力差に比例して抜ける。抜けきったら止まるので暴れない
				const float k = std::min(lift * m_exhaustFlowRate * dt, 1.0f);
				const float flow = (c.pressure - m_plenum) * k;

				c.pressure -= flow;
				// シリンダーの体積ぶんの気体が集合部へ移る＝体積比で圧力に効く
				exhaustIn += flow * vNow;
				// 吐き出したぶん、中身が減る
				c.charge = std::max(c.charge * (1.0f - k), 0.0f);
			}
		}

		//----- 吸気弁 -----
		// 開いている間、吸気圧(アクセル開度で決まる)へ近づく。
		// 吸い込んだ量が次の燃焼の強さになる。
		{
			const float lift = ValveLift(deg, IntakeOpenDeg, IntakeCloseDeg);
			if (lift > 0.001f)
			{
				const float intakeP = AtmosPressure * thr;
				const float k = std::min(lift * IntakeFlowRate * dt, 1.0f);
				const float flow = (intakeP - c.pressure) * k;

				c.pressure += flow;
				if (flow > 0.0f) { c.charge += flow * vNow; }
			}
		}

		// 数値が暴れないよう常識的な範囲に収める
		c.pressure = std::clamp(c.pressure, 0.02f, 400.0f);
		c.charge   = std::clamp(c.charge,   0.0f,  6.0f);
	}

	//----- ③ 排気集合部 -----
	// 各気筒から吹き出した気体が集まる部屋。
	// 大気へ抜けていくぶんを引きながら、圧力を更新する。
	const float outflow = (m_plenum - AtmosPressure) * std::min(m_plenumOutflow * dt, 1.0f);
	m_plenum += (exhaustIn / std::max(m_plenumVolume, 0.05f)) - outflow;
	m_plenum = std::clamp(m_plenum, 0.02f, 60.0f);

	// 音は集合部の圧力の変動。
	// ※微分(前サンプルとの差)を取ると物理的には正しいが、
	//   高域を+6dB/octで持ち上げるので耳障りな音になる。
	//   engine-simはこの後にインパルス応答の畳み込みで丸めているが、
	//   こちらにはそれが無いので、圧力そのものを使って後段のフィルタに任せる。
	const float raw = m_plenum - AtmosPressure;

	// 直流成分を抜く。取らないと波形が片側へ寄って音量を食う
	const float out = raw - m_dcPrevIn + DcBlock * m_dcState;
	m_dcPrevIn = raw;
	m_dcState  = out;
	m_prevPlenum = m_plenum;

	return out * m_outputGain;
}
