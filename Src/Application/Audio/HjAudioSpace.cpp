#include "HjAudioSpace.h"

//----------------------------------------------------------
// バス構成を組む。
//
//   エンジン ─┐
//             ├─→ 車両バス(反射・残響) ─→ マスター
//   タイヤ  ─┘
//
// 反射を車両バスに1つだけ掛けるのが要点。音源ごとに響かせると、
// 音が別々の場所で鳴っているように分離して聞こえる。
// ステレオで作るのは、音源を左右へ振れるようにするため。
//----------------------------------------------------------
void HjAudioSpace::Init()
{
	if (m_vehicle) { return; }

	IXAudio2* xa = KdAudioManager::Instance().GetXAudio2();
	if (!xa) { return; }
	m_xa = xa;   // 以後、このエンジンが生きている間だけボイスを触る

	// 車両バス。出力先を指定しないと既定でマスターボイスへ流れる
	if (FAILED(xa->CreateSubmixVoice(&m_vehicle, 2, 44100, 0, 0, nullptr, nullptr)))
	{
		m_vehicle = nullptr;
		return;
	}

	// エンジン・タイヤの各バスは車両バスへ送る
	XAUDIO2_SEND_DESCRIPTOR send{};
	send.Flags        = 0;
	send.pOutputVoice = m_vehicle;
	XAUDIO2_VOICE_SENDS sends{};
	sends.SendCount = 1;
	sends.pSends    = &send;

	xa->CreateSubmixVoice(&m_engine, 2, 44100, 0, 1, &sends, nullptr);
	xa->CreateSubmixVoice(&m_tire,   2, 44100, 0, 1, &sends, nullptr);

	// 反射・残響は車両バスにまとめて掛ける
	IUnknown* reverb = nullptr;
	if (SUCCEEDED(XAudio2CreateReverb(&reverb)))
	{
		XAUDIO2_EFFECT_DESCRIPTOR desc{};
		desc.InitialState   = TRUE;
		desc.OutputChannels = 2;
		desc.pEffect        = reverb;

		XAUDIO2_EFFECT_CHAIN chain{};
		chain.EffectCount        = 1;
		chain.pEffectDescriptors = &desc;

		m_vehicle->SetEffectChain(&chain);
		reverb->Release();   // SetEffectChainが参照を持つので手放してよい

		ApplyReverb();
	}
}

//----------------------------------------------------------
// ボイスがまだ生きているか。
// XAudio2エンジンが作り直された(あるいは破棄された)場合、
// こちらが持っているボイスのポインタは既に解放済みになっている。
//----------------------------------------------------------
bool HjAudioSpace::IsVoiceAlive() const
{
	return m_xa && (KdAudioManager::Instance().GetXAudio2() == m_xa);
}

void HjAudioSpace::Release()
{
	// エンジンが先に消えていれば、ボイスは既に解放済み。
	// ここで DestroyVoice を呼ぶと解放済みメモリへのアクセスになる。
	// 静的オブジェクトの破棄はエンジンの破棄より後に走ることがあるので、
	// デストラクタから呼ばれる経路では実際にこの状態になる。
	if (!IsVoiceAlive())
	{
		m_engine = m_tire = m_vehicle = nullptr;
		m_xa = nullptr;
		return;
	}

	auto kill = [](IXAudio2SubmixVoice*& v) { if (v) { v->DestroyVoice(); v = nullptr; } };
	// 送り先より先に送り元を壊す
	kill(m_engine);
	kill(m_tire);
	kill(m_vehicle);
	m_xa = nullptr;
}

IXAudio2SubmixVoice* HjAudioSpace::GetBus(Bus b)
{
	switch (b)
	{
	case Bus::Tire:   return m_tire;
	case Bus::Engine:
	default:          return m_engine;
	}
}

