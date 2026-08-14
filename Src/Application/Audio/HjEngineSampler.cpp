#include "HjEngineSampler.h"

using namespace EngineSamplerConst;

//----------------------------------------------------------
// 等パワークロスフェード。
// 単純な線形で混ぜると、交差点で合計の音量が落ちて凹んで聞こえる。
// 余弦を使うと、混ざっている途中でも音の大きさが保たれる。
//----------------------------------------------------------
void HjEngineSampler::CrossFade(float value, float start, float end,
                                float& outHigh, float& outLow)
{
	const float x = std::clamp((value - start) / std::max(end - start, 1e-4f), 0.0f, 1.0f);
	outHigh = cosf((1.0f - x) * 0.5f * 3.14159265f);
	outLow  = cosf(x * 0.5f * 3.14159265f);
}

//----------------------------------------------------------
// 素材を読み込んでループ再生を始める。音量0で開始し、以後は音量だけ動かす。
// 素材が無ければ何もしない(揃っていない状態でも他の素材は鳴る)。
//----------------------------------------------------------
bool HjEngineSampler::AddLayer(Layer& out, const char* path, float* baseRpm, float* volume)
{
	out.inst = KdAudioManager::Instance().Play(path, true);
	if (!out.inst) { return false; }

	out.inst->SetVolume(0.0f);
	out.baseRpm = baseRpm ? *baseRpm : 2000.0f;
	out.volume  = volume  ? *volume  : 1.0f;
	return true;
}

void HjEngineSampler::Init()
{
	m_hasAny = false;
	m_hasAny |= AddLayer(m_onLow,   PathOnLow,   &m_baseRpmOnLow,   &m_volOnLow);
	m_hasAny |= AddLayer(m_onHigh,  PathOnHigh,  &m_baseRpmOnHigh,  &m_volOnHigh);
	m_hasAny |= AddLayer(m_offLow,  PathOffLow,  &m_baseRpmOffLow,  &m_volOffLow);
	m_hasAny |= AddLayer(m_offHigh, PathOffHigh, &m_baseRpmOffHigh, &m_volOffHigh);

	// バックファイアは鳴らす瞬間に単発で再生するので、パスだけ持っておく。
	// 同じ音が繰り返されると急に嘘っぽくなるため、複数から選ぶ。
	m_backfirePaths.clear();
	for (int i = 1; i <= BackfireVariations; ++i)
	{
		char buf[256] = {};
		snprintf(buf, sizeof(buf), PathBackfire, i);
		m_backfirePaths.push_back(buf);
	}
}

void HjEngineSampler::Stop()
{
	auto stopOne = [](Layer& l) { if (l.inst) { l.inst->Stop(); l.inst = nullptr; } };
	stopOne(m_onLow); stopOne(m_onHigh); stopOne(m_offLow); stopOne(m_offHigh);
	m_hasAny = false;
}

//----------------------------------------------------------
// 1本ぶんの音量とピッチを反映する。
//
// ピッチは「現在のRPM ÷ 素材の基準RPM」。
// DirectXTKのSetPitchは -1〜+1 で周波数比 0.5〜2.0倍にあたるので、
// 比の対数(何オクターブぶんか)を渡すのが正しい。
//----------------------------------------------------------
void HjEngineSampler::ApplyLayer(Layer& layer, float rpm, float gain)
{
	if (!layer.inst) { return; }

	const float ratio = std::max(rpm, 1.0f) / std::max(layer.baseRpm, 1.0f);
	const float oct   = log2f(std::max(ratio, 0.01f));
	layer.inst->SetPitch(std::clamp(oct, -PitchLimit, PitchLimit));

	layer.inst->SetVolume(std::clamp(gain * layer.volume * m_master, 0.0f, 1.0f));
}

//----------------------------------------------------------
// アクセルを戻した瞬間のバックファイア。
// 高回転で急にアクセルを閉じると、燃え残りが排気管で爆ぜる。
// CarXは5種類以上を用意して個別に鳴らしている。
//----------------------------------------------------------
void HjEngineSampler::UpdateBackfire(float dt, float rpm, float throttleRaw)
{
	m_backfireTimer -= dt;

	const float drop = m_prevThrottle - throttleRaw;
	m_prevThrottle = throttleRaw;

	if (m_backfirePaths.empty()) { return; }
	if (m_backfireTimer > 0.0f)  { return; }
	if (drop < BackfireThrottleDrop || rpm < BackfireMinRpm) { return; }

	// 毎回違う音を選ぶ。同じ音の繰り返しは急に嘘っぽく聞こえる
	m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5;
	const size_t idx = m_rng % m_backfirePaths.size();

	auto s = KdAudioManager::Instance().Play(m_backfirePaths[idx].c_str(), false);
	if (s) { s->SetVolume(std::clamp(m_backfireVolume * m_master, 0.0f, 1.0f)); }

	m_backfireTimer = BackfireInterval;
}

