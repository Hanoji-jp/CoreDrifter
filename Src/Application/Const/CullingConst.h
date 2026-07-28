#pragma once

// 描画カリング(視錐台・距離)の定数。
// 画面に映らない／遠すぎて見えないものを、頂点を積む前に弾いて負荷を下げる。
namespace CullingConst
{
	// ── オブジェクト単位の視錐台カリング ──
	// 影の生成パスには適用しない。画面外の物でも画面内へ影を落とすため、
	// カメラの視錐台で弾くと影だけ消えて不自然になる。
	constexpr bool ObjectFrustumCull = true;

	// ── スモークの粒単位 ──
	// 粒ごとにCPUでワールド変換して頂点を積むので、画面外の粒を弾くと
	// 変換コストと頂点転送量がそのまま減る。
	constexpr bool  SmokeCull     = true;
	constexpr float SmokeCullDist = 90.0f;   // これより遠い粒は描かない(m)
	// 判定に使う球の半径の余裕。粒は膨らむうえ乱流で揺れるので、
	// ぎりぎりで判定すると画面端で粒が消えるのが見えてしまう。
	constexpr float SmokeCullMargin = 1.5f;

	// ── タイヤ痕の区間単位 ──
	// 痕は最大400点×4本あり、峠を走ると大半が画面外に残る。
	constexpr bool  SkidCull     = true;
	constexpr float SkidCullDist = 120.0f;  // これより遠い区間は描かない(m)
	constexpr float SkidCullMargin = 0.6f;  // 区間の判定球に持たせる余裕(m)
}
