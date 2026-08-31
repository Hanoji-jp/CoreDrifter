#pragma once

// 自動更新の設定。
//
// ■ どう動くか
// 起動時に GitHub のリリースを見て、手元より新しければ知らせる。
// 受け入れたら zip を落として、いったんゲームを閉じ、
// 裏で走らせたバッチが展開してから開き直す。
//
// ■ なぜ自分自身を上書きできないか
// 動いている exe は掴まれていて置き換えられない。
// だから「閉じるのを待って、それから入れ替える」役を
// 別のプロセス(バッチ)に任せる。
namespace UpdaterConst
{
	//===== どこを見るか =====
	// リリースを置いてあるリポジトリ。
	// ソースとリリースを分けたくなったら、ここだけ変える
	constexpr const char* Owner = "Hanoji-jp";
	constexpr const char* Repo  = "CoreDrifter";

	constexpr const char* ApiHost = "api.github.com";

	// GitHub API は User-Agent が無いと弾く。
	// 何が繋いできたのか分かる名前にしておく
	constexpr const char* UserAgent = "CoreDrifterUpdater/1.0";

	//===== 手元の版 =====
	// exe と同じ場所に置く。中身は "v1.0.0" の1行。
	// 無ければ v0.0.0 扱いにして、必ず更新がある状態にする
	constexpr const char* VersionFile = "version.txt";
	constexpr const char* UnknownVersion = "v0.0.0";

	//===== 受け取り =====
	// 落とす先。展開したら消す
	constexpr const char* ZipTemp = "update_tmp.zip";
	// 入れ替えを任せるバッチ。走り終わったら自分で消える
	constexpr const char* BatchFile = "update.bat";
	// 開き直す相手
	constexpr const char* ExeName = "Project.exe";

	// 落とすものを選ぶときの目印。
	// リリースには pdb や zip 以外も並ぶことがあるので、
	// 名前の終わりで選ぶ
	constexpr const char* AssetSuffix = ".zip";

	//===== 通信 =====
	// 受け取りに使う一時置き場の大きさ。
	// 小さいと呼び出し回数が増え、大きいと無駄に確保する
	constexpr int ReadChunk = 65536;
	// 応答の本文を読むときの一時置き場
	constexpr int BodyChunk = 4096;

	// 行き先が変わったときに、何回まで追いかけるか。
	// GitHub のリリースは配信元へ回されるので追う必要があるが、
	// 回り続ける相手に当たったときのために上限を置く
	constexpr int MaxRedirect = 5;

	// バッチがゲームの終了を待つときの、1回あたりの待ち(秒)
	constexpr int WaitTickSec = 1;
}