//----------------------------------------------------------
// 毎フレームの更新。
//
// 音量は2軸の掛け算で決まる。
//              低回転の素材 ←─ RPM ─→ 高回転の素材
//   アクセルON       on_low              on_high
//   アクセルOFF      off_low             off_high
//----------------------------------------------------------
void HjEngineSampler::Update(float dt, float rpm, float throttle)
{
	const float thrRaw = std::clamp(throttle, 0.0f, 1.0f);

	// バックファイアは平滑化前の生の入力で見る
	// (平滑化後だと踏み替えの鋭さが消えて鳴らない)
	UpdateBackfire(dt, rpm, thrRaw);

	// 生の値をそのまま使うと踏み替えのたびに音がパチンと切り替わる
	m_throttle += (thrRaw - m_throttle) * std::min(ThrottleSmooth * dt, 1.0f);
	m_rpm      += (rpm - m_rpm) * std::min(RpmSmooth * dt, 1.0f);

	if (!m_hasAny) { return; }

	// RPMで低回転／高回転を混ぜ替える
	float high = 0.0f, low = 0.0f;
	CrossFade(m_rpm, m_crossStart, m_crossEnd, high, low);

	// アクセルでオン／オフを混ぜ替える
	float on = 0.0f, off = 0.0f;
	CrossFade(m_throttle, 0.0f, 1.0f, on, off);

	ApplyLayer(m_onLow,   m_rpm, on  * low);
	ApplyLayer(m_onHigh,  m_rpm, on  * high);
	ApplyLayer(m_offLow,  m_rpm, off * low);
	ApplyLayer(m_offHigh, m_rpm, off * high);
}

void HjEngineSampler::DrawImGui()
{
	if (!m_hasAny)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
		                   U8("素材が見つかりません(Asset/Sound/engine_*.wav)"));
		return;
	}

	ImGui::SliderFloat(U8("全体音量"), &m_master, 0.0f, 1.0f);

	// ここがずれていると音程が合わない。素材を録った回転数を入れること
	ImGui::SeparatorText(U8("素材を録音した回転数"));
	ImGui::DragFloat(U8("オン・低回転"), &m_baseRpmOnLow,   10.0f, 500.0f, 9000.0f, "%.0f");
	ImGui::DragFloat(U8("オン・高回転"), &m_baseRpmOnHigh,  10.0f, 500.0f, 9000.0f, "%.0f");
	ImGui::DragFloat(U8("オフ・低回転"), &m_baseRpmOffLow,  10.0f, 500.0f, 9000.0f, "%.0f");
	ImGui::DragFloat(U8("オフ・高回転"), &m_baseRpmOffHigh, 10.0f, 500.0f, 9000.0f, "%.0f");

	ImGui::SeparatorText(U8("入れ替える回転域"));
	ImGui::DragFloat(U8("切り替え開始"), &m_crossStart, 10.0f, 500.0f, 9000.0f, "%.0f");
	ImGui::DragFloat(U8("切り替え完了"), &m_crossEnd,   10.0f, 500.0f, 9000.0f, "%.0f");

	ImGui::SeparatorText(U8("素材ごとの音量"));
	ImGui::SliderFloat(U8("オン・低"), &m_volOnLow,   0.0f, 1.5f);
	ImGui::SliderFloat(U8("オン・高"), &m_volOnHigh,  0.0f, 1.5f);
	ImGui::SliderFloat(U8("オフ・低"), &m_volOffLow,  0.0f, 1.5f);
	ImGui::SliderFloat(U8("オフ・高"), &m_volOffHigh, 0.0f, 1.5f);
	ImGui::SliderFloat(U8("バックファイア"), &m_backfireVolume, 0.0f, 1.5f);

	// 素材の基準RPMはメンバを直接見ているので、動かしたら反映し直す
	m_onLow.baseRpm   = m_baseRpmOnLow;   m_onLow.volume   = m_volOnLow;
	m_onHigh.baseRpm  = m_baseRpmOnHigh;  m_onHigh.volume  = m_volOnHigh;
	m_offLow.baseRpm  = m_baseRpmOffLow;  m_offLow.volume  = m_volOffLow;
	m_offHigh.baseRpm = m_baseRpmOffHigh; m_offHigh.volume = m_volOffHigh;

	ImGui::Separator();
	ImGui::Text(U8("回転 %.0f rpm / アクセル %.2f"), m_rpm, m_throttle);

	// ピッチが上限に張り付いていると、そこから先は音程が伸びない
	const float octLow  = log2f(std::max(m_rpm, 1.0f) / std::max(m_baseRpmOnLow, 1.0f));
	const float octHigh = log2f(std::max(m_rpm, 1.0f) / std::max(m_baseRpmOnHigh, 1.0f));
	ImGui::Text(U8("ピッチ 低%.2f / 高%.2f オクターブ (±1.0が限界)"), octLow, octHigh);
	if (fabsf(octLow) > 0.98f || fabsf(octHigh) > 0.98f)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
		                   U8("上限に張り付いています。素材の帯を増やすか基準RPMを見直してください"));
	}
}

void HjEngineSampler::CollectTuneParams(std::vector<std::pair<const char*, float*>>& out)
{
	out.push_back({ "smpMaster",     &m_master });
	out.push_back({ "smpRpmOnLow",   &m_baseRpmOnLow });
	out.push_back({ "smpRpmOnHigh",  &m_baseRpmOnHigh });
	out.push_back({ "smpRpmOffLow",  &m_baseRpmOffLow });
	out.push_back({ "smpRpmOffHigh", &m_baseRpmOffHigh });
	out.push_back({ "smpVolOnLow",   &m_volOnLow });
	out.push_back({ "smpVolOnHigh",  &m_volOnHigh });
	out.push_back({ "smpVolOffLow",  &m_volOffLow });
	out.push_back({ "smpVolOffHigh", &m_volOffHigh });
	out.push_back({ "smpCrossStart", &m_crossStart });
	out.push_back({ "smpCrossEnd",   &m_crossEnd });
	out.push_back({ "smpBackfire",   &m_backfireVolume });
}
