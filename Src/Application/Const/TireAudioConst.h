#pragma once

// タイヤの鳴き(スキール音)の定数。
//
// 方式：ノイズ＋共鳴モードの並列。録音素材は使わない。
//
//   タイヤが鳴く仕組みは「スティックスリップ」。
//   接地面のトレッドブロックは、路面に貼り付いて→ねじれて→限界で滑って戻る、
//   を高速で繰り返す。この自励振動がゴムブロックとカーカスの共振周波数で
//   起こるので、鳴きの高さは「タイヤの構造」で決まる。
//
//   ここが効果音として重要な点で、
//     ・鳴きの高さは車速ではほとんど変わらない(ドップラーを除く)
//     ・滑る速さで変わるのは主に「音量」と「音の荒さ」
//   実際、路面を長く引きずったスキール音はずっと同じ高さで鳴り続ける。
//   車速で音程を動かすとサイレンになってタイヤに聞こえない。
//
//   さらに、滑りが深くなるとブロックが貼り付く時間そのものが無くなり、
//   鳴き(音程のある音)は「ゴーッ」という擦れ音(広い帯域のノイズ)へ移る。
//   ドリフト中の音が笛ではなく風切り音寄りになるのはこのため。
//   ここでは2層に分けて、滑り量でその配分を変える。
//
//     鳴き … ノイズ → 高いQの共鳴モード数本(音程が立つ)
//     擦れ … ノイズ → 広いバンドパス      (音程が立たない)
//
//   共鳴の周波数比は、わざと綺麗な整数比から外している。
//   整数比で並べると倍音列＝楽器の構造になり、音程のある「笛」に聞こえる。
namespace TireAudioConst
{
	//===== 出力フォーマット(エンジン音と同じ。XAudio2へ直接PCMを書く) =====
	constexpr int SampleRate   = 44100;
	constexpr int ChannelNum   = 1;      // モノラル(定位はしないので十分)
	constexpr int BlockSamples = 512;    // 約12msの遅延
	constexpr int QueuedBlocks = 4;      // 先行して溜めておく数(少ないと途切れる)

	//===== 車輪 =====
	constexpr int WheelNum  = 4;   // 前左 / 前右 / 後左 / 後右
	constexpr int AxleNum   = 2;   // 前軸 / 後軸
	constexpr int FrontAxle = 0;
	constexpr int RearAxle  = 1;
	// 各軸に属する車輪の番号(CarBase::StepTireForces の並びに合わせる)
	constexpr int AxleWheel[AxleNum][2] = { { 0, 1 }, { 2, 3 } };

	//===== 鳴きの共鳴 =====
	// モード数。1個だとただの笛、多すぎると音程が消えてノイズになる。
	// タイヤは数本の共振がはっきり立つので、この程度が実物に近い。
	constexpr int ModeCount = 4;
	// 一番低い共鳴の高さ。タイヤの構造で決まる値で、車速では動かさない。
	// 実測でも乗用車のスキールはおおむね 700〜1200Hz に芯がある。
	constexpr float SquealBaseHz = 820.0f;
	// 各モードの周波数比。整数比を避けて楽器っぽさを消す
	constexpr float ModeRatio[ModeCount] = { 1.0f, 1.47f, 2.09f, 2.76f };
	// 各モードの強さ。高いモードほど弱い
	constexpr float ModeLevel[ModeCount] = { 1.0f, 0.62f, 0.36f, 0.20f };
	// 共鳴の鋭さ(帯域幅Hz)。狭いほど長く響き、音程がはっきりする
	constexpr float ModeBandwidth     = 48.0f;
	// 高いモードほど速く減衰する(高音ほど早く失われる実際の性質)
	constexpr float ModeBandwidthRise = 30.0f;
	// 前軸は接地面が短く、後軸より少し高く鳴く。
	// 前後で高さをずらすと2本の音がわずかにうねり、1本の笛に聞こえなくなる。
	constexpr float FrontPitchMul = 1.11f;

	//===== 音程の動き =====
	// 滑る速さで上がる量(基準に対する比)。ごく浅くしか動かさない。
	// ここを大きくするとサイレンになり、タイヤに聞こえなくなる。
	constexpr float PitchSlipGain = 0.06f;
	// 荷重で上がる量。荷重が乗るとトレッドが潰れて張りが増し、わずかに高くなる
	constexpr float PitchLoadGain = 0.10f;
	// 音程のゆらぎ。スティックスリップは不安定で、実際の鳴きは常に揺れている。
	// 正弦で揺らすとビブラートになって電子音なので、なました乱数で不規則に揺らす。
	constexpr float WarbleDepth = 0.030f;   // 揺れ幅(比)
	// 揺れの速さ。共鳴の係数はブロックごとにしか組み直さないので、
	// ここもブロックごとの追従率で持つ(小さいほどゆっくり揺れる)。
	constexpr float WarbleSpeed = 0.09f;
	// 目標の高さへの追従。急に動かすと音程が飛んで聞こえる
	constexpr float PitchFollow = 0.25f;   // ブロックごとの追従率

	//===== 滑り量から音量へ =====
	// これ未満はトレッドが貼り付いている(弾性変形だけ)ので鳴かない
	constexpr float SlipStart = 0.9f;    // m/s
	// 鳴きが最大になる滑り速度
	constexpr float SlipFull  = 4.5f;    // m/s
	// 擦れ音(広い帯域)が出始める滑り速度
	constexpr float RoarStart = 2.5f;    // m/s
	// 擦れ音が最大になる滑り速度(ここまで来ると完全に「ゴーッ」)
	constexpr float RoarFull  = 13.0f;   // m/s
	// 擦れが増えたぶん鳴きを削る割合。貼り付く時間が無くなって音程が消える
	constexpr float RoarTakeover = 0.75f;

	//===== 荷重 =====
	constexpr float LoadRef = 0.25f;     // 4輪均等時の1輪ぶんの荷重
	// 荷重による音量の効き。荷重が抜けた側のタイヤは鳴りが細くなる
	constexpr float LoadMax = 1.4f;      // 荷重倍率の上限

	//===== 立ち上がり/消え方 =====
	// 鳴きは食い付いた瞬間に立ち上がり、抜ける時は少し尾を引く
	constexpr float AttackSpeed  = 16.0f;  // 1/s
	constexpr float ReleaseSpeed = 7.0f;   // 1/s

	//===== 擦れ音(広い帯域のノイズ) =====
	constexpr float ScrubCenterHz = 1700.0f;  // 帯域の中心
	constexpr float ScrubQ        = 1.4f;     // 広い(音程が立たない程度)

	//===== 出力 =====
	constexpr float SquealLevel  = 0.60f;  // 鳴きの音量
	constexpr float ScrubLevel   = 0.45f;  // 擦れの音量
	constexpr float MasterVolume = 0.45f;  // 全体音量
	constexpr float ClipLevel    = 0.85f;  // ソフトクリップの上限
}
