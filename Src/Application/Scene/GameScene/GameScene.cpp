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
#include "../../GameObject/UI/HjModMenu.h"
#include "../../GameObject/Car/HjCarChoice.h"
#include "../../GameObject/Car/CpuCar.h"
#include "../../GameObject/Stage/HjTerrain.h"
#include "../../GameObject/Stage/HjRoad.h"
#include "../../GameObject/Stage/HjRoadEditor.h"
#include "../../Input/HjKeyInput.h"
#include "../../GameObject/Camera/HjEditorCamera.h"
#include "../../GameObject/Stage/HjEditCamHolder.h"
#include "../../GameObject/Stage/HjRoad.h"
#include "../../Updater/HjUpdater.h"
#include "../../GameObject/Score/HjRunResult.h"
#include "../../GameObject/Score/HjPlayerProfile.h"
#include "../../GameObject/UI/CountdownUI.h"
#include "../../GameObject/UI/PauseUI.h"
#include "../../GameObject/UI/ToastUI.h"
#include "../../GameObject/UI/HjToastQueue.h"
#include "../../GameObject/UI/HjUiVisibility.h"
#include "../../Editor/HjHierarchy.h"
#include "../../Util/HjProfiler.h"
#include "../../Audio/HjBgm.h"
#include "../../Const/FogConst.h"

