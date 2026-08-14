#pragma once

// エンジン音の定数。
//
// 方式：エンジンオーダー加算合成。
//
//   人間がエンジン音だと認識しているのは「倍音の並び方」なので、
//   その倍音列を直接鳴らす。
//
//     基本周波数 f0 = RPM / 120     … 4ストロークのクランク1サイクル
//     出力 = Σ A(k) × sin(2π × f0 × k × t)
//
//   k は「オーダー」と呼ばれ、どのオーダーが強いかが気筒配置そのもの。
//     直4 … k=4 が主(点火倍音) / 直6 … k=6 / V8 … k=8
//   点火倍音の「間」に挟まる中間のオーダーが、ざらつきとうねりを作る。
//   V8クロスプレーンの「ドロドロ」は、この中間オーダーが強いことによる。
//
//   位相が連続なので、回転に音程がぴったり追従し、途切れも潰れもしない。
//   1発ずつ波形を並べる方式(グレイン)と違い、
//   重なりによる音量の暴れやクリップも起きない。
namespace EngineAudioConst
{
	//===== 出力フォーマット =====
	constexpr int SampleRate   = 44100;
	constexpr int ChannelNum   = 1;      // モノラル(定位はしないので十分)
	// 1回に書き込むサンプル数。小さいほど遅延が減るが、書き込み回数が増える
	constexpr int BlockSamples = 512;    // 約12msの遅延
	// 先行して溜めておくブロック数。少ないと音が途切れる
	constexpr int QueuedBlocks = 4;

	//===== 倍音 =====
	// 何次まで鳴らすか。実際に鳴らす数は「f0 × k が可聴上限を超えない範囲」で
	// 打ち切る(超えると折り返し雑音になって耳に刺さる)。
	constexpr int MaxHarmonics = 48;
	// 倍音の減り方。大きいほど高次が弱まり、丸くこもった音になる
	constexpr float Rolloff = 1.15f;
	// 点火倍音(k=気筒数の倍数)をどれだけ強調するか。エンジンらしさの芯
	constexpr float FiringBoost = 1.0f;
	// アクセルオフで高次がどれだけ失われるか。踏むと音が明るく開く
	constexpr float OffRolloffAdd = 0.9f;

	// 倍音ごとの微妙な揺らぎ。完全に静止した倍音列は電子オルガンになる。
	// ※正弦波で揺らすと、その揺れ自体が規則的なので「うねる電子音」になる。
	//   不規則な揺らぎ(ゆっくりしたランダム)でないと機械っぽさが抜けない。
	constexpr float HarmonicWobble  = 0.55f;   // 揺らぎの深さ(0=なし)
	constexpr float HarmonicWobbleSpeed = 0.0020f; // 揺らぎの速さ(小さいほどゆっくり)

	//===== 回転のゆらぎ =====
	// 実際のエンジンは、燃焼のばらつきとクランクのねじれ振動で
	// 回転数が常に微妙に揺れている。周波数が完全に一定だと電子音になる。
	// 全倍音がまとめて同じ比率で揺れるので、「生き物っぽさ」が一気に出る。
	constexpr float RpmJitterAmount = 0.014f;   // 揺れ幅(回転数に対する比)
	constexpr float RpmJitterSpeed  = 0.0011f;  // 揺れの速さ(小さいほどゆっくり)

	//===== ノイズの脈動 =====
	// 排気の乱流は垂れ流しではなく、点火のたびに吹き出す。
	// 一定のノイズを混ぜるだけだと「シャー」というだけで生気がない。
	constexpr float NoisePulseDepth = 0.75f;   // 脈動の深さ(0=一定)
	constexpr float NoisePulseSharp = 2.5f;    // 立ち上がりの鋭さ

	//===== ノイズ(吸排気の乱流) =====
	// 倍音だけだと機械的なので、空気の音を足す。回転が上がるほど強くなる。
	constexpr float NoiseLevel   = 0.30f;
	constexpr float NoiseRpmGain = 0.8f;   // 回転で増える量
	constexpr float NoiseTone    = 0.25f;  // 0に近いほど低く籠もったノイズ

	//===== マフラー(共鳴フィルタ) =====
	// 管とマフラーの通過を、レゾナンス付きローパスで近似する。
	// アクセルを踏むとカットオフが上がる＝音が開く。これが「抜け」の正体。
	constexpr float CutoffBase    = 420.0f;  // アイドルでのカットオフ(Hz)
	constexpr float CutoffRpmGain = 2600.0f; // 高回転で上がる量(Hz)
	constexpr float CutoffThrGain = 1500.0f; // 全開で上がる量(Hz)
	constexpr float Resonance     = 1.35f;   // 1=素直 大きいほど管が鳴く

	//===== 音量 =====
	constexpr float MasterVolume      = 0.40f;
	constexpr float OffThrottleVolume = 0.55f;  // アクセルオフでの音量倍率
	constexpr float RpmVolumeGain     = 0.35f;  // 高回転ほど大きくする量

