#include "HjTireAudio.h"
#include "HjAudioSpace.h"   // 反射と残響をまとめて掛ける

using namespace TireAudioConst;

float HjTireAudio::Rand01()
{
	m_rng ^= m_rng << 13;
	m_rng ^= m_rng >> 17;
	m_rng ^= m_rng << 5;
	return static_cast<float>(m_rng & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

//----------------------------------------------------------
// 2極の共鳴器。ノイズを通すと、その周波数だけが鳴き出す。
// 極を単位円の近くに置くほど鋭く、長く響く＝「キーッ」に近づく。
//----------------------------------------------------------
float HjTireAudio::Resonator::Process(float in, float freqHz, float q, float sampleRate)
{
	const float f = std::clamp(freqHz, 20.0f, sampleRate * 0.45f);
	// 帯域幅から極の半径を決める。Qが大きいほど帯域が狭く、鋭くなる
	const float bw = f / std::max(q, 0.5f);
	const float r  = expf(-3.14159265f * bw / sampleRate);
	const float w  = 6.2831853f * f / sampleRate;

	const float a1 = 2.0f * r * cosf(w);
	const float a2 = -r * r;

	// 共振の利得(1/(1-r))で割り戻す。Qを上げても音量が上がらないようにする
	const float y = a1 * y1 + a2 * y2 + (1.0f - r) * in;
	y2 = y1;
	y1 = y;
	return y;
}

void HjTireAudio::Init()
{
	if (m_voice) { return; }

	IXAudio2* xa = KdAudioManager::Instance().GetXAudio2();
	if (!xa) { return; }

	WAVEFORMATEX wfx{};
	wfx.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
	wfx.nChannels       = ChannelNum;
	wfx.nSamplesPerSec  = SampleRate;
	wfx.wBitsPerSample  = 32;
	wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize          = 0;

	// 遠いほど高音を落とすため、ボイスにフィルタを持たせる
	if (FAILED(xa->CreateSourceVoice(&m_voice, &wfx, XAUDIO2_VOICE_USEFILTER))) { m_voice = nullptr; return; }
	m_xa = xa;   // 以後、このエンジンが生きている間だけボイスを触る

	// XAudio2は再生し終わるまで中身を参照するので、送ったメモリは使い回す
	m_blocks.assign(QueuedBlocks, std::vector<float>(BlockSamples, 0.0f));

	// エンジンと同じ空間へ通す。別々に響かせると音が分離して聞こえる
	HjAudioSpace::Instance().Init();
	HjAudioSpace::Instance().RouteVoice(m_voice, HjAudioSpace::Bus::Tire);

	m_voice->Start(0);
	SubmitPending();
}

void HjTireAudio::Apply3D(const Math::Vector3& worldPos)
{
	HjAudioSpace::Instance().ApplySource(m_voice, worldPos);
}

//----------------------------------------------------------
// ボイスがまだ生きているか。
// XAudio2エンジンが破棄されると、ぶら下がっているボイスも解放される。
// こちらは生ポインタを持っているだけなので、その後に触ると落ちる。
//----------------------------------------------------------
bool HjTireAudio::IsVoiceAlive() const
{
	return m_xa && (KdAudioManager::Instance().GetXAudio2() == m_xa);
}

void HjTireAudio::Stop()
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
// スキール＝ノイズを2つの共鳴に通した「鳴き」＋広い帯域の「擦れ」
// ブレーキ鳴き＝ずっと高く細い共鳴
//----------------------------------------------------------
void HjTireAudio::RenderBlock(std::vector<float>& out)
{
	const float sr = static_cast<float>(SampleRate);

	// 滑りが激しいほど高く鳴く。整数比を外した2つ目を重ねて厚みを出す
	// (綺麗な倍音比にすると楽器のように聞こえる)
	const float baseHz = m_squealBaseHz + m_squealSlipHz * m_slip;

	for (int n = 0; n < static_cast<int>(out.size()); ++n)
	{
		const float noise = Rand01() * 2.0f - 1.0f;

		float v = 0.0f;

		if (m_squealGain > 0.0005f)
		{
			// 高さの揺らぎ。完全に一定だとブザーになる
			m_wobVal += ((Rand01() * 2.0f - 1.0f) - m_wobVal) * SquealWobSpeed;
			const float hz = baseHz * (1.0f + SquealWobble * m_wobVal * 3.0f);

			//----- スティックスリップ振動 -----
			// タイヤの「キーッ」は、路面に食いついては解放されるのを
			// 高速で繰り返す振動。ノイズを共鳴させただけでは
			// 「シャーッ＋ピー」にしかならず、あの鳴きにはならない。
			//
			// 食いつき→解放のたびに衝撃が出るので、波形は鋸のような
			// 繰り返しになり、倍音が豊富に立つ。それがザラついた鳴きになる。
			m_stickPhase += hz * (1.0f + m_stickJit * 0.12f) / sr;
			if (m_stickPhase >= 1.0f)
			{
				m_stickPhase -= 1.0f;
				// 1周ごとに周期をばらつかせる。完全に一定だと電子ブザーになる
				m_stickJit = Rand01() * 2.0f - 1.0f;
			}
			// 鋸波。立ち上がりが急なので倍音がよく立つ
			const float stick = (m_stickPhase * 2.0f - 1.0f);

			// 解放の瞬間だけ強く出る成分。粘って離れる感じを作る
			const float release = powf(m_stickPhase, 6.0f) * 2.0f - 0.3f;

			// その振動を共鳴に通して、鳴きの「音色」を決める。
			// 共鳴を3つ重ね、整数比から外して楽器っぽさを消す。
			float squeal  = m_sq1.Process(stick, hz, m_squealQ, sr);
			squeal       += m_sq2.Process(release, hz * SquealHarmonic, SquealQ2, sr) * 0.7f;
			squeal       += m_sq3.Process(noise, hz * 3.7f, 6.0f, sr) * 0.35f;
			v += squeal * m_squealLevel * m_squealGain;

			// 擦れ：路面を削る広い帯域の音。鳴きだけだと電子音になる
			m_scrubLp += (noise - m_scrubLp) * std::clamp(m_scrubTone, 0.01f, 1.0f);
			v += m_scrubLp * m_scrubLevel * m_squealGain;
		}

		if (m_brakeGain > 0.0005f)
		{
			// ブレーキ鳴き：パッドの振動。タイヤよりずっと高く細い
			v += m_brake.Process(noise, m_brakeHz, BrakeSquealQ, sr)
			   * m_brakeLevel * m_brakeGain;
		}

		v *= m_master;
		out[n] = ClipLevel * (v / (1.0f + fabsf(v)));   // ソフトクリップ
	}
}

void HjTireAudio::SubmitPending()
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

void HjTireAudio::Update(float dt, float slip01, float speed, float brake01, bool onGround)
{
	if (!m_voice) { return; }

	m_slip  = std::clamp(slip01, 0.0f, 1.0f);
	m_speed = speed;

	//----- スキールの目標音量 -----
	// 停車中や滞空中は鳴らさない。低速では滑っていても鳴きにならない
	float squealTarget = 0.0f;
	if (onGround && speed > SquealMinSpeed && m_slip > SquealSlipMin)
	{
		// 滑り始めから滑らかに立ち上げる
		squealTarget = (m_slip - SquealSlipMin) / std::max(1.0f - SquealSlipMin, 1e-4f);
		// 速度が乗るほど鳴きが強い(接地面の擦れる速さが上がるため)
		squealTarget *= std::clamp(speed / 12.0f, 0.0f, 1.0f);
	}

	//----- ブレーキ鳴きの目標音量 -----
	// 強く踏んでいて、かつ低速の時だけ。高速では鳴かない
	float brakeTarget = 0.0f;
	if (onGround && brake01 > BrakeMinForce && speed < BrakeMaxSpeed && speed > 0.5f)
	{
		brakeTarget = (brake01 - BrakeMinForce) / std::max(1.0f - BrakeMinForce, 1e-4f);
		// 止まる寸前が一番鳴く
		brakeTarget *= 1.0f - std::clamp(speed / BrakeMaxSpeed, 0.0f, 1.0f);
	}

	// 立ち上がりは速く、収まりは緩やかに。
	// 同じ速さで動かすと、滑りが揺れるたびに音がバタついて不自然になる。
	const float sqRate = (squealTarget > m_squealGain) ? SquealAttack : SquealRelease;
	m_squealGain += (squealTarget - m_squealGain) * std::min(sqRate * dt, 1.0f);

	const float brRate = (brakeTarget > m_brakeGain) ? BrakeAttack : BrakeRelease;
	m_brakeGain += (brakeTarget - m_brakeGain) * std::min(brRate * dt, 1.0f);

	SubmitPending();
}

void HjTireAudio::DrawImGui()
{
	if (!ImGui::CollapsingHeader(U8("タイヤ・ブレーキ音"))) { return; }

	if (!m_voice)
	{
		ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), U8("音声の初期化に失敗しています"));
		return;
	}

	ImGui::SliderFloat(U8("全体音量"), &m_master, 0.0f, 1.5f);

	ImGui::SeparatorText(U8("スキール(キーッ)"));
	ImGui::SliderFloat(U8("鳴きの高さ(Hz)"),        &m_squealBaseHz, 300.0f, 2500.0f);
	ImGui::SliderFloat(U8("滑りで上がる量(Hz)"),    &m_squealSlipHz, 0.0f, 2500.0f);
	ImGui::SliderFloat(U8("鋭さ 大=キーッ 小=ゴーッ"), &m_squealQ, 2.0f, 40.0f);
	ImGui::SliderFloat(U8("鳴きの音量"),            &m_squealLevel, 0.0f, 1.5f);
	ImGui::SliderFloat(U8("擦れの音量"),            &m_scrubLevel, 0.0f, 1.5f);
	ImGui::SliderFloat(U8("擦れの明るさ"),          &m_scrubTone, 0.02f, 1.0f);

	ImGui::SeparatorText(U8("ブレーキ鳴き"));
	ImGui::SliderFloat(U8("鳴きの高さ(Hz)##brake"), &m_brakeHz, 1000.0f, 8000.0f);
	ImGui::SliderFloat(U8("音量##brake"),           &m_brakeLevel, 0.0f, 1.0f);

	ImGui::Separator();
	ImGui::Text(U8("滑り %.2f / 速度 %.1f m/s"), m_slip, m_speed);
	ImGui::Text(U8("スキール %.2f / ブレーキ %.2f"), m_squealGain, m_brakeGain);
}

void HjTireAudio::CollectTuneParams(std::vector<std::pair<const char*, float*>>& out)
{
	out.push_back({ "tireMaster",   &m_master });
	out.push_back({ "tireBaseHz",   &m_squealBaseHz });
	out.push_back({ "tireSlipHz",   &m_squealSlipHz });
	out.push_back({ "tireQ",        &m_squealQ });
	out.push_back({ "tireSqLevel",  &m_squealLevel });
	out.push_back({ "tireScrub",    &m_scrubLevel });
	out.push_back({ "tireScrubTone",&m_scrubTone });
	out.push_back({ "brakeHz",      &m_brakeHz });
	out.push_back({ "brakeLevel",   &m_brakeLevel });
}
