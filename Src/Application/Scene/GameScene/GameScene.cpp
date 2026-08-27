#include "GameScene.h"
#include"../SceneManager.h"
#include "../../GameObject/Stage/Ground.h"
#include "../../GameObject/Stage/Stage.h"
#include "../../GameObject/Stage/SkySphere.h"
#include "../../GameObject/Car/Silvia.h"
#include "../../GameObject/Car/RemoteCar.h"
#include "../../Network/HjNetSession.h"
#include "../../Network/HjSteamLobby.h"
#include "../../GameObject/Camera/ChaseCamera.h"
#include "../../GameObject/Score/DriftScore.h"
#include "../../GameObject/UI/RunHudUI.h"
#include "../../GameObject/Score/HjRunResult.h"
#include "../../GameObject/Score/HjPlayerProfile.h"
#include "../../GameObject/UI/CountdownUI.h"
#include "../../GameObject/UI/PauseUI.h"
#include "../../GameObject/UI/ToastUI.h"
#include "../../GameObject/UI/HjToastQueue.h"
#include "../../GameObject/UI/HjUiVisibility.h"
#include "../../Editor/HjHierarchy.h"
#include "../../Util/HjProfiler.h"
#include "../../Const/FogConst.h"

void GameScene::Event()
{
	// 通信は一番先に回す。
	// この下にはポーズなどで途中 return する道があるので、後ろに置くと
	// 止めている間だけ送信が途切れ、相手の画面では自分が固まって見える。
	UpdateNetwork(KdFPSController::GetDt());

	if (GetAsyncKeyState('T') & 0x8000)
	{
		SceneManager::Instance().SetNextScene
		(
			SceneManager::SceneType::Title
		);
	}

	// 数え終わるまでは操作させない。始まりが揃っている方が区切りになる
	auto countdown = m_wpCountdown.lock();
	const bool counting = countdown && !countdown->IsDone();

	// ESC=ポーズ。開いたキーで閉じられるよう、閉じる側はPauseUIが返す
	if (!counting && !m_paused)
	{
		const bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
		if (esc && !m_prevPauseKey) { m_paused = true; }
		m_prevPauseKey = esc;
	}
	else if (!counting)
	{
		// ポーズ中はPauseUI側がESCを見る。二重に拾わないよう状態だけ更新
		m_prevPauseKey = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
	}

	// 開いているかをUI側へ伝える。閉じている間は入力も見ない
	if (auto pause = m_wpPause.lock()) { pause->SetVisible(m_paused); }

	if (m_paused)
	{
		auto pause = m_wpPause.lock();
		if (!pause) { return; }

		// 止まっている間も点数は見たい
		if (auto score = m_wpScore.lock())
		{
			pause->SetScore(score->GetTotal(), score->GetCombo());
		}

		switch (pause->ConsumeAction())
		{
		case PauseUI::Action::Resume:
			m_paused = false;
			break;

		case PauseUI::Action::Restart:
			if (auto car = m_wpCar.lock()) { car->Respawn(); }
			m_paused = false;
			break;

		case PauseUI::Action::EndRun:
			// シーンを切り替えるとオブジェクトが作り直されるので、
			// 点数はここで預けてから移る
			if (auto score = m_wpScore.lock())
			{
				HjRunResult::Instance().Record(score->GetTotal(), score->GetCombo());
				// 累計へも積む(レベル・走行回数はここから求まる)。
				// 積んだ時点でファイルへ保存されるので、強制終了しても残る。
				HjPlayerProfile::Instance().AddRun(score->GetTotal());
			}
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::Results);
			break;

		default:
			break;
		}
		return;
	}

	// R=スポーン地点へリスポーン(押した瞬間だけ)
	const bool respawnKey = (GetAsyncKeyState('R') & 0x8000) != 0;
	if (respawnKey && !m_prevRespawnKey)
	{
		if (auto car = m_wpCar.lock()) { car->Respawn(); }
	}
	m_prevRespawnKey = respawnKey;

	// F2=エディタ表示の切替(押した瞬間だけ)。
	// Gameビュー(シーンをImGuiウィンドウへ貼る)と、Hierarchy/Inspectorを
	// まとめてON/OFFする。走行中はゲーム画面だけを見たいので、既定はOFF。
	//
	// ※F12は使わないこと。Visual Studioのデバッガがブレーク用に予約しており、
	//   デバッガ配下で押すと実行が止まる。
	const bool viewportKey = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
	if (viewportKey && !m_prevViewportKey)
	{
		auto& gui = KdDebugGUI::Instance();
		gui.SetGameViewport(!gui.IsGameViewport());
	}
	m_prevViewportKey = viewportKey;
}