	// アクセル・回転数の追従の滑らかさ(1/s)
	constexpr float ThrottleSmooth = 9.0f;
	constexpr float RpmSmooth      = 16.0f;

	// 出力の上限。ここで切るのではなく、緩やかに寝かせる(ソフトクリップ)
	constexpr float ClipLevel = 0.90f;

	//===== ターボ =====
	// スプール音(ヒューン)は、タービンの回転数に応じた高い周波数の音。
	// タービンは排気で回るので、回転数とアクセルに遅れて追従する。
	// この「遅れ」がターボらしさで、踏んですぐには鳴らない。
	constexpr float SpoolUpSpeed   = 1.6f;   // 立ち上がりの速さ(1/s)。遅いほどドッカン
	constexpr float SpoolDownSpeed = 2.8f;   // 落ちる速さ。踏み替えで抜ける
	constexpr float WhineBaseHz    = 1200.0f;// 最低回転時のスプール音(Hz)
	constexpr float WhineGainHz    = 6000.0f;// 全開で上がる量(Hz)
	constexpr float TurboLevel     = 0.18f;  // スプール音の音量。本体より小さく保つこと

	// ブローオフバルブ(プシュー)。アクセルを閉じた瞬間、行き場を失った
	// 過給空気が逃げる音。ブーストが乗っている時ほど強く鳴る。
	constexpr float BovThrottleDrop = 0.35f; // これ以上アクセルを戻したら鳴らす
	constexpr float BovMinSpool     = 0.25f; // これ未満のブーストでは鳴らない
	constexpr float BovLevel        = 0.75f;
	constexpr float BovDecay        = 5.5f;  // 減衰の速さ。大きいほど短い

	//===== 吸気の共鳴 =====
	// 吸気側の管の鳴き。RB26のような「ヒョー」という金属的な音の正体。
	// 排気のローパスとは別に、狭い帯域だけを強調して足す。
	constexpr float FormantHz      = 1100.0f; // 鳴く周波数(Hz)
	constexpr float FormantRpmGain = 350.0f;  // 回転で上がる量。大きいと母音が動いて声になる
	// ※鋭くしすぎると、倍音の豊かな音に共鳴が1つ立つ＝声道と同じ構造になり、
	//   「人が唸っている」ように聞こえてしまう。広めに取って管の色付けに留める。
	constexpr float FormantQ       = 2.2f;    // 鋭さ。大きいほど細く金属的だが声に寄る
	constexpr float FormantAmount  = 1.30f;   // 混ぜる量(内部でQ倍を打ち消してある)

	//===== エンジン別プリセット =====
	// 気筒配置だけでなく、倍音の配分・マフラー・ターボまで含めて
	// 1つのエンジンとして持つ。切り替えると別のクルマの音になる。
	struct EnginePreset
	{
		int   firingOrder;    // 点火倍音の次数(=気筒数)
		float halfLevel;      // 半分オーダーの強さ(うねり)
		float otherLevel;     // その他のオーダーの強さ(ざらつき)
		float rolloff;        // 高次の落ち方。小さいほど甲高い
		float cutoffBase;     // マフラーの明るさ
		float cutoffRpmGain;
		float cutoffThrGain;
		float resonance;      // 管の鳴き
		float noiseLevel;
		float turboLevel;     // 0=NA
		float formantHz;      // 吸気の鳴く周波数
		float formantAmount;
	};

	// 直4ターボ(S15純正)。ザラつきがあり低めに唸る
	constexpr EnginePreset PresetSr20Det =
		{ 4, 0.28f, 0.10f, 1.20f,  400.0f, 2600.0f, 1500.0f, 1.10f, 0.42f, 0.16f,  900.0f, 0.55f };

	// 直6ツインターボ。中間オーダーが弱く滑らかで、高次がよく伸びる＝甲高い。
	// 吸気の鳴きを強く出すのが「RBらしさ」の肝。
	constexpr EnginePreset PresetRb26Dett =
		{ 6, 0.10f, 0.04f, 0.92f,  520.0f, 3600.0f, 2000.0f, 1.25f, 0.46f, 0.20f, 1250.0f, 0.80f };

	// 直6ツインターボ。RBより太く低い。低回転から重く唸り、上で伸びる
	constexpr EnginePreset Preset2JzGte =
		{ 6, 0.15f, 0.06f, 1.30f,  360.0f, 3000.0f, 1750.0f, 1.15f, 0.40f, 0.18f,  780.0f, 0.65f };

	// V8クロスプレーン。参考用(日本車ではない)
	constexpr EnginePreset PresetV8Cross =
		{ 8, 0.80f, 0.32f, 1.35f,  330.0f, 2200.0f, 1300.0f, 1.10f, 0.38f, 0.00f,  700.0f, 0.50f };
}
