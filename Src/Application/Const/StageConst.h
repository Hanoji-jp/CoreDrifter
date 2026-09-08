#pragma once

// ステージ(コースマップ)の定数
namespace StageConst
{
	// マップモデルの場所。
	//
	// いまは何も置いていない。
	// 借りていたモデルは他所のゲームの吸い出しだったので消した。
	// 表記でどうにかなるものではないので、配る形には残せない。
	//
	// Stage の仕組みそのものは残してある。
	// 素性のはっきりしたモデルが用意できたら、ここを差し替える
	constexpr const char* ModelPath = "";

	// 初期スケール(gltfの実寸に合わせて調整する。ImGuiで詰める)
	constexpr float ModelScale = 1.0f;

	// 配置オフセット(車の初期位置に道が来るように合わせる)
	constexpr float OffsetX = 0.0f;
	constexpr float OffsetY = 0.0f;
	constexpr float OffsetZ = 0.0f;

	// 向き(Y回転, rad)
	constexpr float YawOffset = 0.0f;

	// マップ配置(大きさ・座標・向き)とプレイヤースポーンの保存ファイル
	// key value 形式のテキスト(CarTune_*.txt と同じ簡易フォーマット)
	constexpr const char* ConfigPath = "Asset/Data/StageConfig.txt";

	// プレイヤーの初期スポーン(ImGuiで設定→保存。ロード時に車へ適用)
	constexpr float SpawnX   = 0.0f;
	constexpr float SpawnY   = 0.0f;
	constexpr float SpawnZ   = 0.0f;
	constexpr float SpawnYaw = 0.0f;   // スポーン時の車の向き(Y回転, rad)
	constexpr float SpawnStep = 0.1f;  // スポーン座標スライダーの刻み

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

	//===== 当たり判定から外すノード =====
	// 草や葉は見た目のためのもので、乗ったり当たったりする物ではない。
	// これらを判定に残すと、
	//   ・葉の上に車が乗ってしまう(接地レイが拾う)
	//   ・草むらに突っ込むと壁として押し返される
	//   ・面の枚数が多いので判定そのものが重い
	// という3つが同時に起きる。
	//
	// ノード名に以下のいずれかを含むものを外す。大文字小文字は区別しない。
	// モデルによって命名が違うので、よくある綴りを並べてある。
	// 個別の調整はステージのパネル(ImGui)から行える。
	constexpr const char* NoCollisionKeywords[] =
	{
		"leaf", "leaves", "grass", "foliage", "plant", "bush",
		"weed", "flower", "fern", "branch", "shrub",
		"草", "葉",
	};

	// 除外リストの保存先。配置とは別ファイルにする
	// (配置は数値だけ、こちらは名前の一覧で形式が違うため)
	constexpr const char* NoCollisionPath = "Asset/Data/StageNoCollision.txt";

	//===== 描画するノードの絞り込み =====
	// コースは数千のメッシュノードでできていて、そのまま描くと
	// ノードの数だけ描画命令が出る。しかも影を作るときにも同じ数だけ出る。
	// 見えていない物は描かない。

	// 画面に映る範囲の判定に使う余裕(m)。
	// 境界ボックスは実物より少し大きめなので、ぎりぎりで判定すると
	// 画面の端で物が消えたり出たりする
	constexpr float CullMargin = 2.0f;

	// 影を作るときに描く範囲(m)。
	// 影は光源から見て焼くので画面の視錐台では絞れない。
	// ただし影が要るのは車の周りだけなので、距離で切る。
	// 遠すぎる物の影は、そもそも画面に落ちない
	constexpr float ShadowCullDistance = 120.0f;
}
