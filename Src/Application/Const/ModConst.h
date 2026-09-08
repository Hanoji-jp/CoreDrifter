#pragma once

// 車の見た目を差し替える仕組み(MOD)の定数。
//
// ■ 何をするものか
// 決まったフォルダへ .gltf を置くと、車体・ホイールの候補として出てくる。
// 選ぶとその場で反映される。
//
// ■ 大きさや向きの調整はここには無い
// 外から持ってきたモデルは、原点も向きも大きさもバラバラで、
// 「置いたら乗る」ことはまず無い。ただし合わせるための値は
// 車の調整(CarTune)に既にある(bodyScale / bodyYaw / track など)。
// ここで別に持つと、同じものが2つになって、
// どちらが効いているのか分からなくなる。
namespace ModConst
{
	//===== 置き場所 =====
	// Asset の下に置く。フレームワークが pak へまとめる経路も
	// このパスを鍵に使うので、相対パスで統一する
	// exe と同じ場所の Mods/ に置く。Asset/ の下ではない。
	//
	// 配布ビルドは Asset/ を exe へ埋め込むので、
	// Asset/Mods だけ実体のあるフォルダとして残ると
	// 「アセットが素で置いてある」ように見えて紛らわしい。
	//
	// MOD は遊ぶ側が入れるものなので、外に出しておくほうが分かりやすい
	constexpr const char* BodyDir  = "Mods/Body";
	constexpr const char* WheelDir = "Mods/Wheel";

	// 選んだものを覚えておくファイル。
	// 車の調整とは別にする。あちらは数値だけを並べる作りで、
	// 文字列(パス)を混ぜると読み込みが壊れる
	constexpr const char* SavePrefix = "Asset/Data/CarMod_";
	constexpr const char* SaveSuffix = ".txt";

	// 保存ファイルの中の名前
	constexpr const char* KeyBody  = "body";
	constexpr const char* KeyWheel = "wheel";

	//===== モデルごとの合わせ込み =====
	// 向きや大きさは、モデルごとにまったく違う値になる。
	// 車の調整(CarTune)へ書くと、モデルを替えるたびに前の値が消え、
	// 標準へ戻したときには標準の値まで壊れている。
	//
	// モデルの道を鍵にして、1つずつ別に持つ。
	// 形が入れ子になるので、こちらは JSON にする
	constexpr const char* ProfilePath = "Asset/Data/ModProfiles.json";

	// 字下げの幅。手で開いて直せるほうが、
	// 合わない値を1つ消したいときに早い
	constexpr int JsonIndent = 2;

	//===== 受け入れる形式 =====
	// フレームワークのローダが読めるのはこの2つだけ。
	// それ以外を読ませても失敗するので、一覧の時点で外す
	constexpr const char* ExtGltf = ".gltf";
	constexpr const char* ExtGlb  = ".glb";

	//===== 上限 =====
	// ■ 容量
	// 読み込みの重さと、あとで配布するときの通信量を抑える。
	// 車1台ぶんの見た目なら、これで足りないことはまず無い。
	//
	// ※これは「重すぎるものを弾く」ための線であって、
	//   中身が安全であることを保証するものではない。
	//   小さくても極端に重いデータは作れるので、
	//   読み込んだ後の点数でもう一度見る
	constexpr int MaxFileSizeKb = 8192;   // 8MB

	// ■ 読み込んだ後の点数
	// 容量だけでは、小さいのに極端に重いデータを止められない。
	// このゲームは1画面に何台も出るので、1台がここを超えると
	// それだけでフレームが持たなくなる
	constexpr int MaxVertices = 300000;
	constexpr int MaxNodes    = 512;

	// 一覧に並べる上限。フォルダへ大量に置かれたときに、
	// 起動のたびに全部を調べて待たされることがないように
	constexpr int MaxEntries = 64;

	//===== 標準を表す印 =====
	// 保存ファイルは空白区切りなので、空文字だと項目が消えてしまう。
	// 「標準のまま」を表す文字を決めておく
	constexpr const char* StockMark = "-";
}