void GameScene::Event()
{
	// 道の制御点を編集する。
	//
	// 座標を手で打つのでは道具にならない。掴んで動かして、
	// その場で道と地形が付いてくる形にする
	UpdateRoadEdit();

	// プレイヤーの跡を溜める。CPUはこれを手本にする。
	//
	// 位置と向きだけでは、CPUは釣り合う踏み量を自分で探すことになる。
	// 前を走っている人が既に正解を出しているので、操作も一緒に貸す
	if (auto car = m_wpCar.lock())
	{
		HjCarTrail::Point p;
		p.pos       = car->GetPos();
		p.vel       = car->GetVel();
		p.yaw       = car->GetYaw();
		p.driftDeg  = car->GetDriftAngleDegSigned();
		p.throttle  = car->GetLastInput().throttle;
		p.steer     = car->GetLastInput().steer;
		p.handbrake = car->GetLastInput().handbrake;

		m_trailTime += KdFPSController::GetDt();
		m_playerTrail.Push(m_trailTime, p);
	}

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
	// 背景色を既定へ戻す。
	// タイトルが緑にしているので、戻さないと空の見えない所が緑になる
	KdShaderManager::Instance().m_postProcessShader.SetSceneClearColor(kBlueColor);

	KdShaderManager::Instance().m_postProcessShader.SetDoFEnabled(true);

	// コースマップ(地形＋当たり判定)
	// 借りてきた峠のモデル。
	// 地形へ差し替える途中なので、両方残してある
	std::shared_ptr<Stage>     stage;
	std::shared_ptr<HjTerrain> terrain;
	std::shared_ptr<HjRoad>    road;

	if (TerrainConst::UseTerrain)
	{
		terrain = std::make_shared<HjTerrain>();
		terrain->Init();
		AddObject(terrain);

		// 道。地形へ沿わせて敷く。
		// 高さマップでは路面の断面が出せないので、道は別に持つ
		road = std::make_shared<HjRoad>();
		// 地形を書き換える。道の周りを道の高さへ寄せないと、
		// 埋まったり浮いたりする
		road->Init(&terrain->WorkField());
		AddObject(road);

		// 地形のメッシュは、道が地形を寄せたあとに組む。
		// 先に組むと、寄せる前の形で頂点と法線を作ってしまう
		terrain->BuildChunks();

		// 編集で触り続けるので持っておく
		m_spTerrain = terrain;
		m_spRoad    = road;
	}
	else
	{
		stage = std::make_shared<Stage>();
		stage->Init();
		AddObject(stage);
	}

	// 車（W=前進 / S=ブレーキ・後退 / A,D=ステア / Space=ハンドブレーキ）
	// 選ばれている車種で作る。
	// ここで車種ごとに分岐を書くと、車を足すたびに直すことになる
	HjCarChoice::Instance().Load();
	auto car = HjCarChoice::Instance().Create();
	car->Init();
	// 車が接地・壁判定を飛ばす相手として地形を登録
	// 地形があれば、接地はそちらから取る。
	// メッシュへ光線を飛ばすより桁違いに軽く、
	// サスペンションを入れたときに効いてくる
	if (terrain) { car->SetHeightField(&terrain->Field()); }
	else         { car->AddCollisionTarget(stage); }
	if (road)    { car->SetRoad(road.get()); }
	// 保存済みスポーン位置へ配置(StageConfig.txtから読まれた値)
	// 道があれば、その始点へ置く。地形の谷底より確実
	if (road)         { car->SetSpawn(road->GetStartPos(), road->GetStartYaw()); }
	else if (terrain) { car->SetSpawn(terrain->GetSpawnPos(), 0.0f); }
	else              { car->SetSpawn(stage->GetSpawnPos(), stage->GetSpawnYaw()); }
	AddObject(car);

	// 追走のCPU(試作)。
	//
	// 手本はプレイヤーの生きた跡。走り出すまで何も持っていないので、
	// 最初のうちは動かない。それでよい(目標が来た瞬間に飛ぶより良い)
	{
		auto cpu = std::make_shared<CpuCar>();
		cpu->Init();
		cpu->SetTrail(&m_playerTrail);

		// 当たる相手を渡す。
		// これが無いと地形を拾えず、床をすり抜けて落ちていく。
		// 物理は本物を回しているので、プレイヤーと同じものが要る
		if (terrain) { cpu->SetHeightField(&terrain->Field()); }
		else         { cpu->AddCollisionTarget(stage); }
		if (road)    { cpu->SetRoad(road.get()); }

		// プレイヤーの少し後ろへ置く。同じ場所に出すと、開始の瞬間に押し合う。
		// 向きも揃える(横を向いて始まると、いきなり復帰状態から始まる)
		const Math::Vector3 back(sinf(car->GetYaw()), 0.0f, cosf(car->GetYaw()));
		cpu->SetSpawn(car->GetPos() - back * ChaseConst::StartGap, car->GetYaw());

		m_wpCpuCar = cpu;
		AddObject(cpu);
	}
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

	// MODメニュー(TABで開く)。
	// 走行画面の上へ重ねるので、HUDより後ろに足して上に出す
	auto modMenu = std::make_shared<HjModMenu>();
	modMenu->Init();
	modMenu->SetCar(car);
	m_wpModMenu = modMenu;
	AddObject(modMenu);

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
	KdDebugGUI::Instance().SetPersistentGuiCallback([this, car, stage, terrain, road]()
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
				if (stage)
				{
					stage->SetSpawn(car->GetPos(), car->GetYaw());
					stage->SaveConfig();   // 押した時点で即保存
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(U8("スポーンへ移動(R)")))
			{
				if (stage) { car->SetSpawn(stage->GetSpawnPos(), stage->GetSpawnYaw()); }
			}
		});

		// 車が今ぶつかっているノードを渡す。
		// 地形の名前からは種類が読み取れないので、
		// 実際にぶつかった物をその場で外せるようにする
		if (stage)
		{
			hier.Add("Stage",    [stage, car]() { stage->DrawTuningImGui(car->GetLastWallNode()); });
		}
		if (terrain)
		{
			// 地形の様子。まとまりをいくつ描いているかを見る
			hier.Add("Terrain",  [this, terrain, road]()
			{
				const auto& f = terrain->Field();
				ImGui::Text(U8("格子 %d x %d  間隔 %.1f m"),
				            f.GetSizeX(), f.GetSizeZ(), f.GetCellSize());
				ImGui::Text(U8("広さ %.0f x %.0f m"), f.GetWorldW(), f.GetWorldD());
				ImGui::Text(U8("まとまり %d / %d を描画"),
				            terrain->GetDrawnChunks(), terrain->GetChunkCount());

				if (road)
				{
					ImGui::Separator();

					// 編集の入口。
					// 入れている間だけ制御点をギズモで掴める
					if (ImGui::Checkbox(U8("道を編集する"), &m_roadEditing))
					{
						// ゲーム画面のウィンドウを出す。
						//
						// マウスの位置はそのウィンドウの中で測っているので、
						// 出ていないと視線が作れず、何も掴めない
						KdDebugGUI::Instance().SetGameViewport(m_roadEditing);

						// 抜けたら書き出す。押し忘れて消えるのを防ぐ
						if (!m_roadEditing)
						{
							road->SavePath();

							// 走行中のカメラへ戻す
							if (auto h = m_wpEditCamHolder.lock()) { h->SetEnabled(false); }
						}
					}

					if (m_roadEditing)
					{
						// 一覧から選んで、数値で動かす。
						// 選んでいる点は3Dの側で球と縦線が出る
						// 上から見て線を引く。
						// 3Dで掴むより狙いが合うので、こちらを先に置く
						if (terrain)
						{
							ImGui::SeparatorText(U8("地図で引く"));
							m_roadMap.DrawGui(*road, terrain->Field());
						}

						ImGui::SeparatorText(U8("一覧と数値"));
						m_roadEditor.DrawGui(*road);
						road->DrawEditImGui();
					}

					ImGui::Separator();
					ImGui::Text(U8("道の全長 %.0f m"), road->Spline().TotalLength());
					ImGui::Text(U8("制御点 %d 個"),
					            static_cast<int>(road->Spline().GetPoints().size()));

					// いま道のどこを走っているか。
					// 追走で「道のりで追う」ときの土台になる値
					if (auto c = m_wpCar.lock())
					{
						float rs = 0.0f, roff = 0.0f;
						if (road->Spline().Project(c->GetPos(), rs, roff))
						{
							ImGui::Text(U8("走行地点 %.0f m / 中心から %.2f m"), rs, roff);
						}
					}
				}
			});
		}
		hier.Add("Text Effects", []() { KdShaderManager::Instance().m_postProcessShader.DrawFluidTextImGui(); });
		hier.Add("UI Layers",    []() { HjUiVisibility::Instance().DrawImGui(); });
		// 通信の状態と、ホスト/参加の操作。
		// ルーム画面から繋ぐ作りになるまでは、ここから直接繋いで試す
		// 1フレームの時間がどこで使われているか。
		// FPSは60で頭打ちなので、速くなったかどうかはこちらでしか分からない
		hier.Add("Profiler",     []() { HjProfiler::Instance().DrawImGui(); });

		// コントローラーが反応しないときの切り分け用。
		// 車が持っているものをそのまま覗く(別に作ると、
		// 実際に使われているのと違うものを見ることになる)
		hier.Add("Pad",          [car]() { car->DrawPadImGui(); });

		// 追走のCPUの中身。
		//
		// 目標と実際を並べて出さないと、振れているのが
		// P が強いのか D が足りないのか分からない。
		// 画面を見ていても判断できない
		hier.Add("Chase",        [this]()
		{
			auto cpu = m_wpCpuCar.lock();
			if (!cpu) { ImGui::TextUnformatted("no cpu"); return; }

			const auto& d = cpu->GetDriverDebug();

			ImGui::Text(U8("目標の角度 %6.1f 度"), d.targetDeg);
			ImGui::Text(U8("いまの角度 %6.1f 度"), d.actualDeg);
			ImGui::Text(U8("ズレ       %6.1f 度"), d.targetDeg - d.actualDeg);
			ImGui::Separator();
			ImGui::Text(U8("目標までの距離 %5.2f m"), d.gap);
			ImGui::Text(U8("手本を信じる度 %5.2f"), d.trust);

			if (d.recovering)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), U8("復帰中"));
			}
			if (d.kicking)
			{
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), U8("サイド中"));
			}

			// ズレの推移。1回で収まるか、振れてから収まるかを見る。
			// 振れが大きくなっていくなら P が強すぎる
			static float hist[120] = {};
			static int   head = 0;
			hist[head] = d.targetDeg - d.actualDeg;
			head = (head + 1) % IM_ARRAYSIZE(hist);
			ImGui::PlotLines(U8("ズレの推移"), hist, IM_ARRAYSIZE(hist), head,
			                 nullptr, -60.0f, 60.0f, ImVec2(0.0f, 80.0f));
		});

		// 更新の様子。実際に繋がるかを目で見るため。
		// 遊ぶ人へ出す画面は別に用意する
		hier.Add("Updater",      []()
		{
			auto& up = HjUpdater::Instance();

			ImGui::Text(U8("いまの版 : %s"), HjUpdater::GetCurrentVersion().c_str());

			const std::string latest = up.GetLatestVersion();
			if (!latest.empty()) { ImGui::Text(U8("向こうの版 : %s"), latest.c_str()); }

			ImGui::TextUnformatted(up.StateText());

			const std::string err = up.GetErrorMessage();
			if (!err.empty())
			{
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", err.c_str());
			}

			if (up.GetState() == HjUpdater::State::Downloading)
			{
				ImGui::ProgressBar(up.GetProgress());
				if (ImGui::Button(U8("やめる"))) { up.CancelDownload(); }
				return;
			}

			if (up.IsWorking()) { return; }

			if (ImGui::Button(U8("確認する"))) { up.StartCheck(); }

			if (up.GetState() == HjUpdater::State::Available)
			{
				ImGui::SameLine();
				if (ImGui::Button(U8("受け取る"))) { up.StartDownload(); }
			}

			if (up.GetState() == HjUpdater::State::Ready)
			{
				// ここを押すとゲームが閉じる。押し間違いが痛いので、
				// 何が起きるかを先に書いておく
				ImGui::TextDisabled(U8("押すと閉じて、入れ替えてから開き直します"));
				if (ImGui::Button(U8("入れ替えて再起動"))) { up.Apply(); }
			}
		});
		hier.Add("BGM",          []() { HjBgm::Instance().DrawImGui(); });
		// 観戦。相手の走りを見て、同期が正しいかを目で確かめられる
		hier.Add("Spectate",     [this]() { DrawSpectateImGui(); });
		hier.Add("Network",      []() { HjNetSession::Instance().DrawImGui(); });
		hier.Add("Lobby",        []() { HjSteamLobby::Instance().DrawImGui(); });

		hier.DrawImGui();
	});

	// 追従カメラ
	auto cam = std::make_shared<ChaseCamera>();
	cam->Init();
	cam->SetTarget(car);
	m_wpCamera = cam;   // 観戦で追う相手を差し替えるために持つ
	AddObject(cam);

	// 編集中の自由カメラを渡す物。
	//
	// カメラより後に足すこと。前に足すと、走行中のカメラに
	// 上書きされて効かない
	{
		auto holder = std::make_shared<HjEditCamHolder>();
		m_wpEditCamHolder = holder;
		AddObject(holder);
	}
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
// 道の制御点の編集
//
// 既にあるエディタ基盤へ登録するだけで、ギズモもUndoも乗る。
// 制御点用に別の仕組みを作ると、操作の癖が2つになる
//----------------------------------------------------------
void GameScene::UpdateRoadEdit()
{
	if (!m_roadEditing || !m_spRoad) { return; }

	// 自由カメラ。
	//
	// 走行中のカメラは車を追うので、道を引く間は使えない。
	// 引きたい所へ行けないと、そもそも点を掴めない。
	//
	// 初回だけ作る。毎回作ると、切り替えるたびに視点が戻る
	if (!m_spEditCam)
	{
		m_spEditCam = std::make_shared<HjEditorCamera>();

		// いま見ている所から始める。原点へ飛ぶと、
		// どこを編集していたのか見失う
		if (auto car = m_wpCar.lock())
		{
			m_spEditCam->SetPos(car->GetPos() + Math::Vector3(0.0f, 40.0f, -60.0f));
		}
	}
	m_spEditCam->Update();

	// 描画のときに差し替えてもらう
	if (auto h = m_wpEditCamHolder.lock())
	{
		h->SetCamera(m_spEditCam);
		h->SetRoad(m_spRoad.get(), &m_roadEditor);
		h->SetEnabled(true);
	}

	// 掴む・動かす。
	//
	// 前の作品で実際に動いていたギズモをそのまま持ってきた。
	// 自分で書き直したときに、視線を軸へ落とす式の符号を間違えて
	// 軸の反対側を掴むことになっていた
	if (m_spTerrain)
	{
		m_roadEditor.Update(*m_spRoad, m_spTerrain->Field());
	}

	// 地図に車の位置を出す。どこを走っているか分かる
	if (auto c = m_wpCar.lock()) { m_roadMap.SetCarPos(c->GetPos()); }

	// ※印は描画の番で出す。ここでは積まない。
	//   デバッグ線は KdGameObject が持つ仕組みで出されるので、
	//   物でないと出せない(HjEditCamHolder が受け持つ)

	// ※選ぶ・掴む・足す・消すは、全部エディタが持つ。
	//   ここで分けると、Blender式の一連の操作が2か所に散る

	// 地形は道が削ったあとに組み直す。
	// 毎フレーム組むと重いので、変わったときだけ
	if (m_spTerrain && m_spRoad->ConsumeDirty())
	{
		// 削れたのは道の周りだけ。
		// 全部を組み直すと、数千個ぶんの頂点バッファを
		// 毎フレーム作ることになり、操作が固まる
		Math::Vector3 mn, mx;
		if (m_spRoad->GetDirtyArea(mn, mx)) { m_spTerrain->RebuildInArea(mn, mx); }
		else                                { m_spTerrain->BuildChunks(); }
	}
}

