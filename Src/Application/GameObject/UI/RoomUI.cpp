#include "RoomUI.h"
#include "../../Input/HjKeyInput.h"
#include "../../Network/HjUdpTransport.h"

using namespace UIConst;
using namespace RoomConst;
namespace U = HjUI;

namespace
{
	float EntryX(int i) { return SideX + i * (EntryW + EntryGap); }
}

//----------------------------------------------------------
// 押された瞬間だけ真。押しっぱなしで連続入力にならないようにする。

void RoomUI::Update()
{
	U::BeginInput();

	switch (m_phase)
	{
	case Phase::Host:  UpdateHost(); break;
	case Phase::Join:  UpdateJoin(); break;
	default:           UpdateEntry(); break;
	}

	// ESCは一段戻る。Entryまで戻り切っていたら画面ごと戻す
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Cancel))
	{
		if (m_phase == Phase::Entry) { m_back = true; }
		else                         { m_phase = Phase::Entry; }
	}
}

void RoomUI::UpdateEntry()
{
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Left)) { m_sel = 0; }
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Right)) { m_sel = 1; }

	bool decide = HjKeyInput::Instance().Pressed(HjKeyInput::Key::Decide);

	for (int i = 0; i < 2; ++i)
	{
		if (U::Hover(EntryX(i), EntryY, EntryW, EntryH))   { m_sel = i; }
		if (U::Clicked(EntryX(i), EntryY, EntryW, EntryH)) { m_sel = i; decide = true; }
	}

	if (!decide) { return; }

	if (m_sel == 0)
	{
		m_phase = Phase::Host;
		// 自分のアドレスは、相手が打ち込むために画面へ出す必要がある。
		// 取得は毎フレームやるようなものではないので一度だけ。
		if (!m_localResolved)
		{
			m_localResolved = HjUdpTransport::GetLocalAddress(m_localAddress,
			                                                  sizeof(m_localAddress));
		}
	}
	else
	{
		m_phase = Phase::Join;
	}
}

void RoomUI::UpdateHost()
{
	// 準備完了の切り替え。通信が入れば相手へ送る値になるので、
	// 今のうちから同じ操作で持っておく
	if (HjKeyInput::Instance().Pressed(VK_SPACE)) { m_ready = !m_ready; }

	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Decide) ||
	    U::Clicked(1536.0f - SideX - StartW, FootY, StartW, FootH))
	{
		m_start = true;
	}
}

void RoomUI::UpdateJoin()
{
	UpdateAddressInput();

	// 何も打っていないうちは繋げない
	if (m_addressLen <= 0) { return; }

	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Decide) ||
	    U::Clicked(1536.0f - SideX - StartW, FootY, StartW, FootH))
	{
		m_start = true;
	}
}

//----------------------------------------------------------
// アドレス欄への文字入力。
// IPv4なので数字と「.」だけを受け付ける。
// 余計な文字を弾いておくと、繋ぐ直前まで間違いに気付けない事故が減る。
//----------------------------------------------------------
void RoomUI::UpdateAddressInput()
{
	if (HjKeyInput::Instance().Pressed(VK_BACK) && m_addressLen > 0)
	{
		m_address[--m_addressLen] = '\0';
	}

	if (m_addressLen >= AddressMax) { return; }

	auto push = [&](char ch)
	{
		m_address[m_addressLen++] = ch;
		m_address[m_addressLen]   = '\0';
	};

	for (int i = 0; i <= 9; ++i)
	{
		if (HjKeyInput::Instance().Pressed('0' + i) || HjKeyInput::Instance().Pressed(VK_NUMPAD0 + i))
		{
			push(static_cast<char>('0' + i));
			return;
		}
	}
	// 「.」はキーボードとテンキーのどちらからでも
	if (HjKeyInput::Instance().Pressed(VK_OEM_PERIOD) || HjKeyInput::Instance().Pressed(VK_DECIMAL)) { push('.'); }
}

