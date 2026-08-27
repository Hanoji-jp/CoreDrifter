#pragma once

// マルチプレイのルーム画面の配置。
// デザイン座標(1536x864, 左上原点)。
//
// 構成は「入口の2枚 → 主パネル → 参加者リスト → 下段のボタン」。
// 質感はタイトル画面に合わせる。
//   ・枠を減らし、罫線と余白で区切る
//   ・角は立てたまま、線は細く
//   ・小さな装飾(帯・バッジ・縦レール)で余白を締める
namespace RoomConst
{
	//===== 共通の余白 =====
	constexpr float SideX = 64.0f;
	constexpr float ContentW = 1536.0f - SideX * 2.0f;

	//===== 上部の細い帯(タイトル画面と同じ質感) =====
	constexpr float StripY = 46.0f;
	constexpr float StripRuleY = 66.0f, StripRuleW = 120.0f;
	// 右上のバッジ
	constexpr float BadgeW = 92.0f, BadgeH = 28.0f;
	constexpr float BadgeY = 38.0f;

	//===== 見出し =====
	// 大きな文字は指定した位置より下へ伸びる。副題を近づけすぎると足に潜り込む。
	constexpr float TitleX = 64.0f, TitleY = 108.0f;
	constexpr float TitleTrack = -0.03f * 60.0f;   // 少し詰めて塊に見せる
	constexpr float SubY   = 214.0f;
	// 見出しの下の罫線。ここから下が中身、という区切り
	constexpr float HeadRuleY = 236.0f;

	//===== 入口の2枚(HOST / JOIN) =====
	// 最初に決めるのが「立てるか入るか」なので、他より大きく取る。
	constexpr float EntryY = 276.0f, EntryH = 196.0f;
	constexpr float EntryGap = 28.0f;
	constexpr float EntryW = (ContentW - EntryGap) * 0.5f;

	//===== パネルの中身の位置(パネル上端からの距離) =====
	constexpr float PadX      = 32.0f;
	constexpr float CaptionDy = 30.0f;    // 小さい見出し
	constexpr float ValueDy   = 104.0f;   // 大きい値
	constexpr float NoteDy    = 156.0f;   // 補足

	//===== 参加者のカード =====
	// 1行に並べる。上限が4人なので、4×2に組むと下の段が丸ごと空く。
	// 空の段は「まだ入れる」ではなく「壊れている」ように見える。
	constexpr float SlotY = 470.0f;
	constexpr float SlotGap = 18.0f;
	constexpr float SlotW = (ContentW - SlotGap * 3.0f) / 4.0f;
	constexpr float SlotH = 208.0f;

	// カードの中身(カード左上からの距離)
	constexpr float SlotPad     = 18.0f;
	constexpr float SlotNumDy   = 26.0f;    // P01
	constexpr float SlotSwatchDy = 46.0f;   // 車の色見本
	constexpr float SlotSwatchH = 44.0f;
	constexpr float SlotNameDy  = 128.0f;
	constexpr float SlotNoteDy  = 176.0f;
	// 準備完了の印(カード右下)
	constexpr float SlotReadyDy = 168.0f, SlotReadyH = 26.0f;
	// HOSTの印(カード右上)
	constexpr float SlotHostDy  = 15.0f, SlotHostH = 22.0f;

	//===== 準備の進み具合 =====
	constexpr float ReadyY = 700.0f;
	constexpr float ReadyBarX = 194.0f, ReadyBarH = 14.0f;
	constexpr float ReadyBarRight = 220.0f;   // 右端に開ける幅(STARTのぶん)

	//===== 下段のボタン =====
	constexpr float FootY = 764.0f, FootH = 52.0f;
	constexpr float BackW = 200.0f, StartW = 300.0f;

	//===== 縦レール(右端・縦中央) =====
	constexpr float RailX = 1508.0f, RailY = 432.0f;
	constexpr float RailTrack = 0.18f * 13.0f;

	//===== アドレス入力 =====
	constexpr int   AddressMax = 15;      // "255.255.255.255"
	constexpr float CaretBlink = 2.0f;    // カーソルの点滅(1秒の往復回数)

	//===== 動き =====
	constexpr float ScanSpeed = 0.30f;    // 待機中に流れる帯
}
