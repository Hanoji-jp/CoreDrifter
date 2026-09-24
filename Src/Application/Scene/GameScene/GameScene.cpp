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
#include "../../GameObject/UI/HjCheats.h"
#include "../../GameObject/UI/HjEsp.h"
#include "../../GameObject/Car/HjNoClip.h"
#include "../../GameObject/Car/HjCarChoice.h"
#include "../../GameObject/Car/CpuCar.h"
#include "../../GameObject/Stage/HjTerrain.h"
#include "../../GameObject/Stage/HjRoad.h"
#include "../../GameObject/Stage/HjGuardRail.h"
#include "../../GameObject/Stage/HjFoliage.h"
#include "../../GameObject/Stage/HjProps.h"
#include "../../GameObject/Stage/HjRetainWall.h"
#include "../../GameObject/Stage/HjRoadMark.h"
#include "../../GameObject/Stage/HjDelineator.h"
#include "../../GameObject/Stage/HjRoadSign.h"
#include "../../GameObject/Stage/HjCurveMirror.h"
#include "../../GameObject/Stage/HjSpawnPoint.h"
#include "../../GameObject/Stage/HjStageChoice.h"
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
	// 走りながら使う小細工。
	// 車から離れて見回る、相手の位置を透かす
	UpdateCheats();

	// ステージが替わったら、場面ごと入り直す。
	//
	// 地形も道も車の置き場所も全部変わるので、
	// この場面のまま差し替えるより作り直すほうが確実
	if (auto menu = m_wpModMenu.lock())
	{
		if (menu->ConsumeStageChanged())
		{
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
			return;
		}
	}

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

	// どのステージを走るか。
	//
	// もとは定数で分けていたので、組み直さないと切り替えられなかった。
	// 見比べができないと、どちらを詰めるかも決められない
	HjStageChoice::Instance().Load();

	if (HjStageChoice::Instance().UsesTerrain())
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

		// 裾に覆われた所は地形を張らない。
		// 重ねて張ると、同じ高さで深度が争ってちらつく
		terrain->SetRoad(road);

		// 地形のメッシュは、道が地形を寄せたあとに組む。
		// 先に組むと、寄せる前の形で頂点と法線を作ってしまう
		terrain->BuildChunks();

		// ガードレール。地形の落ち方から自動で置く。
		// 道と地形が決まってからでないと、置き場所が出せない
		auto rail = std::make_shared<HjGuardRail>();
		rail->Build(*road, &terrain->Field());
		AddObject(rail);

		// 路面標示(白線)。
		// 一様な灰色の帯では、カーブでどこに車を置いているのかが読めない
		auto mark = std::make_shared<HjRoadMark>();
		mark->Build(*road);
		AddObject(mark);

		// カーブミラー。見通しの効かないカーブの外側に立てる
		auto mirror = std::make_shared<HjCurveMirror>();
		mirror->Build(*road);
		AddObject(mirror);

		// カーブ注意の標識。入口の手前へ下げて立てる
		auto sign = std::make_shared<HjRoadSign>();
		sign->Build(*road);
		AddObject(sign);

		// 視線誘導標。カーブの外側に等間隔で立てる。
		// 路面が見えていない段階で、この先の曲がりが読める
		auto delin = std::make_shared<HjDelineator>();
		delin->Build(*road);
		AddObject(delin);

		// 山側の擁壁。制御点ごとに持たせた高さから立てる。
		// 地形からの自動当てはめは編集パネルのボタンで行う
		auto wall = std::make_shared<HjRetainWall>();
		wall->Build(*road);
		AddObject(wall);

		// 手で置く飾り(木・低木)。置いたものを props.txt から読む
		auto props = std::make_shared<HjProps>();
		props->Init();
		props->Refresh(terrain->Field());
		AddObject(props);

		// 木と草。斜面の急な所と道の上には生やさない。
		// 地形と道が決まってからでないと、どちらも判定できない
		auto foliage = std::make_shared<HjFoliage>();
		foliage->Init();
		foliage->BuildGrass(terrain->Field(), road.get());
		AddObject(foliage);

		// 編集で触り続けるので持っておく
		m_spTerrain = terrain;
		m_spRoad    = road;
		m_spRail    = rail;
		m_spWall    = wall;
		m_spMark    = mark;
		m_spDelin   = delin;
		m_spSign    = sign;
		m_spMirror  = mirror;
		m_spFoliage = foliage;
		m_spProps   = props;
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

	// 剛体側にも地面を渡す。
	// 旧モデルとは別に持っているので、両方へ渡す必要がある
	car->SetRigidGround(terrain ? &terrain->Field() : nullptr, road.get());
	//===== スポーン =====
	// 自分で決めた場所があれば、何より先にそれを使う。
	//
	// 道の始点を無条件で優先していたので、決めても上書きされて
	// 変えられなかった。道は制御点を動かすたびに始点も動くので、
	// 自分で決めた場所があれば、何より先にそれを使う。
	//
	// 道の始点を無条件で優先していたので、決めても上書きされて
	// 変えられなかった。道は制御点を動かすたびに始点も動くので、
	// 何も決めていない間だけ道に任せる
	HjSpawnPoint::Instance().Load();

	if (HjSpawnPoint::Instance().Has())
	{
		car->SetSpawn(HjSpawnPoint::Instance().Pos(),
		              HjSpawnPoint::Instance().Yaw());
	}
	else if (road)    { car->SetSpawn(road->GetStartPos(), road->GetStartYaw()); }
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

	// 相手の位置を透かす。
	// デバッグ線は物からしか出せないので、物として置く
	auto esp = std::make_shared<HjEsp>();
	AddObject(esp);
	m_wpEsp = esp;
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

		hier.Add(car->GetTuningName(), [car]()
		{
			car->DrawImGui();

			//===== 走り出す場所 =====
			// 以前は Stage が持っていたが、地形のステージでは Stage を
			// 作らないので、ボタンが全部素通りして「設定しても変わらない」
			// 状態になっていた。ステージの作り方に関係なく在るものへ移した
			auto& sp = HjSpawnPoint::Instance();

			// 保存は「まとめて書き出す」に任せる。
			// ここだけ即保存だと、他と作法が違って覚えられない

			ImGui::Separator();

			if (ImGui::Button(U8("現在の車位置をスポーンに設定")))
			{
				sp.Set(car->GetPos(), car->GetYaw());
			}

			ImGui::SameLine();
			if (ImGui::Button(U8("スポーンへ移動(R)")))
			{
				if (sp.Has()) { car->SetSpawn(sp.Pos(), sp.Yaw()); }
			}

			// いまどちらが使われているかを出す。
			// 決めたのに道の始点から出る、を見えるようにしておく
			if (!sp.Has())
			{
				ImGui::TextUnformatted(U8("スポーン: 道の始点"));
				return;
			}

			ImGui::TextUnformatted(U8("スポーン: 自分で決めた場所"));
			ImGui::SameLine();
			if (ImGui::Button(U8("道の始点に任せる")))
			{
				sp.Clear();
			}

			// 数値でも動かせるようにする。
			// 車を置きに行くほどでもない微調整のため
			Math::Vector3 pos = sp.Pos();
			float yaw = sp.Yaw();

			bool edited = ImGui::DragFloat3(U8("スポーン位置"), &pos.x, 0.1f);
			edited |= ImGui::DragFloat(U8("スポーン向き(rad)"), &yaw, 0.01f);

			if (edited)
			{
				sp.Set(pos, yaw);
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
							SaveAllEdits();

							// 走行中のカメラへ戻す
							if (auto h = m_wpEditCamHolder.lock()) { h->SetEnabled(false); }
						}
					}

					if (m_roadEditing)
					{
						//===== まとめて書き出す =====
						// 触るものが増えて、書き出しボタンがあちこちに散っていた。
						// どれを押したか覚えていられないので、1つにまとめる。
						//
						// 保存先は別々のファイルのままだが、押す側から見れば
						// 「いまの状態を残す」の1つで足りる
						if (ImGui::Button(U8("まとめて書き出す"), ImVec2(-1.0f, 0.0f)))
						{
							SaveAllEdits();
						}

						ImGui::TextDisabled(U8("道 / 地形 / 飾り / スポーン"));
						ImGui::Separator();

						if (ImGui::CollapsingHeader(U8("地形を彫る"), ImGuiTreeNodeFlags_DefaultOpen))
						{
							// 地形を筆で彫る。
							// 道より先に置く。地面が無いと道の載る場所が決まらない
							if (terrain)
							{
								m_terrainBrush.DrawGui(terrain->WorkField());
							}
						}

						if (ImGui::CollapsingHeader(U8("道を引く"), ImGuiTreeNodeFlags_DefaultOpen))
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

							//===== 選んでいる点の裾の幅 =====
							// 区間ごとに裾を伸ばしたいので、制御点に持たせている。
							//
							// 左右で別に持つ。谷側だけ伸ばして山側は詰める、
							// という使い方をするので、1つの値だと片側に合わせるしかない
							{
								const int sel = (m_roadEditor.GetSelected() >= 0)
								? m_roadEditor.GetSelected()
								: m_roadMap.GetSelected();

								if (sel >= 0 && sel < road->PointCount())
								{
									ImGui::TextDisabled(U8("点 %d の裾と平場(進む向きに対して)"), sel);

									float wl = road->GetApronAt(sel, 0);
									if (ImGui::DragFloat(U8("左の裾(m)"), &wl, 0.1f,
									0.0f, RoadConst::ApronWidthMax))
									{
										road->SetApronAt(sel, 0, wl);
									}

								float wr = road->GetApronAt(sel, 1);
								if (ImGui::DragFloat(U8("右の裾(m)"), &wr, 0.1f,
								0.0f, RoadConst::ApronWidthMax))
								{
									road->SetApronAt(sel, 1, wr);
								}

							float fl = road->GetFlatAt(sel, 0);
							if (ImGui::DragFloat(U8("左の平場(m)"), &fl, 0.05f,
							0.0f, RoadConst::ApronFlatMax))
							{
								road->SetFlatAt(sel, 0, fl);
							}

							float fr = road->GetFlatAt(sel, 1);
							if (ImGui::DragFloat(U8("右の平場(m)"), &fr, 0.05f,
							0.0f, RoadConst::ApronFlatMax))
							{
								road->SetFlatAt(sel, 1, fr);
							}

							//===== ガードレール =====
							// 有る無しだけ。柵の高さは規格で決まっている
							bool rl = road->GetRailAt(sel, 0) > 0.5f;
							bool rr = road->GetRailAt(sel, 1) > 0.5f;

							bool railEdited = ImGui::Checkbox(U8("左の柵"), &rl);
							ImGui::SameLine();
							railEdited |= ImGui::Checkbox(U8("右の柵"), &rr);

							if (railEdited)
							{
								road->SetRailAt(sel, 0, rl ? 1.0f : 0.0f);
								road->SetRailAt(sel, 1, rr ? 1.0f : 0.0f);
								if (m_spRail && terrain)
								{
									m_spRail->Build(*road, &terrain->Field());
								}
							}

							//===== 擁壁 =====
							// 0 で壁なし。制御点の間はなめらかに繋がるので、
							// 端の点を 0 にすれば壁がそこで消えていく
							float ml = road->GetWallAt(sel, 0);
							if (ImGui::DragFloat(U8("左の擁壁(m)"), &ml, 0.05f,
							0.0f, RetainWallConst::MaxHeight))
							{
								road->SetWallAt(sel, 0, ml);
								if (m_spWall) { m_spWall->Build(*road); }
							}

							float mr = road->GetWallAt(sel, 1);
							if (ImGui::DragFloat(U8("右の擁壁(m)"), &mr, 0.05f,
							0.0f, RetainWallConst::MaxHeight))
							{
								road->SetWallAt(sel, 1, mr);
								if (m_spWall) { m_spWall->Build(*road); }
							}

							// 片側だけ触ると左右がちぐはぐになりやすい。
							// 揃えたいときのために一手で戻せるようにする
							if (ImGui::Button(U8("左右を揃える")))
							{
								road->SetApronAt(sel, 1, wl);
								road->SetFlatAt(sel, 1, fl);
							}
							ImGui::SetItemTooltip(U8("右を左に合わせる"));

							ImGui::SetItemTooltip(U8(
							"制御点の間はなめらかに繋がる。"
							"向かいの裾と重なる所は、互いの真ん中で止まる"));
							}
							else
							{
								ImGui::TextDisabled(U8("点を選ぶと、その区間の裾の幅を変えられる"));
							}
							}

							road->DrawEditImGui();
						}

						if (ImGui::CollapsingHeader(U8("ガードレール")))
						{
							//===== ガードレール =====
							// 置く場所は制御点ごとに決める。
							// 落差だけで自動に出すと、要らない所に立って欲しい所に立たない
							if (m_spRail && terrain)
							{
								bool vis = m_spRail->IsVisible();
								if (ImGui::Checkbox(U8("出す##rail"), &vis)) { m_spRail->SetVisible(vis); }

								ImGui::SameLine();
								if (ImGui::Button(U8("地形から入れる##rail")))
								{
									HjGuardRail::AutoFill(*road, &terrain->Field());
									m_spRail->Build(*road, &terrain->Field());
								}

							ImGui::SameLine();
							if (ImGui::Button(U8("全部消す##rail")))
							{
								for (int i = 0; i < road->PointCount(); ++i)
								{
									road->SetRailAt(i, 0, 0.0f);
									road->SetRailAt(i, 1, 0.0f);
								}
							m_spRail->Build(*road, &terrain->Field());
							}

							ImGui::Text(U8("全長 %.0f m / 支柱 %d 本"),
							m_spRail->GetLength(), m_spRail->GetPostCount());
							}
						}

						if (ImGui::CollapsingHeader(U8("擁壁")))
						{
							//===== 擁壁 =====
							// 立てる場所は制御点ごとの高さが決める。
							// 地形からの自動は、手で直す下敷きとして用意する
							if (m_spWall)
							{
								bool wv = m_spWall->IsVisible();
								if (ImGui::Checkbox(U8("出す##wall"), &wv)) { m_spWall->SetVisible(wv); }

								ImGui::SameLine();
								if (ImGui::Button(U8("地形から入れる")) && terrain)
								{
									HjRetainWall::AutoFill(*road, &terrain->Field());
									m_spWall->Build(*road);
								}

							ImGui::SameLine();
							if (ImGui::Button(U8("全部消す##wall")))
							{
								for (int i = 0; i < road->PointCount(); ++i)
								{
									road->SetWallAt(i, 0, 0.0f);
									road->SetWallAt(i, 1, 0.0f);
								}
							m_spWall->Build(*road);
							}

							ImGui::Text(U8("全長 %.0f m"), m_spWall->GetLength());
							}
						}

						if (ImGui::CollapsingHeader(U8("飾り(木・低木)")))
						{
							//===== 飾り(木・低木) =====
							// 置く前に、そのモデルを実際の大きさで地面に出す。
							// 「置いてから直す」の往復が無くなるのが一番効く
							if (m_spProps && terrain)
							{
								m_propEditor.DrawGui(*m_spProps);

								if (ImGui::Button(U8("読み直す##props")))
								{
									m_spProps->Load();
									m_spProps->Refresh(terrain->Field());
								}

							ImGui::SameLine();
							if (ImGui::Button(U8("全部消す##props"))) { m_spProps->Clear(); }
							}
						}

						if (ImGui::CollapsingHeader(U8("草")))
						{
							//===== 草 =====
							// こちらは撒く。1株ずつ置く意味がない
							if (m_spFoliage && terrain)
							{
								ImGui::Text(U8("%d 株"), m_spFoliage->GetGrassCount());

								// 撒き直すのに時間が掛かるので、押した時だけ
								if (ImGui::Button(U8("撒き直す")))
								{
									m_spFoliage->BuildGrass(terrain->Field(), road.get());
								}
							}
						}

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
// 走りながら使う小細工
//
// ■ 自由に飛ぶ
// 道の編集で使っている自由カメラをそのまま借りる。
// 別に作ると、操作の癖が2つになる。
//
// ■ 相手を透かす
// 出す相手を毎フレーム入れ直す。
// HjEsp が相手一覧の持ち方を知ると、通信の作りに縛られる
//----------------------------------------------------------
void GameScene::UpdateCheats()
{
	auto& ch = HjCheats::Instance();

	//===== 車ごと飛ばす =====
	// カメラだけ動かすと、車は元の場所に残る。
	// 見に行った先で走り出せないし、多人数のときは
	// 相手からは動いていないように見える。
	//
	// 道の編集中は触らない。あちらは車を置いたまま見て回るもの
	if (auto car = m_wpCar.lock())
	{
		m_noClip.Update(*car, ch.IsFreeFly() && !m_roadEditing,
		                KdFPSController::GetDt());
	}

		//===== 相手を透かす =====
	if (auto esp = m_wpEsp.lock())
	{
		esp->Clear();

		if (ch.IsEsp())
		{
			for (const auto& wp : m_wpRemoteCars)
			{
				if (!wp.expired()) { esp->Add(wp); }
			}

			// 追走のCPUも出す。相手には違いない
			if (!m_wpCpuCar.expired()) { esp->Add(m_wpCpuCar); }

			if (auto car = m_wpCar.lock()) { esp->SetEye(car->GetPos()); }
		}
	}
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
		h->SetBrush(&m_terrainBrush);
		h->SetEnabled(true);
	}

	// 掴む・動かす。
	//
	// 前の作品で実際に動いていたギズモをそのまま持ってきた。
	// 自分で書き直したときに、視線を軸へ落とす式の符号を間違えて
	// 軸の反対側を掴むことになっていた
	if (m_spTerrain)
	{
		// 筆が動いている間は、点を掴む処理を止める。
		// 両方が同じ左ボタンを見ているので、混ざると地面を彫りながら
		// 制御点を動かすことになる
		// 木を置いている間も、点を掴む処理を止める。
		// どれも同じ左ボタンを見ているので、混ざると
		// 木を置きながら制御点が動く
		if (m_propEditor.IsEnabled() && m_spProps)
		{
			m_propEditor.Update(*m_spProps, m_spTerrain->Field(),
			                    m_spRoad.get(), m_terrainBrush);
		}
		else if (m_terrainBrush.IsEnabled())
		{
			m_terrainBrush.Update(m_spTerrain->WorkField(), *m_spTerrain, m_spRoad.get());

			// 地形を彫ったら道も作り直す。
			//
			// 裾は地形に沿って溶けるので、彫る前の形のままだと
			// 浮くか埋まる。柵の置き場所も地形の落ち方で決まる
			if (m_terrainBrush.ConsumeTerrainChanged())
			{
				m_spRoad->Rebuild();
			}
		}
		else
		{
			m_roadEditor.Update(*m_spRoad, m_spTerrain->Field());
		}
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

		// 柵も置き直す。道が動けば、谷の位置も縁の高さも変わる
		if (m_spRail) { m_spRail->Build(*m_spRoad, &m_spTerrain->Field()); }

		// 擁壁も組み直す。道が動けば断面の向きも路面の高さも変わる
		if (m_spWall) { m_spWall->Build(*m_spRoad); }

		// 白線も引き直す。路面の高さも幅も変わっている
		if (m_spMark) { m_spMark->Build(*m_spRoad); }

		// 視線誘導標も立て直す。曲がりの位置が変わっている
		if (m_spDelin) { m_spDelin->Build(*m_spRoad); }

		// 標識も立て直す。カーブの入口が動いている
		if (m_spSign) { m_spSign->Build(*m_spRoad); }

		// ミラーも立て直す。カーブの頂点が動いている
		if (m_spMirror) { m_spMirror->Build(*m_spRoad); }

		// 飾りは高さだけ取り直す。置いた場所は動かさない
		if (m_spProps) { m_spProps->Refresh(m_spTerrain->Field()); }

		// 草も撒き直す。
		// 道が動いた所は法面になっているので、そのままだと
		// 削った斜面に草が刺さったまま残る
		if (m_spFoliage) { m_spFoliage->BuildGrass(m_spTerrain->Field(), m_spRoad.get()); }
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

	// 相手が選んだ車種で作る。
	// 決め打ちにすると、相手がNSXでもシルビアで出る
	const int kindNo = HjNetSession::Instance().GetPeerCarKind(playerId);

	auto car = std::make_shared<RemoteCar>(
		playerId, static_cast<CarChoiceConst::Kind>(kindNo));
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


//----------------------------------------------------------
// 編集したものを一度に書き出す
//
// ■ なぜまとめるか
// 触るものが増えて、書き出しボタンがあちこちに散っていた。
// どれを押したか覚えていられないので、押し忘れて消える。
//
// 保存先は別々のファイルのままでよい。道は road_path.txt、
// 地形は height_edit.r32、飾りは props.txt。
// まとめるのは操作であって、ファイルではない
//----------------------------------------------------------
void GameScene::SaveAllEdits()
{
	// 道の制御点。裾・平場・擁壁・柵も同じ行に入っている
	if (m_spRoad) { m_spRoad->SavePath(); }

	// 彫った地形。書き出すのは素地で、道の削りは焼き込まない
	if (m_spTerrain) { m_terrainBrush.Save(m_spTerrain->Field()); }

	// 手で置いた飾り
	if (m_spProps) { m_spProps->Save(); }

	// 走り出す場所
	HjSpawnPoint::Instance().Save();
}
