#include "DriftScore.h"
#include "../Car/CarBase.h"
#include "../UI/HjToastQueue.h"
#include "../UI/HjUiVisibility.h"

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
	m_active = drift;   // 今この瞬間の成立。猶予の残りを出すのに使う

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
	}
	else if (m_drifting)
	{
		// ドリフトが途切れた：猶予内に再開しなければチェーン確定
		m_graceTimer -= dt;
		if (m_graceTimer <= 0.0f)
		{
			const double banked = m_chain * m_combo;
			m_total += banked;

			// GREAT に届かない程度のドリフトでいちいち文字が出ると、
			// 走っている間ずっと何か出ている状態になって重みが無くなる。
			// 出す価値のある2段だけにする。
			m_judge = (banked >= PerfectScore) ? Judge::Perfect
			        : (banked >= GreatScore)   ? Judge::Great
			        : Judge::None;
			m_judgeText = (m_judge == Judge::Perfect) ? "PERFECT"
			            : (m_judge == Judge::Great)   ? "GREAT" : "";
			if (m_judge != Judge::None)
			{
				m_judgeTimer = BankFlash;
				m_judgeFresh = true;   // 文字の焼き直しは出す瞬間の1回だけ
				// 上位の判定ほど強く光らせる。同じ見た目だと段の意味が伝わらない
				m_flash = (m_judge == Judge::Perfect) ? 1.0f : 0.6f;
			}

			// 通知へ積む。
			// 出来事を起こす側と見せる側を直接つながないことで、
			// 画面の作りを変えても採点に手が入らない。
			if (m_judge != Judge::None)
			{
				char v[24];
				snprintf(v, sizeof(v), "+%d", static_cast<int>(banked));
				HjToastQueue::Instance().Push("CHAIN", "CHAIN BANKED", v, true);
			}
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

	// 判定・フラッシュの減衰
	m_judgeTimer = std::max(0.0f, m_judgeTimer - dt);
	m_flash = std::max(0.0f, m_flash - dt / FlashTime);
}


//----------------------------------------------------------
// 判定の段階ごとの色。上位ほど派手にして、段の違いを目で分からせる。
//----------------------------------------------------------
Math::Color DriftScore::JudgeColor(Judge j) const
{
	switch (j)
	{
	case Judge::Perfect:
	{
		// 最上位だけ虹。ここぞという時にしか出ない色にしておく
		const float t = HjUI::Time() * 3.0f;
		return Math::Color(0.5f + 0.5f * sinf(t),
		                   0.5f + 0.5f * sinf(t + 2.094f),
		                   0.5f + 0.5f * sinf(t + 4.188f), 1.0f);
	}
	default:           return UIConst::ACID;   // GREAT
	}
}

//----------------------------------------------------------
// ドリフトが成立していることを示すバナーと、横滑り角のバー。
//
// 数字が伸びているだけでは「何をした結果なのか」が読み取れない。
// 今どれだけ角度が付いているかを同時に見せると、
// 角度を保つ操作と点の伸びが結び付く。

//----------------------------------------------------------
// 伸びているスコアとコンボ倍率。

//----------------------------------------------------------
// NICE/GREAT/PERFECT の叩きつけ。
//
// 単なるフェードでは「当たった」感じが出ない。
//   ① 大きい状態から一気に縮めて着弾させる
//   ② 着弾の瞬間だけ揺らす＋輪を広げる
//   ③ 最後に抜けていく
// 動きの速さが変わる瞬間を作ることで、そこに衝撃があったと感じる。
//----------------------------------------------------------
void DriftScore::DrawJudge()
{
	using namespace UIConst;
	using namespace DriftScoreConst;
	namespace U = HjUI;

	auto& pp = KdShaderManager::Instance().m_postProcessShader;

	if (m_judgeTimer <= 0.0f || m_judge == Judge::None)
	{
		pp.HideFluidJudge();
		return;
	}

	// 経過(0=出た瞬間 1=消える)
	const float p = std::clamp(1.0f - m_judgeTimer / BankFlash, 0.0f, 1.0f);

	//----- ① 叩きつけ -----
	// 終盤を急に緩めると、ぶつかって止まったように見える
	float scale = 1.0f;
	const float slamP = JudgeSlamTime / BankFlash;
	if (p < slamP)
	{
		const float t = p / slamP;
		const float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
		scale = JudgeSlamScale + (1.0f - JudgeSlamScale) * ease;
	}

	//----- ③ 抜ける -----
	float alpha = 1.0f;
	if (p > 1.0f - JudgeOutRatio)
	{
		alpha = std::clamp((1.0f - p) / JudgeOutRatio, 0.0f, 1.0f);
		scale *= 0.9f + 0.1f * alpha;
	}

	//----- ② 着弾の揺れ -----
	// 減衰させながら細かく揺らす。等速で揺らすと機械的に見える
	float sx = 0.0f, sy = 0.0f;
	const float shakeP = m_judgeTimer - (BankFlash - JudgeShakeTime);
	if (shakeP > 0.0f)
	{
		const float k = shakeP / JudgeShakeTime;
		const float t = HjUI::Time() * 60.0f;
		sx = sinf(t * 1.7f) * JudgeShakeAmp * k * k;
		sy = sinf(t * 2.3f) * JudgeShakeAmp * k * k * 0.6f;
	}

	// 広がる輪。衝撃の大きさを目で示す
	const float ringP = std::clamp(p / (RingTime / BankFlash), 0.0f, 1.0f);
	if (ringP < 1.0f)
	{
		Math::Color rc = JudgeColor(m_judge);
		rc.w = (1.0f - ringP) * 0.55f;
		U::RingD(JudgeCx, JudgeCy, RingRadius * ringP, rc);
	}

	// 判定は既存の文字流体化へ流す。
	// 文字を焼いたRTをドメインワープでウネらせる仕組みが既にあるので、
	// 一番の見せ場である判定にはそちらを使う。
	{
		if (m_judgeFresh)
		{
			const int style = (m_judge == Judge::Perfect) ? FluidStylePerfect : FluidStyleGreat;
			// 炎は芯が白熱→外が橙。虹スモークは色が自分で回るので芯だけ白く置く
			const Math::Vector4 core  = { 1.0f, 0.96f, 0.72f, 1.0f };
			const Math::Vector4 fluid = (m_judge == Judge::Perfect)
			                          ? Math::Vector4{ 0.85f, 0.95f, 0.30f, 1.0f }
			                          : Math::Vector4{ 1.0f, 0.42f, 0.08f, 1.0f };
			pp.SetFluidJudge(m_judgeText, style, core, fluid);
			m_judgeFresh = false;
		}
		// 叩きつけと揺れは表示枠の大きさ・位置で作る
		pp.UpdateFluidJudge(alpha,
		                    FluidCX + sx * FluidShake / JudgeShakeAmp,
		                    FluidCY + sy * FluidShake / JudgeShakeAmp,
		                    FluidW * scale, FluidH * scale);
	}

}

//----------------------------------------------------------
// 右上の総合スコア。

//----------------------------------------------------------
// 画面全体の演出＋各パーツ。
// 1つの関数に演出を詰め込まず、役割ごとに分けて呼ぶ。
//----------------------------------------------------------
void DriftScore::DrawSprite()
{
	if (!HjUiVisibility::Instance().showJudge)
	{
		// 判定文字は文字流体化(DrawFluidText)が別経路で描いており、
		// ここで return しただけでは消えない。明示的に止める。
		KdShaderManager::Instance().m_postProcessShader.HideFluidJudge();
		return;
	}

	using namespace DriftScoreConst;
	namespace U = HjUI;

	// ドリフト中ビネット(画面端を暗く)
	U::Vignette(m_driftIntensity * VignetteMax);

	DrawJudge();

	// 着弾の瞬間だけ画面を白く飛ばす。
	// 最後に描くことで、文字ごと一瞬持っていかれる
	if (m_flash > 0.001f)
	{
		Math::Color fc = UIConst::WHITE;
		fc.w = m_flash * m_flash * FlashMax;
		U::RectTL(0.0f, 0.0f, UIConst::DesignW, UIConst::DesignH, fc);
	}
}
