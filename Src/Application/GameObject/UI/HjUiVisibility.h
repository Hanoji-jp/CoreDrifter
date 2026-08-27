#pragma once

//==========================================================
// HjUiVisibility
//   走行中のUIレイヤーを個別に選んで非表示にするためのスイッチ集。
//
//   スクリーンショットや配信で「HUDだけ消したい」「通知だけ消したい」
//   といった需要は、UIオブジェクトごとに別のクラスなので1つのフラグでは
//   まとめられない。かといって各UIクラスへ個別のON/OFFメソッドを生やすと
//   呼ぶ側(GameScene)が全種類を把握しないといけなくなる。
//   ここへ集約し、各UIのDrawSprite冒頭で1行読むだけにする。
//==========================================================
class HjUiVisibility
{
public:
	static HjUiVisibility& Instance()
	{
		static HjUiVisibility inst;
		return inst;
	}

	bool showHud    = true;   // 速度・ギア・回転数・ドリフト角・スコア(RunHudUI)
	bool showToast  = true;   // 走行中の通知(ToastUI)
	bool showJudge  = true;   // ビネット・NICE/GREAT/PERFECTの演出(DriftScore)

	// 調整パネル。GameSceneの常時ImGuiから呼ぶ
	void DrawImGui();

private:
	HjUiVisibility() = default;

	HjUiVisibility(const HjUiVisibility&) = delete;
	void operator=(const HjUiVisibility&) = delete;
};
