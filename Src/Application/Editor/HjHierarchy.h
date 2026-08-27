#pragma once

//==========================================================
// HjHierarchy
//   Unity の Hierarchy と同じ役割の、シーン内オブジェクト一覧。
//
//   これまで調整パネルは「車」「ステージ」「エンジン音」…と
//   それぞれが独立したウィンドウを常時開いていた。
//   車が10台に増える予定なので、そのままだと画面がパネルで埋まる。
//
//   ここで一覧を持ち、選ばれたものだけが自分のパネルを描く形にする。
//   各クラスの DrawImGui() はウィンドウ生成込みのまま使えるので、
//   既存の調整パネルを書き換える必要はない。
//
//   使い方(毎フレーム):
//     Begin();                          … 前フレームの登録を捨てる
//     Add("Silvia", [&]{ car->DrawImGui(); });
//     Add("Stage",  [&]{ stage->DrawTuningImGui(); });
//     DrawImGui();                      … 一覧＋選択物のパネルを描く
//==========================================================
class HjHierarchy
{
public:
	static HjHierarchy& Instance()
	{
		static HjHierarchy inst;
		return inst;
	}

	// 一覧に並べる1項目。
	// draw は「選ばれているときだけ」呼ばれる＝パネルの描画そのもの。
	struct Entry
	{
		std::string           label;
		std::function<void()> draw;
	};

	// 毎フレーム、登録を作り直す(シーンが切り替わっても取り残しが出ない)
	void Begin() { m_entries.clear(); }
	void Add(const std::string& label, const std::function<void()>& draw)
	{
		m_entries.push_back(Entry{ label, draw });
	}

	// 一覧ウィンドウと、選択中の項目のパネルを描く
	void DrawImGui();

private:
	HjHierarchy() = default;

	std::vector<Entry> m_entries;
	// 選択中の項目。名前で覚える。
	// 添字で覚えると、登録順が変わったときに別のものが選ばれてしまう。
	std::string m_selected;

	HjHierarchy(const HjHierarchy&) = delete;
	void operator=(const HjHierarchy&) = delete;
};
