#include "HjEngineSim.h"

using namespace EngineSimConst;

//==========================================================
// デジタル導波管
//==========================================================
void HjEngineSim::Waveguide::Alloc(int n)
{
	fwd.assign(n, 0.0f);
	bwd.assign(n, 0.0f);
	pos = 0;
	lpState  = 0.0f;
	hpPrevIn = hpState = 0.0f;
}

void HjEngineSim::Waveguide::Clear()
{
	std::fill(fwd.begin(), fwd.end(), 0.0f);
	std::fill(bwd.begin(), bwd.end(), 0.0f);
	pos = 0;
	lpState  = 0.0f;
	hpPrevIn = hpState = 0.0f;
}

//----------------------------------------------------------
// いま根元へ戻ってきている波。弁が押し返される背圧に効く。
//----------------------------------------------------------
float HjEngineSim::Waveguide::ReturnedAtSource(int delay) const
{
	const int n = static_cast<int>(bwd.size());
	if (n <= 2) { return 0.0f; }

	int rd = pos - std::clamp(delay, 1, n - 1);
	while (rd < 0) { rd += n; }
	return bwd[rd];
}

//----------------------------------------------------------
// 管の中を走る圧力波を1サンプル進める。
//
// 波は行き(fwd)と帰り(bwd)の2本が同時に走っている。
//   出口側 … 断面が広がるので「負圧」として反射する(位相が反転)。
//             反射しなかったぶんが外へ抜けて音になる。
//   根元側 … ほぼ全反射。
//
// 両端で反射するから管の中に定在波ができ、管の長さで決まる音程を持つ。
// これが「排気の音程」の正体。
//----------------------------------------------------------
float HjEngineSim::Waveguide::Step(float injected, int delay, float endReflect, float damp)
{
	const int n = static_cast<int>(fwd.size());
	if (n <= 2) { return injected; }
	delay = std::clamp(delay, 1, n - 1);

	// 管へ入るのは「流れの変化」だけ。
	// 定常的に流れているぶんまで波として入れると、管の固有振動が
	// 回転数と無関係にずっと鳴り続ける(＝一定の低い唸り)。
	const float ac = injected - hpPrevIn + PipeDcBlock * hpState;
	hpPrevIn = injected;
	hpState  = ac;

	int rd = pos - delay;
	while (rd < 0) { rd += n; }

	const float atOut = fwd[rd];   // 出口へ届いた行きの波
	const float atSrc = bwd[rd];   // 根元へ戻ってきた帰りの波

	// 出口(開放端)で反転して戻る。
	// 管の壁では高い成分から失われるので、戻る波を鈍らせる。
	// これが無いと減衰せず、金属的に鳴り続ける。
	float back = -endReflect * atOut;
	lpState += (back - lpState) * std::clamp(1.0f - damp, 0.02f, 1.0f);
	back = lpState;

	pos = (pos + 1) % n;
	fwd[pos] = ac + ValveEndReflect * atSrc;
	bwd[pos] = back;

	// 反射しなかったぶんが外へ抜ける＝これが音になる
	return atOut * (1.0f - endReflect);
}

//==========================================================
// 本体
//==========================================================

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

	// 管は1本ずつ。気筒ごとに持たせると本数ぶん同時に鳴って濁る
	m_exPipe.Alloc(RunnerMaxDelay);
	m_inPipe.Alloc(RunnerMaxDelay);

	Reset();
}

