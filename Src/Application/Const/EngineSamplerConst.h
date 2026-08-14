#pragma once

// 録音素材によるエンジン音の定数。CarXと同じ構造。
//
// CarXはUpdate 1.7.0でFMODを導入し、「エンジン回転数によるイベントのトリム」
// ＝RPMに応じた素材の切り替えで50台以上の音を作っている。特別な合成はしていない。
//
// 仕組みは2軸のクロスフェード。
//
//              低回転の素材 ←─ RPM ─→ 高回転の素材
//   アクセルON       on_low              on_high
//   アクセルOFF      off_low             off_high
//
//   ・素材は全部を最初から鳴らしっぱなしにして、音量だけを動かす
//     (再生し直すと開始の遅れと途切れが出る)
//   ・現在のRPMと素材の基準RPMの比でピッチを変える
//   ・音量は「RPMの重み」×「オン/オフの重み」の掛け算
namespace EngineSamplerConst
{
	// 素材のパス。無いものは読み込みに失敗するだけで、残りは問題なく鳴る。
	// 1本だけでも動く。揃えるほど回転域ごとの質感が出る。
	constexpr const char* PathOnLow   = "Asset/Sound/engine_on_low.wav";
	constexpr const char* PathOnHigh  = "Asset/Sound/engine_on_high.wav";
	constexpr const char* PathOffLow  = "Asset/Sound/engine_off_low.wav";
	constexpr const char* PathOffHigh = "Asset/Sound/engine_off_high.wav";

	// 各素材を録音した時の回転数。ここからの比でピッチを決めるので、
	// 実際の素材に合わせて必ず設定すること。ずれていると音程が合わない。
	constexpr float BaseRpmOnLow   = 2000.0f;
	constexpr float BaseRpmOnHigh  = 5500.0f;
	constexpr float BaseRpmOffLow  = 2000.0f;
	constexpr float BaseRpmOffHigh = 5500.0f;

	// 低回転用と高回転用を入れ替える範囲。この間で滑らかに移る
	constexpr float CrossRpmStart = 3000.0f;
	constexpr float CrossRpmEnd   = 6000.0f;

	// ピッチ変更の上限(オクターブ)。
	// DirectXTKのSetPitchは -1〜+1 で周波数比 0.5〜2.0倍にあたるため、
	// これを超える指定はできない。張り付くようなら素材の帯を増やすこと。
	constexpr float PitchLimit = 1.0f;

	// 各素材の音量バランス
	constexpr float VolOnLow   = 0.90f;
	constexpr float VolOnHigh  = 1.00f;
	constexpr float VolOffLow  = 0.70f;
	constexpr float VolOffHigh = 0.80f;

	constexpr float MasterVolume = 0.70f;

	// アクセルのオン/オフの切り替わりの滑らかさ(1/s)。
	// 生の入力をそのまま使うと、踏み替えのたびに音がパチンと切り替わる。
	constexpr float ThrottleSmooth = 9.0f;
	constexpr float RpmSmooth      = 16.0f;

	// バックファイア。CarXは5種類以上を用意して個別に鳴らしている。
	// 同じ音が繰り返されると急に嘘っぽくなるので、複数から選ぶ。
	constexpr int   BackfireVariations = 5;
	constexpr const char* PathBackfire = "Asset/Sound/backfire_%d.wav";  // %dに1〜Nが入る
	constexpr float BackfireThrottleDrop = 0.4f;   // これ以上アクセルを戻したら鳴らす
	constexpr float BackfireMinRpm       = 3500.0f;// この回転以上でのみ鳴る
	constexpr float BackfireVolume       = 0.8f;
	constexpr float BackfireInterval     = 0.25f;  // 連続で鳴らさない最短間隔(秒)
}
