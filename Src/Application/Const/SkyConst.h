#pragma once

// 天球(skysphere)の定数
namespace SkyConst
{
	// skysphereの大きさ。モデル自体が既に半径約285(ノードscale92込み)なので、
	// 掛け過ぎると遠クリップ(2000)を超えて消える。最終半径≒850に収める。
	constexpr float Scale = 3.0f;
}
