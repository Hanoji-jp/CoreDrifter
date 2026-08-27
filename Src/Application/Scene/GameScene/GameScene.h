#pragma once

class DriftScore;

#include"../BaseScene/BaseScene.h"
#include "../../Const/NetConst.h"   // 参加人数の上限(規約上constヘッダはinclude可)

class GameScene : public BaseScene
{
public :

	GameScene()  { Init(); }
	~GameScene() {}

private:

	void Event() override;
	void Init()  override;

	// ゲーム中シーンなのでドリフトのスコア文字演出を使う
	bool UsesFluidText() const override { return true; }

	// Rキーのリスポーン用に車を保持(所有はシーンのオブジェクトリスト側)
	std::weak_ptr<class CarBase> m_wpCar;
	bool m_prevRespawnKey = false;
	bool m_prevViewportKey = false;   // F2: エディタ表示ON/OFF
	// 走行終了時に点数を読むために保持する
	std::weak_ptr<DriftScore> m_wpScore;
	// カウントダウン中とポーズ中は車と採点を止める
	bool IsFrozen() const override;

	//===== 通信 =====
	// 自分の車の状態を送り、届いた状態を他人の車へ配る。
	// 通信そのものは HjNetSession が持ち、ここは橋渡しだけをする
	void UpdateNetwork(float dt);
	// 番号に対応する他人の車を用意する(いなければ作る)
	std::shared_ptr<class RemoteCar> EnsureRemoteCar(int playerId);
	void RemoveRemoteCar(int playerId);

	// 他人の車。番号で引けるようにしておく(所有はシーンのオブジェクトリスト側)
	std::weak_ptr<class RemoteCar> m_wpRemoteCars[NetConst::MaxPlayers];

	std::weak_ptr<class CountdownUI> m_wpCountdown;
	std::weak_ptr<class PauseUI>     m_wpPause;
	bool m_paused = false;
	bool m_prevPauseKey = false;
};