//==========================================================
// 描画
//==========================================================
void RoomUI::DrawSprite()
{
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	sp.DrawBox(0, 0, ScreenW / 2, ScreenH / 2, &PAPER, true);

	DrawChrome();

	switch (m_phase)
	{
	case Phase::Host:  DrawHost(); break;
	case Phase::Join:  DrawJoin(); break;
	default:           DrawEntry(); break;
	}

	DrawFooter();
}

//----------------------------------------------------------
// 段階が変わっても動かない部分。
// 小さな装飾で余白を締めると、同じ配置でも間延びして見えなくなる。
//----------------------------------------------------------
void RoomUI::DrawChrome()
{
	// 上部の細い帯
	U::TextAt(FontStrip, SideX, StripY, "RUN TOGETHER.", INK);
	U::LineD(SideX, StripRuleY, SideX + StripRuleW, StripRuleY, 2.0f, INK);

	// 右上のバッジ。P2Pであることをここで示して、見出しは名前だけにする
	const float bx = 1536.0f - SideX - BadgeW;
	U::RectTL(bx, BadgeY, BadgeW, BadgeH, ACID);
	U::TextAt(FontSmall, bx + 16.0f, BadgeY + 8.0f, "P2P", INK);

	// 見出し。少し字を詰めて塊に見せる
	U::TextAtTracked(FontHead, TitleX, TitleY, "MULTIPLAYER", INK, TitleTrack);
	U::TextAt(FontFoot, TitleX, SubY, "DIRECT CONNECT  //  UP TO 4 DRIVERS", SUBTXT);

	// ここから下が中身、という区切り。枠を減らすかわりに罫線で締める
	U::LineD(SideX, HeadRuleY, 1536.0f - SideX, HeadRuleY, 2.0f, INK30);

	// 縦レール(右端・縦中央)
	U::TextRotated(FontFoot, RailX, RailY, "CONNECT // SHARE // SLIDE", INK, RailTrack);
}

//----------------------------------------------------------
// 入口。立てるか入るかの2枚。
// 選んだ側だけ塗り、選んでいない側は罫線だけにして差を面で見せる。
//----------------------------------------------------------
void RoomUI::DrawEntry()
{
	const char* title[2] = { "HOST",  "JOIN" };
	const char* sub[2]   = { "OPEN A ROOM ON THIS MACHINE",
	                         "CONNECT TO A ROOM ALREADY OPEN" };

	for (int i = 0; i < 2; ++i)
	{
		const float x = EntryX(i);
		const bool  on = (m_sel == i);

		if (on)
		{
			U::RectTL(x, EntryY, EntryW, EntryH, ACID);
		}
		else
		{
			// 枠で囲わず、下の罫線だけ。選択との差は塗りで付ける
			U::LineD(x, EntryY + EntryH, x + EntryW, EntryY + EntryH, 2.0f, INK30);
		}

		U::TextAt(FontHead, x + PadX, EntryY + ValueDy, title[i], INK);
		U::TextAt(FontFoot, x + PadX, EntryY + NoteDy, sub[i],
		        on ? INK : SUBTXT);
	}
}

