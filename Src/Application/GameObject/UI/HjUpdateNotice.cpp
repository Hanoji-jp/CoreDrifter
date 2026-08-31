#include "HjUpdateNotice.h"

#include "HjUI.h"
#include "../../Updater/HjUpdater.h"

namespace UN = UpdateNoticeConst;

namespace
{
	// この状態のときだけ出す。
	// 「最新です」を出し続けると、伝えることが無いのに
	// 画面の隅が埋まったままになる
	bool ShouldShow(HjUpdater::State s)
	{
		// 見た目を決めている間は、状態に関わらず出す
		if (UN::ForceShow) { return true; }

		return s == HjUpdater::State::Available
		    || s == HjUpdater::State::Downloading
		    || s == HjUpdater::State::Ready;
	}
}

//----------------------------------------------------------
void HjUpdateNotice::Update()
{
	const float dt = KdFPSController::GetDt();

	if (!ShouldShow(HjUpdater::Instance().GetState()))
	{
		// 出す条件から外れたら、間の計測ごと畳む。
		// 残したままだと、次に条件を満たした瞬間に
		// 出るまでの間を飛ばして現れる
		m_age    = 0.0f;
		m_appear = 0.0f;
		return;
	}

	m_age += dt;
	if (m_age < UN::AppearDelay) { return; }

	const float speed = (UN::AppearSec > 0.0f) ? (dt / UN::AppearSec) : 1.0f;
	m_appear = std::min(m_appear + speed, 1.0f);
}

//----------------------------------------------------------
void HjUpdateNotice::DrawSprite()
{
	if (m_appear <= 0.0f) { return; }

	auto& up = HjUpdater::Instance();
	const HjUpdater::State st = up.GetState();
	if (!ShouldShow(st)) { return; }

	const bool busy = (st == HjUpdater::State::Downloading);

	// 明るさの行き来。
	// パッと消えて出る点滅は視界の端で気が散るので、
	// 波で滑らかに往復させる。
	// 受け取り中は止める(進んだ量が読めなくなるため)
	float glow = UN::BusyGlow;
	if (!busy)
	{
		constexpr float TwoPi = 6.28318530f;
		const float phase = (UN::PulseSec > 0.0f)
			? (HjUI::Time() / UN::PulseSec * TwoPi) : 0.0f;

		// sin は -1..1。0..1 に均してから、下限と上限の間へ移す
		const float wave = (sinf(phase) + 1.0f) * 0.5f;
		glow = UN::GlowMin + (UN::GlowMax - UN::GlowMin) * wave;
	}

	//===== 文字 =====
	Math::Color text = UN::Text;
	text.w *= m_appear;
	// 試し表示のときは、まだ何も起きていないので仮の文言を出す
	const char* label = up.StateText();
	if (UN::ForceShow && (label == nullptr || label[0] == 0)) { label = UN::ForceText; }

	HjUI::TextAt(UIConst::FontCJKRow, UN::X, UN::TextY, label, text);

	//===== 帯 =====
	// 下地を先に敷く。
	// これが無いと、明るさが下がったときに帯そのものが消えて、
	// どこに何があったのか分からなくなる
	Math::Color track = UN::Glow;
	track.w = UN::TrackAlpha * m_appear;
	HjUI::RectTL(UN::X, UN::BarY, UN::BarW, UN::BarH, track);

	// 受け取り中は、進んだぶんだけ伸ばす。
	// それ以外は全長で、明るさだけを動かす
	const float ratio = busy ? std::clamp(up.GetProgress(), 0.0f, 1.0f) : 1.0f;

	Math::Color bar = UN::Glow;
	bar.w = glow * m_appear;
	HjUI::RectTL(UN::X, UN::BarY, UN::BarW * ratio, UN::BarH, bar);

	//===== 添え字 =====
	// 版の番号。何が新しくなるのかが分かる
	Math::Color sub = UN::Sub;
	sub.w *= m_appear;

	std::string from = HjUpdater::GetCurrentVersion();
	std::string latest = up.GetLatestVersion();

	// 試し表示のときは仮の番号を出す。
	// 空のままだと、帯の下が抜けて間延びして見える
	if (UN::ForceShow && latest.empty())
	{
		from   = UN::ForceFromVer;
		latest = UN::ForceToVer;
	}

	if (!latest.empty())
	{
		const std::string line = from + "  >  " + latest;
		HjUI::TextAt(UIConst::FontFoot, UN::X, UN::SubY, line.c_str(), sub);
	}
}
