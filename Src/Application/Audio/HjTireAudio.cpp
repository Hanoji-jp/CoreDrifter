#include "HjTireAudio.h"

using namespace TireAudioConst;

float HjTireAudio::Rand01()
{
	// xorshift。毎サンプル呼ぶので軽さ優先
	m_rng ^= m_rng << 13;
	m_rng ^= m_rng >> 17;
	m_rng ^= m_rng << 5;
	return static_cast<float>(m_rng & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

void HjTireAudio::Init()
{
	if (m_voice) { return; }

	IXAudio2* xa = KdAudioManager::Instance().GetXAudio2();
	if (!xa) { return; }

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

	// 前軸は後軸より少し高く鳴く。前後をずらすと2本の音がわずかにうねり、
	// 1本の笛に聞こえなくなる。
	m_axle[FrontAxle].hz = m_baseHz * m_frontPitchMul;
	m_axle[RearAxle].hz  = m_baseHz;
	for (Axle& a : m_axle) { a.hzTarget = a.hz; }

	m_voice->Start(0);
	SubmitPending();
}

void HjTireAudio::Stop()
{
	if (!m_voice) { return; }
	m_voice->Stop(0);
	m_voice->FlushSourceBuffers();
	m_voice->DestroyVoice();
	m_voice = nullptr;
}

//----------------------------------------------------------
// 物理側の滑り具合から、軸ごとの目標(音量・高さ)を決める。
//
// 滑る速さで動かすのは音量と音の荒さで、音程はほとんど動かさない。
// 鳴きの高さはゴムとカーカスの共振で決まる＝タイヤの構造の値なので、
// 車速や滑り速度で大きく動かすとサイレンになってタイヤに聞こえない。
//----------------------------------------------------------
void HjTireAudio::Update(const HjTireSlipState (&wheels)[WheelNum], bool grounded)
{
	if (!m_voice) { return; }

	for (int ax = 0; ax < AxleNum; ++ax)
	{
		Axle& a = m_axle[ax];

		if (!grounded)
		{
			// 滞空中はタイヤが路面に触れていないので鳴りようがない
			a.squealTarget = 0.0f;
			a.roarTarget   = 0.0f;
			continue;
		}

		// その軸で最も滑っている輪が音を支配する。
		// 平均にすると、片輪だけ流れている状態(荷重が抜けた内輪)で音が半分になる。
		float slip = 0.0f;
		float load = 0.0f;
		for (int k = 0; k < 2; ++k)
		{
			const HjTireSlipState& w = wheels[AxleWheel[ax][k]];
			slip = std::max(slip, w.slipSpeed);
			load += w.load;
		}

		// 荷重の重み。荷重が抜けた軸のタイヤは接地圧が下がって鳴りが細くなる
		const float loadW = std::clamp(load / (2.0f * LoadRef), 0.0f, LoadMax);

		// 鳴き：貼り付きと滑りを繰り返せる範囲で最大になる
		float squeal = std::clamp(
			(slip - m_slipStart) / std::max(m_slipFull - m_slipStart, 1e-4f), 0.0f, 1.0f);
		// 擦れ：滑りが深いほど広い帯域のノイズへ寄る
		const float roar = std::clamp(
			(slip - m_roarStart) / std::max(m_roarFull - m_roarStart, 1e-4f), 0.0f, 1.0f);

		// 深く滑るほどブロックが貼り付く時間が無くなり、音程のある鳴きが消える。
		// ドリフト中の音が笛ではなく風切り音寄りになるのはこれ。
		squeal *= (1.0f - m_roarTakeover * roar);

		a.squealTarget = squeal * loadW;
		a.roarTarget   = roar   * loadW;

		// 音程はごく浅くしか動かさない。滑ると張力が上がってわずかに高くなる程度。
		const float slipN = std::clamp(slip / std::max(m_slipFull, 1e-4f), 0.0f, 1.0f);
		const float loadN = std::clamp(load / (2.0f * LoadRef) - 1.0f, -1.0f, 1.0f);
		const float axleMul = (ax == FrontAxle) ? m_frontPitchMul : 1.0f;
		a.hzTarget = m_baseHz * axleMul
		           * (1.0f + m_pitchSlipGain * slipN)
		           * (1.0f + m_pitchLoadGain * loadN);
	}

	SubmitPending();
}

//----------------------------------------------------------
// 1ブロックぶんの波形を書き出す。
//
//   ① 軸ごとに共鳴の高さを決め、係数を組み直す
//   ② ノイズを共鳴モードへ通す      … 鳴き(音程が立つ)
//   ③ ノイズを広いバンドパスへ通す  … 擦れ(音程が立たない)
//   ④ 音量はサンプル単位で追従させる(フレーム境界で段を作らない)
//
// 共鳴の状態はブロックを跨いで持ち越すので、音が途切れない。
//----------------------------------------------------------
void HjTireAudio::RenderBlock(std::vector<float>& out)
{
	const float sr = static_cast<float>(SampleRate);
	const float nyquist = sr * 0.45f;

	// 立ち上がりは速く、消える時は尾を引く。
	// 食い付いた瞬間の「キュッ」は立ち上がりの速さで決まる。
	const float atk = std::clamp(m_attack  / sr, 0.0f, 1.0f);
	const float rel = std::clamp(m_release / sr, 0.0f, 1.0f);

	//----- ① 軸ごとに共鳴の係数を組み直す -----
	for (Axle& a : m_axle)
	{
		// 音程のゆらぎ。スティックスリップは不安定で、実際の鳴きは常に揺れている。
		// 正弦で揺らすとビブラートになって電子音なので、なました乱数で不規則に揺らす。
		a.warble += ((Rand01() * 2.0f - 1.0f) - a.warble) * WarbleSpeed;
		a.hz += (a.hzTarget - a.hz) * PitchFollow;

		const float baseHz = std::max(a.hz * (1.0f + m_warbleDepth * a.warble), 20.0f);

		for (int i = 0; i < ModeCount; ++i)
		{
			Mode& m = a.modes[i];
			const float f = baseHz * ModeRatio[i];
			if (f >= nyquist)
			{
				// 鳴らせない高さのモードは黙らせる(折り返し雑音になるため)
				m.a1 = m.a2 = m.gain = 0.0f;
				continue;
			}

			// 高いモードほど速く減衰する(高音ほど早く失われる実際の性質)
			const float bw = m_bandwidth + m_bandwidthRise * static_cast<float>(i);

			// 2極の共振器。極の半径rが1に近いほど長く響く
			const float r = expf(-3.14159265f * bw / sr);
			const float w = 6.2831853f * f / sr;

			m.a1 = 2.0f * r * cosf(w);
			m.a2 = -r * r;
			// 鋭さ(帯域幅)を変えても音量が変わらないよう、共振の利得で割り戻す。
			// これが無いと「響きを長くする」つまみが「音量を上げる」つまみになる。
			m.gain = ModeLevel[i] * sqrtf(std::max(1.0f - r * r, 1e-6f));
		}
	}

	//----- ③ 擦れ用バンドパスの係数(軸で共通) -----
	const float scrubHz = std::clamp(m_scrubHz, 100.0f, nyquist);
	const float scrubF  = 2.0f * sinf(3.14159265f * scrubHz / sr);
	const float scrubQ  = 1.0f / std::max(m_scrubQ, 0.5f);

	for (int n = 0; n < static_cast<int>(out.size()); ++n)
	{
		float mix = 0.0f;

		for (Axle& a : m_axle)
		{
			//----- ④ 音量の追従 -----
			a.squeal += (a.squealTarget - a.squeal)
			          * ((a.squealTarget > a.squeal) ? atk : rel);
			a.roar   += (a.roarTarget - a.roar)
			          * ((a.roarTarget > a.roar) ? atk : rel);

			// 鳴っていない軸は計算ごと飛ばす(直進中は毎サンプルここで抜ける)
			if (a.squeal < 1e-4f && a.roar < 1e-4f) { continue; }

			// 軸ごとに別のノイズを使う。同じノイズを共有すると前後が相関して
			// 1つの音源に聞こえ、前後で高さをずらした意味が無くなる。
			const float noise = Rand01() * 2.0f - 1.0f;

			//----- ② 鳴き：ノイズを高いQの共鳴へ通す -----
			if (a.squeal > 1e-4f)
			{
				float s = 0.0f;
				for (Mode& m : a.modes)
				{
					if (m.gain == 0.0f) { continue; }
					const float y = m.a1 * m.y1 + m.a2 * m.y2 + m.gain * noise;
					m.y2 = m.y1;
					m.y1 = y;
					s += y;
				}
				mix += s * a.squeal * m_squealLevel;
			}

			//----- ③ 擦れ：広い帯域のノイズ -----
			if (a.roar > 1e-4f)
			{
				const float high = noise - a.scrubLow - scrubQ * a.scrubBand;
				a.scrubBand += scrubF * high;
				a.scrubLow  += scrubF * a.scrubBand;
				mix += a.scrubBand * a.roar * m_scrubLevel;
			}
		}

		// ソフトクリップ。上限で切らず、大きいほど緩やかに寝かせる
		const float o = mix * m_master;
		out[n] = ClipLevel * (o / (1.0f + fabsf(o)));
	}
}

//----------------------------------------------------------
// 空いているぶんだけPCMを作ってボイスへ流し込む。
//----------------------------------------------------------
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

//----------------------------------------------------------
// 音作り用パネル。値を変えた瞬間に音が変わるので、作り直しの処理は要らない。
//----------------------------------------------------------
void HjTireAudio::DrawImGui()
{
	if (!ImGui::CollapsingHeader(U8("タイヤの鳴き(スキール)"))) { return; }

	if (!m_voice)
	{
		ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), U8("音声の初期化に失敗しています"));
		return;
	}

	ImGui::SliderFloat(U8("全体音量"), &m_master, 0.0f, 1.0f);

	// 鳴きの高さはタイヤの構造で決まる値。車速では動かさない。
	ImGui::SeparatorText(U8("鳴きの高さ(タイヤの共振)"));
	ImGui::SliderFloat(U8("基準の高さ(Hz)"),        &m_baseHz, 300.0f, 2000.0f);
	ImGui::SliderFloat(U8("前軸の高さ倍率"),        &m_frontPitchMul, 0.8f, 1.4f);
	ImGui::SliderFloat(U8("響きの長さ 小=よく響く"), &m_bandwidth, 10.0f, 250.0f);
	ImGui::SliderFloat(U8("高音の減衰の速さ"),      &m_bandwidthRise, 0.0f, 150.0f);
	ImGui::SliderFloat(U8("滑りで上がる量 大=サイレン"), &m_pitchSlipGain, 0.0f, 0.5f);
	ImGui::SliderFloat(U8("荷重で上がる量"),        &m_pitchLoadGain, 0.0f, 0.5f);
	ImGui::SliderFloat(U8("ゆらぎ 0=電子音"),       &m_warbleDepth, 0.0f, 0.12f);

	ImGui::SeparatorText(U8("滑り量から音量へ"));
	ImGui::SliderFloat(U8("鳴き始める滑り(m/s)"),   &m_slipStart, 0.0f, 4.0f);
	ImGui::SliderFloat(U8("鳴きが最大の滑り(m/s)"), &m_slipFull, 1.0f, 15.0f);
	ImGui::SliderFloat(U8("擦れ始める滑り(m/s)"),   &m_roarStart, 0.0f, 10.0f);
	ImGui::SliderFloat(U8("擦れが最大の滑り(m/s)"), &m_roarFull, 2.0f, 30.0f);
	ImGui::SliderFloat(U8("擦れが鳴きを消す量"),    &m_roarTakeover, 0.0f, 1.0f);

	ImGui::SeparatorText(U8("擦れ(音程の無いゴーッ)"));
	ImGui::SliderFloat(U8("帯域の中心(Hz)"),   &m_scrubHz, 300.0f, 5000.0f);
	ImGui::SliderFloat(U8("帯域の狭さ 大=音程が立つ"), &m_scrubQ, 0.5f, 6.0f);

	ImGui::SeparatorText(U8("立ち上がり / 音量配分"));
	ImGui::SliderFloat(U8("食い付きの速さ"),   &m_attack, 1.0f, 60.0f);
	ImGui::SliderFloat(U8("抜けの速さ"),       &m_release, 1.0f, 40.0f);
	ImGui::SliderFloat(U8("鳴きの音量"),       &m_squealLevel, 0.0f, 2.0f);
	ImGui::SliderFloat(U8("擦れの音量"),       &m_scrubLevel, 0.0f, 2.0f);

	ImGui::Separator();
	ImGui::Text(U8("前軸 鳴き %.2f / 擦れ %.2f / %.0f Hz"),
	            m_axle[FrontAxle].squeal, m_axle[FrontAxle].roar, m_axle[FrontAxle].hz);
	ImGui::Text(U8("後軸 鳴き %.2f / 擦れ %.2f / %.0f Hz"),
	            m_axle[RearAxle].squeal, m_axle[RearAxle].roar, m_axle[RearAxle].hz);
}