//----------------------------------------------------------
// ホスト側。自分のアドレスを一番大きく出す。
// これを相手が打ち込むので、画面の中で一番読めないといけない。
//----------------------------------------------------------
void RoomUI::DrawHost()
{
	U::RectTL(SideX, EntryY, ContentW, EntryH, INK);

	// 待っている間だけ帯を流す。止まった画面に動きが無いと、
	// 待っているのか固まったのか区別が付かない。
	// 文字より先に敷くこと。後から描くと文字の上を帯が通って読めなくなる。
	U::FxScanBar(SideX + 2.0f, EntryY + 2.0f, ContentW - 4.0f, EntryH - 4.0f,
	             ACID, U::Time() * ScanSpeed);

	U::TextAt(FontFoot, SideX + PadX, EntryY + CaptionDy, "YOUR ADDRESS  //  TELL THIS TO THE OTHER DRIVERS", ACID);

	char line[64] = {};
	if (m_localResolved)
	{
		snprintf(line, sizeof(line), "%s:%u", m_localAddress, NetConst::DefaultPort);
	}
	else
	{
		// 取得できなかったことを黙って隠さない。原因を追える文言にする
		snprintf(line, sizeof(line), "%s", "NO NETWORK ADAPTER FOUND");
	}
	U::TextAt(FontHead, SideX + PadX, EntryY + ValueDy, line, PAPER);

	// 点が明滅していれば、待ち受けが動いていることが分かる
	U::TextAt(FontFoot, SideX + PadX, EntryY + NoteDy, "LISTENING", GREY9);
	U::DotFieldTwinkle(SideX + PadX + 92.0f, EntryY + NoteDy - 12.0f, 110.0f, 14.0f,
	                   ACID, U::Time());

	DrawSlots();
	DrawReadyBar();
}

//----------------------------------------------------------
// 参加側。アドレスの入力欄。
//----------------------------------------------------------
void RoomUI::DrawJoin()
{
	// 入力欄は塗らない。打ち込む場所だと分かるよう、値の下に線を引くだけ
	U::LineD(SideX, EntryY + EntryH, SideX + ContentW, EntryY + EntryH, 2.0f, INK30);

	U::TextAt(FontFoot, SideX + PadX, EntryY + CaptionDy, "HOST ADDRESS  //  NUMBERS AND DOTS ONLY", SUBTXT);

	// 入力中の文字＋点滅するカーソル。
	// カーソルが無いと、打てる状態なのかどうかが分からない
	char line[64] = {};
	const bool caretOn = fmodf(U::Time() * CaretBlink, 1.0f) < 0.5f;
	snprintf(line, sizeof(line), "%s%s", m_address, caretOn ? "_" : " ");
	U::TextAt(FontHead, SideX + PadX, EntryY + ValueDy, line, INK);

	U::LineD(SideX + PadX, EntryY + ValueDy + 18.0f,
	         SideX + PadX + 520.0f, EntryY + ValueDy + 18.0f, 2.0f, INK);

	char port[48] = {};
	snprintf(port, sizeof(port), "PORT %u", NetConst::DefaultPort);
	U::TextAt(FontFoot, SideX + PadX, EntryY + NoteDy, port, SUBTXT);
}

//----------------------------------------------------------
// 参加者のカードを1枚。
//
// 埋まっていれば紙で塗って枠を濃く、空きなら枠だけで薄く落とす。
// 塗りの有無で「入っている/空いている」が一目で分かるので、
// 文字を読まなくても部屋の埋まり具合がつかめる。
//----------------------------------------------------------
void RoomUI::DrawSlot(int index, bool filled, bool host, bool ready)
{
	const float x = SideX + index * (SlotW + SlotGap);
	const float y = SlotY;

	Math::Color edge = INK;
	if (!filled) { edge.w = 0.35f; }

	if (filled) { U::RectTL(x, y, SlotW, SlotH, WHITE); }
	U::FrameTL(x, y, SlotW, SlotH, 2.0f, edge);

	char num[8];
	snprintf(num, sizeof(num), "P%02d", index + 1);
	U::TextAt(FontTiny, x + SlotPad, y + SlotNumDy, num, SUBTXT);

	// HOSTの印。誰が部屋を持っているかは、抜けた時に誰へ移るかに関わる
	if (host)
	{
		const float hw = U::Measure(FontTiny, "HOST", 0.0f) / Scale + 16.0f;
		U::RectTL(x + SlotW - SlotPad - hw, y + SlotHostDy, hw, SlotHostH, INK);
		U::TextAt(FontTiny, x + SlotW - SlotPad - hw + 8.0f, y + SlotHostDy + 15.0f, "HOST", WHITE);
	}

	// 車の色見本。空きは枠だけにして、埋まる場所であることを示す
	const float sy = y + SlotSwatchDy;
	if (filled) { U::RectTL(x + SlotPad, sy, SlotW - SlotPad * 2.0f, SlotSwatchH, ACID); }
	else        { U::FrameTL(x + SlotPad, sy, SlotW - SlotPad * 2.0f, SlotSwatchH, 2.0f, edge); }

	U::TextAt(FontCard, x + SlotPad, y + SlotNameDy, filled ? "YOU" : "OPEN", filled ? INK : MUTE);
	U::TextAt(FontFoot, x + SlotPad, y + SlotNoteDy, filled ? "THIS MACHINE" : "WAITING FOR PLAYER", SUBTXT);

	if (!filled) { return; }

	// 準備完了。点いていればアシッド塗り、消えていれば枠だけ
	const char* rl = ready ? "READY" : "NOT READY";
	const float rw = U::Measure(FontTiny, rl, 0.0f) / Scale + 20.0f;
	const float rx = x + SlotW - SlotPad - rw;
	const float ry = y + SlotReadyDy;
	if (ready) { U::RectTL(rx, ry, rw, SlotReadyH, ACID); }
	else       { U::FrameTL(rx, ry, rw, SlotReadyH, 2.0f, INK); }
	U::TextAt(FontTiny, rx + 10.0f,
	          ry + CenterInBox(SlotReadyH, FontPx(FontTiny)), rl, INK);
}

