#include "HjEngineAudio.h"
#include "HjAudioSpace.h"   // 反射と残響をまとめて掛ける

using namespace EngineAudioConst;

float HjEngineAudio::Rand01()
{
	// xorshift。毎サンプル呼ぶので軽さ優先
	m_rng ^= m_rng << 13;
	m_rng ^= m_rng >> 17;
	m_rng ^= m_rng << 5;
	return static_cast<float>(m_rng & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

//----------------------------------------------------------
// 選んだエンジンの設定を一式読み込む。
// 気筒配置(倍音の配分)だけでなく、マフラー・ターボ・吸気の鳴きまで
// まとめて切り替わるので、別のクルマの音になる。
//----------------------------------------------------------
void HjEngineAudio::ApplyEnginePreset()
{
	EnginePreset p = PresetRb26Dett;
	switch (m_engine)
	{
	case Engine::Sr20Det: p = PresetSr20Det;  break;
	case Engine::Jz2Gte:  p = Preset2JzGte;   break;
	case Engine::V8Cross: p = PresetV8Cross;  break;
	case Engine::Rb26Dett:
	default:              p = PresetRb26Dett; break;
	}

	m_firingOrder = static_cast<float>(p.firingOrder);
	m_halfLevel   = p.halfLevel;
	m_otherLevel  = p.otherLevel;
	m_rolloff     = p.rolloff;

	m_cutoffBase    = p.cutoffBase;
	m_cutoffRpmGain = p.cutoffRpmGain;
	m_cutoffThrGain = p.cutoffThrGain;
	m_resonance     = p.resonance;
	m_noiseLevel    = p.noiseLevel;

	m_turboLevel    = p.turboLevel;
	m_formantHz     = p.formantHz;
	m_formantAmount = p.formantAmount;

	// エンジンの質感。ここが共通だとフィルタの色が違うだけの音になり、
	// 暗い設定のものが「安っぽいモーター音」に聞こえてしまう。
	m_pulseWidthMs  = p.pulseWidthMs;
	m_cylImbalance  = p.cylImbalance;

	// 吸気の性格。直6とV6は点火倍音が同じ(k=6)なので、
	// ここを変えないと同じ気筒数のエンジンが区別できない。
	m_formantRpmGain = p.formantRpmGain;
	m_noiseTone      = p.noiseTone;
}

//----------------------------------------------------------
// 気筒配置から点火角を作り、シミュレーションへ渡す。
// 4ストロークはクランク2回転(720度)で全気筒が1回ずつ点火する。
// V8クロスプレーンだけは等間隔ではなく、左右のバンクが偏っている。
// これが「ドロドロ」の正体なので、そこだけ実際の角度を書く。
//----------------------------------------------------------
void HjEngineAudio::ConfigureSim()
{
	const int n = std::clamp(static_cast<int>(m_firingOrder), 1, 12);

	float angles[12] = {};
	if (m_engine == Engine::V8Cross && n == 8)
	{
		const float cross[8] = { 0.0f, 180.0f, 270.0f, 450.0f, 90.0f, 360.0f, 540.0f, 630.0f };
		for (int i = 0; i < 8; ++i) { angles[i] = cross[i]; }
	}
	else
	{
		// 等間隔。直4なら180度ごと、直6なら120度ごと
		for (int i = 0; i < n; ++i)
		{
			angles[i] = EngineSimConst::CycleDeg / static_cast<float>(n) * static_cast<float>(i);
		}
	}

	m_sim.Configure(n, angles);
}

void HjEngineAudio::Init()
{
	if (m_voice) { return; }

	IXAudio2* xa = KdAudioManager::Instance().GetXAudio2();
	if (!xa) { return; }

	ApplyEnginePreset();
	ConfigureSim();
	m_sampler.Init();
	m_exhaust.Init(static_cast<float>(SampleRate));

	// 素材が1本も無ければ合成へ落とす(無音にしない)
	if (m_source == Source::Sample && !m_sampler.HasAnyLayer()) { m_source = Source::Synth; }

	// 揺らぎだけ倍音ごとにばらす。位相は共通のものを使うので初期化しない
	// (位相をばらすと波形が均されてオルガンのような音になる)
	for (int k = 0; k < MaxHarmonics; ++k)
	{
		m_harm[k].wobVal = Rand01() * 2.0f - 1.0f;
		m_harm[k].amp    = 0.0f;

		// 気筒ごとの燃焼の強さ。1.0を中心にばらつかせる。
		// 起動のたびに変わると音が安定しないので、ここで一度だけ決める。
		m_cylGain[k] = 0.75f + 0.5f * Rand01();
	}

	WAVEFORMATEX wfx{};
	wfx.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;   // float32で直接書く(変換不要)
	wfx.nChannels       = ChannelNum;
	wfx.nSamplesPerSec  = SampleRate;
	wfx.wBitsPerSample  = 32;
	wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize          = 0;

	// 遠いほど高音を落とすため、ボイスにフィルタを持たせる
	if (FAILED(xa->CreateSourceVoice(&m_voice, &wfx, XAUDIO2_VOICE_USEFILTER))) { m_voice = nullptr; return; }
	m_xa = xa;   // 以後、このエンジンが生きている間だけボイスを触る

	// 送信用バッファを使い回す。XAudio2は再生し終わるまで中身を参照するので、
	// 送ったメモリは解放せず、一周してから上書きする。
	m_blocks.assign(QueuedBlocks, std::vector<float>(BlockSamples, 0.0f));

	// 完全にドライな音は現実では聞くことがないため、どれだけ作り込んでも
	// 作り物に聞こえる。反射と残響を持つ空間へ通す。
	HjAudioSpace::Instance().Init();
	HjAudioSpace::Instance().RouteVoice(m_voice, HjAudioSpace::Bus::Engine);

	m_voice->Start(0);
	SubmitPending();
}

void HjEngineAudio::Apply3D(const Math::Vector3& worldPos)
{
	HjAudioSpace::Instance().ApplySource(m_voice, worldPos);
}

//----------------------------------------------------------
// ボイスがまだ生きているか。
// XAudio2エンジンが破棄されると、ぶら下がっているボイスも解放される。
// こちらは生ポインタを持っているだけなので、その後に触ると落ちる。
//----------------------------------------------------------
bool HjEngineAudio::IsVoiceAlive() const
{
	return m_xa && (KdAudioManager::Instance().GetXAudio2() == m_xa);
}

void HjEngineAudio::Stop()
{
	if (!m_voice) { return; }

	// エンジンが先に消えていれば、ボイスは既に解放済み。
	// ここで DestroyVoice を呼ぶと解放済みメモリへのアクセスになる。
	if (!IsVoiceAlive())
	{
		m_voice = nullptr;
		m_xa    = nullptr;
		return;
	}

	m_voice->Stop(0);
	m_voice->FlushSourceBuffers();
	m_voice->DestroyVoice();
	m_voice = nullptr;
	m_xa    = nullptr;
}

//----------------------------------------------------------
// 1ブロックぶんの波形を書き出す。
//
// ① 各オーダー(倍音)の目標の強さを決める
// ② 位相を進めながら全部足す
// ③ 吸排気の乱流としてノイズを足す
// ④ マフラー(レゾナンス付きローパス)を通す
//
// 位相はブロックを跨いで持ち越すので、回転が変わっても音が途切れない。
//----------------------------------------------------------
//----------------------------------------------------------
// 出力段：マフラー・吸気の共鳴・ターボを通して1サンプル書き出す。
// 音源(物理シミュレーション / 倍音合成)のどちらから来ても共通の処理。
//----------------------------------------------------------
void HjEngineAudio::RenderTail(float& dst, float v, float svfF, float svfQ,
                               float fmtF, float fmtQ, float spoolTarget,
                               float spoolStep, float rpmN, float cutScale)
{
	const float sr = static_cast<float>(SampleRate);

		//----- ④ マフラーを通す -----
		// 状態変数フィルタ(レゾナンス付きローパス)。踏むとカットオフが上がって開く。
		// さらに点火の直後だけカットオフを開く＝1発の中で明るさが落ちる。
		// これが「ブローダウンの鋭い破裂 → 鈍い押し出し」で、パンチの正体。
		// フィルタ係数はカットオフにほぼ比例するので、倍率を掛けるだけでよい。
		// 上げすぎるとフィルタが発散するので上限を設ける。
		svfF = std::min(svfF * cutScale, 1.35f);

		const float high = v - m_svfLow - svfQ * m_svfBand;
		m_svfBand += svfF * high;
		m_svfLow  += svfF * m_svfBand;
		float body = m_svfLow;

	// 排気系を通す。共鳴を1個だけにすると、倍音の豊かな音に鋭い共鳴が
	// 1つ立つ構造になり、これは人間の母音とまったく同じ。
	// 何十個ものモードを並列に置くことで、どれか1つが突出せず
	// 「管の中で音が転げ回る」感じになる。
	body = m_exhaust.Process(body);

		//----- ⑤ 吸気の共鳴を足す -----
		// 同じ音の狭い帯域だけを取り出して重ねる＝管が鳴いている感じ。
		// RBのような「ヒョー」という金属的な音はこれで出る。
		if (m_formantAmount > 0.001f)
		{
			const float fh = v - m_fmtLow - fmtQ * m_fmtBand;
			m_fmtBand += fmtF * fh;
			m_fmtLow  += fmtF * m_fmtBand;

			// バンドパスは共鳴周波数の付近をQ倍に増幅する。
			// 鋭さ(Q)を上げるほど音量まで一緒に上がってしまい、
			// 回転が上がって倍音が共鳴点に差し掛かった瞬間に爆音になる。
			// Qで割り戻して、鋭さと音量を切り離す。
			body += m_fmtBand * m_formantAmount * fmtQ;
		}

		//----- ⑥ ターボ -----
		if (m_turboLevel > 0.001f)
		{
			m_spool += (spoolTarget - m_spool) * spoolStep;

			// スプール音：タービンの回転が上がるほど高くなる
			const float whineHz = m_whineBase + m_whineGain * m_spool;
			m_whinePhase += 6.2831853f * whineHz / sr;
			if (m_whinePhase > 6.2831853f) { m_whinePhase -= 6.2831853f; }

			// 二乗で効かせる＝低ブーストではほとんど鳴らない。
			// さらに高回転では抑える。純度の高い高音は電子音として耳につく。
			const float whineAmp = m_spool * m_spool * m_turboLevel * (1.0f - 0.45f * rpmN);
			// 純粋な正弦は完全に電子音なので、ノイズを混ぜて空気の音に寄せる
			const float whineNoise = m_noiseLp * 0.5f;
			body += (sinf(m_whinePhase) * 0.7f + whineNoise) * whineAmp;

			// コンプレッサーサージ：逃がし弁が無い/閉じている時、
			// 過給空気がコンプレッサーを逆流して羽根を叩く「ストゥトゥトゥ」。
			// 圧力が抜けるにつれて逆流の周期が延びるので、だんだん遅くなる。
			if (m_surgeEnv > 0.0001f && m_surgeLevel > 0.001f)
			{
				// 残りの圧力が高いほど速く震える＝減衰につれて遅くなる
				const float rate = SurgeRateHz * (1.0f - SurgeRateFall * (1.0f - m_surgeEnv));
				m_surgePhase += rate / sr;
				if (m_surgePhase >= 1.0f) { m_surgePhase -= 1.0f; }

				// 1周ごとに羽根を叩く。立ち上がりが鋭く、すぐ減衰する形
				const float beat = powf(1.0f - m_surgePhase, 3.0f);

				const float rawS = Rand01() * 2.0f - 1.0f;
				m_surgeLp += (rawS - m_surgeLp) * 0.55f;

				// 共鳴に通して「叩いている」音色にする(素のノイズだと風の音になる)
				const float r = expf(-3.14159265f * (SurgeToneHz / SurgeToneQ) / sr);
				const float w = 6.2831853f * SurgeToneHz / sr;
				const float y = 2.0f * r * cosf(w) * m_surgeR1 - r * r * m_surgeR2
				              + (1.0f - r) * m_surgeLp * beat;
				m_surgeR2 = m_surgeR1;
				m_surgeR1 = y;

				body += y * m_surgeEnv * m_surgeLevel;
				m_surgeEnv -= m_surgeEnv * std::min(SurgeDecay / sr, 1.0f);
			}

			// ブローオフ：閉じた瞬間に溜まった空気が抜ける「プシュー」
			if (m_bovEnv > 0.0001f)
			{
				const float raw2 = Rand01() * 2.0f - 1.0f;
				m_bovLp += (raw2 - m_bovLp) * 0.45f;
				body += m_bovLp * m_bovEnv * m_bovLevel;
				m_bovEnv -= m_bovEnv * std::min(BovDecay / sr, 1.0f);
			}
	}

	// 低域を整理する。排気管の最低モードは非常に低く(2.4mで約36Hz)しかも一番強い。
	// 理屈上は正しいが、実物の超低域は強く減衰するうえ、
	// スピーカーでは「ボー」という濁りにしかならない。
	{
		const float k = std::clamp(6.2831853f * m_highPassHz / sr, 0.0f, 1.0f);
		m_hpState += (body - m_hpState) * k;   // 低い成分だけを取り出して
		body -= m_hpState;                     // 引く＝ハイパス
	}

	// ソフトクリップ。上限で切らず、大きいほど緩やかに寝かせる
	const float o = body * m_master;
	dst = ClipLevel * (o / (1.0f + fabsf(o)));
	m_peakOut = std::max(m_peakOut, fabsf(dst));
}

void HjEngineAudio::RenderBlock(std::vector<float>& out)
{
	const float sr  = static_cast<float>(SampleRate);
	const float rpm = std::max(m_rpm, 1.0f);

	// 4ストロークはクランク2回転で1サイクル。その周波数が倍音列の土台になる
	const float f0 = rpm / 120.0f;

	const float rpmN   = std::clamp(rpm / std::max(m_maxRpm, 1.0f), 0.0f, 1.2f);
	const float thr    = std::clamp(m_throttle, 0.0f, 1.0f);
	const float volAll = (m_offVolume + (1.0f - m_offVolume) * thr) * (1.0f + m_rpmVolGain * rpmN);

	//----- ① 各オーダーの目標の強さ -----
	// アクセルを抜くと高次が失われて音が丸くなる＝踏むと開く
	const float rolloff = m_rolloff + m_offRolloffAdd * (1.0f - thr);
	const int   firing  = std::max(static_cast<int>(m_firingOrder), 1);
	const int   half    = std::max(firing / 2, 1);

	// 折り返し雑音を避けるため、可聴上限を超える倍音は鳴らさない
	const float nyquist = sr * 0.45f;
	int useCount = MaxHarmonics;
	if (f0 > 1.0f) { useCount = std::min(MaxHarmonics, static_cast<int>(nyquist / f0)); }
	useCount = std::max(useCount, 1);

	float target[MaxHarmonics] = {};
	float sumAmp = 0.0f;
	for (int i = 0; i < useCount; ++i)
	{
		const int k = i + 1;

		// 点火倍音を芯に、その間のオーダーで質感を作る
		float w;
		if      (k % firing == 0) { w = m_firingBoost; }
		else if (k % half   == 0) { w = m_halfLevel; }
		else                      { w = m_otherLevel; }

		// 排気パルスの長さから決まる、絶対的な周波数のエンベロープ。
		// 排気弁が開いた瞬間の吹き出しは、回転数に関係なく数ミリ秒で終わる。
		// つまりスペクトルの形は「何Hzか」で決まっていて、
		// 回転が上がっても上へ伸びていくわけではない。
		// 次数だけで減らすと、高回転でスペクトルが青天井に伸びて
		// 金切り声のようになる(高回転が汚くなる主因)。
		const float fk    = f0 * static_cast<float>(k);
		const float fCut  = 1000.0f / std::max(m_pulseWidthMs, 0.2f);   // パルス長→帯域
		const float shape = 1.0f / (1.0f + (fk / fCut) * (fk / fCut));

		const float a = w * powf(1.0f / static_cast<float>(k), rolloff) * shape;
		target[i] = a;
		sumAmp += a;
	}
	// 合計で正規化。倍音の数や配分を変えても音量が変わらないようにする
	const float norm = (sumAmp > 1e-4f) ? (1.0f / sumAmp) : 0.0f;

	//----- ④ マフラーのカットオフ。踏むと開く -----
	float cutoff = m_cutoffBase + m_cutoffRpmGain * rpmN + m_cutoffThrGain * thr;
	cutoff = std::clamp(cutoff, 40.0f, sr * 0.45f);
	const float svfF = 2.0f * sinf(3.14159265f * cutoff / sr);
	const float svfQ = 1.0f / std::max(m_resonance, 0.5f);

	// アクセルを離しても、ノイズは減らすのではなく増やす。
	// 実物のオーバーラン(エンジンブレーキ)は荒くパチパチ鳴る。
	// 音量・倍音・ノイズを一緒に減らすと、純粋な低い倍音だけが剥き出しで残り、
	// それが「アクセルを離した瞬間のシンセ感」の正体になる。
	const float overrun = (1.0f - thr) * std::clamp((rpmN - OverrunMinRpmN) / 0.4f, 0.0f, 1.0f);
	const float noiseAmt = m_noiseLevel * (1.0f + NoiseRpmGain * rpmN)
	                     * (0.55f + 0.45f * thr) * (1.0f + OverrunNoiseGain * overrun);
	// 高回転ほど回転が安定するので揺らぎを減らす。
	// 深いまま高回転へ行くと、強い倍音が何十本も独立に振れて汚くなる。
	const float wobScale = 1.0f - 0.6f * rpmN;
	// 回転が上がるほど、1発ごとの起伏を薄めて滑らかにする。
	// 実機も低回転では1発1発が聞き分けられ、回転が上がると融合する。
	// 深いまま回すと、どの回転域でも「ドッドッドッ」が残って
	// 農機のような音になる(これが「トラクターっぽさ」の正体)。
	const float blurScale = 1.0f - PulseBlurRpm * std::clamp(rpmN, 0.0f, 1.0f);
	// 排気の乱流は垂れ流しではなく、点火のたびに吹き出す
	const float fireHz  = f0 * m_firingOrder;   // 1秒あたりの点火回数
	const float fireInc = fireHz / sr;

	// 吸気の共鳴。狭い帯域だけを強調して金属的な鳴きを出す
	float fmtHz = m_formantHz + m_formantRpmGain * rpmN;
	fmtHz = std::clamp(fmtHz, 80.0f, sr * 0.45f);
	const float fmtF = 2.0f * sinf(3.14159265f * fmtHz / sr);
	const float fmtQ = 1.0f / std::max(m_formantQ, 0.5f);

	// ターボ。タービンは排気で回るので、回転とアクセルに遅れて追従する。
	// 目標は「回転 × アクセル」＝排気の量。
	const float spoolTarget = rpmN * thr;
	const float spoolRate   = (spoolTarget > m_spool) ? m_spoolUp : m_spoolDown;
	const float spoolStep   = std::min(spoolRate / sr, 1.0f);

	// 素材モードでは音はKdAudio側(サンプラー)が鳴らしているので、
	// こちらのボイスからは何も出さない。ターボは別トラックとして残す。
	const bool sampleMode = (m_source == Source::Sample);

	for (int n = 0; n < static_cast<int>(out.size()); ++n)
	{
		if (sampleMode)
		{
			// エンジン本体は素材に任せ、ターボとブローオフだけ重ねる
			RenderTail(out[n], 0.0f, svfF, svfQ, fmtF, fmtQ, spoolTarget, spoolStep, rpmN, 1.0f);
			continue;
		}

		//----- ②-1 回転のゆらぎ -----
		// 実機は燃焼のばらつきとクランクのねじれで、回転が常に微妙に揺れている。
		// 周波数が完全に一定だと電子オルガンになる。
		// 全倍音がまとめて同じ比率で揺れるので、これだけで生気が出る。
		m_rpmJitter += ((Rand01() * 2.0f - 1.0f) - m_rpmJitter) * RpmJitterSpeed;
		const float f0Now = f0 * (1.0f + m_rpmJitter * RpmJitterAmount * 8.0f);

		//----- ②-2 音源：物理シミュレーション or 倍音合成 -----
		if (m_source == Source::Physics)
		{
			// シリンダー内の圧力を解き、排気の吹き出しをそのまま音にする。
			// 波形が「その瞬間の圧力」から出てくるので、回転やアクセルで
			// 音の形そのものが変わる。倍音を並べる合成では作れない部分。
			float v = m_sim.Step(1.0f / sr, rpm * (1.0f + m_rpmJitter * RpmJitterAmount * 8.0f), thr);

			// 乱流ノイズは物理側に無いので、こちらで足す
			m_firePhase += fireInc;
			if (m_firePhase >= 1.0f) { m_firePhase -= 1.0f; }
			const float pulseP = powf(1.0f - m_firePhase, NoisePulseSharp);
			const float rawP = Rand01() * 2.0f - 1.0f;
			m_noiseLp += (rawP - m_noiseLp) * std::clamp(m_noiseTone, 0.01f, 1.0f);
			v += m_noiseLp * noiseAmt * ((1.0f - NoisePulseDepth) + NoisePulseDepth * pulseP);

			v *= volAll;
			m_peakSource = std::max(m_peakSource, fabsf(v));
			const float atkP = powf(1.0f - m_firePhase, m_pulseAttackSharp);
			v *= 1.0f + m_pulsePunch * atkP;
			RenderTail(out[n], v, svfF, svfQ, fmtF, fmtQ, spoolTarget, spoolStep, rpmN,
			           1.0f + m_pulseAttack * atkP);
			continue;
		}

		//----- 倍音を足し合わせる(合成モード) -----
		// すべての倍音を1つの基準位相から作る＝位相が揃う。
		// これにより波形が「鋭いパルスの繰り返し」になり、爆発の連続らしくなる。
		// 倍音ごとに独立した位相を持たせると、同じ倍音構成でも波形が均されて
		// フルートやオルガンのような音色になってしまう。
		m_masterPhase += 6.2831853f * f0Now / sr;
		if (m_masterPhase > 6.2831853f) { m_masterPhase -= 6.2831853f; }

		float v = 0.0f;
		for (int i = 0; i < useCount; ++i)
		{
			Harmonic& h = m_harm[i];

			// 目標の強さへ滑らかに寄せる(急に変えるとブツッと鳴る)
			h.amp += (target[i] * norm - h.amp) * 0.002f;

			// 不規則な揺らぎ。正弦波で揺らすと、その揺れ自体が規則的なので
			// 「うねる電子音」になってしまう。なました乱数で不規則に揺らす。
			// 深く揺らしすぎると、強い倍音が何十本も独立に振れて
			// コーラスがかかったようなグチャグチャした音になる。
			// 実機も高回転ほど回転が安定するので、回転で揺らぎを減らす。
			h.wobVal += ((Rand01() * 2.0f - 1.0f) - h.wobVal) * HarmonicWobbleSpeed;
			const float wob = 1.0f + m_wobble * h.wobVal * wobScale;

			v += sinf(m_masterPhase * static_cast<float>(i + 1)) * h.amp * wob;
		}

		//----- ②-3 気筒ごとの個体差 -----
		// 実機は気筒ごとに燃焼の強さがわずかに違う。
		// クランクが1サイクル回る間に、強い気筒と弱い気筒が順に来るので、
		// 振幅がサイクルごとに揺れる。これが各次数の周りに細かい成分を生み、
		// 「ざらついた回り方」になる。
		// 全気筒が同じだと純粋な倍音列＝ブザーやノコギリ波と同じ構造になる。
		if (m_cylImbalance > 0.001f)
		{
			// 回転が上がるほど個体差の効きを薄める。
			// 実機も低回転では1発1発が聞き分けられるが、回転が上がると
			// パルスが融合して滑らかになる。深いまま回すと、どの回転域でも
			// 「ドッドッドッ」が残って農機のような音になる。
			// m_masterPhase はクランク1サイクル(720度)の位相そのもの
			const float cyc = m_masterPhase / 6.2831853f;             // 0〜1
			const float pos = cyc * static_cast<float>(firing);       // 0〜気筒数
			const int   ia  = static_cast<int>(pos) % firing;
			const int   ib  = (ia + 1) % firing;
			const float t   = pos - floorf(pos);

			// 隣り合う気筒の間を滑らかに繋ぐ(排気の圧力波は重なり合うため)
			const float g = m_cylGain[ia] + (m_cylGain[ib] - m_cylGain[ia]) * t;
			v *= 1.0f + (g - 1.0f) * m_cylImbalance * blurScale;
		}

		//----- ③ 吸排気の乱流 -----
		// 点火に合わせて脈打たせる。一定のノイズを混ぜるだけだと
		// 「シャー」というだけで生気がない。
		const float firePrev = m_firePhase;
		m_firePhase += fireInc;
		if (m_firePhase >= 1.0f) { m_firePhase -= 1.0f; }

		//----- ③-2 レブリミッター(点火カット) -----
		// 実物のリミッターは点火を飛ばす。飛んだ気筒は燃えないので、
		// 生ガスが排気管へ流れ込み、そこで爆ぜる。あの「ババババッ」の正体。
		// 音量を絞るだけでは「壁に当たっている感じ」が出ない。
		if (m_firePhase < firePrev)   // 点火1回ぶん進んだ
		{
			m_cutPrev = m_cutNow;

			// 飛ばす点火を「均等に散らす」。
			// 確率で毎回独立に決めると、効き0.5でも3連続カットが普通に起き、
			// そこで音が大きく途切れてガサつく。
			// 実物のECUも失火が偏らないよう均等に分散させるので、
			// 繰り上がりで配る(4回に1回→2回に1回→全カット と自然に移る)。
			m_cutCarry += m_revCut;
			if (m_cutCarry >= 1.0f)
			{
				m_cutCarry -= 1.0f;
				m_cutNow = 1.0f;

				// 飛んだ生ガスは排気管へ流れてから熱い排気に触れて着火する。
				// つまり爆ぜるのは「飛んだ瞬間」ではなく少し遅れて。
				// 同時に鳴らすと「消えた瞬間に爆音」になり音量が減らない。
				// 強さは消えた燃焼のぶんに比例させる＝勝手に釣り合う。
				m_popPending      = m_cutDepth * (0.7f + 0.6f * Rand01());
				m_popPendingLevel = m_cutPopLevel;
				m_popDelay        = (0.6f + 0.8f * Rand01()) / std::max(fireHz, 1.0f);
			}
			else
			{
				m_cutNow = 0.0f;
			}

			// オーバーラン：アクセルを離した高回転では、燃え残りが
			// 排気管で不定期に爆ぜる。これが無いと離した瞬間が急に静かで
			// 純粋な音になり、シンセっぽく聞こえる。
			if (Rand01() < overrun * m_overrunCrackle)
			{
				m_popPending      = std::max(m_popPending, 0.45f * (0.6f + 0.8f * Rand01()));
				m_popPendingLevel = m_cutPopLevel;
				m_popDelay        = (0.5f + 1.0f * Rand01()) / std::max(fireHz, 1.0f);
			}
		}

		// アフターファイア：予約した発数を、間隔を空けて順に撃つ
		if (m_afterfireLeft > 0)
		{
			m_afterfireNext -= 1.0f / sr;
			if (m_afterfireNext <= 0.0f)
			{
				--m_afterfireLeft;
				// 1発ごとに大きさと間隔をばらつかせる。等間隔・同じ大きさだと
				// 機械的な連打になって「燃えている」感じが出ない。
				m_popPending      = 0.6f + 0.8f * Rand01();
				m_popPendingLevel = m_afterfireLevel;   // レブの破裂音とは別の音量
				m_popDelay        = 0.0f;
				m_afterfireNext = (AfterfireGapMs * 0.001f) * (0.5f + 1.0f * Rand01());
			}
		}

		// 予約した破裂音を、遅れて鳴らし始める
		if (m_popPending > 0.0f)
		{
			m_popDelay -= 1.0f / sr;
			if (m_popDelay <= 0.0f)
			{
				m_popEnv    = m_popPending;
				m_popLevel  = m_popPendingLevel;
				m_popAttack = 0.0f;
				m_popPending = 0.0f;
			}
		}
		const float pulse = powf(1.0f - m_firePhase, NoisePulseSharp);
		// 脈動の深さも回転で薄める。深いまま高回転へ行くと、
		// 点火のたびに「ドッ」と切れて農機のような音になる。
		const float pulseDepth = NoisePulseDepth * blurScale;
		const float pulseGain = (1.0f - pulseDepth) + pulseDepth * pulse;

		const float raw = Rand01() * 2.0f - 1.0f;
		m_noiseLp += (raw - m_noiseLp) * std::clamp(m_noiseTone, 0.01f, 1.0f);
		v += m_noiseLp * noiseAmt * pulseGain;

		// 点火が飛んだ気筒は燃焼の音を出さない。
		// ただし点火の切れ目でステップ状に切り替えると波形が不連続になり、
		// そこでクリックが鳴る(ブツブツの正体)。
		// 前の点火と今の点火を、点火位相に沿って滑らかに繋ぐ。
		const float cutBlend = m_cutPrev + (m_cutNow - m_cutPrev) * m_firePhase;
		v *= 1.0f - cutBlend * m_cutDepth;

		// 飛んだぶんの生ガスが排気管で爆ぜる。短く鋭い破裂音
		if (m_popEnv > 0.0001f)
		{
			const float rawPop = Rand01() * 2.0f - 1.0f;
			m_popLp += (rawPop - m_popLp) * 0.6f;

			// 立ち上がりも持たせる。一瞬で最大へ跳ねると波形が段差になり、
			// それ自体が「バチッ」というノイズになる。
			m_popAttack += (1.0f - m_popAttack) * std::min(700.0f / sr, 1.0f);

			v += m_popLp * m_popEnv * m_popAttack * m_popLevel;
			m_popEnv -= m_popEnv * std::min(CutPopDecay / sr, 1.0f);
		}

		v *= volAll;

		m_peakSource = std::max(m_peakSource, fabsf(v));

		// 1発の中のアタック。点火直後だけフィルタを開き、音量も少し持ち上げる。
		// 開くだけだと「明るくなる」に留まり、叩かれたような手応えが出ない。
		const float atk = powf(1.0f - m_firePhase, m_pulseAttackSharp);
		v *= 1.0f + m_pulsePunch * atk;

		RenderTail(out[n], v, svfF, svfQ, fmtF, fmtQ, spoolTarget, spoolStep, rpmN,
		           1.0f + m_pulseAttack * atk);
	}
}

//----------------------------------------------------------
// 空いているぶんだけPCMを作ってボイスへ流し込む。
//----------------------------------------------------------
void HjEngineAudio::SubmitPending()
{
	if (!m_voice) { return; }

	XAUDIO2_VOICE_STATE state{};
	m_voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

	while (state.BuffersQueued < static_cast<UINT32>(QueuedBlocks))
	{
		std::vector<float>& block = m_blocks[m_nextBlock];
		m_nextBlock = (m_nextBlock + 1) % static_cast<int>(m_blocks.size());

		RenderBlock(block);

		XAUDIO2_BUFFER buf{};
		buf.AudioBytes = static_cast<UINT32>(block.size() * sizeof(float));
		buf.pAudioData = reinterpret_cast<const BYTE*>(block.data());
		buf.Flags      = 0;
		if (FAILED(m_voice->SubmitSourceBuffer(&buf))) { break; }

		++state.BuffersQueued;
	}
}

void HjEngineAudio::Update(float dt, float rpm, float throttle, float maxRpm, float revCut)
{
	if (!m_voice) { return; }

	const float thrRaw = std::clamp(throttle, 0.0f, 1.0f);

	// ブローオフバルブ：アクセルを閉じた瞬間、行き場を失った過給空気が抜ける。
	// 平滑化前の生の入力で見る(平滑化後だと踏み替えの鋭さが消えて鳴らない)。
	if (m_turboLevel > 0.001f)
	{
		const float drop = m_prevThrottleForBov - thrRaw;
		if (drop > BovThrottleDrop && m_spool > BovMinSpool)
		{
			m_bovEnv = std::min(m_bovEnv + m_spool, 1.0f);
			// サージも同じ条件で立てる。実車は逃がし弁の有無でどちらかが出るので、
			// 音量で好みの配分にできるようにしておく。
			m_surgeEnv   = std::min(m_surgeEnv + m_spool, 1.0f);
			m_surgePhase = 0.0f;
		}
	}

	// アフターファイア：高回転で一気に閉じると、行き場を失った混合気が
	// 排気管へ流れ、熱い排気に触れて一気に燃える。
	// 「パパパンッ」と数発まとめて出るのが特徴で、
	// 常時パラパラ鳴るオーバーランのクラックルとは別物。
	{
		const float drop = m_prevThrottleForBov - thrRaw;
		const float rpmN = rpm / std::max(maxRpm, 1.0f);
		if (drop > AfterfireDrop && rpmN > AfterfireMinRpmN && m_afterfireLeft <= 0)
		{
			m_afterfireLeft = AfterfireShots;
			m_afterfireNext = 0.0f;   // 1発目はすぐ
		}
	}
	m_prevThrottleForBov = thrRaw;
	// 生の値をそのまま使うと踏み替えのたびに音がパチンと切り替わる
	m_throttle += (thrRaw - m_throttle) * std::min(ThrottleSmooth * dt, 1.0f);
	m_rpm      += (rpm - m_rpm) * std::min(RpmSmooth * dt, 1.0f);
	m_maxRpm    = maxRpm;
	m_revCut    = std::clamp(revCut, 0.0f, 1.0f);

	m_sampler.Update(dt, m_rpm, m_throttle);

	SubmitPending();
}

//----------------------------------------------------------
// 音作り用パネル。
// 加算合成は値を変えた瞬間に音が変わるので、作り直しの処理は要らない。
//----------------------------------------------------------
void HjEngineAudio::DrawImGui()
{
	if (!ImGui::CollapsingHeader(U8("エンジン音"))) { return; }

	if (!m_voice)
	{
		ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), U8("音声の初期化に失敗しています"));
		return;
	}

	ImGui::SliderFloat(U8("全体音量"), &m_master, 0.0f, 1.0f);

	// 耳だけで詰めると、飽和しているのか小さすぎるのか判別できない。
	// 音源が1.0を大きく超えていれば出力段で潰れており、
	// 出力が0.2に届かなければ持ち上げる余地がある。
	ImGui::Text(U8("音源 %.2f"), m_peakSource);
	ImGui::ProgressBar(std::clamp(m_peakSource, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
	ImGui::Text(U8("出力 %.2f %s"), m_peakOut,
	            (m_peakOut > 0.88f) ? U8("← 潰れています") : "");
	ImGui::ProgressBar(std::clamp(m_peakOut, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
	// ピークは見終わったら少し落とす(張り付いたままにしない)
	m_peakSource *= 0.90f;
	m_peakOut    *= 0.90f;

	ImGui::SeparatorText(U8("積んでいるエンジン"));
	const char* names[] = { U8("SR20DET (直4ターボ / S15純正)"),
	                        U8("RB26DETT (直6ツインターボ)"),
	                        U8("2JZ-GTE (直6ツインターボ / 太い)"),
	                        U8("V8クロスプレーン (参考)") };
	int engine = static_cast<int>(m_engine);
	if (ImGui::Combo(U8("エンジン"), &engine, names, IM_ARRAYSIZE(names)))
	{
		m_engine = static_cast<Engine>(engine);
		ApplyEnginePreset();
	}

	ImGui::SeparatorText(U8("ターボ"));
	ImGui::SliderFloat(U8("スプール音の音量 0=NA"), &m_turboLevel, 0.0f, 1.2f);
	ImGui::SliderFloat(U8("最低回転の高さ(Hz)"),    &m_whineBase, 200.0f, 4000.0f);
	ImGui::SliderFloat(U8("全開で上がる量(Hz)"),    &m_whineGain, 0.0f, 12000.0f);
	ImGui::SliderFloat(U8("立ち上がりの遅さ"),      &m_spoolUp, 0.3f, 8.0f);
	ImGui::SliderFloat(U8("抜けの速さ"),            &m_spoolDown, 0.3f, 12.0f);
	ImGui::SliderFloat(U8("ブローオフの音量"),      &m_bovLevel, 0.0f, 2.0f);
	// 逃がし弁が無い/閉じている時に出る「ストゥトゥトゥ」。
	// ブローオフとは別物なので、片方だけにも両方にもできる。
	ImGui::SliderFloat(U8("サージ(ストゥトゥトゥ)"), &m_surgeLevel, 0.0f, 2.0f);
	ImGui::Text(U8("過給 %.2f"), m_spool);

	// 排気系の共鳴。ここが「管を通った音」の正体。
	// 共鳴を1個にすると母音になるので、モードを何十個も並べている。
	ImGui::SeparatorText(U8("排気系の共鳴(管・マフラー)"));
	{
		bool rebuild = false;
		rebuild |= ImGui::SliderFloat(U8("管の長さ(m) 長い=低く唸る"), &m_exhaust.m_pipeLength, 0.3f, 6.0f);
		rebuild |= ImGui::SliderFloat(U8("響きの長さ 小=よく響く"),   &m_exhaust.m_bandwidth, 8.0f, 300.0f);
		rebuild |= ImGui::SliderFloat(U8("高音の減衰の速さ"),         &m_exhaust.m_bandwidthRise, 0.0f, 120.0f);
		rebuild |= ImGui::SliderFloat(U8("高いモードの弱さ 大=丸い"), &m_exhaust.m_rolloff, 0.0f, 2.5f);
		rebuild |= ImGui::SliderFloat(U8("モードの不揃いさ 0=楽器的"), &m_exhaust.m_detune, 0.0f, 0.25f);
		if (rebuild) { m_exhaust.Rebuild(); }

		ImGui::SliderFloat(U8("混ぜ具合 0=管を通す前"), &m_exhaust.m_mix, 0.0f, 1.0f);
		ImGui::SliderFloat(U8("出力"),                  &m_exhaust.m_trim, 0.05f, 2.0f);
	}

	ImGui::SeparatorText(U8("吸気の共鳴(金属的な鳴き)"));
	ImGui::SliderFloat(U8("鳴く周波数(Hz)"),     &m_formantHz, 200.0f, 4000.0f);
	// ここが「立ち上がり方」を決める。大きいほど回転と一緒に鳴き上がる。
	// 小さいと大きなプレナムのようにモワッとした立ち上がりになる。
	ImGui::SliderFloat(U8("回転で鳴き上がる量(Hz)"), &m_formantRpmGain, 0.0f, 4000.0f);
	ImGui::SliderFloat(U8("鋭さ 大=細く金属的"), &m_formantQ, 1.0f, 14.0f);
	// ※鋭さ(Q)を上げても音量が上がらないよう内部で割り戻してあるので、
	//   鋭さと混ぜる量を独立に触れる
	ImGui::SliderFloat(U8("混ぜる量"),           &m_formantAmount, 0.0f, 4.0f);

	// 音源の切り替え。物理は「圧力を解いて音を出す」、合成は「倍音を並べる」。
	// 物理側は回転やアクセルで波形の形そのものが変わるのが強み。
	ImGui::SeparatorText(U8("音源"));
	const char* srcNames[] = { U8("録音素材 (CarXと同じ構造)"),
	                           U8("物理シミュレーション"),
	                           U8("倍音合成") };
	int src = static_cast<int>(m_source);
	if (ImGui::Combo(U8("音の作り方"), &src, srcNames, IM_ARRAYSIZE(srcNames)))
	{
		m_source = static_cast<Source>(src);
	}

	if (m_source == Source::Sample)
	{
		m_sampler.DrawImGui();
	}

	if (m_source == Source::Physics)
	{
		ImGui::SeparatorText(U8("エンジンの中身"));
		bool reconf = false;
		reconf |= ImGui::SliderFloat(U8("気筒数"), &m_firingOrder, 1.0f, 12.0f, "%.0f");
		if (reconf) { ConfigureSim(); }

		ImGui::SliderFloat(U8("圧縮比 高=パンチが出る"),   &m_sim.m_compressionRatio, 6.0f, 14.0f);
		ImGui::SliderFloat(U8("燃焼の強さ"),               &m_sim.m_combustionHeat, 2.0f, 80.0f);
		ImGui::SliderFloat(U8("燃焼時間(度) 短=鋭い"),     &m_sim.m_burnDurationDeg, 8.0f, 120.0f);
		ImGui::SliderFloat(U8("排気弁が開く角度"),         &m_sim.m_exhaustOpenDeg, 440.0f, 560.0f);
		ImGui::SliderFloat(U8("排気の抜けの良さ"),         &m_sim.m_exhaustFlowRate, 100.0f, 4000.0f);
		ImGui::SliderFloat(U8("コンロッド比 小=歪む"),     &m_sim.m_rodRatio, 1.4f, 6.0f);

		// 各気筒から集合部までの管。吹き出した波が走って反射して戻る。
		// 戻った負圧が排気弁の開いている間に着くと燃焼ガスを吸い出す。
		// 当たり外れが回転数で変わるので、回すと抜けの良さが変化する。
		ImGui::SeparatorText(U8("排気管"));
		ImGui::TextWrapped(U8("吹き出した波が管を走り、集合部で負圧として反射して戻る。"
		                      "戻りが排気弁の開いている間に着くと抜けが良くなる。"
		                      "当たる回転数が長さで変わる。"));
		ImGui::SliderFloat(U8("排気管の長さ(m)"), &m_sim.m_runnerLength, 0.10f, 3.0f);
		ImGui::SliderFloat(U8("反射の強さ 0=無し"), &m_sim.m_runnerReflect, 0.0f, 0.9f);
		ImGui::SliderFloat(U8("管の減衰 大=丸い"),  &m_sim.m_runnerDamp, 0.0f, 0.9f);

		// 吸気側。排気だけで音を作ると、吸い込む側の音が丸ごと抜け落ちる。
		// NAの日本車で「吸気音」と呼ばれるのがこれ。
		ImGui::SeparatorText(U8("吸気(吸い込む側の音)"));
		ImGui::TextWrapped(U8("吸い込むと管に負圧の波ができ、入口で反射して戻る。"
		                      "排気だけだとこの音が丸ごと無い。"));
		ImGui::SliderFloat(U8("吸気管の長さ(m)"),   &m_sim.m_intakeLength, 0.05f, 1.2f);
		ImGui::SliderFloat(U8("吸気の反射"),        &m_sim.m_intakeReflect, 0.0f, 0.9f);
		ImGui::SliderFloat(U8("吸気の混ぜ具合 0=無し"), &m_sim.m_intakeLevel, 0.0f, 1.5f);

		// エンジン本体。BeamNGも排気音と別レイヤーで持っている。
		// 弁が座面へ当たる音で、回転が上がるほど強くなる。
		ImGui::SeparatorText(U8("本体の機械音(弁の着座)"));
		ImGui::TextWrapped(U8("弁が座面へ当たる音。クランク角に同期して出る。"
		                      "排気だけだとマフラーの音しか鳴らない。"));
		ImGui::SliderFloat(U8("機械音の量 0=無し"), &m_sim.m_mechLevel, 0.0f, 1.5f);
		ImGui::SliderFloat(U8("機械音の高さ(Hz)"),  &m_sim.m_mechHz, 600.0f, 6000.0f);

		ImGui::SeparatorText(U8("集合部"));
		ImGui::SliderFloat(U8("集合部の大きさ 大=こもる"), &m_sim.m_plenumVolume, 0.2f, 8.0f);
		ImGui::SliderFloat(U8("大気へ抜ける速さ"),         &m_sim.m_plenumOutflow, 0.3f, 12.0f);
		// 自動レベル合わせの後に掛ける微調整。基準は1.0
		ImGui::SliderFloat(U8("出力の微調整"), &m_sim.m_outputGain, 0.0f, 2.0f);
		ImGui::Text(U8("自動で %.1f 倍にしています"), m_sim.GetAutoGain());
		ImGui::Text(U8("クランク角 %.0f度"), m_sim.GetCrankDeg());
	}

	ImGui::SeparatorText(U8("倍音の配分(合成モード)"));
	ImGui::SliderFloat(U8("点火倍音の次数(=気筒数)"), &m_firingOrder, 2.0f, 12.0f, "%.0f");
	ImGui::SliderFloat(U8("うねり(半分オーダー)"),   &m_halfLevel, 0.0f, 1.2f);
	ImGui::SliderFloat(U8("ざらつき(その他)"),       &m_otherLevel, 0.0f, 1.0f);
	ImGui::SliderFloat(U8("高次の落ち方 大=丸い"),   &m_rolloff, 0.3f, 3.0f);
	// 排気パルスの長さ＝スペクトルの形を決める絶対的な周波数。
	// 短いほど高い帯域まで伸び、鋭く硬い音になる。
	// これが無いと高回転でスペクトルが青天井に伸びて金切り声になる。
	ImGui::SliderFloat(U8("排気パルスの長さ(ms) 短=鋭い"), &m_pulseWidthMs, 0.5f, 12.0f);
	// 気筒ごとの燃焼の差。0にすると純粋な倍音列＝ブザーになる
	ImGui::SliderFloat(U8("気筒ごとの個体差 0=ブザー"),    &m_cylImbalance, 0.0f, 0.8f);
	ImGui::SliderFloat(U8("アクセルオフのこもり"),   &m_offRolloffAdd, 0.0f, 2.5f);
	// 低域を切る。排気管の最低モードは非常に低く(2.4mで約36Hz)、
	// そのままだとスピーカーでは「ボー」という濁りにしかならない
	ImGui::SliderFloat(U8("低域を切る(Hz) 大=すっきり"), &m_highPassHz, 20.0f, 300.0f);

	// レブリミッターは点火を飛ばす処理。深く切ると音がブツブツに途切れ、
	// 破裂音も大きいと暴れすぎて聞いていられなくなる。
	ImGui::SeparatorText(U8("レブリミッター・オーバーラン"));
	ImGui::SliderFloat(U8("点火を飛ばす深さ"),       &m_cutDepth, 0.0f, 1.0f);
	ImGui::SliderFloat(U8("排気で爆ぜる音の大きさ"), &m_cutPopLevel, 0.0f, 0.8f);
	ImGui::SliderFloat(U8("オーバーランのパチパチ"), &m_overrunCrackle, 0.0f, 1.0f);
	// 高回転で一気に閉じた時だけ、数発まとめて出る「パパパンッ」
	ImGui::SliderFloat(U8("アフターファイアの大きさ"), &m_afterfireLevel, 0.0f, 1.5f);
	ImGui::SliderFloat(U8("揺らぎ 0=電子オルガン"),  &m_wobble, 0.0f, 0.8f);

	// 1発の中で明るさが落ちる動き。倍音が一定のままでは作れない要素で、
	// これが無いと「同じ波形の繰り返し」になってパンチが出ない。
	ImGui::SeparatorText(U8("1発のアタック(パンチ)"));
	ImGui::SliderFloat(U8("開く量 大=鋭い破裂"),   &m_pulseAttack, 0.0f, 8.0f);
	ImGui::SliderFloat(U8("閉じる速さ 大=短い"),   &m_pulseAttackSharp, 0.5f, 12.0f);
	ImGui::SliderFloat(U8("頭の押し出し"),         &m_pulsePunch, 0.0f, 1.5f);

	ImGui::SeparatorText(U8("吸排気の乱流"));
	ImGui::SliderFloat(U8("ノイズ量"),          &m_noiseLevel, 0.0f, 1.5f);
	ImGui::SliderFloat(U8("ノイズの明るさ"),    &m_noiseTone, 0.01f, 1.0f);

	ImGui::SeparatorText(U8("マフラー(踏むと開く)"));
	ImGui::SliderFloat(U8("アイドルの明るさ(Hz)"), &m_cutoffBase, 60.0f, 2000.0f);
	ImGui::SliderFloat(U8("回転で開く量(Hz)"),     &m_cutoffRpmGain, 0.0f, 6000.0f);
	ImGui::SliderFloat(U8("アクセルで開く量(Hz)"), &m_cutoffThrGain, 0.0f, 5000.0f);
	ImGui::SliderFloat(U8("管の鳴き(レゾナンス)"), &m_resonance, 0.5f, 4.0f);

	ImGui::SeparatorText(U8("音量の変化"));
	ImGui::SliderFloat(U8("アクセルオフの音量"),   &m_offVolume, 0.0f, 1.0f);
	ImGui::SliderFloat(U8("高回転で上がる量"),     &m_rpmVolGain, 0.0f, 1.5f);

	ImGui::Separator();
	ImGui::Text(U8("回転 %.0f rpm / アクセル %.2f"), m_rpm, m_throttle);
	ImGui::Text(U8("基本周波数 %.1f Hz / 点火 %.1f Hz"),
	            m_rpm / 120.0f, m_rpm / 120.0f * m_firingOrder);
}

//----------------------------------------------------------
// 保存/読込の対象。CarBaseのチューニングファイルへ相乗りする。
//----------------------------------------------------------
void HjEngineAudio::CollectTuneParams(std::vector<std::pair<const char*, float*>>& out)
{
	out.push_back({ "engVolume",     &m_master });
	out.push_back({ "engFiringOrder",&m_firingOrder });
	out.push_back({ "engHalfLevel",  &m_halfLevel });
	out.push_back({ "engOtherLevel", &m_otherLevel });
	out.push_back({ "engRolloff",    &m_rolloff });
	out.push_back({ "engHighPass",   &m_highPassHz });
	out.push_back({ "engCutDepth",   &m_cutDepth });
	out.push_back({ "engCutPop",     &m_cutPopLevel });
	out.push_back({ "engOverrun",    &m_overrunCrackle });
	out.push_back({ "engAfterfire",  &m_afterfireLevel });
	out.push_back({ "engPulseMs",    &m_pulseWidthMs });
	out.push_back({ "engCylImb",     &m_cylImbalance });
	out.push_back({ "engOffRolloff", &m_offRolloffAdd });
	out.push_back({ "engWobble",     &m_wobble });
	out.push_back({ "engPulseAtk",  &m_pulseAttack });
	out.push_back({ "engPulseSharp",&m_pulseAttackSharp });
	out.push_back({ "engPulsePunch",&m_pulsePunch });
	out.push_back({ "engNoise",      &m_noiseLevel });
	out.push_back({ "engNoiseTone",  &m_noiseTone });
	out.push_back({ "engCutBase",    &m_cutoffBase });
	out.push_back({ "engCutRpm",     &m_cutoffRpmGain });
	out.push_back({ "engCutThr",     &m_cutoffThrGain });
	out.push_back({ "engResonance",  &m_resonance });
	out.push_back({ "engOffVolume",  &m_offVolume });
	out.push_back({ "engRpmVolume",  &m_rpmVolGain });
	out.push_back({ "engTurbo",      &m_turboLevel });
	out.push_back({ "engWhineBase",  &m_whineBase });
	out.push_back({ "engWhineGain",  &m_whineGain });
	out.push_back({ "engSpoolUp",    &m_spoolUp });
	out.push_back({ "engSpoolDown",  &m_spoolDown });
	out.push_back({ "engBov",        &m_bovLevel });
	out.push_back({ "engSurge",      &m_surgeLevel });
	out.push_back({ "engFormantHz",  &m_formantHz });
	out.push_back({ "engFormantQ",   &m_formantQ });
	out.push_back({ "engFormantRpm", &m_formantRpmGain });
	out.push_back({ "engFormantAmt", &m_formantAmount });
	// 物理シミュレーション側
	out.push_back({ "simCompression", &m_sim.m_compressionRatio });
	out.push_back({ "simHeat",        &m_sim.m_combustionHeat });
	out.push_back({ "simBurnDeg",     &m_sim.m_burnDurationDeg });
	out.push_back({ "simExOpenDeg",   &m_sim.m_exhaustOpenDeg });
	out.push_back({ "simExFlow",      &m_sim.m_exhaustFlowRate });
	out.push_back({ "simRodRatio",    &m_sim.m_rodRatio });
	out.push_back({ "simRunnerLen",   &m_sim.m_runnerLength });
	out.push_back({ "simRunnerRefl",  &m_sim.m_runnerReflect });
	out.push_back({ "simRunnerDamp",  &m_sim.m_runnerDamp });
	out.push_back({ "simIntakeLen",   &m_sim.m_intakeLength });
	out.push_back({ "simIntakeRefl",  &m_sim.m_intakeReflect });
	out.push_back({ "simIntakeLevel", &m_sim.m_intakeLevel });
	out.push_back({ "simMechLevel",   &m_sim.m_mechLevel });
	out.push_back({ "simMechHz",      &m_sim.m_mechHz });
	out.push_back({ "simPlenumVol",   &m_sim.m_plenumVolume });
	out.push_back({ "simPlenumOut",   &m_sim.m_plenumOutflow });
	out.push_back({ "simOutGain",     &m_sim.m_outputGain });

	// 録音素材側
	m_sampler.CollectTuneParams(out);

	// 排気系の共鳴
	out.push_back({ "exPipeLen",  &m_exhaust.m_pipeLength });
	out.push_back({ "exBandwidth",&m_exhaust.m_bandwidth });
	out.push_back({ "exBwRise",   &m_exhaust.m_bandwidthRise });
	out.push_back({ "exRolloff",  &m_exhaust.m_rolloff });
	out.push_back({ "exDetune",   &m_exhaust.m_detune });
	out.push_back({ "exMix",      &m_exhaust.m_mix });
	out.push_back({ "exTrim",     &m_exhaust.m_trim });
}