void HjEngineSim::Reset()
{
	m_crankDeg = 0.0f;
	m_plenum   = AtmosPressure;
	m_dcState  = 0.0f;
	m_dcPrevIn = 0.0f;
	m_mech1 = m_mech2 = 0.0f;
	m_aaLp1 = m_aaLp2 = 0.0f;
	m_peakEnv  = 0.0f;
	m_autoGain = 1.0f;

	m_exPipe.Clear();
	m_inPipe.Clear();

	for (auto& c : m_cylinders)
	{
		c.pressure = AtmosPressure;
		c.charge   = 0.0f;
		c.prevExLift = 0.0f;
		c.prevInLift = 0.0f;
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

	// ピストンの位置はクランクとコンロッドの幾何で決まる。
	//   x = r(1-cosθ) + l - √(l² - r²sin²θ)
	// 単純な余弦は、コンロッドが無限に長い場合の近似でしかない。
	// 実際は有限長なので上死点側の動きが速く下死点側が遅くなり、
	// 上下で非対称になる。圧力の波形もそれに従って歪む。
	const float rad = deg * 3.14159265f / 180.0f;
	const float L   = std::max(m_rodRatio, 1.2f);   // クランク半径を1とした長さ
	const float s   = sinf(rad);
	const float x   = (1.0f - cosf(rad)) + L - sqrtf(std::max(L * L - s * s, 1e-4f));

	// 下死点(θ=180)で x=2 になるので、行程は 2 で正規化する
	return vMin + vDisp * (x * 0.5f);
}

//----------------------------------------------------------
// 管の片道の遅延サンプル数。
// 排気は高温なので音速が速く、吸気は常温なので遅い。
//----------------------------------------------------------
int HjEngineSim::PipeDelaySamples(float lengthM, float soundSpeed, float subDt)
{
	const float sr = 1.0f / std::max(subDt, 1e-9f);
	const int   d  = static_cast<int>(std::max(lengthM, 0.02f) /
	                                  std::max(soundSpeed, 50.0f) * sr);
	return std::clamp(d, 1, RunnerMaxDelay - 2);
}

//----------------------------------------------------------
// 圧力比から「流れやすさ」を返す(0〜1)。
//
// 圧力差に比例して流すのは、ゆっくりした流れの近似でしかない。
// 実際の気体は音速で頭打ちになる(チョーク)。排気弁が開いた瞬間は
// 圧力差が非常に大きく必ずチョークするので、比例のままだと
// 立ち上がりが鈍り「破裂」ではなく「吹き出し」に聞こえる。
//----------------------------------------------------------
float HjEngineSim::OrificeFlowFactor(float pUp, float pDown)
{
	const float r = std::clamp(pDown / std::max(pUp, 1e-4f), 0.0f, 1.0f);
	if (r <= ChokeRatio) { return 1.0f; }   // 音速で頭打ち

	// 亜音速側。圧力比が1に近づくほど流れなくなる
	return sqrtf(std::max(1.0f - (r - ChokeRatio) / (1.0f - ChokeRatio), 0.0f));
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
// 出力1サンプルぶん進める。
// 中では OverSample 倍の細かさで解き、ローパスを通してから落とす。
// 流れの変化は非常に鋭いので、出力レートのまま解くと可聴上限を
// 超える成分が折り返して金属的なノイズになる。
//----------------------------------------------------------
float HjEngineSim::Step(float dt, float rpm, float throttle)
{
	const int   n     = std::max(OverSample, 1);
	const float subDt = dt / static_cast<float>(n);

	// 細かい方のレートでのローパス係数
	const float sub_sr = 1.0f / std::max(subDt, 1e-9f);
	const float k = std::clamp(6.2831853f * AntiAliasHz / sub_sr, 0.0f, 1.0f);

	for (int i = 0; i < n; ++i)
	{
		const float v = StepInner(subDt, rpm, throttle);

		// 一次を2段。落とす前に高域を十分に減らす
		m_aaLp1 += (v - m_aaLp1) * k;
		m_aaLp2 += (m_aaLp1 - m_aaLp2) * k;
	}

	//----- 自動レベル合わせ -----
	// 出力の大きさは気筒数や燃焼の強さなど、ほぼ全部のパラメータで
	// 何倍にも動く。合っていないと出力段のソフトクリップが常時掛かり、
	// 波形が矩形波になってパルス感が消え「ずっと鳴っている」音になる。
	//
	// ※実効値ではなく「山の高さ」で合わせること。
	//   実効値で合わせると、パルスの間の静けさが長いほど倍率が上がり、
	//   山だけが突き抜けて逆にうるさくなる。
	{
		const float decay = std::clamp(dt / std::max(PeakHoldSeconds, 0.01f), 0.0f, 1.0f);
		m_peakEnv = std::max(fabsf(m_aaLp2), m_peakEnv * (1.0f - decay));

		const float want = std::clamp(AutoLevelPeak / std::max(m_peakEnv, 1e-6f),
		                              0.0f, AutoGainMax);
		const float coef = std::clamp(dt / std::max(AutoLevelSeconds, 0.05f), 0.0f, 1.0f);
		m_autoGain += (want - m_autoGain) * coef;
	}

	return m_aaLp2 * m_autoGain * m_outputGain;
}

//----------------------------------------------------------
// 内部の1ステップ。
//
// ① クランクを回す
// ② 気筒ごとに：断熱圧縮/膨張 → 点火 → 排気弁 → 吸気弁 → 弁の着座
// ③ 弁を通った流れを管へ通し、排気・吸気・本体を混ぜて返す
//----------------------------------------------------------
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

	// 管の片道の遅延。排気は高温で音速が速く、吸気は常温
	const int exDelay = PipeDelaySamples(m_runnerLength, HotSoundSpeed,  dt);
	const int inDelay = PipeDelaySamples(m_intakeLength, ColdSoundSpeed, dt);

	// 管から戻ってきている波。弁が押し返される背圧になる。
	// 戻った負圧が排気弁の開いている間に着けば燃焼ガスを吸い出す(掃気)。
	// 当たり外れが回転数で変わるのが、実車で「ある回転域から急に
	// 抜けが良くなる」現象そのもの。
	const float exReturn = m_exPipe.ReturnedAtSource(exDelay);

	// 弁が座面へ当たる強さ。回転が上がるほど着座速度が上がる
	const float seatStrength = std::min(rpm / MechRpmRef, 3.0f);

	float exFlowSum = 0.0f;   // 排気弁を通った流れの合計(＝音のもと)
	float inFlowSum = 0.0f;   // 吸気弁を通った流れの合計
	float mechHit   = 0.0f;   // 弁の着座による衝撃

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
		// 弁を通った流れがそのまま音のもとになる。
		// 弁が閉じていれば流れはゼロ＝静かになるので、パルスの間に
		// きちんと休符ができる。集合部の圧力を音に使うと、集合部が
		// 積分器なので休符まで戻らず、一本調子の唸りになってしまう。
		const float exLift = ValveLift(deg, m_exhaustOpenDeg, ExhaustCloseDeg);
		{
			// 弁が見ている背圧。集合部の圧力＋管から戻ってきた波
			const float valveP = std::max(m_plenum + exReturn, 0.02f);

			if (exLift > 0.001f)
			{
				const float k = std::min(exLift * m_exhaustFlowRate * dt, 1.0f);

				// 上流・下流を圧力の高い方/低い方で決める(吹き返しもありうる)
				const float pUp   = std::max(c.pressure, valveP);
				const float pDown = std::min(c.pressure, valveP);
				const float sign  = (c.pressure >= valveP) ? 1.0f : -1.0f;

				// チョークを含む流量。上流の圧力に比例するので、
				// 圧力が抜けきるまで流れが落ちず、鋭さが保たれる。
				float mag = pUp * OrificeFlowFactor(pUp, pDown) * k;
				// 釣り合いを飛び越えないように上限を付ける(発散防止)
				mag = std::min(mag, fabsf(c.pressure - valveP));

				const float flow = sign * mag;
				c.pressure -= flow;
				c.charge = std::max(c.charge * (1.0f - k), 0.0f);

				exFlowSum += flow * vNow;
			}
		}

		//----- 吸気弁 -----
		// 吸い込む側にも音がある。排気だけで作ると丸ごと抜け落ちる。
		const float inLift = ValveLift(deg, IntakeOpenDeg, IntakeCloseDeg);
		{
			if (inLift > 0.001f)
			{
				const float intakeP = AtmosPressure * thr;
				const float k = std::min(inLift * IntakeFlowRate * dt, 1.0f);
				const float flow = (intakeP - c.pressure) * k;

				c.pressure += flow;
				if (flow > 0.0f) { c.charge += flow * vNow; }

				// 吸い込んだぶん、管の側は引かれる＝負圧の波になる
				inFlowSum -= flow * vNow;
			}
		}

		//----- 弁の着座(エンジン本体の機械音) -----
		// カムに押された弁が座面へ戻る瞬間に当たる音。クランク角に同期して
		// 規則正しく出て、回転が上がるほど着座速度が上がり強くなる。
		if (c.prevExLift > 0.001f && exLift <= 0.001f) { mechHit += seatStrength; }
		if (c.prevInLift > 0.001f && inLift <= 0.001f) { mechHit += seatStrength * 0.7f; }
		c.prevExLift = exLift;
		c.prevInLift = inLift;

		// 数値が暴れないよう常識的な範囲に収める
		c.pressure = std::clamp(c.pressure, 0.02f, 400.0f);
		c.charge   = std::clamp(c.charge,   0.0f,  6.0f);
	}

	//----- ③ 集合部（背圧としてだけ持つ。音には使わない）-----
	// 吹き出した気体が溜まって弁を押し返す。溜まりすぎないよう大気へ逃がす。
	{
		const float out = (m_plenum - AtmosPressure) * std::min(m_plenumOutflow * dt, 1.0f);
		m_plenum += (exFlowSum / std::max(m_plenumVolume, 0.05f)) - out;
		m_plenum = std::clamp(m_plenum, 0.02f, 60.0f);
	}

	//----- 管を通す -----
	// 気筒ごとではなく1本ずつ。耳に届くのは集合部から先の1本と、
	// 吸気の入口までの1本で、響きはそこで決まる。
	const float exhaust = m_exPipe.Step(exFlowSum, exDelay, m_runnerReflect, m_runnerDamp);
	const float intake  = m_inPipe.Step(inFlowSum, inDelay, m_intakeReflect, m_runnerDamp);

	//----- 本体の機械音 -----
	// 当たった衝撃を共鳴器へ入れて、金属が鳴く音にする
	{
		const float sr = 1.0f / std::max(dt, 1e-9f);
		const float w  = 6.2831853f * std::clamp(m_mechHz, 200.0f, sr * 0.45f) / sr;
		const float r  = std::clamp(1.0f - w / (2.0f * MechQ), 0.0f, 0.9999f);

		const float y = mechHit + 2.0f * r * cosf(w) * m_mech1 - r * r * m_mech2;
		m_mech2 = m_mech1;
		m_mech1 = y;
	}

	// 3つの出口を混ぜる。実車と同じく、排気・吸気・本体は別の音源
	const float raw = exhaust
	                + intake  * m_intakeLevel
	                + m_mech1 * m_mechLevel * 0.02f;

	// 直流成分を抜く。取らないと波形が片側へ寄って音量を食う
	const float out = raw - m_dcPrevIn + DcBlock * m_dcState;
	m_dcPrevIn = raw;
	m_dcState  = out;

	return out;
}
