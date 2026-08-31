#pragma once

#include "../Const/ModConst.h"

//==========================================================
// HjModCatalog
//   差し替え用モデルの一覧。自作なので Hj 接頭辞。
//
//   ■ 何をするか
//   決められたフォルダを見て、読める形のファイルだけを候補として並べる。
//   実際の読み込みはしない。ここは「何が置いてあるか」を答えるだけ。
//
//   ■ なぜ読み込みと分けるか
//   フレームワークのモデル読み込みは、失敗すると assert で止まる。
//   外から持ってきたファイルは壊れていることがあるので、
//   そのまま渡すと落ちる。
//   ここで先に「明らかに違うもの」を外しておく。
//
//   ■ ここで防げること・防げないこと
//   防げる  : 形式が違う、大きすぎる、そもそも読めない
//   防げない: 形は正しいが中身が極端に重いもの
//   後者は読み込んだ後に点数で見る(HjModLoader)。
//==========================================================
class HjModCatalog
{
public:
	// 候補1件。
	// 表示名とパスを分けて持つのは、画面には拡張子を出さず、
	// 読み込みにはパスが要るため
	struct Entry
	{
		std::string name;   // 画面に出す名前(拡張子なし)
		std::string path;   // 読み込みに使う相対パス
		int         sizeKb = 0;
	};

	// どちらのフォルダを見るか。
	// 車体とホイールで置き場所を分けているので、取り違えないよう型で持つ
	enum class Kind { Body, Wheel };

	static HjModCatalog& Instance()
	{
		static HjModCatalog inst;
		return inst;
	}

	// フォルダを見て一覧を作り直す。
	// 起動時と、画面で「更新」を押したときに呼ぶ。
	// 毎フレーム呼ぶものではない(フォルダを見に行くので遅い)
	void Rescan();

	const std::vector<Entry>& List(Kind kind) const
	{
		return (kind == Kind::Body) ? m_body : m_wheel;
	}

	// パスから何番目かを引く。選択中の項目を画面で示すために使う。
	// 見つからなければ -1
	int IndexOf(Kind kind, const std::string& path) const;

	// 一度でも調べたか。まだなら画面側が Rescan を呼ぶ
	bool IsScanned() const { return m_scanned; }

	// 直前の走査で外したファイルの数。
	// 画面へ「3件は形式が違うか大きすぎるため外しました」と出すために持つ。
	// 黙って消すと、置いたのに出てこない理由が分からない
	int SkippedCount() const { return m_skipped; }

private:
	HjModCatalog() = default;

	// 1つのフォルダを見て一覧を作る
	void ScanDir(const char* dir, std::vector<Entry>& out);

	std::vector<Entry> m_body;
	std::vector<Entry> m_wheel;
	bool m_scanned = false;
	int  m_skipped = 0;

	HjModCatalog(const HjModCatalog&) = delete;
	void operator=(const HjModCatalog&) = delete;
};