void GameScene::Init()
{
	// ゲーム開始演出：グレースケール→徐々にフルカラーへ戻す
	KdShaderManager::Instance().m_postProcessShader.TriggerColorRestore();

	// 天球(背景の星空)。最初に追加＝背景として描画。
	auto sky = std::make_shared<SkySphere>();
	sky->Init();
	AddObject(sky);

	// 影の範囲を広げる(既定25×25は狭くカメラ近くしか影が出ない。広大マップ向けに拡大)
	KdShaderManager::Instance().WorkAmbientController().SetDirLightShadowArea(
		{ StageConst::ShadowAreaSize, StageConst::ShadowAreaSize }, StageConst::ShadowLightHeight);

	// 距離フォグ：遠くの背景を空の色へ溶け込ませて奥行きを出す。
	// 峠コースは稜線まで見渡せるぶん、フォグが無いと書き割りの書き割り感が出る。
	{
		auto& amb = KdShaderManager::Instance().WorkAmbientController();
		amb.SetDistanceFog({ FogConst::ColorR, FogConst::ColorG, FogConst::ColorB },
		                   FogConst::Density);
		amb.SetFogEnable(true, false);   // 距離フォグのみ。高さフォグは使わない
	}

	// 被写界深度(DoF)：手前(車・路面)はくっきり、遠景だけ柔らかくぼかす。
	// 焦点そのものはChaseCamera::Initで設定している(カメラが持つ値のため)。
	KdShaderManager::Instance().m_postProcessShader.SetDoFEnabled(true);

	// コースマップ(地形＋当たり判定)
	auto stage = std::make_shared<Stage>();
	stage->Init();
	AddObject(stage);

	// 車（W=前進 / S=ブレーキ・後退 / A,D=ステア / Space=ハンドブレーキ）
	auto car = std::make_shared<Silvia>();
	car->Init();
	// 車が接地・壁判定を飛ばす相手として地形を登録
	car->AddCollisionTarget(stage);
	// 保存済みスポーン位置へ配置(StageConfig.txtから読まれた値)
	car->SetSpawn(stage->GetSpawnPos(), stage->GetSpawnYaw());
	AddObject(car);
	m_wpCar = car;   // Rキーのリスポーン用に保持

	// ドリフト採点＆スコア表示演出(車の速度・向きを参照)
	auto score = std::make_shared<DriftScore>();
	score->SetCar(car);
	AddObject(score);
	m_wpScore = score;   // 走行終了時に点数を読む

	// 走行中のHUD(速度・ギア・回転数・ドリフト角・スコア)。
	// 車から切り出してある。車の役目は走ることで、画面の作りとは関係がない。
	auto hud = std::make_shared<RunHudUI>();
	hud->SetCar(car);
	hud->SetScore(score);
	AddObject(hud);

	// 走行中の通知。前の走行のぶんが残っていると混ざるので空にしてから始める
	HjToastQueue::Instance().Clear();
	AddObject(std::make_shared<ToastUI>());

	// 走り出しの合図。数え終わるまで車と採点を止める
	auto countdown = std::make_shared<CountdownUI>();
	AddObject(countdown);
	m_wpCountdown = countdown;

	// ポーズ。最後に足す＝一番手前に描く
	auto pause = std::make_shared<PauseUI>();
	AddObject(pause);
	m_wpPause = pause;

	// 調整パネル(ImGui)：車のチューニングとマップ配置を1つのコールバックにまとめて登録
	//   ※SetPersistentGuiCallbackは単一スロット(上書き)なので合成して渡す
	KdDebugGUI::Instance().SetPersistentGuiCallback([car, stage]()
	{
		// エディタ表示(F2)のときだけ Hierarchy/Inspector を出す。
		// 走行中は画面を塞ぎたくないので、常時表示にはしない。
		if (!KdDebugGUI::Instance().IsGameViewport()) { return; }

		// Hierarchy へシーンのものを並べる。
		// 選ばれた項目だけが自分のパネルを描くので、
		// 車が増えても常時開くウィンドウは増えない。
		auto& hier = HjHierarchy::Instance();
		hier.Begin();

		hier.Add(car->GetTuningName(), [car, stage]()
		{
			car->DrawImGui();

			// 車とステージの両方を触れるこの場所で、スポーン設定の橋渡しを出す。
			// Inspector の中なので、そのまま続けて描けばよい。
			ImGui::Separator();
			if (ImGui::Button(U8("現在の車位置をスポーンに設定")))
			{
				stage->SetSpawn(car->GetPos(), car->GetYaw());
				stage->SaveConfig();   // 押した時点で即保存
			}
			ImGui::SameLine();
			if (ImGui::Button(U8("スポーンへ移動(R)")))
			{
				car->SetSpawn(stage->GetSpawnPos(), stage->GetSpawnYaw());
			}
		});

		// 車が今ぶつかっているノードを渡す。
		// 地形の名前からは種類が読み取れないので、
		// 実際にぶつかった物をその場で外せるようにする
		hier.Add("Stage",        [stage, car]() { stage->DrawTuningImGui(car->GetLastWallNode()); });
		hier.Add("Text Effects", []() { KdShaderManager::Instance().m_postProcessShader.DrawFluidTextImGui(); });
		hier.Add("UI Layers",    []() { HjUiVisibility::Instance().DrawImGui(); });
		// 通信の状態と、ホスト/参加の操作。
		// ルーム画面から繋ぐ作りになるまでは、ここから直接繋いで試す
		// 1フレームの時間がどこで使われているか。
		// FPSは60で頭打ちなので、速くなったかどうかはこちらでしか分からない
		hier.Add("Profiler",     []() { HjProfiler::Instance().DrawImGui(); });
		hier.Add("Network",      []() { HjNetSession::Instance().DrawImGui(); });
		hier.Add("Lobby",        []() { HjSteamLobby::Instance().DrawImGui(); });

		hier.DrawImGui();
	});

	// 追従カメラ
	auto cam = std::make_shared<ChaseCamera>();
	cam->Init();
	cam->SetTarget(car);
	AddObject(cam);
}

