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

	// 出るときは下から持ち上げる。
	// その場で濃くなるより、下辺から立ち上がるほうが
	// 「下に張り付いているもの」として読める
	const float rise = UN::RiseY * (1.0f - m_appear);

	// 明るさの行き来。
	// パッと消えて出る点滅は視界の端で気が散るので、波で滑らかに往復させる。
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

	//===== 出す文字を決める =====
	const char* label = up.StateText();
	if (UN::ForceShow && (label == nullptr || label[0] == 0)) { label = UN::ForceText; }

	const std::string from = HjUpdater::GetCurrentVersion();
	std::string to = up.GetLatestVersion();

	// 試し表示のときは仮の番号を出す。
	// 空のままだと、一番見せたい所が抜ける
	if (UN::ForceShow && to.empty()) { to = UN::ForceToVer; }

	//===== 帯(画面の下端に張り付く) =====
	// 下地を先に敷く。
	// これが無いと、明るさが下がったときに帯そのものが消えて、
	// どこに何があったのか分からなくなる
	Math::Color track = UN::Glow;
	track.w = UN::TrackAlpha * m_appear;
	HjUI::RectTL(UN::BarX, UN::BarY + rise, UN::BarW, UN::BarH, track);

	// 受け取り中は進んだぶんだけ伸ばす。
	// それ以外は全長で、明るさだけを動かす
	const float ratio = busy ? std::clamp(up.GetProgress(), 0.0f, 1.0f) : 1.0f;

	Math::Color bar = UN::Glow;
	bar.w = glow * m_appear;
	HjUI::RectTL(UN::BarX, UN::BarY + rise, UN::BarW * ratio, UN::BarH, bar);

	//===== 一言 =====
	Math::Color lab = UN::Label;
	lab.w *= m_appear;
	HjUI::TextAt(UIConst::FontCJKRow, UN::TextX, UN::LabelY + rise, label, lab);

	//===== 版の番号(一番見せたいもの) =====
	if (to.empty()) { return; }

	Math::Color ver = UN::Ver;
	ver.w *= m_appear;

	// フォントに無い大きさなので、伸ばして出す。
	// 版の番号は英数字だけなので、欧文の書体で通る
	HjUI::TextScaled(UIConst::FontCard, UN::TextX, UN::VerY + rise,
	                 UN::VerPx, to.c_str(), ver);

	// いまの版を右へ小さく添える。どこから上がるのかが分かる。
	// 伸ばした後の幅は、元の幅に倍率を掛けたもの
	const float srcW  = HjUI::Measure(UIConst::FontCard, to.c_str(), 0.0f);
	const float srcPx = UIConst::FontPx(UIConst::FontCard);
	const float wDesign = (srcPx > 0.0f)
		? (srcW / UIConst::Scale) * (UN::VerPx / srcPx) : 0.0f;

	Math::Color sub = UN::Sub;
	sub.w *= m_appear;

	const std::string fromLine = std::string("FROM ") + from;
	HjUI::TextAt(UIConst::FontFoot,
	             UN::TextX + wDesign + UN::FromGap,
	             UN::VerY + UN::FromDy + rise,
	             fromLine.c_str(), sub);
}
