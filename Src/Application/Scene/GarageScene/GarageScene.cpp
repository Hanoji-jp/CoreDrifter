#include "GarageScene.h"

#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../Input/HjKeyInput.h"
#include "../../GameObject/UI/GarageUI.h"
#include "../../GameObject/UI/HjCarPortrait.h"
#include "../../GameObject/Car/HjCarChoice.h"

void GarageScene::Event()
{
	auto ui = m_wpUI.lock();
	if (!ui) { return; }

	// 選んでいる車を絵にする。
	// 同じ車なら中で弾かれるので、毎フレーム渡してよい
	if (auto p = m_wpPortrait.lock()) { p->SetCar(ui->Selected()); }

	// 決めた。選択を覚えてタイトルへ戻る。
	//
	// ここでゲームへ直行しない。車庫は「乗る車を決める所」で、
	// 走り出すのは PLAY から。決定と発進を混ぜると、
	// 見に来ただけのつもりが走り出す
	if (ui->ConsumeDecided())
	{
		HjCarChoice::Instance().Set(ui->Selected());
		HjTransition::Instance().Go(SceneManager::SceneType::Title);
		return;
	}

	if (ui->ConsumeBack()
	 || HjKeyInput::Instance().Pressed(HjKeyInput::Key::Cancel))
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Title);
	}
}

void GarageScene::Init()
{
	m_objList.clear();

	// 車の絵を先に作る。UIより前に PreDraw を回したいわけではなく
	// (PreDraw は描画の前に全部走る)、UIへ渡す相手を先に用意するため
	auto portrait = std::make_shared<HjCarPortrait>();
	portrait->Init();
	AddObject(portrait);
	m_wpPortrait = portrait;

	auto ui = std::make_shared<GarageUI>();
	ui->Init();
	ui->SetPortrait(portrait);
	AddObject(ui);
	m_wpUI = ui;

	portrait->SetCar(ui->Selected());
}