//----------------------------------------------------------
// 数えている間とポーズ中は、車と採点を止める。
// 止めている間も、動き続けると名乗ったUIだけは回る。
//----------------------------------------------------------
bool GameScene::IsFrozen() const
{
	if (m_paused) { return true; }

	auto countdown = m_wpCountdown.lock();
	return countdown && !countdown->IsDone();
}

//----------------------------------------------------------
// 通信の橋渡し。
// 自分の状態を渡して、届いた状態をそれぞれの車へ配るだけ。
// 送受信の中身は HjNetSession が持つ。
//----------------------------------------------------------
void GameScene::UpdateNetwork(float dt)
{
	auto& net = HjNetSession::Instance();

	// ロビーの返事を処理する。
	// 通信を始める前(部屋を探している間)も回す必要があるので、
	// 接続中かどうかに関係なく先に呼ぶ
	auto& lobby = HjSteamLobby::Instance();
	lobby.Update();

	// 部屋に入れた瞬間に通信を始める。
	// 部屋主なら待ち受け、そうでなければ部屋主へ繋ぎに行く。
	// 番号を手で入力する必要が無くなるのがロビーを使う利点
	if (lobby.ConsumeJoinedFlag())
	{
		net.SetLink(HjNetSession::Link::Steam);
		if (lobby.IsOwner())
		{
			net.StartHost(HjPlayerProfile::Instance().GetName().c_str(),
			              HjPlayerProfile::Instance().GetColor());
		}
		else
		{
			char ownerId[32] = {};
			snprintf(ownerId, sizeof(ownerId), "%llu", lobby.GetOwnerId());
			net.StartJoin(ownerId, HjPlayerProfile::Instance().GetName().c_str(),
			              HjPlayerProfile::Instance().GetColor());
		}
	}

	// 見分け色は繋いでいる間だけ乗せる。
	//
	// 起動時から乗せてしまうと、調整パネルで設定して保存した
	// アウトラインと煙の色を上書きしてしまい、
	// 「調整が読み込まれていない」ように見える。
	if (auto car = m_wpCar.lock())
	{
		if (net.IsActive())
		{
			car->ApplyPlayerColor(HjPlayerProfile::Instance().GetColor());
		}
		else
		{
			car->ClearPlayerColor();
		}
	}

	if (!net.IsActive()) { return; }

	// 自分の状態を預ける。実際に送るのは送信の番が来たとき
	if (auto car = m_wpCar.lock())
	{
		HjCarSyncState st;
		st.pos          = car->GetPos();
		st.vel          = car->GetVel();
		st.yaw          = car->GetYaw();
		st.steer        = car->GetSteerAngle();
		st.terrainPitch = car->GetTerrainPitch();
		st.terrainRoll  = car->GetTerrainRoll();
		st.bodyPitch    = car->GetBodyPitch();
		st.bodyRoll     = car->GetBodyRoll();
		st.slipRear01   = car->GetSlipRear01();
		st.slipFront01  = car->GetSlipFront01();
		st.handbrake    = car->IsHandbrake();
		st.onGround     = car->IsOnGround();
		net.SetLocalState(st);
	}

	net.Update(dt);

	// 届いた状態を、それぞれの車へ配る
	HjNetStatePacket state;
	while (net.PopState(state))
	{
		if (auto remote = EnsureRemoteCar(state.id))
		{
			remote->PushState(state);

			// 名前と色は名簿が届いて初めて分かる。
			// 車のほうが先にできることがあるので、毎回入れ直す
			remote->SetPlayerName(net.GetPeerName(state.id));
			remote->SetPlayerColor(net.GetPeerColor(state.id));
		}
	}

	// 抜けた相手の車を片付ける
	int goneId = -1;
	while (net.PopRemovedId(goneId)) { RemoveRemoteCar(goneId); }
}

//----------------------------------------------------------
// 番号に対応する他人の車を返す。いなければ作る。
//
// 名簿を待たずに作るのは、状態のほうが先に届くことがあるため。
// 待つと、その間だけ相手の車が出てこない。
//----------------------------------------------------------
std::shared_ptr<RemoteCar> GameScene::EnsureRemoteCar(int playerId)
{
	if (playerId < 0 || playerId >= NetConst::MaxPlayers) { return nullptr; }

	if (auto exist = m_wpRemoteCars[playerId].lock()) { return exist; }

	auto car = std::make_shared<RemoteCar>(playerId);
	car->Init();
	// 地形は登録しない。物理を回さないので接地も壁判定も使わない
	AddObject(car);
	m_wpRemoteCars[playerId] = car;
	return car;
}

void GameScene::RemoveRemoteCar(int playerId)
{
	if (playerId < 0 || playerId >= NetConst::MaxPlayers) { return; }

	if (auto car = m_wpRemoteCars[playerId].lock())
	{
		// 印を付けるとシーン側の更新でリストから外れる
		car->Expire();
	}
	m_wpRemoteCars[playerId].reset();
}