//----------------------------------------------------------
// 通信の橋渡し。
// 自分の状態を渡して、届いた状態をそれぞれの車へ配るだけ。
// 送受信の中身は HjNetSession が持つ。
//----------------------------------------------------------
//----------------------------------------------------------
// 自分の車の見た目を作る。
//
// 通信側で色を決めず、車の調整パネルで設定した色をそのまま配る。
// そうしないと、せっかく詰めた配色が接続した瞬間に上書きされる。
//----------------------------------------------------------
HjCarLook GameScene::BuildMyLook() const
{
	HjCarLook look;
	if (auto car = m_wpCar.lock())
	{
		look.outline = car->GetOutlineColor();
		look.smokeA  = car->GetSmokeColorA();
		look.smokeB  = car->GetSmokeColorB();
		look.accent  = car->GetAccentColor();
		look.neonA   = car->GetNeonColorA();
		look.neonB   = car->GetNeonColorB();
		look.smokeHi = car->GetSmokeHiColor();
		look.smokeGradDist = car->GetSmokeGradDist();
	}
	return look;
}

void GameScene::UpdateNetwork(float dt)
{
	auto& net = HjNetSession::Instance();

	// 自分の車の見た目を毎フレーム預ける。
	//
	// 繋ぐ経路が複数ある(ロビーから自動・調整パネルから直接)ので、
	// 接続のたびに渡す作りだと、どれか一つで渡し忘れて
	// 既定の色が送られる。ここで常に最新にしておけば取りこぼさない。
	// 走行中に色を変えても、次の名簿配布で相手へ届く。
	net.SetMyLook(BuildMyLook());

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
			net.StartHost(HjPlayerProfile::Instance().GetName().c_str());
		}
		else
		{
			char ownerId[32] = {};
			snprintf(ownerId, sizeof(ownerId), "%llu", lobby.GetOwnerId());
			net.StartJoin(ownerId, HjPlayerProfile::Instance().GetName().c_str());
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
			remote->SetLook(net.GetPeerLook(state.id));
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

	// 見ていた相手が抜けたら自分へ戻す。
	// 戻さないと、いなくなった位置をカメラが見続けることになる
	if (m_spectateId == playerId) { SetSpectate(-1); }
}

