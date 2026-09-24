#pragma once

#include "RoadConst.h"

// 路面標示(白線)の定数。
//
// ■ 飾りではない
// いまの路面は一様な灰色の帯なので、カーブでどこに車を置いているのかが
// 読めない。ドリフトは進入位置がすべてなので、白線は計器として要る。
//
// ■ 道と同じ仕掛けで作る
// 断面を刻みごとに並べて押し出す。道の刻みの決め方(曲がりに合わせる)と
// ミター接合をそのまま借りるので、ヘアピンでも破綻しない。
//
// ■ 峠の線は黄色の実線
// センターラインが黄色の実線なのは追越禁止の意味。
// 白の破線にすると、追い越してよい道に見えてしまう
namespace RoadMarkConst
{
	//===== 位置 =====
	// 外側線は路面の縁から少し内側。
	// 縁ちょうどに置くと、裾との継ぎ目に線が乗って汚く見える
	constexpr float EdgeInset = 0.25f;
	constexpr float EdgeOffset = RoadConst::HalfWidth - EdgeInset;

	//===== 太さ(m) =====
	// 実物の外側線は15cm、センターラインは15cm。
	// 画面では細すぎて消えるので、少しだけ太らせる
	constexpr float EdgeWidth   = 0.18f;
	constexpr float CenterWidth = 0.20f;

	//===== 路面から浮かせる量(m) =====
	// 路面と同じ高さに置くと、どちらが手前か決まらずちらつく。
	// 浮かせすぎると、低い視点で線が路面から剥がれて見える
	constexpr float Lift = 0.02f;

	//===== 破線 =====
	// 峠のセンターラインは実線なので、既定では使わない。
	// 使うときの1本の長さと空きの長さ(m)
	constexpr bool  CenterDashed = false;
	constexpr float DashOn  = 5.0f;
	constexpr float DashOff = 5.0f;

	//===== 色 =====
	// 白は真っ白にしない。真っ白だと日向で飛んで、輪郭も潰れる
	constexpr float WhiteR = 0.88f, WhiteG = 0.88f, WhiteB = 0.86f;

	// センターラインの黄色。彩度を上げすぎると玩具に見える
	constexpr float YellowR = 0.86f, YellowG = 0.72f, YellowB = 0.22f;

	//===== 減速マーク =====
	// カーブの強さで、出すものが変わる。
	//
	// いろは坂を見ると、緩いカーブには何も無く、きつくなるほど
	// 外側だけ → 両車線 → センターラインごと消えて1車線、と
	// 段が上がっていく。一律に引くと、その段が消えて
	// 「どこも同じ危険度」に見える
	// 既定では出さない。
	//
	// いろは坂を見た限り、峠の路面に減速の帯は入っていない。
	// あれは交差点や料金所の手前のもので、山道のものではない。
	//
	// 仕組みは残す。市街地のステージを足すときに要る
	constexpr bool ShowSlowBars = false;

	// 段の境目(1mあたりのラジアン)。括弧内はおよその曲がり半径
	constexpr float TierMid     = 0.016f;   // R=62m  外側だけ
	constexpr float TierStrong  = 0.030f;   // R=33m  両車線にも出す
	constexpr float TierHairpin = 0.055f;   // R=18m  センターラインを切る

	// 何メートル手前から並べるか
	constexpr float SlowLead = 46.0f;

	// 間隔。遠い所から近い所へ、これだけ詰まる(m)
	constexpr float SlowGapFar  = 7.0f;
	constexpr float SlowGapNear = 1.7f;

	// 帯1本の、道に沿った長さ(m)
	constexpr float SlowBarLen = 0.45f;

	// 帯を引く範囲。中心線からの距離(m)。
	//
	// 片側の車線だけに引く。センターラインの外から、
	// 外側線の内側まで。どちらの線にも重ねない
	constexpr float SlowInner = CenterWidth * 0.5f + 0.15f;
	constexpr float SlowOuter = EdgeOffset - EdgeWidth * 0.5f - 0.1f;

	constexpr float SlowInnerWide = 0.0f;

	//===== センターラインが無くなる区間 =====
	// ヘアピンが続く所は、道幅が足りなくて1車線扱いになり、
	// センターラインが無くなる。
	//
	// 外側線は残る。あれは路肩との境を示すもので、車線を分ける線とは
	// 役目が違う。幅が足りなくても、路肩の位置は示す必要がある。
	//
	// ■ 連続していることが条件
	// ヘアピンが1つあるだけの道なら幅は足りているので、線は残る。
	// 消えるのは九十九折りになっている区間
	//
	// ヘアピン同士がこれ以内なら、ひと続きと見なす(m)
	constexpr float HairpinLinkDist = 140.0f;

	// ひと続きの中にヘアピンがこれだけあれば、センターラインを消す
	constexpr int HairpinRunMin = 2;

	// 消す区間を前後へどれだけ延ばすか(m)。
	// カーブの真上でぶつ切りにすると、消え方が唐突になる
	constexpr float NoMarkPad = 15.0f;

	// 並べる区間どうしの最小の間隔(m)
	constexpr float SlowMinGap = 40.0f;

	// 1つのカーブに並べる帯の上限。
	// 長い曲がりが続く道で、際限なく並べないための保険
	constexpr int SlowMaxBars = 60;

	// 強いカーブの帯は黄色。
	//
	// 日本の路面標示は 白=指示・区画 / 黄=規制。
	// きついカーブは追越禁止が掛かるので、その区間の標示は黄になる
	constexpr bool StrongUsesYellow = true;

	//===== 出す・出さない =====
	// センターラインは出さない。
	//
	// この峠は全体がヘアピン主体で、道幅も一定。
	// 区間ごとに消したり出したりするより、最初から無いほうが
	// 道として筋が通る(実際、こういう道にはセンターラインが無い)。
	//
	// 消す区間を決める仕組み(CenterCutRanges)は残してある。
	// 広い道のステージを足すときに要る
	constexpr bool ShowCenter = false;

	// 外側線は出す。路肩との境を示すもので、車線を分ける線とは
	// 役目が違う。1車線の道にもある
	constexpr bool ShowEdge   = true;
}
