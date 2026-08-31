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

	// 縁取り文字のずらし量(デザインpx)。3D画面の上では背景の明暗が
	// 毎フレーム変わるので、縁が無いと文字が読めなくなる
	const float OutlineOffset = 2.5f;

	//===== 箱の中に文字を置くときの上端 =====
	// 文字は上端が基準なので、箱の中で中央に見せるには
	// (箱の高さ - 文字の高さ) / 2 だけ下げる。
	// この計算を各画面で手打ちすると、箱や文字の大きさを変えるたびにずれる。
	constexpr float CenterInBox(float boxH, float textPx)
	{
		return (boxH - textPx) * 0.5f;
	}

	//===== ゲームのバージョン =====
	// 表示する場所が増えたときに食い違わないよう、文字列はここだけに持つ
	const char* const GameVersion = "Ver. 0.1.0";

	//===== タイトル上部のバッジ / 銘板 =====
	// 描画と当たり判定で同じ数値を使う。
	// 別々に書くと、片方だけ動かしたときに「見た目と押せる場所」がずれる。
	const float TitleBadgeY = 40.0f, TitleBadgeH = 34.0f;
	const float TitleBadgeNameX = 1262.0f, TitleBadgeNameW = 118.0f;
	const float TitleBadgeLvX = 1380.0f, TitleBadgeLvW = 70.0f;
	// 名前とレベルを合わせた範囲(ここ全体を押せるようにする)
	const float TitleBadgeAllX = TitleBadgeNameX;
	const float TitleBadgeAllW = (TitleBadgeLvX + TitleBadgeLvW) - TitleBadgeNameX;
	// レベルの進み具合を示す細い帯(バッジの下辺に敷く)
	const float TitleBadgeGaugeH = 3.0f;

	//===== 状態チップ =====
	// 点いていればアシッド塗り、消えていれば細い枠。
	// 面と線の差で状態を見せるので、色を増やさずに済む。
	// 文字の高さに対して上下の余白を広く取る。
	// 詰まっていると窮屈に見えるうえ、隣のチップとの境も曖昧になる。
	const float ChipH      = 42.0f;
	const float ChipPadX   = 16.0f;
	const float ChipTextDy = 17.0f;   // 文字のベースライン(チップ上端から)

	//===== 表の見出し帯 =====
	const float TableHeadH      = 44.0f;
	const float TableHeadTextDy = 28.0f;

	//===== 装飾 =====
	const float DotGridRadius = 1.6f;   // 等間隔の点の半径
	const float CropTickLen   = 5.0f;   // ガイド線の切れ目に置く印の長さ
	// バーコードの線幅。等間隔だと機械的な縞になって記号に見えないので、
	// 数種類を巡回させて「規則はあるが読めない」並びにする
	const float BarcodeWidths[3] = { 2.0f, 4.0f, 3.0f };
	const int   BarcodeWidthCount = 3;
	const float BarcodeGap = 3.0f;

	// ── 画面(1280x720, 中心原点) ──
	const int   ScreenW   = 1280;
	const int   ScreenH   = 720;
	const float HalfW     = 640.0f;
	const float HalfH     = 360.0f;

	// デザインは1536x864基準。1280x720へ一様スケール。
	const float DesignW   = 1536.0f;
	const float DesignH   = 864.0f;

	//===== 名前入力のダイアログ(画面中央) =====
	// バッジの中で直接打たせると、枠が狭くて今どこまで打ったか見えない。
	// 画面を暗く落として中央に大きく出し、入力だけに集中できるようにする。
	const float NameDlgW = 560.0f, NameDlgH = 220.0f;
	const float NameDlgX = (DesignW - NameDlgW) * 0.5f;
	const float NameDlgY = (DesignH - NameDlgH) * 0.5f;
	// 背景を落とす濃さ。真っ暗にすると元の画面が見えず、
	// どこから来たのか分からなくなる
	const float NameDlgDim = 0.72f;
	// 中身(ダイアログ左上からの距離)
	const float NameDlgPadX  = 36.0f;
	const float NameDlgCapDy = 34.0f;    // 小見出し
	const float NameDlgValDy = 84.0f;    // 入力中の文字
	const float NameDlgValPx = 47.0f;
	const float NameDlgRuleDy = 142.0f;  // 入力欄の下線
	const float NameDlgHintDy = 166.0f;  // 操作の案内

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
	const int   FontBtn   = 12;  // Archivo 15/800    (ボタン/トグル)
	const int   FontRow   = 13;  // Archivo 14/700    (設定行/バー)
	const int   FontTab   = 14;  // Archivo 17/800    (タブ/見出し)
	const int   FontHead  = 15;  // Archivo 47/900    (画面見出し)
	const int   FontCard  = 16;
	// 日本語・中国語が入りうる場所で使う(名前入力など)。
	// 欧文フォントはCJKのグリフを持たないので、別書体を割り当てる。
	// 描画側は字の大きさを変えられないので、用途ごとにサイズを分けて登録する
	const int   FontCJK      = 17;   // 名前入力(大)
	const int   FontCJKSmall = 18;   // バッジ(小)  // Archivo 34/900    (カード見出し・中サイズ)
	const int   FontCJKRow   = 19;   // 走行中のパネルの行
	const int   FontCJKHead  = 20;   // 走行中のパネルの見出し

	//===== フォントIDごとの実際の大きさ(px) =====
	// HjUI::Text の pxH は「縦位置を決める値」であって、
	// 文字の大きさは変えない。大きさはフォントIDで決まっている。
	// ここが食い違うと、文字が上下にずれて置かれる。
	// 呼ぶ側が数字を書かなくて済むよう、対応表をここに持つ。
	// (main.cpp の AddFont と対で管理すること)
	constexpr float FontPx(int fontId)
	{
		switch (fontId)
		{
		case 2:  return 125.0f;   // FontTitle
		case 3:  return 88.0f;    // FontSub
		case 4:  return 18.0f;    // FontMenu
		case 5:  return 12.0f;    // FontSmall
		case 6:  return 11.0f;    // FontStrip
		case 7:  return 11.0f;    // FontFoot
		case 8:  return 13.0f;    // FontNpTtl
		case 9:  return 10.0f;    // FontNpArt
		case 10: return 17.0f;    // FontSlash
		case 11: return 10.0f;    // FontTiny
		case 12: return 15.0f;    // FontBtn
		case 13: return 14.0f;    // FontRow
		case 14: return 17.0f;    // FontTab
		case 15: return 47.0f;    // FontHead
		case 16: return 30.0f;    // FontCard
		case 17: return 47.0f;    // FontCJK
		case 18: return 12.0f;    // FontCJKSmall
		case 19: return 15.0f;    // FontCJKRow
		case 20: return 17.0f;    // FontCJKHead
		default: return 14.0f;
		}
	}

	// ── 追加パレット ──
	const Math::Color ACID_HL = { 0.756f, 0.831f, 0.098f, 1.0f }; // 枠のアシッド(#cfe021濃色)
	const Math::Color NPBG    = { 0.984f, 0.984f, 0.976f, 1.0f }; // NOW PLAYING背景 #fbfbf9
	const Math::Color GREY9   = { 0.604f, 0.604f, 0.596f, 1.0f }; // #9a9a98
	const Math::Color MUTE    = { 0.541f, 0.541f, 0.533f, 1.0f }; // 無効ラベル #8a8a88
	// モーダルヘッダーのアクセント色
	const Math::Color PINK    = { 0.906f, 0.620f, 0.753f, 1.0f }; // #e79ec0
	const Math::Color BLUE    = { 0.290f, 0.592f, 0.722f, 1.0f }; // #4a97b8
	const Math::Color SAND    = { 0.788f, 0.761f, 0.706f, 1.0f }; // #c9c2b4

	// ── メニュー ──
	const int   MenuCount   = 5;
	const float MenuRowH    = 46.0f;   // 行の高さ(screen px)
	const float MenuWidth   = 340.0f;  // 行の幅
	const float DotCell     = 11.0f;   // ハーフトーンの点間隔(screen px)
	const float DotRadius   = 1.4f;    // 点の半径
}
