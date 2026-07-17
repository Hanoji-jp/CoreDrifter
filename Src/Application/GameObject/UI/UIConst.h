#pragma once

// Drift Project UI(claude.ai/design由来)の配色・レイアウト定数。
// ブルータリスト系：アシッド緑 × インク黒 × オフホワイト、0角丸・2px罫線・左寄せ。
namespace UIConst
{
	// ── パレット(0..1) ──
	const Math::Color ACID   = { 0.812f, 0.878f, 0.129f, 1.0f };  // #cfe021 主色(アシッド緑)
	const Math::Color INK    = { 0.078f, 0.078f, 0.078f, 1.0f };  // #141414 墨(黒)
	const Math::Color PAPER  = { 0.945f, 0.941f, 0.925f, 1.0f };  // #f1f0ec 紙(背景)
	const Math::Color GREY   = { 0.776f, 0.776f, 0.776f, 1.0f };  // #c6c6c6 副色(灰)
	const Math::Color WHITE  = { 1.0f, 1.0f, 1.0f, 1.0f };
	const Math::Color SUBTXT = { 0.333f, 0.333f, 0.333f, 1.0f };  // #555 小さめ文字
	const Math::Color INK30  = { 0.078f, 0.078f, 0.078f, 0.30f }; // 罫線(墨30%)
	const Math::Color DOTS   = { 0.078f, 0.078f, 0.078f, 0.42f }; // ハーフトーン点

	// ── 画面(1280x720, 中心原点) ──
	const int   ScreenW   = 1280;
	const int   ScreenH   = 720;
	const float HalfW     = 640.0f;
	const float HalfH     = 360.0f;

	// デザインは1536x864基準。1280x720へ一様スケール。
	const float DesignW   = 1536.0f;
	const float DesignH   = 864.0f;
	const float Scale     = 1280.0f / 1536.0f;   // = 0.8333

	// ── フォントID(main.cppでArchivoを登録) ──
	const int   FontTitle = 2;   // Archivo Black 125 (DRIFT)
	const int   FontSub   = 3;   // Archivo Black 88  (PROJECT)
	const int   FontMenu  = 4;   // Archivo 18/800    (メニュー)
	const int   FontSmall = 5;   // Archivo 12/800    (タグライン/バッジ)
	const int   FontStrip = 6;   // Archivo 11/800    (トップ帯)
	const int   FontFoot  = 7;   // Archivo 11/700    (フッター/小見出し)
	const int   FontNpTtl = 8;   // Archivo 13/800    (NOW PLAYING曲名)
	const int   FontNpArt = 9;   // Archivo 10/600    (NOW PLAYINGアーティスト)
	const int   FontSlash = 10;  // Archivo 17/900    ("///")
	const int   FontTiny  = 11;  // Archivo 10/700    (極小)

	// ── 追加パレット ──
	const Math::Color ACID_HL = { 0.756f, 0.831f, 0.098f, 1.0f }; // 枠のアシッド(#cfe021濃色)
	const Math::Color NPBG    = { 0.984f, 0.984f, 0.976f, 1.0f }; // NOW PLAYING背景 #fbfbf9
	const Math::Color GREY9   = { 0.604f, 0.604f, 0.596f, 1.0f }; // #9a9a98

	// ── メニュー ──
	const int   MenuCount   = 5;
	const float MenuRowH    = 46.0f;   // 行の高さ(screen px)
	const float MenuWidth   = 340.0f;  // 行の幅
	const float DotCell     = 11.0f;   // ハーフトーンの点間隔(screen px)
	const float DotRadius   = 1.4f;    // 点の半径
}
