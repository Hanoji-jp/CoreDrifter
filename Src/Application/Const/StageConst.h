#pragma once

// ステージ(コースマップ)の定数
namespace StageConst
{
	// マップモデルのパス(Sketchfab: burnout dominator bushido peak / CC-BY 4.0)
	constexpr const char* ModelPath = "Asset/Data/burnout_dominator_bushido_peak/scene.gltf";

	// 初期スケール(gltfの実寸に合わせて調整する。ImGuiで詰める)
	constexpr float ModelScale = 1.0f;

	// 配置オフセット(車の初期位置に道が来るように合わせる)
	constexpr float OffsetX = 0.0f;
	constexpr float OffsetY = 0.0f;
	constexpr float OffsetZ = 0.0f;

	// 向き(Y回転, rad)
	constexpr float YawOffset = 0.0f;

	// フラスタムカリング半径(広大なマップなので大きめにして消えないように)
	constexpr float CullingRadius = 100000.0f;

	// 影(平行光シャドウマップ)のカバー範囲。既定25は狭すぎて近くしか影が出ないので広げる。
	//   ※広げるほど遠くまで影が出るが、同じ解像度を広範囲へ引き伸ばすので影は少し粗くなる。
	constexpr float ShadowAreaSize    = 120.0f;  // 影の描画範囲(一辺, ユニット)
	constexpr float ShadowLightHeight = 100.0f;  // 光源の高さ(影の投影距離)

	// ImGui調整スライダーの範囲
	constexpr float ScaleMin  = 0.001f;
	constexpr float ScaleMax  = 1000.0f;
	constexpr float ScaleStep = 0.01f;
	constexpr float OffsetStep = 0.5f;
	constexpr float YawStep    = 0.01f;
}
