#include "TitleScene.h"
#include "../../Input/HjKeyInput.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/TitleMenuUI.h"
#include "../../GameObject/UI/HjUpdateNotice.h"
#include "../../GameObject/Stage/TitleShowcase.h"
#include "../../Const/ShowcaseConst.h"

void TitleScene::Event()
{
	// ELEMENTS(UIキット)確認用：F5 でいつでも開ける(デバッグ)
	if (HjKeyInput::Instance().Pressed(VK_F5))
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Elements);
		return;
	}

	// Enter/Space または マウスクリックで決定。選択中メニューに応じて遷移。
	// ※押しっぱなしを条件にしないこと。押している間ずっと決定が成立し、
	//   遷移先でも同じキーで決定が続いてしまう。
	bool decide = HjKeyInput::Instance().Pressed(HjKeyInput::Key::Decide);
	int sel = 0;
	if (auto menu = m_wpMenu.lock())
	{
		// 名前の編集中はENTERを決定に使わせない。
		// 名前の確定に使うキーなので、そのまま画面遷移してしまう。
		if (menu->IsNameEditing()) { return; }

		sel = menu->GetSelected();
		if (menu->ConsumeActivated()) { decide = true; }
	}
	if (decide)
	{
		switch (sel)
		{
		case 0: // PLAY：PLAYバーが伸びて次レイアウトへ変形するモーフ遷移
			HjTransition::Instance().GoMorph(SceneManager::SceneType::PlayMode, 54.0f, 380.0f, 400.0f, 51.0f);
			break;
		case 2: HjTransition::Instance().Go(SceneManager::SceneType::Settings); break;  // SETTINGS
		case 4: PostQuitMessage(0);                                             break;  // QUIT
		default: HjTransition::Instance().Go(SceneManager::SceneType::Game);    break;  // GARAGE/STATS(未実装)→暫定Game
		}
	}
}

void TitleScene::Init()
{
	m_objList.clear();

	// 背景は緑一色。タイトルは3Dの背景を持たないので、
	// ここで指定した色がそのまま窓の中に出る
	KdShaderManager::Instance().m_postProcessShader.SetSceneClearColor(
		Math::Color(ShowcaseConst::BgR, ShowcaseConst::BgG, ShowcaseConst::BgB, 1.0f));

	// 窓の中に映る車。走らせず、回転台に載せたように回すだけ
	auto showcase = std::make_shared<TitleShowcase>();
	showcase->Init();
	AddObject(showcase);

	auto menu = std::make_shared<TitleMenuUI>();
	menu->Init();
	AddObject(menu);
	m_wpMenu = menu;

	// 更新の知らせ(左下)。新しい版があるときだけ出る。
	//
	// ※メニューより後に足すこと。
	//   あちらは画面いっぱいに紙色の地を描くので、
	//   先に足すとその下へ潜って見えなくなる
	auto notice = std::make_shared<HjUpdateNotice>();
	AddObject(notice);
}
