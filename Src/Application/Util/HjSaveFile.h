#pragma once

#include <string>
#include <sstream>

//==========================================================
// HjSaveFile
//   遊ぶ側が書き換えるものを1つのファイルへまとめる。
//   自作なので Hj 接頭辞。
//
//   ■ なぜまとめるか
//   保存物の置き場が Asset/Data/ の下に散らばっていた。
//   配布ビルドは Asset/ を exe へ埋め込むので、書き込むと
//   exe の隣に Asset/Data/ が生える。アセットが素で置いてある
//   ように見えて紛らわしいし、消していいのかも分からない。
//
//   save.dat 1つなら、消せば初期状態に戻ると一目で分かる。
//
//   ■ 中身は「名前つきの文字列」
//   項目を並べた形にすると、保存を1つ足すたびに読み書き両方を
//   直すことになり、順番を間違えた瞬間に全部ずれる。
//
//   ここは入れ物だけ。中身の書き方は今までどおり各所が決める。
//   既存の保存の形をそのまま入れられるので、移し替えが要らない。
//
//   ■ 形式(リトルエンディアン)
//     'H''S''A''V'      : 目印
//     uint32 version    : = 1
//     uint32 count      : 項目数
//     [count 回]
//       uint32 keyLen / char[] key
//       uint32 dataLen / char[] data
//==========================================================
namespace HjSaveFile
{
	// 読み込む。起動時に1回。
	// 無いのは異常ではない(初回起動)
	void Load();

	// 書き出す。溜めてから一度に書く。
	// 項目ごとに書き出すと、保存のたびにファイル全体を作り直すことになる
	void Save();

	// その項目があるか
	bool Has(const std::string& key);

	// 中身を取る。無ければ空
	std::string Get(const std::string& key);

	// 中身を入れる。
	//
	// ここでは書き出さない。終了時と、区切りのいい所で Save() を呼ぶ。
	// ただし「落ちても消えてほしくない」ものは、その場で Save() してよい
	void Set(const std::string& key, const std::string& value);
}

//==========================================================
// HjSaveIStream / HjSaveOStream
//   save.dat の1項目を、ファイルと同じ書き方で読み書きする。
//
//   ■ なぜ流れの形にするか
//   各所は既に「1行ずつ書く／読む」で出来ている。
//   入れ物が変わっただけなのに、書き方まで変えると
//   保存の中身を全部作り直すことになる。
//
//   std::ifstream / std::ofstream をこれに差し替えるだけで、
//   以降の if(!ifs) / >> / << はそのまま動く。
//==========================================================

// save.dat の1項目を読む。
//
// 無ければ、配ってある既定(assetPath)を読む。
// どちらも無ければ失敗した状態にするので、if(!ifs) が真になる
class HjSaveIStream : public std::istringstream
{
public:
	explicit HjSaveIStream(const std::string& key, const char* assetPath = nullptr);
};

// save.dat の1項目へ書く。
//
// 閉じた時点で save.dat ごと書き出す。
// 溜めておくと、途中で落ちたときに設定が丸ごと消える
class HjSaveOStream : public std::ostringstream
{
public:
	explicit HjSaveOStream(const std::string& key) : m_key(key) {}
	~HjSaveOStream();

private:
	std::string m_key;
};
