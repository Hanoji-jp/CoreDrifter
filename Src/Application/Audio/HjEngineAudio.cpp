#include "HjEngineAudio.h"

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
	}

	WAVEFORMATEX wfx{};
	wfx.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;   // float32で直接書く(変換不要)
	wfx.nChannels       = ChannelNum;
	wfx.nSamplesPerSec  = SampleRate;
	wfx.wBitsPerSample  = 32;
	wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize          = 0;

	if (FAILED(xa->CreateSourceVoice(&m_voice, &wfx))) { m_voice = nullptr; return; }

	// 送信用バッファを使い回す。XAudio2は再生し終わるまで中身を参照するので、
	// 送ったメモリは解放せず、一周してから上書きする。
	m_blocks.assign(QueuedBlocks, std::vector<float>(BlockSamples, 0.0f));

	m_voice->Start(0);
	SubmitPending();
}

void HjEngineAudio::Stop()
{
	if (!m_voice) { return; }
	m_voice->Stop(0);
	m_voice->FlushSourceBuffers();
	m_voice->DestroyVoice();
	m_voice = nullptr;
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
                               float spoolStep, float rpmN)
{
	const float sr = static_cast<float>(SampleRate);
		//----- ④ マフラーを通す -----
		// 状態変数フィルタ(レゾナンス付きローパス)。踏むとカットオフが上がって開く
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

			// ブローオフ：閉じた瞬間に溜まった空気が抜ける「プシュー」
			if (m_bovEnv > 0.0001f)
			{
				const float raw2 = Rand01() * 2.0f - 1.0f;
				m_bovLp += (raw2 - m_bovLp) * 0.45f;
				body += m_bovLp * m_bovEnv * m_bovLevel;
				m_bovEnv -= m_bovEnv * std::min(BovDecay / sr, 1.0f);
			}
	}

	// ソフトクリップ。上限で切らず、大きいほど緩やかに寝かせる
	const float o = body * m_master;
	dst = ClipLevel * (o / (1.0f + fabsf(o)));
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

		const float a = w * powf(1.0f / static_cast<float>(k), rolloff);
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

	const float noiseAmt = m_noiseLevel * (1.0f + NoiseRpmGain * rpmN) * (0.4f + 0.6f * thr);
	// 高回転ほど回転が安定するので揺らぎを減らす。
	// 深いまま高回転へ行くと、強い倍音が何十本も独立に振れて汚くなる。
	const float wobScale = 1.0f - 0.6f * rpmN;
	// 排気の乱流は垂れ流しではなく、点火のたびに吹き出す
	const float fireInc = f0 * m_firingOrder / sr;

	// 吸気の共鳴。狭い帯域だけを強調して金属的な鳴きを出す
	float fmtHz = m_formantHz + FormantRpmGain * rpmN;
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
			RenderTail(out[n], 0.0f, svfF, svfQ, fmtF, fmtQ, spoolTarget, spoolStep, rpmN);
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
			RenderTail(out[n], v, svfF, svfQ, fmtF, fmtQ, spoolTarget, spoolStep, rpmN);
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

		//----- ③ 吸排気の乱流 -----
		// 点火に合わせて脈打たせる。一定のノイズを混ぜるだけだと
		// 「シャー」というだけで生気がない。
		m_firePhase += fireInc;
		if (m_firePhase >= 1.0f) { m_firePhase -= 1.0f; }
		const float pulse = powf(1.0f - m_firePhase, NoisePulseSharp);
		const float pulseGain = (1.0f - NoisePulseDepth) + NoisePulseDepth * pulse;

		const float raw = Rand01() * 2.0f - 1.0f;
		m_noiseLp += (raw - m_noiseLp) * std::clamp(m_noiseTone, 0.01f, 1.0f);
		v += m_noiseLp * noiseAmt * pulseGain;

		v *= volAll;

		RenderTail(out[n], v, svfF, svfQ, fmtF, fmtQ, spoolTarget, spoolStep, rpmN);
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

void HjEngineAudio::Update(float dt, float rpm, float throttle, float maxRpm)
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
		}
	}
	m_prevThrottleForBov = thrRaw;

	// 生の値をそのまま使うと踏み替えのたびに音がパチンと切り替わる
	m_throttle += (thrRaw - m_throttle) * std::min(ThrottleSmooth * dt, 1.0f);
	m_rpm      += (rpm - m_rpm) * std::min(RpmSmooth * dt, 1.0f);
	m_maxRpm    = maxRpm;

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
		ImGui::SliderFloat(U8("集合部の大きさ 大=こもる"), &m_sim.m_plenumVolume, 0.2f, 8.0f);
		ImGui::SliderFloat(U8("大気へ抜ける速さ"),         &m_sim.m_plenumOutflow, 0.3f, 12.0f);
		ImGui::SliderFloat(U8("出力"),                     &m_sim.m_outputGain, 0.05f, 3.0f);
		ImGui::Text(U8("クランク角 %.0f度"), m_sim.GetCrankDeg());
	}

	ImGui::SeparatorText(U8("倍音の配分(合成モード)"));
	ImGui::SliderFloat(U8("点火倍音の次数(=気筒数)"), &m_firingOrder, 2.0f, 12.0f, "%.0f");
	ImGui::SliderFloat(U8("うねり(半分オーダー)"),   &m_halfLevel, 0.0f, 1.2f);
	ImGui::SliderFloat(U8("ざらつき(その他)"),       &m_otherLevel, 0.0f, 1.0f);
	ImGui::SliderFloat(U8("高次の落ち方 大=丸い"),   &m_rolloff, 0.3f, 3.0f);
	ImGui::SliderFloat(U8("アクセルオフのこもり"),   &m_offRolloffAdd, 0.0f, 2.5f);
	ImGui::SliderFloat(U8("揺らぎ 0=電子オルガン"),  &m_wobble, 0.0f, 0.8f);

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
	out.push_back({ "engOffRolloff", &m_offRolloffAdd });
	out.push_back({ "engWobble",     &m_wobble });
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
	out.push_back({ "engFormantHz",  &m_formantHz });
	out.push_back({ "engFormantQ",   &m_formantQ });
	out.push_back({ "engFormantAmt", &m_formantAmount });
	// 物理シミュレーション側
	out.push_back({ "simCompression", &m_sim.m_compressionRatio });
	out.push_back({ "simHeat",        &m_sim.m_combustionHeat });
	out.push_back({ "simBurnDeg",     &m_sim.m_burnDurationDeg });
	out.push_back({ "simExOpenDeg",   &m_sim.m_exhaustOpenDeg });
	out.push_back({ "simExFlow",      &m_sim.m_exhaustFlowRate });
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
