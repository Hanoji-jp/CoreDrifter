#include "HjUiVisibility.h"

void HjUiVisibility::DrawImGui()
{
	// ※ウィンドウは開かない。Hierarchy の Inspector の中へ描く。
	ImGui::TextWrapped(U8("チェックを外したUIは描画されなくなる。"
	                      "バックバッファへ描く前に止めるので、Gameビューの画像からも消える。"));
	ImGui::Separator();

	ImGui::Checkbox(U8("HUD(速度・ギア・回転数・ドリフト角・スコア)"), &showHud);
	ImGui::Checkbox(U8("通知(チェーン確定などのトースト)"), &showToast);
	ImGui::Checkbox(U8("ビネット・判定演出(NICE/GREAT/PERFECT)"), &showJudge);

}
