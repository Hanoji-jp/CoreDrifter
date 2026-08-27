#pragma once

// タイヤのスキール音とブレーキ鳴きの定数。
//
// どちらも「ノイズを共鳴させた音」なので、合成で素直に作れる。
// エンジン音のように音程を持つ音と違い、声っぽくなる問題が起きない。
//
// タイヤの「キーッ」は、路面に食いついては滑るのを高速で繰り返す
// スティックスリップ振動。だから「ほぼ音程のある鳴き」＋「擦れるノイズ」で
// できている。滑り方が激しいほど鳴きが強く、高くなる。
namespace TireAudioConst
{
	//===== 出力フォーマット =====
	constexpr int SampleRate   = 44100;
	constexpr int ChannelNum   = 1;
	constexpr int BlockSamples = 512;
	constexpr int QueuedBlocks = 4;

	//===== スキール(キーッ) =====
	// 鳴きの周波数。滑り量で上がる。共鳴を2つ重ねて厚みを出す
	constexpr float SquealBaseHz  = 950.0f;   // 滑り始めの高さ
	constexpr float SquealSlipHz  = 850.0f;   // 全開スリップで上がる量
	constexpr float SquealHarmonic = 2.35f;   // 2つ目の共鳴の倍率(整数比を外す)
	// 鋭さ。高いほど「キーッ」と細く鳴く。低いと「ゴーッ」と擦れる音寄り
	constexpr float SquealQ       = 14.0f;
	constexpr float SquealQ2      = 9.0f;
	constexpr float SquealLevel   = 0.55f;

	// 擦れるノイズ(路面を削る音)。鳴きだけだと電子音になる
	constexpr float ScrubLevel = 0.40f;
	constexpr float ScrubTone  = 0.30f;   // 大きいほど高くシャーッとする

	// 鳴きの高さの揺らぎ。完全に一定だとブザーになる
	constexpr float SquealWobble  = 0.06f;
	constexpr float SquealWobSpeed = 0.0008f;

	// 音が立ち上がる/消える速さ(1/s)。滑り出しは速く、収まりは緩やかに
	constexpr float SquealAttack  = 14.0f;
	constexpr float SquealRelease = 6.0f;

	// これ未満の滑りでは鳴らない(直進中に鳴きっぱなしにしない)
	constexpr float SquealSlipMin = 0.10f;
	// これ未満の速度では鳴らない(停車中に鳴かない)
	constexpr float SquealMinSpeed = 2.5f;

	//===== ブレーキ鳴き =====
	// ブレーキパッドの振動。タイヤの鳴きよりずっと高く、細い。
	// 低速で強く踏んだ時にだけ出る(高速では風切りとロータ温度で鳴かない)。
	constexpr float BrakeSquealHz    = 3400.0f;
	constexpr float BrakeSquealQ     = 30.0f;   // 非常に細い
	constexpr float BrakeSquealLevel = 0.30f;
	constexpr float BrakeMinForce    = 0.45f;   // これ以上踏んだら鳴る
	constexpr float BrakeMaxSpeed    = 14.0f;   // これ以上の速度では鳴らない(m/s)
	constexpr float BrakeAttack      = 9.0f;
	constexpr float BrakeRelease     = 7.0f;

	//===== 全体 =====
	constexpr float MasterVolume = 0.60f;
	constexpr float ClipLevel    = 0.90f;
}
