#pragma once

// 小細工(free fly / ESP)の定数。
//
// ■ 何のためのものか
// 自分たちで走るためのゲームなので、他所のゲームを騙す話ではない。
// 広いマップを見て回る、相手がどこにいるか分かる、
// という「作る側の道具」として置く。
//
// ■ なぜMODメニューに置くか
// F3のパネルは編集用で、走りながらは開けない。
// TABのメニューはパッドで動かせるので、走行中に切り替えられる。
namespace CheatConst
{
	//===== 相手の位置を透かす(ESP) =====
	// 地形の向こう側にいても線が見えるように、深度を切って描く。
	//
	// 箱の大きさ(m)。車がすっぽり入るくらい
	constexpr float BoxHalfW = 1.0f;
	constexpr float BoxHalfH = 0.75f;
	constexpr float BoxHalfD = 2.3f;

	// 箱を車の中心からどれだけ上げるか(m)。
	// 車の原点は接地点あたりなので、上げないと地面に埋まって見える
	constexpr float BoxLift = 0.75f;

	// 自分から相手へ線を引く。
	// 箱だけだと、遠くで点になったときに見つけられない。
	// 線をたどれば、画面の外にいても方向が分かる
	constexpr bool DrawTracer = true;

	// 線を自分の位置からどれだけ上げて出すか(m)。
	// 足元から引くと、地面と重なって見えない
	constexpr float TracerLift = 1.2f;

	// これより遠い相手は出さない(m)。
	// 全部出すと画面が線だらけになる
	constexpr float MaxRange = 3000.0f;

	// 色。近いほど赤、遠いほど青に寄せる。
	// 同じ色だと、どれが近いのか分からない
	constexpr float NearDist = 60.0f;
	constexpr float FarDist  = 600.0f;

	//===== 自由に飛ぶ(free fly) =====
	// 車から離れて自由に動く。マップの見回りと、
	// 道や地形を上から確かめるのに使う。
	//
	// 飛ぶ速さ(m/秒)。押し込むと速いほう。
	// 広いマップを端まで見るので、普通の速さだけでは足りない
	constexpr float FlySpeed     = 40.0f;
	constexpr float FlySpeedFast = 200.0f;

	// 速いほうへ切り替えるボタン(XINPUT_GAMEPAD_LEFT_SHOULDER)。
	// XInput のヘッダをここへ持ち込まないよう、値で持つ
	constexpr unsigned short PadBoost = 0x0100;
}
