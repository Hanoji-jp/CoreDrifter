#pragma once

// 手で置く飾り(木・低木)の定数。
//
// ■ 手続き生成をやめた理由
// 幹と円錐で組んだ木は、置き場所の仕組みを作るには十分だったが、
// 絵としては借りてきたモデルに敵わない。
// 仕組みはそのまま残して、置くものだけモデルへ差し替える。
//
// ■ 焼き込まない
// 地形の草は変換済みの頂点として焼いたが、こちらは焼かない。
// モデルは材質とテクスチャを持っているので、頂点だけ写しても
// 絵にならない。1つずつ DrawModel で描く。
//
// 手で置く前提なので数は数百のはず。まとまりに焼く手間に見合わない。
//
// ■ 置いたものは props.txt へ
// モデルは「名前」で覚える。番号で覚えると、フォルダへ1つ足した
// 瞬間に全部の木が別のモデルに化ける
namespace PropConst
{
	//===== モデルの置き場 =====
	// ここから下を全部さらう。フォルダの階層は問わない
	constexpr const char* Dir = "Asset/Data/Map_Props";

	// 置いたものの記録
	constexpr const char* SavePath = "Asset/Data/terrain/props.txt";

	// 拾うモデルの上限。壊れたフォルダで固まらないため
	constexpr int MaxModels = 256;

	// ランダムで選ぶときに外す名前の末尾。
	// LOD は遠景用の粗いモデルなので、近くに置くと形が破綻して見える。
	// 一覧からは選べるままにする(遠景に置きたいときのため)
	constexpr const char* RandomSkipSuffix = "_LOD";

	// ランダムの種。カーソルの位置から選ぶので、
	// 下見に出ているものがそのまま置かれる
	constexpr float RandomSalt = 4177.0f;

	// 置ける数の上限。
	// 1つずつ描くので、増えるほど描画命令が増える
	constexpr int MaxPlaced = 4000;

	//===== 置き方 =====
	// 置ける斜面の限界。法線の上向き成分。0.70 = 45度
	constexpr float UpMin = 0.70f;

	// 大きさの振れ幅。同じ大きさばかりだと並べた感が出る
	constexpr float ScaleMin = 0.85f;
	constexpr float ScaleMax = 1.35f;

	// 根元を地面へ埋める量(m)。
	// ちょうどに置くと、斜面で根元が浮いて影が切れる
	constexpr float Sink = 0.10f;

	// 向きと大きさは置いた座標から作る。
	// 置くたびに乱数を引くと、同じ場所へ置き直すだけで姿が変わる
	constexpr float YawSalt   = 733.0f;
	constexpr float ScaleSalt = 911.0f;

	//===== 掴む =====
	// 消すときの当たりの太さ(m)。
	// モデルの大きさで変えると、細い木が掴めなくなる
	constexpr float PickRadius = 1.2f;

	// 掴む当たりの下限(m)。
	// 大きさに比例させるだけだと、小さくしたものが掴めなくなって
	// 消すことも動かすこともできなくなる
	constexpr float PickRadiusMin = 0.6f;

	//===== 置く操作(フォートナイトのクリエイティブ風) =====
	// 置く前に、そのモデルを実際の大きさで地面に出す。
	// 「置いてから直す」ではなく「見てから置く」ようにするのが要
	constexpr float GhostR = 0.55f, GhostG = 1.0f, GhostB = 0.55f;

	// 押しっぱなしで引くときの間隔(m)。
	// 0 にすると1フレームに1個置かれて団子になる
	constexpr float DragSpacing = 2.2f;

	// ホイール1目盛りで回る角度(度)と、大きさの倍率
	constexpr float YawStep   = 15.0f;
	constexpr float ScaleStep = 1.08f;

	// 大きさの上下限。青天井にすると、誤操作で山より大きい木ができる
	// 下は思い切り小さくまで許す。
	// 同じモデルを小さくして下草や苔として撒く、という使い方をするので、
	// 「木として自然な大きさ」で下限を切ると、その手が使えなくなる
	constexpr float ScaleLimitMin = 0.01f;
	constexpr float ScaleLimitMax = 6.0f;

	// 升目に合わせる幅(m)。並木や柵を等間隔で並べるとき用
	constexpr float SnapSize = 2.0f;

	//===== 画面外を切る =====
	// 1つぶんの見込みの大きさ(m)。
	// モデルごとに測ってもよいが、飾りの精度としては過剰
	constexpr float CullRadius = 4.0f;
}
