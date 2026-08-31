#pragma once

#include "../GameObject/UI/UIConst.h"   // デザイン座標の大きさ

// タイトル画面の車ショーケース。
//
// タイトルは3Dのシーンを窓抜きで貼る作りになっている。
// これまでその中身が空だったので、車を1台置いて見せる場にする。
//
// ■ 走らせない
// タイトルで走らせると、地形の読み込みと物理が要る。
// 見せたいのは「車」であって走行ではないので、
// 回転台に載せたように回すだけにする。
namespace ShowcaseConst
{
	//===== 背景 =====
	// 緑一色の空間。作品の主色(アシッド緑)より暗く落とす。
	// 同じ明るさだとUIの緑と車の輪郭がぶつかって、車が沈む
	constexpr float BgR = 0.42f, BgG = 0.62f, BgB = 0.16f;

	//===== 車の置き方 =====
	// 原点に置いて、その周りをカメラが回る作りにはしない。
	// 車を回すほうが、背景が動かないぶん落ち着いて見える
	constexpr float TurnSpeed = 0.35f;   // 1秒あたりの回転(rad)
	// 少し傾けて置く。真横から見ると平面的になる
	constexpr float TiltDeg = -8.0f;

	//===== カメラ =====
	// 車の全長が約4mなので、少し離して斜め上から
	constexpr float CamDist   = 7.2f;    // 車からの距離(m)
	constexpr float CamHeight = 2.1f;    // 高さ(m)
	constexpr float CamLookY  = 0.75f;   // 見る高さ(車の腰あたり)
	constexpr float CamFovDeg = 38.0f;   // 望遠寄り。広角だと車が歪む
	constexpr float CamNear   = 0.5f;    // 近すぎると深度の精度を食う
	constexpr float CamFar    = 200.0f;

	//===== 画面上のどこへ置くか =====
	// タイトルは3Dシーンを右側の窓へ切り抜いて貼っている。
	// カメラは画面の中心を向くので、そのままだと車が窓から外れる。
	//
	// デザイン座標(1536x864)で、画面中央からのずれを指定する。
	// 右が正、上が正。ここを触れば窓の中で好きな位置へ置ける。
	//
	// ※カメラを逆方向へ平行移動して実現している。
	//   車そのものを動かすと、回転台の中心までずれて回り方が変わる。
	constexpr float ScreenOffsetX = 290.0f;   // 右へ
	constexpr float ScreenOffsetY = 70.0f;    // 上へ
}
