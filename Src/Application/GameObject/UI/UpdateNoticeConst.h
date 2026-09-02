#pragma once

#include "UIConst.h"

// タイトル画面の左下に出す、更新の知らせ。
//
// ■ どういう見せ方か
// 帯を画面の下端へ張り付け、その上に版の番号を大きく置く。
//
// 右下には既に黒帯があるので、そこへ突き当てて下辺を1本に繋げる。
// 途中で切れているより、端まで通っているほうが収まりがよい。
//
// ■ なぜ点滅ではなく明るさの行き来か
// パッと消えて出る点滅は、視界の端で起きると気が散る。
// タイトルは眺めている時間が長いので、
// 明るさを滑らかに往復させて、うるさくならないようにする。
//
// ■ なぜ左下か
// 右下は再生中の曲、右上は装飾、中央は3Dの窓が占めている。
// 左下は地図を外して空いたまま。
namespace UpdateNoticeConst
{
	//===== 試し表示 =====
	// 見た目を決めるための仮の設定。
	//
	// 本物の更新はGitHubにリリースを置かないと起きないので、
	// 置く前に見た目を見たいときはここを true にする。
	// 位置や明るさが決まったら false に戻すこと。
	//
	// ※true のままだと、更新が無くても出続ける
	constexpr bool ForceShow = true;
	constexpr const char* ForceText  = "新しい版があります";
	constexpr const char* ForceToVer = "v1.1.0";

	//===== 帯(画面の下端へ張り付ける) =====
	// 右下の黒帯が x=960 から始まるので、そこへ突き当てる。
	// 隙間を空けると、下辺が途中で切れて見える
	constexpr float BarX = 0.0f;
	constexpr float BarW = 960.0f;
	constexpr float BarH = 9.0f;
	constexpr float BarY = UIConst::DesignH - BarH;

	//===== 版の番号(帯の上) =====
	// ここが一番見せたいもの。文字を大きくして帯のすぐ上に置く
	constexpr float TextX = 64.0f;

	// 帯との間。詰めると帯が下線に見えて、番号の一部になる
	constexpr float VerY  = 782.0f;
	constexpr float VerPx = 34.0f;   // 番号の大きさ

	// 番号の上に置く一言。
	// 何の番号なのかが無いと、ただの数字になる
	constexpr float LabelY = 752.0f;

	// いまの版。番号の右に小さく添える。どこから上がるのかが分かる
	constexpr float FromGap = 16.0f;   // 番号の右端からの間
	constexpr float FromDy  = 18.0f;   // 番号の上端からの下げ

	//===== 明るさの行き来 =====
	// 1往復にかける時間(秒)。速いと急かされている感じになり、
	// 遅いと止まって見える
	constexpr float PulseSec = 1.9f;

	// 明るさの下限と上限。
	// 0まで落とすと消えたように見えて、点滅と変わらなくなる
	constexpr float GlowMin = 0.30f;
	constexpr float GlowMax = 1.0f;

	// 帯の後ろに敷く薄い下地。
	// これが無いと、明るさが下がったときに帯そのものが消える
	constexpr float TrackAlpha = 0.18f;

	// 受け取り中は、進んだぶんだけ帯を伸ばす。
	// そのときは行き来を止める(進み具合が読めなくなるため)
	constexpr float BusyGlow = 0.92f;

	//===== 出るまでの間 =====
	// 起動してすぐ出すと、画面が組み上がる前に現れてちらつく。
	// 少し置いてから、すっと出す
	constexpr float AppearDelay = 0.6f;
	constexpr float AppearSec   = 0.45f;

	// 出るときに下から持ち上げる量。
	// その場で濃くなるより下辺から立ち上がるほうが、
	// 「下に張り付いているもの」として読める
	constexpr float RiseY = 14.0f;

	//===== 色 =====
	// 主色(アシッド緑)。タイトルの他の要素と同じ色を使う。
	// ここだけ別の色にすると、知らせだけが浮く
	const Math::Color Glow  = UIConst::ACID;
	const Math::Color Ver   = UIConst::INK;
	const Math::Color Label = UIConst::INK;
	const Math::Color Sub   = UIConst::MUTE;
}