//----------------------------------------------------------
// 保存/読込の対象。CarBaseのチューニングファイルへ相乗りする。
//----------------------------------------------------------
void HjTireAudio::CollectTuneParams(std::vector<std::pair<const char*, float*>>& out)
{
	out.push_back({ "tireVolume",     &m_master });
	out.push_back({ "tireBaseHz",     &m_baseHz });
	out.push_back({ "tireFrontPitch", &m_frontPitchMul });
	out.push_back({ "tireBandwidth",  &m_bandwidth });
	out.push_back({ "tireBwRise",     &m_bandwidthRise });
	out.push_back({ "tirePitchSlip",  &m_pitchSlipGain });
	out.push_back({ "tirePitchLoad",  &m_pitchLoadGain });
	out.push_back({ "tireWarble",     &m_warbleDepth });
	out.push_back({ "tireSlipStart",  &m_slipStart });
	out.push_back({ "tireSlipFull",   &m_slipFull });
	out.push_back({ "tireRoarStart",  &m_roarStart });
	out.push_back({ "tireRoarFull",   &m_roarFull });
	out.push_back({ "tireRoarTake",   &m_roarTakeover });
	out.push_back({ "tireScrubHz",    &m_scrubHz });
	out.push_back({ "tireScrubQ",     &m_scrubQ });
	out.push_back({ "tireAttack",     &m_attack });
	out.push_back({ "tireRelease",    &m_release });
	out.push_back({ "tireSquealLvl",  &m_squealLevel });
	out.push_back({ "tireScrubLvl",   &m_scrubLevel });
}