//----------------------------------------------------------
// 音源ボイスの出力先を、指定したバスへ差し替える。
// 既定ではマスターボイスへ直接出ている(＝バスもエフェクトも通らない)。
//----------------------------------------------------------
void HjAudioSpace::RouteVoice(IXAudio2SourceVoice* voice, Bus bus)
{
	if (!IsVoiceAlive()) { return; }
	IXAudio2SubmixVoice* dst = GetBus(bus);
	if (!voice || !dst) { return; }

	XAUDIO2_SEND_DESCRIPTOR send{};
	send.Flags        = 0;
	send.pOutputVoice = dst;

	XAUDIO2_VOICE_SENDS sends{};
	sends.SendCount = 1;
	sends.pSends    = &send;

	voice->SetOutputVoices(&sends);
}

//----------------------------------------------------------
// 聴取点をカメラから取る。
// シェーダーへ渡しているカメラ行列から、位置と向きをそのまま使う。
//----------------------------------------------------------
void HjAudioSpace::UpdateListener()
{
	const auto& cam = KdShaderManager::Instance().GetCameraCB();
	const Math::Matrix world = cam.mView.Invert();

	m_listenPos   = cam.CamPos;
	m_listenFwd   = world.Backward(); m_listenFwd.Normalize();
	m_listenRight = world.Right();    m_listenRight.Normalize();
}

//----------------------------------------------------------
// 音源の位置を反映する。
//   ・距離で減衰させる
//   ・カメラの向きに対してどちらにいるかで左右へ振る
//   ・遠いほど高音が失われる(空気による吸収)
//
// 音が常に耳元で同じ大きさで鳴っていると、距離と方向の手がかりが無い。
// 現実の音には必ずそれが入っているので、無いと「実在しない音」に聞こえる。
//----------------------------------------------------------
void HjAudioSpace::ApplySource(IXAudio2SourceVoice* voice, const Math::Vector3& worldPos)
{
	if (!voice || !m_vehicle || !IsVoiceAlive()) { return; }

	const Math::Vector3 d = worldPos - m_listenPos;
	const float dist = d.Length();

	//----- 距離減衰 -----
	// 基準距離までは減衰させない(すぐ近くで音量が暴れないように)。
	// そこから先は距離に反比例して落とす。
	float atten = 1.0f;
	if (dist > m_refDistance)
	{
		atten = m_refDistance / dist;
		// 最遠でちょうど0になるよう、なだらかに落とし切る
		const float fade = std::clamp(1.0f - (dist - m_refDistance) /
		                              std::max(m_maxDistance - m_refDistance, 1.0f), 0.0f, 1.0f);
		atten *= fade;
	}
	atten *= m_volMaster;

	//----- 左右への振り分け -----
	// カメラの右方向とのなす角で決める。真後ろ・真正面なら中央。
	float pan = 0.0f;
	if (dist > 0.01f)
	{
		pan = std::clamp(m_listenRight.Dot(d / dist) * m_panWidth, -1.0f, 1.0f);
	}
	// 等パワーで振る。単純な線形だと中央で音量が凹む
	const float ang = (pan * 0.5f + 0.5f) * 1.5707963f;
	float mtx[2] = { cosf(ang) * atten, sinf(ang) * atten };
	voice->SetOutputMatrix(nullptr, 1, 2, mtx);

	//----- 空気による吸収 -----
	// 遠いほど高音から失われる。距離の手がかりとして音量より効く。
	if (m_airAbsorb > 0.001f)
	{
		const float far01 = std::clamp(dist / std::max(m_maxDistance, 1.0f), 0.0f, 1.0f);
		XAUDIO2_FILTER_PARAMETERS f{};
		f.Type = LowPassFilter;
		// XAudio2のFrequencyは 2*sin(pi*fc/fs) 相当。1.0で素通しに近い
		f.Frequency = std::clamp(1.0f - m_airAbsorb * far01 * far01, 0.05f, 1.0f);
		f.OneOverQ  = 1.0f;
		voice->SetFilterParameters(&f);
	}
}