//----------------------------------------------------------
// 観戦する相手を切り替える。-1 は自分。
//
// カメラの追う相手を差し替えるだけで、自分の車は走り続ける。
// 止めたい場合はポーズを使う(そちらは別の役目)。
//----------------------------------------------------------
void GameScene::SetSpectate(int playerId)
{
	auto cam = m_wpCamera.lock();
	if (!cam) { return; }

	// 前に見ていた相手の印を消す
	if (m_spectateId >= 0 && m_spectateId < NetConst::MaxPlayers)
	{
		if (auto prev = m_wpRemoteCars[m_spectateId].lock()) { prev->SetSpectated(false); }
	}

	if (playerId < 0)
	{
		auto car = m_wpCar.lock();
		if (!car) { return; }

		cam->SetTarget(car);
		// 自分へ戻ったら走行を再開する
		car->SetHalted(false);
		m_spectateId = -1;
		return;
	}

	if (playerId >= NetConst::MaxPlayers) { return; }

	// まだ来ていない相手は選べない
	auto remote = m_wpRemoteCars[playerId].lock();
	if (!remote) { return; }

	cam->SetTarget(remote);
	remote->SetSpectated(true);

	// 観戦中は自分の車を止める。
	// 動いたままだと、見ていない間に崖から落ちたり壁に刺さったりする
	if (auto car = m_wpCar.lock()) { car->SetHalted(true); }

	m_spectateId = playerId;
}

