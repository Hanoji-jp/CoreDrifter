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
	// 倍音の減り方。大きいほど高次が弱まり、丸くこもった音になる。
	// ※これは「次数」に対する減り方で、これだけだとRPMが上がるほど
	//   スペクトル全体が上へ引き伸ばされ、高回転が金切り声のようになる。
	constexpr float Rolloff = 1.15f;

	// 排気パルスの長さ(ミリ秒)。
	// 排気弁が開いた瞬間の圧力の吹き出しは、回転数に関係なく
	// おおよそ決まった時間で終わる(数ミリ秒)。
	// つまりスペクトルの形は「絶対的な周波数」で決まっていて、
	// 回転が上がっても上へ伸びていくわけではない。
	// これを入れないと、高回転で倍音が青天井に伸びて汚くなる。
	constexpr float PulseWidthMs = 2.6f;

	//===== 気筒ごとの個体差 =====
	// 実機は気筒ごとに燃焼の強さがわずかに違う。
	// その差がクランク1サイクルごとの振幅の揺れになり、
	// 各次数の周りに細かい成分を生む＝あの「ざらついた回り方」になる。
	// これが無いと、純粋な倍音列＝ブザーやノコギリ波と同じ構造になってしまう。
	constexpr float CylinderImbalance = 0.22f;   // 気筒ごとの強さの差(0=全部同じ)

	//===== レブリミッター(点火カット) =====
	// 実物のリミッターは点火を飛ばす。飛んだ気筒は燃えないので、
	// 生ガスが排気管へ流れ込んでそこで爆ぜる。あの「ババババッ」の正体。
	// 音量を絞るだけでは「壁に当たっている感じ」がまったく出ない。
	// ※深く切りすぎると音がブツブツに途切れ、破裂音も大きいと
	//   「暴れすぎ」で聞いていられなくなる。当たったと分かる程度に留める。
	constexpr float CutDepth    = 0.45f;   // 飛んだ点火をどれだけ黙らせるか
	constexpr float CutPopLevel = 0.18f;   // 排気管で爆ぜる音の大きさ
	constexpr float CutPopDecay = 90.0f;   // その減衰の速さ。大きいほど短く鋭い
	// 点火倍音(k=気筒数の倍数)をどれだけ強調するか。エンジンらしさの芯
	constexpr float FiringBoost = 1.0f;
	// アクセルオフで高次がどれだけ失われるか。踏むと音が明るく開く。
	// ※大きくしすぎると、離した瞬間に低い倍音だけが剥き出しで残り、
	//   純粋な正弦の重なり＝シンセ音に聞こえてしまう。
	constexpr float OffRolloffAdd = 0.35f;

	//===== オーバーラン(エンジンブレーキ) =====
	// アクセルを離した高回転では、実物はむしろ荒くパチパチ鳴る。
	// 音量とノイズを一緒に減らすと、純粋な倍音だけが残ってシンセ音になる。
	constexpr float OverrunCrackle   = 0.22f;  // 燃え残りが爆ぜる頻度(0=なし)
	constexpr float OverrunMinRpmN   = 0.45f;  // この回転(正規化)以上で鳴る
	constexpr float OverrunNoiseGain = 0.9f;   // 離した時にノイズを増やす量

	//===== アフターファイア =====
	// 高回転で一気にアクセルを閉じると、行き場を失った混合気が排気管へ流れ、
	// 熱い排気に触れて一気に燃える。「パパパンッ」と数発まとめて出るのが特徴で、
	// オーバーランのパチパチ(常時パラパラ)とは別物。
	constexpr float AfterfireDrop    = 0.55f;  // これ以上一気に戻したら出る
	constexpr float AfterfireMinRpmN = 0.55f;  // この回転(正規化)以上で出る
	constexpr int   AfterfireShots   = 4;      // 何発まとめて出るか
	constexpr float AfterfireLevel   = 0.55f;  // 1発の大きさ
	constexpr float AfterfireGapMs   = 55.0f;  // 発と発の間隔(ミリ秒)。ばらつかせる

	//===== 低域の整理 =====
	// 排気管の最低モードは非常に低く(2.4mで約36Hz)、しかも一番強い。
	// 理屈上は正しいが、実物の超低域は強く減衰するうえ、
	// ゲームのスピーカーでは「ボー」という濁りにしかならない。
	constexpr float HighPassHz = 75.0f;

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

	// 回転が上がるほど1発ごとの起伏を薄める量。
	// 実機も低回転では1発1発が聞き分けられ、回転が上がると融合して滑らかになる。
	// これが無いと、どの回転域でも「ドッドッドッ」が残って農機のような音になる。
	// ※薄めすぎるとノイズが途切れず垂れ流しになり、
	//   広い帯域のノイズ＋定常的な唸り＝掃除機のような音になる。
	//   高回転でも脈動は半分以上残すこと。
	constexpr float PulseBlurRpm = 0.40f;

	//===== 1発の中でのアタック(パンチ) =====
	// 実物の排気パルスは、1発の中で質が変わる。
	//   頭 … ブローダウン。高圧の気体が一気に噴き出す鋭く明るい破裂
	//   後 … 排気行程の押し出し。鈍く低い
	// この「1発の中で明るさが落ちる」動きがパンチの正体で、
	// 倍音の強さが一定のままでは原理的に作れない。
	// 点火のたびにフィルタを開いて閉じることで作る。
	constexpr float PulseAttack      = 2.6f;   // 点火直後にカットオフを何倍に開くか
	constexpr float PulseAttackSharp = 4.0f;   // 閉じる速さ。大きいほど鋭く短い
	// 頭の一瞬だけ音量も持ち上げる。開くだけだと「明るくなる」に留まり、
	// 叩かれたような手応えが出ない。
	constexpr float PulsePunch       = 0.45f;

	//===== ノイズ(吸排気の乱流) =====
	// 倍音だけだと機械的なので、空気の音を足す。回転が上がるほど強くなる。
	constexpr float NoiseLevel   = 0.30f;   // ※アクセルオフでも痩せさせないこと
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

	//===== コンプレッサーサージ(タービンのフラッター) =====
	// 逃がし弁が無い、または閉じている時、行き場を失った過給空気が
	// コンプレッサーホイールを逆流して羽根を叩く。
	// 「ストゥトゥトゥ…トゥ…」と、圧力が抜けるにつれて周期が延びるのが特徴。
	// ブローオフの「プシュー」とは別物なので、両方を混ぜて好みで選べるようにする。
	constexpr float SurgeLevel    = 0.55f;   // 音量(0=鳴らさない)
	constexpr float SurgeRateHz   = 42.0f;   // 逆流の周期(Hz)。速いほど細かく震える
	constexpr float SurgeRateFall = 0.55f;   // 減衰につれて遅くなる量(0=遅くならない)
	constexpr float SurgeDecay    = 3.2f;    // 収まる速さ。小さいほど長く続く
	constexpr float SurgeToneHz   = 520.0f;  // 羽根を叩く音の高さ
	constexpr float SurgeToneQ    = 3.5f;    // その鋭さ

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
		// ここから下がエンジンの「質感」を決める。
		// これが共通だと、フィルタの色が違うだけの音になり、
		// 暗い設定のものが「安っぽいモーター音」に聞こえてしまう。
		float pulseWidthMs;   // 排気パルスの長さ。短いほど鋭く硬い
		float cylImbalance;   // 気筒ごとの燃焼の差。大きいほどざらつく
		// 吸気の性格。ここが同じだと、気筒数が同じエンジンは区別がつかない。
		// 直6とV6はどちらも点火倍音がk=6なので、倍音の骨格は完全に同一。
		// 実物で違って聞こえるのは吸気と排気の性格の差でしかない。
		float formantRpmGain; // 吸気の鳴きが回転で上がる量。
		                      // 大きい＝6連スロットルのように回転と一緒に鳴き上がる
		                      // 小さい＝大きなプレナムでモワッとした立ち上がり
		float noiseTone;      // 乱流の明るさ。大きいほど「シャー」と高く、
		                      // 小さいほど「ゴー」と低く籠もる
	};

	// 直4ターボ(S15純正)。ザラつきがあり低めに唸る
	constexpr EnginePreset PresetSr20Det =
		{ 4, 0.34f, 0.16f, 1.20f,  420.0f, 2700.0f, 1600.0f, 1.15f, 0.26f, 0.16f,  900.0f, 0.55f, 3.2f, 0.34f,  600.0f, 0.30f };

	// 直6ツインターボ。中間オーダーが弱く滑らかで、高次がよく伸びる＝甲高い。
	// 吸気の鳴きを強く出すのが「RBらしさ」の肝。
	constexpr EnginePreset PresetRb26Dett =
		{ 6, 0.11f, 0.05f, 0.92f,  560.0f, 4000.0f, 2300.0f, 1.45f, 0.20f, 0.14f, 1150.0f, 1.15f, 2.1f, 0.18f, 1900.0f, 0.42f };

	// 直6ツインターボ。RBより太く低い。低回転から重く唸り、上で伸びる
	constexpr EnginePreset Preset2JzGte =
		{ 6, 0.18f, 0.09f, 1.15f,  400.0f, 3200.0f, 1850.0f, 1.20f, 0.24f, 0.18f,  820.0f, 0.75f, 3.6f, 0.24f,  250.0f, 0.18f };

	// V8クロスプレーン。参考用(日本車ではない)
	constexpr EnginePreset PresetV8Cross =
		{ 8, 0.80f, 0.32f, 1.25f,  360.0f, 2400.0f, 1400.0f, 1.15f, 0.26f, 0.00f,  700.0f, 0.55f, 4.0f, 0.45f,  200.0f, 0.16f };
}
