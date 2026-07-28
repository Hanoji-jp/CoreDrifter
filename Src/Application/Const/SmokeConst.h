#pragma once

// ドリフトスモーク(トゥーン煙)の定数
namespace SmokeConst
{
	// パーティクルプールの上限(リングバッファで古いものから上書き)
	// 生存中の粒を上書きすると"ぱっと消える"ので、放出数×最大寿命より十分多く確保する。
	//   50枚/秒 × 4クラスタ × 2輪 × 1.4秒 ≒ 560 が最大生存数 → 余裕を持って確保
	// 描画コストは生存数依存なのでプールを大きくしても重くならない。
	constexpr int MaxParticles = 1500;

	// 寿命(秒)：生成時に Min〜Max のランダム
	constexpr float LifeMin = 0.70f;
	constexpr float LifeMax = 1.40f;

	// 板ポリのサイズ(m)：生成時→消滅時にかけて膨らむ(③膨張)
	constexpr float SizeStart = 0.55f;
	constexpr float SizeEnd   = 2.30f;   // 消え際に巨大な円にならないよう控えめ
	// 粒ごとのサイズばらつき(倍率の範囲)。均一だと卵の列に見える
	constexpr float SizeVarMin = 0.65f;
	constexpr float SizeVarMax = 1.45f;

	// 初速。タイヤから勢いよく吹き出して、だんだん漂う。
	// 減衰(Drag)を強くしすぎると出た場所に留まり、煙ではなく"軌跡(トレイル)"に見える。
	constexpr float RiseSpeed   = 1.10f;  // 上昇速度(m/s)
	constexpr float SpreadSpeed = 1.20f;  // 水平のランダムな散り(m/s)
	// 車の外側(横方向)へ吹き出す速度。本家は左右へ大きく張り出して車体の後ろが空く。
	// ランダムな散りだけだと発生点の周りに留まって車にまとわりつく。
	constexpr float OutwardSpeed = 1.80f; // 外向きの初速(m/s)
	constexpr float OutwardJitter = 0.35f;// 外向き速度のばらつき(±割合)
	constexpr float TrailFactor = 0.32f;  // 車速の逆向きへ吹き飛ぶ割合(進行方向と逆へ流れる)
	constexpr float Drag        = 1.10f;  // 水平方向の速度減衰(1/s) 大きいほど早く止まって漂う
	// 上下は別扱いにする。上下にも同じ減衰を掛けると立ち上る勢いが即死して煙が伸びない。
	constexpr float VertDragMul = 0.30f;  // 上下の減衰倍率(水平に対する比)
	constexpr float Buoyancy    = 1.30f;  // 浮力(m/s^2)。煙が上へ伸び続ける
	constexpr float MaxRiseSpeed = 2.20f; // 上昇速度の上限(m/s)

	// 1回の放出で近接して撒く粒数(⑥小粒を重ねて密度を稼ぐ＝薄い粒でも繋がる)
	constexpr int   ClusterCount    = 4;
	constexpr float ClusterRadius   = 0.24f; // クラスタ内のばらけ半径(m)

	// ④乱流：ゆっくり渦を巻くように揺らして規則性を壊す(消え際を散らす)
	constexpr float TurbStrength = 0.9f;   // 揺らしの強さ
	constexpr float TurbFreq     = 3.0f;   // 揺らしの周波数

	// ①ディゾルブ：消え際に雲アルファの薄い所から穴が開いて崩れる(不透明度フェードと併用)
	constexpr float ErodeStrength = 0.6f;  // 穴あきの強さ(控えめ。フェードを主役にする)
	constexpr float ErodeEdge     = 0.45f; // 穴の縁の柔らかさ(大きいほど滑らか)

	// 放出制御(密な塊をたくさん重ねる)
	constexpr float SlipThreshold = 2.0f;  // これ以上の後輪スリップ量で煙が出始める
	constexpr float SlipFull      = 8.0f;  // この横滑りで放出レート最大
	constexpr float SpawnPerSec   = 50.0f; // 全開スリップ時の毎秒放出数(1輪あたり。⑥密度増)
	constexpr float HandbrakeBoost = 4.0f; // サイド中はスリップ量に加算(常時煙)
	// これ未満の車速では煙を出さない(停止中にサイドを引いても出さない)
	constexpr float MinSpeed = 2.5f;

	// 板ポリの面内回転(rad/s)
	constexpr float SpinMax = 1.2f;

	// 放出位置の後輪ローカル配置
	constexpr float WheelGroundY = 0.06f;  // 接地点のわずかな浮き

	// テクスチャアトラス(瘤状ブロブ4種を2x2に格納。陰影・輪郭は焼き込み済み)
	constexpr int SplitX = 2;
	constexpr int SplitY = 2;
	constexpr int VariantCount = 4;