//----------------------------------------------------------
// 観戦の切り替えパネル。
//----------------------------------------------------------
void GameScene::DrawSpectateImGui()
{
	if (!ImGui::CollapsingHeader(U8("観戦"))) { return; }

	auto& net = HjNetSession::Instance();

	ImGui::TextWrapped(
		U8("カメラの追う相手を切り替えます。自分の車は走り続けるので、"
		   "止めたい場合はポーズを使ってください。"));
	ImGui::Separator();

	// 自分
	{
		const bool on = (m_spectateId < 0);
		ImGui::BeginDisabled(on);
		if (ImGui::Button(U8("自分"))) { SetSpectate(-1); }
		ImGui::EndDisabled();

		ImGui::SameLine();
		if (on) { ImGui::TextColored(ImVec4(0.4f, 1, 0.5f, 1), U8("← 見ています")); }
		else    { ImGui::TextDisabled(U8("%s"), HjPlayerProfile::Instance().GetName().c_str()); }
	}

	// 相手
	int shown = 0;
	for (int id = 0; id < NetConst::MaxPlayers; ++id)
	{
		auto remote = m_wpRemoteCars[id].lock();
		if (!remote) { continue; }
		++shown;

		ImGui::PushID(id);

		const bool on = (m_spectateId == id);
		ImGui::BeginDisabled(on);
		if (ImGui::Button(U8("見る"))) { SetSpectate(id); }
		ImGui::EndDisabled();

		ImGui::SameLine();

		// 名簿が届く前は名前が空なので、番号で出す
		const char* name = net.GetPeerName(id);
		if (!name || !name[0]) { name = U8("(名前待ち)"); }

		if (on) { ImGui::TextColored(ImVec4(0.4f, 1, 0.5f, 1), U8("[%d] %s  ← 見ています"), id, name); }
		else    { ImGui::Text(U8("[%d] %s"), id, name); }

		ImGui::PopID();
	}

	if (shown == 0)
	{
		ImGui::TextDisabled(U8("(他のプレイヤーがいません)"));
	}
}
