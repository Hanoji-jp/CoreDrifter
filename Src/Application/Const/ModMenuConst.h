#pragma once

#include "../GameObject/UI/UIConst.h"

// MODメニューの見た目と配置。
//
// ■ どういう画面か
// 走行中に TAB で開く、画面左の縦長のパネル。
// 上下で選び、決定で階層を潜り、戻るで一段上がる。
//
// ■ なぜ他の画面と作りが違うか
// 設定やポーズは「画面を止めて開く」ものだが、これは走りながら触る。
// 見た目を差し替えて、その場で向きや大きさを合わせるための道具なので、
// 車が見えていないと合わせようがない。
//
// だから画面の左端に寄せて細くしてある。右側を空けて車を見せる。
//
// ■ 色が他の画面と違う理由
// 他の画面は紙色(明るい)地だが、ここは墨(暗い)地にしている。
// 走行画面の上へ重ねるので、明るい面を置くと路面の上で白飛びして、
// 後ろの車が見えなくなる。
namespace ModMenuConst
{
	//===== パネルの位置と大きさ =====
	constexpr float PanelX = 48.0f;
	constexpr float PanelY = 96.0f;
	constexpr float PanelW = 372.0f;

	//===== 各部の高さ =====
	constexpr float HeadH   = 54.0f;   // 見出し(いまいる階層の名前)
	constexpr float RowH    = 34.0f;   // 1行
	constexpr float FootH   = 34.0f;   // 何番目か・操作の案内
	constexpr float PadX    = 16.0f;   // 左右の余白

	// 一度に見せる行数。これを超えたら送る。
	// 画面の高さに対して長くなりすぎると、選んでいる所を見失う
	constexpr int   VisibleRows = 14;

	//===== 選択の示し方 =====
	// 選んでいる行は面で塗る。
	// 枠だけだと、走行画面の上では背景に紛れて見つけられない
	constexpr float MarkW = 4.0f;    // 左端の柱の太さ
	constexpr float ValueRightPad = 14.0f;   // 右寄せの値の右余白

	//===== 色 =====
	// 墨地。完全な不透明にはしない。
	// 後ろが少し透けるほうが、走っている感じが残る
	const Math::Color Panel   = { 0.055f, 0.055f, 0.055f, 0.92f };
	const Math::Color Head    = { 0.078f, 0.078f, 0.078f, 0.98f };
	const Math::Color RowSel  = UIConst::ACID;                      // 選択行の塗り
	const Math::Color TextOn  = UIConst::INK;                       // 選択行の文字(塗りの上なので墨)
	const Math::Color TextOff = { 0.870f, 0.870f, 0.855f, 1.0f };   // 通常の文字
	const Math::Color TextDim = { 0.560f, 0.560f, 0.548f, 1.0f };   // 値・注釈
	const Math::Color Line    = { 1.0f, 1.0f, 1.0f, 0.10f };        // 区切り
	const Math::Color Warn    = { 0.906f, 0.451f, 0.361f, 1.0f };   // 読み込めなかったとき

	//===== 開閉 =====
	// 開くときに横から出す。一瞬で現れると、
	// 何が起きたのか分からないまま視界の左が埋まる
	constexpr float SlideSec = 0.14f;
	constexpr float SlideFrom = -40.0f;   // この距離だけ左から寄せる

	//===== 調整の刻み =====
	// 押しっぱなしで動かすときの速さ。
	// 1回押しでは細かく、押し続けると速く動かないと、
	// 100倍のモデルを合わせるのに時間がかかりすぎる
	constexpr float HoldDelay = 0.35f;   // 押しっぱなしと見なすまで(秒)
	constexpr float HoldRate  = 14.0f;   // 押しっぱなし中の1秒あたりの回数

	//===== 調整できる幅 =====
	// 外から持ってきたモデルは100倍や1/100で来るので、倍率は広く取る
	constexpr float ScaleMin  = 0.001f;
	constexpr float ScaleMax  = 100.0f;
	constexpr float ScaleStep = 0.005f;

	constexpr float AngleMin  = -3.14159265f;
	constexpr float AngleMax  =  3.14159265f;
	constexpr float AngleStep = 0.01f;

	constexpr float OffsetMin  = -3.0f;
	constexpr float OffsetMax  =  3.0f;
	constexpr float OffsetStep = 0.005f;
}
