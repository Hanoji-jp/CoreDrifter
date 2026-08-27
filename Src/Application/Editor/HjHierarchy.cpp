#include "HjHierarchy.h"

void HjHierarchy::DrawImGui()
{
	// --- Hierarchy：シーンにあるものを並べる ---
	ImGui::Begin(U8("Hierarchy"));

	if (m_entries.empty())
	{
		ImGui::TextUnformatted(U8("(このシーンには調整できるものがありません)"));
	}

	for (const Entry& e : m_entries)
	{
		// Selectable は「選ばれている状態」を色で示せるので、一覧の行に向く
		const bool selected = (e.label == m_selected);
		if (ImGui::Selectable(e.label.c_str(), selected))
		{
			// 選ばれているものをもう一度押したら閉じる。
			// 一覧から目的のパネルへ行き来する際、閉じる操作を探さずに済む。
			m_selected = selected ? std::string() : e.label;
		}
	}
	ImGui::End();

	// --- Inspector：選ばれた項目の中身をここへ描く ---
	//
	// ウィンドウを開くのはここ1箇所だけにする。
	// 各パネルが自前で Begin する作りだと、選ぶたびに違う名前のウィンドウが
	// 現れて位置もドッキング状態もバラバラになる。
	// Inspector という決まった場所に必ず出る方が、
	// 一度レイアウトを組めばそこを見ればよくなる。
	ImGui::Begin(U8("Inspector"));

	const Entry* sel = nullptr;
	for (const Entry& e : m_entries)
	{
		if (e.label == m_selected) { sel = &e; break; }
	}

	if (!sel)
	{
		ImGui::TextUnformatted(U8("(Hierarchy で項目を選んでください)"));
	}
	else
	{
		// 何を編集しているかを常に出す。
		// Inspector は中身が入れ替わるので、見出しが無いと迷子になる。
		ImGui::TextUnformatted(sel->label.c_str());
		ImGui::Separator();
		if (sel->draw) { sel->draw(); }
	}

	ImGui::End();
}