	// 不透明度(柔らかい煙アルファを半透明で重ねて"繋がった塊"にする。
	// 1粒は薄く、重なりで密度を積み上げる＝つなぎ目が消える)
	constexpr float AlphaPeak    = 0.40f;  // 最大不透明度(薄め＝濃すぎ回避・重なりで繋がる)
	constexpr float FadeInRatio  = 0.12f;  // 寿命比これまでで立ち上がる
	constexpr float FadeOutRatio = 0.45f;  // 寿命比これぶん残して消える(長めで滑らかに散る)

	// スモークテクスチャ
	//   ToonSmoke.png … 輪郭焼き込みの硬いトゥーン(重ねると輪郭線が汚い)
	//   SoftSmoke.png … 縁フェードの柔らかい煙アルファ(重なりが繋がる。本家寄り)
	constexpr const char* TexturePath = "Asset/Textures/System/SoftSmoke.png";

	//===== メッシュ煙(NFS Unbound風)：板ポリではなく本物の3D頂点で塊を作る =====
	// 変形した球(ローポリ)を面法線でトゥーン陰影し、頂点カラーへ焼き込む。
	// 立体に光が当たり、シルエット輪郭(SmokeOutline)が効いて塊感が出る。
	// 本家は「大きな平面が組み合わさった低ポリの岩」。面ごとにフラットな色が乗り、
	// 陰影の境目が面の縁と一致している。滑らかな球にすると質感が出ないので低ポリにする。
	// 塊の形は「岩を割ったような多面体」。平らな面・直線的な稜線・鋭い角が本家の質感。
	// 正二十面体を細分割したアイコスフィアを使う(UV球と違い極が無く、面が均一)。
	//   細分割 0=20面 / 1=80面 / 2=320面
	// 1粒の三角形数は 20 * 4^Subdiv。2だと320枚あり、粒数を掛けると数十万枚になって重い。
	// 1(80枚)でもゴツゴツした輪郭は保てるので、こちらを既定にする。
	constexpr int   MeshSubdiv   = 1;      // 面の粗さ(少ない=面が大きくゴツい/明暗の境目がカクつく)
	constexpr int   MeshVariants = 10;     // 生成する塊の形の種類
	// 方向で決まる滑らかなうねりを3段重ねて、岩のようにゴツゴツした輪郭を作る。
	// (低周波=大きな塊のうねり / 中周波=瘤 / 高周波=細かい岩肌)
	// 中・高周波を強くすると表面に小さな出っぱりが多数でき、その一つ一つが光を拾って
	// ハイライトが細かく散る。輪郭のゴツゴツは低周波(Lump1)が作るので、そちらを主役にする。
	constexpr float MeshLump1    = 0.36f;  // 大きなうねり(全体の不揃いさ・輪郭)
	constexpr float MeshLump2    = 0.21f;  // 中くらいの瘤
	constexpr float MeshLump3    = 0.11f;  // 細かい岩肌(大きいとハイライトが散る)
	// 粒ごとの縦横比のばらつき(球っぽさを崩す)
	constexpr float MeshAspectMin = 0.72f;
	constexpr float MeshAspectMax = 1.28f;

	// トゥーン陰影は3段に塗り分ける
	//   ① 一番光が当たる面 … 白へ寄せたハイライト
	//   ② 中間            … 基準色そのもの
	//   ③ 光が当たらない面 … 暗い色
	constexpr float ToonDark   = 0.30f;    // ③暗い面の倍率(小さいほど暗い)
	constexpr float ToonWhite  = 0.90f;    // ①白へ寄せる量(1=完全に白)
	constexpr float ToonMidThr = 0.38f;    // ②標準色になる受光量(上げると暗部が広がる)
	constexpr float ToonHiThr  = 0.88f;    // ①ハイライトになる受光量(下げると白が広がる)
	// 光を真上へ寄せる量。真上に寄せすぎると上を向いた面が必ず明るくなり、
	// 本家にある「塊の上部の谷間や陰になる面の影」が出せない。斜めの光を残す。
	constexpr float ToonUpBias = 0.45f;
	// 光をカメラ基準にする割合。1にすると、カメラを回したときハイライトの位置も一緒に回る。
	// 本家の煙はこの見え方(イラストで常に画面の決まった方向から光を描くのと同じ考え方)。
	// 0=ワールド固定(カメラを回すと光と反対側に回り込める)
	constexpr float SmokeViewLight = 1.0f;
	// 2つ目の光(横上から)の強さ。キーライトと反対側の斜め上に置いて、横向きの面にも
	// ハイライトを乗せる。1灯だと片側しか光らず、本家のように面が光らない。
	constexpr float SmokeFillLight = 0.72f;
	// 粒ごとの陰影の出し方。0=面の法線 / 1=その粒の中での上下位置。
	// ローカル高さは水平に切れすぎて本家と違ったので0(法線ベース)。
	constexpr float SmokeLocalY = 0.0f;