void RoomUI::DrawSlots()
{
	// 通信が入るまでは自分の1枠だけ埋まっている
	for (int i = 0; i < NetConst::MaxPlayers; ++i)
	{
		const bool self = (i == 0);
		DrawSlot(i, self, self, self && m_ready);
	}
}

//----------------------------------------------------------
// 準備の進み具合。
// 何人中何人が押したかを、数と長さの両方で出す。
// 数字だけだと、あと何人待てばいいのかが感覚として入らない。
//----------------------------------------------------------
void RoomUI::DrawReadyBar()
{
	const int ready = m_ready ? 1 : 0;

	U::TextAt(FontFoot, SideX, ReadyY + 12.0f, "READY", INK);

	char n[16];
	snprintf(n, sizeof(n), "%d", ready);
	U::TextAt(FontRow, SideX + 62.0f, ReadyY + 12.0f, n, ACID);

	char t[16];
	snprintf(t, sizeof(t), "/ %d", NetConst::MaxPlayers);
	U::TextAt(FontRow, SideX + 84.0f, ReadyY + 12.0f, t, INK);

	const float bx = SideX + ReadyBarX;
	const float bw = ContentW - ReadyBarX - ReadyBarRight;
	U::RectTL(bx, ReadyY, bw, ReadyBarH, INK);
	U::RectTL(bx, ReadyY, bw * (ready / static_cast<float>(NetConst::MaxPlayers)),
	          ReadyBarH, ACID);
}

//----------------------------------------------------------
// 下段。戻ると決定。
//----------------------------------------------------------
void RoomUI::DrawFooter()
{
	U::Button(SideX, FootY, BackW, FootH, "BACK", U::BtnKind::Secondary, "ESC");

	// 準備完了の操作はキーだけなので、押せることを文字で出しておく
	if (m_phase == Phase::Host)
	{
		U::Keycap(SideX + BackW + 24.0f, FootY + 14.0f, "SPACE", "READY");
	}

	if (m_phase == Phase::Entry) { return; }

	// 参加側はアドレスを打つまで押せない。押せない状態を見た目で示す
	const bool ready = (m_phase == Phase::Host) || (m_addressLen > 0);
	const char* label = (m_phase == Phase::Host) ? "START" : "CONNECT";

	U::Button(1536.0f - SideX - StartW, FootY, StartW, FootH, label,
	          ready ? U::BtnKind::Primary : U::BtnKind::Disabled, "ENTER");
}
