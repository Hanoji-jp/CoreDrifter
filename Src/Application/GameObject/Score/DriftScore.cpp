#include "DriftScore.h"
#include "../Car/CarBase.h"

using namespace DriftScoreConst;

void DriftScore::Update()
{
	auto car = m_wpCar.lock();
	if (!car) { return; }

	const float dt = KdFPSController::GetDt();

	// ── 横滑り角(sideslip)と速度を車から算出 ──
	const Math::Vector3 v = car->GetVel();
	const float yaw = car->GetYaw();
	const Math::Vector3 fwd(sinf(yaw), 0.0f, cosf(yaw));
	const Math::Vector3 right(cosf(yaw), 0.0f, -sinf(yaw));
	const float vLong = v.Dot(fwd);
	const float vLat  = v.Dot(right);
	const float speed = v.Length();                                  // m/s
	const float speedKmh = speed * 3.6f;
	const float angleDeg = fabsf(atan2f(vLat, fabsf(vLong) + 1.0f)) * 57.29578f;

	const bool drift = (speedKmh > MinSpeedKmh && angleDeg > MinAngleDeg);

	if (drift)
	{
		m_drifting = true;
		m_graceTimer = GraceTime;
		m_chainTime += dt;
		// 角度×速度×時間で加点
		const double add = static_cast<double>(angleDeg) * speed * ScoreRate * dt;
		m_chain += add;
		// 継続時間で倍率アップ
		m_combo = std::min(ComboMax, 1 + static_cast<int>(m_chainTime / ComboStep));
		// 加点ぶんパンチを盛る
		m_punch = std::min(PunchMax, m_punch + static_cast<float>(add) * PunchGain);
	}
	else if (m_drifting)
	{
		// ドリフトが途切れた：猶予内に再開しなければチェーン確定
		m_graceTimer -= dt;
		if (m_graceTimer <= 0.0f)
		{
			const double banked = m_chain * m_combo;
			m_total += banked;
			m_judgeText  = (banked >= PerfectScore) ? "PERFECT!"
			             : (banked >= GreatScore)   ? "GREAT!"
			             : (banked >= NiceScore)    ? "NICE"
			             : "";
			if (m_judgeText[0] != '\0') { m_judgeTimer = BankFlash; }
			m_punch = BankPunch;
			// リセット
			m_chain = 0.0; m_combo = 1; m_chainTime = 0.0f; m_drifting = false;
		}
	}

	// ドリフト強度(ビネット用)：角度と速度から0..1を作り平滑化
	float target = 0.0f;
	if (drift)
	{
		const float a = std::clamp((angleDeg - MinAngleDeg) / VignetteAngle, 0.0f, 1.0f);
		const float s = std::clamp(speedKmh / VignetteSpeed, 0.3f, 1.0f);
		target = a * s;
	}
	m_driftIntensity += (target - m_driftIntensity) * std::min(1.0f, dt * VignetteSmooth);

	// パンチ・判定フラッシュの減衰
	m_punch = std::max(0.0f, m_punch - PunchDecay * dt * std::max(m_punch, 0.15f));
	m_judgeTimer = std::max(0.0f, m_judgeTimer - dt);
}

Math::Color DriftScore::ComboColor(int combo) const
{
	if (combo <= 2) { return UIConst::WHITE; }                       // 白
	if (combo <= 5) { return Math::Color(1.0f, 0.84f, 0.20f, 1.0f); } // 金
	// 6以上＝虹(時間で色相を回す)
	const float t = HjUI::Time() * 1.5f;
	const float r = 0.5f + 0.5f * sinf(t);
	const float g = 0.5f + 0.5f * sinf(t + 2.094f);
	const float b = 0.5f + 0.5f * sinf(t + 4.188f);
	return Math::Color(r, g, b, 1.0f);
}

void DriftScore::DrawSprite()
{
	using namespace UIConst;
	namespace U = HjUI;

	// ── ドリフト中ビネット(画面端を暗く) ──
	U::Vignette(m_driftIntensity * VignetteMax);

	// ── ライブスコア(現チェーン×倍率) ──
	if (m_drifting || m_chain > 0.0)
	{
		const int live = static_cast<int>(m_chain * m_combo);
		char buf[32]; snprintf(buf, sizeof(buf), "%d", live);
		const Math::Color col = ComboColor(m_combo);
		U::TextCenteredScaled(FontHead, LiveCx, LiveCy, 1.0f + m_punch, buf, col);

		// コンボ倍率
		if (m_combo > 1)
		{
			char cb[16]; snprintf(cb, sizeof(cb), "x%d", m_combo);
			U::TextCenteredScaled(FontCard, ComboCx, ComboCy, 1.0f + m_punch * 0.5f, cb, col);
		}
	}

	// ── GREAT/PERFECT フラッシュ ──
	if (m_judgeTimer > 0.0f)
	{
		const float a = std::clamp(m_judgeTimer / BankFlash, 0.0f, 1.0f);
		Math::Color jc = ACID; jc.w = a;
		const float pop = 1.0f + (1.0f - a) * 0.5f;   // 出た瞬間だけ大きく
		U::TextCenteredScaled(FontHead, JudgeCx, JudgeCy, pop, m_judgeText, jc);
	}

	// ── 総合スコア(右上) ──
	char tb[40]; snprintf(tb, sizeof(tb), "%d", static_cast<int>(m_total));
	U::Text(FontFoot, TotalCx - 40.0f, TotalCy - 26.0f, 11.0f, "TOTAL", SUBTXT);
	U::TextCenteredScaled(FontTab, TotalCx, TotalCy, 1.0f, tb, INK);
}