	// 重なった粒を1つの塊に見せる「共有の陰影」。全粒共通の高さから明るさを作って混ぜる。
	// 高さだけで決めると上部に影が出ないので、一体感を保てる範囲に留める。
	// 0=粒ごとの陰影のみ / 1=全体の高さのみ(完全に一体化するが粒の影は消える)
	constexpr float SmokeMerge  = 0.35f;
	constexpr float SmokePlumeH = 2.2f;   // 根元から上端までの高さ(m)

	// スクリーン空間のハーフトーン模様（印刷物っぽい質感。煙が動いても模様は画面に固定）
	constexpr float SmokePatScale    = 16.0f;  // 模様の周期(px)。小さいほど細かい網点
	constexpr float SmokePatStrength = 0.35f;  // 模様の濃さ(0=無効)
	constexpr float SmokePatDarkBias = 0.70f;  // 暗い面ほど模様を強く出す量(0=一律)

	// 明暗の境界のうねり。高さで陰影を決めると境目が水平一直線に切れて不自然なので、
	// ワールドXZで決まるうねりを足して波打たせる(位置だけの関数なので一体感は保たれる)。
	constexpr float SmokeWobAmp  = 0.35f;  // 境界の揺れ幅(m)。0でまっすぐ
	constexpr float SmokeWobFreq = 0.90f;  // 揺れの細かさ(1/m)。大きいほど細かく波打つ
	// ※陰影に使う光の向き・色はシーンの平行光(g_DL_Dir / g_DL_Color)と環境光を
	//   シェーダ側で直接参照する。ここで固定値は持たない。

	// 立体なので1粒が濃い＝粒数を減らし、サイズを上げる(大きな塊がもくもく)
	// 本家は「大きな塊が少数」。数を減らしてサイズを大きく取る。
	constexpr int   MeshMaxParticles = 640;
	constexpr float MeshSpawnPerSec  = 26.0f;  // 全開スリップ時の毎秒放出数(1輪)
	// 毎秒だけで放出すると、速度が上がったとき粒の間隔が開いて煙の帯が途切れる。
	// 「進んだ距離あたり」の放出を足すと、速度に関わらず路面上の粒の密度が一定になる。
	constexpr float MeshSpawnPerMeter = 2.6f;  // 全開スリップ時の1mあたり放出数(1輪)
	// 前輪から出す煙の量(後輪に対する倍率)。前輪は駆動せず路面を擦るだけなので、
	// うっすら白く煙る程度に留める。後輪と同量出すと何をしているのか分からなくなる。
	constexpr float FrontSpawnMul = 0.16f;
	// 前輪の煙の大きさ(後輪に対する倍率)。もくもくした塊ではなく、
	// 路面から擦れ出る小さな砂埃のように見せたいので極端に小さくする。
	constexpr float FrontSizeMul  = 0.30f;
	constexpr int   MeshClusterCount = 2;      // 1回の放出でまとめて撒く数
	// 大きさの変化：出た直後に膨らみ、ピークを過ぎたらしぼんで消える。
	// (単調に膨らむだけだと、消える瞬間が一番でかくて不自然)
	// ※実際の大きさは SizeVarMin〜Max(0.65〜1.45)が掛かる。
	constexpr float MeshSizeStart    = 0.17f;  // 生成時の半径(m)
	constexpr float MeshSizePeak     = 0.55f;  // 最大時の半径(m)
	constexpr float MeshSizePeakAt   = 0.40f;  // 寿命比のどこで最大になるか
	constexpr float MeshSizeEndScale = 0.55f;  // 消える時の大きさ(最大に対する比)
	constexpr float MeshAlphaPeak    = 1.00f;  // 不透明度(深度書き込みを使うので完全不透明)

	// 後方グラデーション：発生源から離れるほど色A→色Bへ。
	// 粒ごとに色を変えると、少しずつ違う色の塊が並んでパッチワークに見えるため、
	// シェーダ側で「発生源からの距離」という全粒共通の関数として掛ける。
	// これなら粒の境目が一切出ず、色が滑らかにフェードする。
	constexpr float SmokeGradDist = 6.0f;   // この距離(m)で色Bになりきる

	// 消え方：不透明度を薄くするのではなく、セル状に砕けて散るディゾルブで消す。
	constexpr bool  PopOut       = true;   // true=不透明度は一定のまま / false=フェードアウト
	constexpr float PopFadeInEnd = 0.04f;  // 出現時だけごく短く立ち上げる(0で完全に瞬間表示)

	// ディゾルブ：寿命の終盤で穴が広がって崩れながら消える
	constexpr float DissolveStart = 0.85f; // この寿命比からディゾルブ開始
	constexpr float DissolveScale = 14.0f; // 崩れる粒の細かさ(セル/m)。大きいほど細かい
}
