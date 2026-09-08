#pragma once

// 地形を筆で彫るための定数。
//
// ■ なぜ筆が要るか
// 高さマップは基盤地図情報から読めるが、それは実在の地形。
// 走って面白い峠にするには、手で盛って削る必要がある。
//
// ■ 素地と仕上がり
// 道は地形を削る。彫った形に直接書くと、道を動かした瞬間に
// 消えるか、削りが二重に掛かって掘り進む。
//
// 筆が書くのは素地。道は毎回そこから削り直す。
//
// ■ どこを指しているか
// メッシュを1枚ずつ調べる必要はない。
// 高さマップがあるので、光線を刻んで進めて、
// 高さを下回った所で二分すれば出る。ずれようがない。
namespace TerrainBrushConst
{
	//===== 筆 =====
	// 半径(m)。峠1本ぶんの尾根を作るなら数十m欲しい
	constexpr float RadiusMin =  2.0f;
	constexpr float RadiusMax = 120.0f;
	constexpr float RadiusInit = 30.0f;

	// 1秒あたりに動かす高さ(m)。
	// 押しっぱなしで盛るので、時間で効かせる
	constexpr float StrengthMin = 0.1f;
	constexpr float StrengthMax = 40.0f;
	constexpr float StrengthInit = 8.0f;

	// 中心から外へ向かう効きの落ち方。
	// 1で直線、2以上で中心が尖る。0.5で縁まで平たく効く
	constexpr float FalloffMin = 0.2f;
	constexpr float FalloffMax = 4.0f;
	constexpr float FalloffInit = 1.0f;

	//===== 光線を刻んで進める =====
	// 1回に進む距離(m)。
	// 粗いと、薄い尾根を突き抜けて向こう側に当たる
	constexpr float MarchStep = 1.5f;

	// どこまで探すか(m)。地形の対角より長ければ足りる
	constexpr float MarchFar = 4000.0f;

	// 当たったあと、前後を狭めて詰める回数。
	// 12回で MarchStep の 1/4096 まで寄る
	constexpr int MarchRefine = 12;

	//===== 取り消し =====
	// 何手ぶん覚えておくか。
	// 半径30mの筆で1手あたり約 60x60 マス = 14KB。
	// 64手でも1MBに満たない
	constexpr int UndoDepth = 64;

	//===== 見た目 =====
	// 筆の輪を何本の線で描くか
	constexpr int RingSegments = 48;

	// 輪を地面からどれだけ浮かせるか(m)。
	// 地面と同じ高さだと、線が地形へ潜って途切れる
	constexpr float RingLift = 0.35f;

	// 内側にもう1本、効きの強い範囲を出す割合
	constexpr float InnerRingRatio = 0.5f;

	//===== 道を避ける =====
	// 道のそばを盛ると、地形が路面を突き抜ける。
	// 突き抜けるたびに削って直すのは時間の無駄。
	//
	// 道が地形を寄せた強さ(0〜1)をそのまま覆いに使う。
	// 路面の上では1で筆が効かず、削りの端に向けて0へ落ちる。
	// 境目に段差ができない。
	//
	// 0で完全に守る。1にすると覆いが効かない
	constexpr float RoadMaskInit = 0.0f;

	//===== 均す =====
	// 1回にどれだけ周りへ寄せるか。
	// 1.0にすると、押した瞬間に平らになって使いにくい
	constexpr float SmoothRate = 3.0f;

	//===== ざらつき =====
	// 高さの散らばり(m)。自然な荒れを足すため
	constexpr float NoiseScale = 0.35f;

	// 山の細かさ(m)。この間隔で高い所と低い所が入れ替わる
	constexpr float NoiseCell = 12.0f;

	//===== 保存 =====
	// 書き出し先は TerrainConst::EditPath。
	//
	// 取り込んだ標高データ(height.r32)とは必ず分ける。
	// 同じ名前にすると、保存したときに DEM が消える。実際に一度消した
}
