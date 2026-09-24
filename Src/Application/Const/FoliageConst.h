#pragma once

// 植生(木・草)の定数。
//
// ■ なぜインスタンシングではないか
// この枠組みには per-instance の描画が無い。付けるならシェーダーと
// 頂点レイアウトから足すことになる。
//
// 代わりに、まとまりごとに変換済みの頂点として焼き込む。
// 地形と同じ区切りなので、まとまり単位の画面外判定がそのまま効く。
// 木は動かないので、焼いて困ることもない。
//
// ■ なぜモデルを持たないか
// 木のモデルは持っていない。借りると1本ずつ権利を確かめて回ることになる。
// この作品はフラットシェーディングなので、幹と円錐を組めば十分見える。
//
// ■ 置き場所は毎回同じ
// 座標から作った整数ハッシュで決める。乱数を使うと起動のたびに
// 木が生え変わり、「あの木の陰」で覚えた道が毎回変わる。
// 保存する必要も無くなる
namespace FoliageConst
{
	//===== 木を置く =====
	// 木は手で置く。自動で撒かない。
	//
	// ■ なぜ手で置くか
	// 峠で木が効くのは「見通しを切る」ためで、それは道の形に
	// 合わせて決めるもの。傾きと道からの距離で機械的に撒いても、
	// 曲がりの先が見える所に限って生えてくれない。
	//
	// 置いた場所は trees.txt へ書き出す
	constexpr const char* TreePath = "Asset/Data/terrain/trees.txt";

	// 1本ごとの向きと大きさは、置いた座標から作る。
	// 置くたびに乱数を引くと、同じ場所へ置き直したときに姿が変わる
	constexpr float TreeYawSalt   = 733.0f;
	constexpr float TreeScaleSalt = 911.0f;

	// 置ける斜面の限界。法線の上向き成分。
	// 0.70 = 45度。これより急だと根が張れない。
	// 手で置くときも、崖に刺さった木は置かせない
	constexpr float TreeUpMin = 0.70f;

	//===== 木の形 =====
	constexpr float TrunkH      = 1.9f;    // 幹の高さ(m)
	constexpr float TrunkR      = 0.17f;   // 幹の太さ(半径m)
	constexpr float TrunkTaper  = 0.65f;   // 上へ行くほど細る割合
	constexpr int   TrunkSides  = 4;

	constexpr int   CanopyCones = 2;       // 円錐を何段積むか
	constexpr int   CanopySides = 5;
	constexpr float CanopyR     = 1.45f;   // 一番下の円錐の半径(m)
	constexpr float CanopyH     = 2.6f;    // 円錐1つの高さ(m)
	constexpr float CanopyStep  = 0.62f;   // 段ごとに半径と高さへ掛ける割合
	constexpr float CanopyDrop  = 0.45f;   // 段の重なり(m)。離すと串団子になる

	// 1本ごとの大きさの振れ幅
	constexpr float TreeScaleMin = 0.75f;
	constexpr float TreeScaleMax = 1.45f;

	//===== 木の色 =====
	constexpr float TrunkR_ = 0.24f, TrunkG_ = 0.19f, TrunkB_ = 0.14f;
	constexpr float LeafR   = 0.17f, LeafG   = 0.29f, LeafB   = 0.13f;

	// 葉の色の振れ幅。全部同じ色だと1本の巨大な塊に見える
	constexpr float LeafVary = 0.22f;

	//===== 草を置く =====
	// 道の周りだけに置く。世界中に撒くと数百万枚になり、
	// しかも走っていて目に入るのは道の周りだけ
	constexpr float GrassSpacing = 2.6f;
	constexpr float GrassDensity = 0.55f;
	constexpr float GrassJitter  = 0.5f;

	// 道の持ち分がこの範囲のマス目にだけ生やす。
	// 上限は「舗装と土の上には生えない」、下限は「道から遠すぎない」
	constexpr float GrassRoadMax = 0.70f;
	constexpr float GrassRoadMin = 0.06f;

	constexpr float GrassUpMin = 0.60f;   // 53度より急には生えない

	//===== 草の形 =====
	// 板を十字に組む。1枚だと横から見たときに消える
	constexpr int   GrassBlades = 2;
	constexpr float GrassW      = 0.55f;   // 板の幅(m)
	constexpr float GrassH      = 0.62f;   // 板の高さ(m)
	constexpr float GrassScaleMin = 0.7f;
	constexpr float GrassScaleMax = 1.5f;

	constexpr float GrassR = 0.28f, GrassG = 0.40f, GrassB = 0.18f;
	constexpr float GrassVary = 0.26f;

	// 根本を地面へ埋める量(m)。
	// ちょうどに置くと、斜面で板の角が浮いて隙間が見える
	constexpr float GrassSink = 0.06f;

	//===== まとまり =====
	// 1辺の長さ(m)。地形のまとまり(64マス × 1.25m = 80m)に合わせる
	constexpr float ChunkSize = 80.0f;
}
