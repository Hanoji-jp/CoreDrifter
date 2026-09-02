#pragma once

class DriftScore;

#include"../BaseScene/BaseScene.h"
#include "../../Const/NetConst.h"   // 参加人数の上限(規約上constヘッダはinclude可)
#include "../../Network/HjNetProtocol.h"   // 車の見た目(HjCarLook)
#include "../../GameObject/Car/HjCarTrail.h"   // 追走の手本(規約上constヘッダはinclude可)
#include "../../GameObject/Stage/HjRoadEditor.h"   // 道の制御点を掴む(3D)
#include "../../GameObject/Stage/HjRoadMap.h"      // 上から見て線を引く

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

	// MODメニュー(TABで開く)。開いている間は車の操作を止めるので、
	// 開いているかを毎フレーム見に行く
	std::weak_ptr<class HjModMenu> m_wpModMenu;

	//===== 追走のCPU(試作) =====
	// プレイヤーの通ってきた跡。CPUはこれを手本にする。
	//
	// 追走の目標は録画では代用できない。前を走るのが人なら
	// どこを走るかは毎回変わるので、その場で溜めるしかない。
	// 場面が持つのは、車より寿命が長いから(車を作り直しても跡は残る)
	// 道と地形。編集で触り続けるので、場面が持つ。
	// 弱い参照だと、掴んでいる間に消える心配をすることになる
	std::shared_ptr<class HjTerrain> m_spTerrain;
	std::shared_ptr<class HjRoad>    m_spRoad;

	// 道を編集しているか。編集中だけ制御点をギズモで掴める
	bool m_roadEditing = false;

	// 道の制御点を触るためのエディタ。
	// 既にあるものをそのまま使う。制御点用に別の仕組みを作ると、
	// ギズモの操作の癖が2つになる
	// 道の制御点を掴む。
	//
	// 共通のギズモは軸方向にしか動かない。道を引くのは
	// 「地図の上で線を引く」操作なので、軸に縛られると
	// 東へ動かしてから北へ動かす、を繰り返すことになる
	// 3Dで点を掴む形。狙いが合わせにくいので、いまは地図が主
	HjRoadEditor m_roadEditor;

	// 上から見て線を引く。
	//
	// 地図の上ならマウスの位置がそのまま座標になるので、
	// 3Dのように狙いが外れることがない
	HjRoadMap m_roadMap;

	// 編集中の自由カメラ。
	//
	// 走行中のカメラは車を追うので、道を引く間は使えない。
	// 引きたい所へ行けないと、そもそも点を掴めない
	std::shared_ptr<class HjEditorCamera> m_spEditCam;

	// 描画のときにカメラを差し替える物。
	// カメラより後に足してあるので、走行中のものを上書きできる
	std::weak_ptr<class HjEditCamHolder> m_wpEditCamHolder;

	HjCarTrail m_playerTrail;
	float      m_trailTime = 0.0f;

	std::weak_ptr<class CpuCar> m_wpCpuCar;
	bool m_prevRespawnKey = false;
	bool m_prevViewportKey = false;   // F2: エディタ表示ON/OFF
	// 走行終了時に点数を読むために保持する
	std::weak_ptr<DriftScore> m_wpScore;
	// カウントダウン中とポーズ中は車と採点を止める
	bool IsFrozen() const override;

private:
	// 道の制御点をエディタへ登録する。
	// 既にあるエディタ基盤へ乗せるので、ギズモもUndoもそのまま使える
	void UpdateRoadEdit();

public:

	//===== 通信 =====
	// 自分の車の状態を送り、届いた状態を他人の車へ配る。
	// 通信そのものは HjNetSession が持ち、ここは橋渡しだけをする
	// 自分の車の見た目を作る。通信側で色を決めず、
	// 車の調整パネルで設定した色をそのまま配る
	HjCarLook BuildMyLook() const;
	void UpdateNetwork(float dt);
	// 番号に対応する他人の車を用意する(いなければ作る)
	std::shared_ptr<class RemoteCar> EnsureRemoteCar(int playerId);
	void RemoveRemoteCar(int playerId);

	// 他人の車。番号で引けるようにしておく(所有はシーンのオブジェクトリスト側)
	std::weak_ptr<class RemoteCar> m_wpRemoteCars[NetConst::MaxPlayers];

	//===== 観戦 =====
	// カメラの追う相手を切り替えるだけ。
	// 自分の車は走り続けるので、止めたい場合はポーズを使う。
	//
	// 相手の走りを見られると、同期がどれだけ正しいか自分の目で分かる。
	// 通信の作りを直したときに、真っ先に確かめたい所でもある。
	void SetSpectate(int playerId);   // -1 = 自分
	void DrawSpectateImGui();

	std::weak_ptr<class ChaseCamera> m_wpCamera;
	int m_spectateId = -1;

	std::weak_ptr<class CountdownUI> m_wpCountdown;
	std::weak_ptr<class PauseUI>     m_wpPause;
	bool m_paused = false;
	bool m_prevPauseKey = false;
};