void HjAudioSpace::SetPreset(Preset p) { m_preset = p;  ApplyReverb(); }
void HjAudioSpace::SetWetness(float w) { m_wetness = std::clamp(w, 0.0f, 1.0f); ApplyReverb(); }

//----------------------------------------------------------
// 場所に応じた響き。I3D L2のプリセットを土台に、混ぜ具合だけこちらで決める。
//----------------------------------------------------------
void HjAudioSpace::ApplyReverb()
{
	if (!m_vehicle || !IsVoiceAlive()) { return; }

	XAUDIO2FX_REVERB_I3DL2_PARAMETERS i3dl2 = XAUDIO2FX_I3DL2_PRESET_DEFAULT;
	switch (m_preset)
	{
	case Preset::OpenRoad:     i3dl2 = XAUDIO2FX_I3DL2_PRESET_PLAIN;         break;
	case Preset::MountainPass: i3dl2 = XAUDIO2FX_I3DL2_PRESET_FOREST;        break;
	case Preset::Tunnel:       i3dl2 = XAUDIO2FX_I3DL2_PRESET_STONECORRIDOR; break;
	case Preset::City:         i3dl2 = XAUDIO2FX_I3DL2_PRESET_PARKINGLOT;    break;
	}

	XAUDIO2FX_REVERB_PARAMETERS params{};
	ReverbConvertI3DL2ToNative(&i3dl2, &params);

	// 掛けすぎると近くで鳴っているはずの音が遠くの音になってしまう
	params.WetDryMix = m_wetness * 100.0f;
	m_vehicle->SetEffectParameters(0, &params, sizeof(params));
}

void HjAudioSpace::DrawImGui()
{
	if (!ImGui::CollapsingHeader(U8("音のミキサー・空間"))) { return; }

	if (!m_vehicle || !IsVoiceAlive())
	{
		ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), U8("初期化に失敗しています"));
		return;
	}

	// バスごとの音量。各クラスが個別に持つとバランスが取れない
	ImGui::SeparatorText(U8("バス音量"));
	if (ImGui::SliderFloat(U8("全体"),     &m_volMaster, 0.0f, 2.0f)) {}
	if (ImGui::SliderFloat(U8("エンジン"), &m_volEngine, 0.0f, 2.0f) && m_engine)
	{
		m_engine->SetVolume(m_volEngine);
	}
	if (ImGui::SliderFloat(U8("タイヤ"),   &m_volTire, 0.0f, 2.0f) && m_tire)
	{
		m_tire->SetVolume(m_volTire);
	}

	ImGui::SeparatorText(U8("3D(距離・定位)"));
	ImGui::TextWrapped(U8("常に耳元で同じ大きさで鳴っていると距離と方向の手がかりが無く、"
	                      "実在しない音に聞こえる。"));
	ImGui::SliderFloat(U8("減衰し始める距離(m)"), &m_refDistance, 1.0f, 30.0f);
	ImGui::SliderFloat(U8("聞こえなくなる距離(m)"), &m_maxDistance, 20.0f, 300.0f);
	ImGui::SliderFloat(U8("左右への振り 0=中央固定"), &m_panWidth, 0.0f, 1.0f);
	ImGui::SliderFloat(U8("遠いほど高音が減る量"), &m_airAbsorb, 0.0f, 1.0f);

	ImGui::SeparatorText(U8("反射・残響"));
	const char* names[] = { U8("開けた道"), U8("峠(斜面の返り)"),
	                        U8("トンネル"), U8("市街地") };
	int p = static_cast<int>(m_preset);
	if (ImGui::Combo(U8("場所"), &p, names, IM_ARRAYSIZE(names)))
	{
		SetPreset(static_cast<Preset>(p));
	}
	float w = m_wetness;
	if (ImGui::SliderFloat(U8("響きの量 0=ドライ"), &w, 0.0f, 1.0f)) { SetWetness(w); }
}
