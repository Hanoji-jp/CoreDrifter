#include "main.h"
#include "Util/HjProfiler.h"
#include "Util/AssetVault.h"
#include "Util/HjSaveDirs.h"
#include "Util/HjSaveFile.h"
#include "Const/WindowConst.h"
#include "Updater/HjUpdater.h"
#include "Audio/HjAudioSettings.h"
#include "Audio/HjAudioSpace.h"
#include "Util/HjPostFxSettings.h"
#include "Input/HjKeyInput.h"
#include "GameObject/Score/HjPlayerProfile.h"

#include "Scene/SceneManager.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エントリーポイント
// アプリケーションはこの関数から進行する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_  HINSTANCE, _In_ LPSTR , _In_ int)
{
	// メモリリークを知らせる
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// COM初期化
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
	{
		CoUninitialize();

		return 0;
	}

	// mbstowcs_s関数で日本語対応にするために呼ぶ
	setlocale(LC_ALL, "japanese");

	//===================================================================
	// 実行]
	//===================================================================
	Application::Instance().Execute();

	// COM解放
	CoUninitialize();

	return 0;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdBeginUpdate()
{
	// 入力状況の更新
	KdInputManager::Instance().Update();

	// キーボード入力の更新。
	// ここで1回だけ状態を取り、以降は全クラスがこれを参照する。
	// 各クラスが個別に前フレーム状態を持つと、画面を跨いだときに
	// 押しっぱなしが新規入力として拾われてしまう。
	HjKeyInput::Instance().Update();

	// 空間環境の更新
	KdShaderManager::Instance().WorkAmbientController().Update();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostUpdate()
{
	// 1フレームぶんの計測を締めて、次のフレームを始める
	HjProfiler::Instance().BeginFrame();

	// 調整パネルで画面演出を触っていたら書き出す。
	// スライダーを掴んでいる間は待って、手を離してから1回だけ保存する
	HjPostFxSettings::Instance().Update();

	// この1フレームに届いた文字を捨てる。
	// シーンの更新が終わってから捨てるので、UIは確実に読める。
	HjKeyInput::Instance().EndFrame();

	// 3DSoundListnerの行列を更新
	KdAudioManager::Instance().SetListnerMatrix(KdShaderManager::Instance().GetCameraCB().mView.Invert());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新の前処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PreUpdate()
{
	SceneManager::Instance().PreUpdate();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Update()
{
	SceneManager::Instance().Update();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新の後処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PostUpdate()
{
	SceneManager::Instance().PostUpdate();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdBeginDraw(bool usePostProcess)
{
	KdDirect3D::Instance().ClearBackBuffer();

	KdShaderManager::Instance().WorkAmbientController().Draw();

	if (!usePostProcess) return;
	KdShaderManager::Instance().m_postProcessShader.Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostDraw()
{
	// Imguiのレンダリング
	KdDebugGUI::Instance().GuiProcess();

	// BackBuffer -> 画面表示
	KdDirect3D::Instance().WorkSwapChain()->Present(0, 0);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画の前処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PreDraw()
{
	SceneManager::Instance().PreDraw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Draw()
{
	SceneManager::Instance().Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画の後処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PostDraw()
{
	// 画面のぼかしや被写界深度処理の実施
	KdShaderManager::Instance().m_postProcessShader.PostEffectProcess();

	// 現在のシーンのデバッグ描画
	SceneManager::Instance().DrawDebug();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 2Dスプライトの描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::DrawSprite()
{
	SceneManager::Instance().DrawSprite();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション初期設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool Application::Init(int w, int h)
{
	//===================================================================
	// ウィンドウ作成
	//===================================================================
	if (m_window.Create(w, h, "DRIFT PROJECT", "Window") == false) {
		MessageBoxA(nullptr, "ウィンドウ作成に失敗", "エラー", MB_OK);
		return false;
	}

	//===================================================================
	// フルスクリーン確認
	//===================================================================
	bool bFullScreen = false;
//	if (MessageBoxA(m_window.GetWndHandle(), "フルスクリーンにしますか？", "確認", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
//		bFullScreen = true;
//	}

	//===================================================================
	// Direct3D初期化
	//===================================================================

	// デバイスのデバッグモードを有効にする
	bool deviceDebugMode = false;
#ifdef _DEBUG
	deviceDebugMode = true;
#endif

	// Direct3D初期化
	std::string errorMsg;
	if (KdDirect3D::Instance().Init(m_window.GetWndHandle(), w, h, deviceDebugMode, errorMsg) == false) {
		MessageBoxA(m_window.GetWndHandle(), errorMsg.c_str(), "Direct3D初期化失敗", MB_OK | MB_ICONSTOP);
		return false;
	}

	// フルスクリーン設定
	if (bFullScreen) {
		HRESULT hr;

		hr = KdDirect3D::Instance().SetFullscreenState(TRUE, 0);
		if (FAILED(hr))
		{
			MessageBoxA(m_window.GetWndHandle(), "フルスクリーン設定失敗", "Direct3D初期化失敗", MB_OK | MB_ICONSTOP);
			return false;
		}
	}

	//===================================================================
	// imgui初期化
	//===================================================================
	KdDebugGUI::Instance().GuiInit(w, h);

	//===================================================================
	// シェーダー初期化
	//===================================================================
	// 埋め込んだアセットを開く。
	//
	// 配布ビルドでは Asset/ がフォルダとして無い。
	// 何かを読み込むより先に開いておかないと、
	// 最初のテクスチャで既に見つからないことになる
	AssetVault::Init();

	// 書き込む先を用意する。
	//
	// 保存物の置き場が Asset/Data/ の下にある。
	// 配布ビルドは Asset/ を exe へ埋め込むので実体が無く、
	// ofstream が黙って失敗する
	HjSaveDirs::Ensure();

	// 遊ぶ側の保存物を読む。
	//
	// 設定も成績も車のセッティングも、save.dat 1つにまとめてある。
	// Asset/ の下へ散らばらせると、配布ビルドで exe の隣に
	// Asset/Data/ が生えて、消していいのか分からなくなる
	HjSaveFile::Load();

	KdShaderManager::Instance().Init();

	// モーションブラーはOFF（車ゲームでは追従カメラで常時ブレるため）
	KdShaderManager::Instance().m_postProcessShader.SetMotionBlurEnabled(false);

	//===================================================================
	// オーディオ初期化
	//===================================================================
	KdAudioManager::Instance().Init();

	//===================================================================
	// フォント初期化
	//===================================================================
	KdFontManager::Instance().Init(GetWindowHandle());

	// 更新の確認を始める。
	//
	// 別スレッドでやるので、ここで待たされることはない。
	// 繋がらなくても失敗として残るだけで、ゲームは普通に始まる。
	//
	// ※起動のたびに1回だけ。画面ごとに呼ぶと、
	//   画面を行き来するたびに GitHub へ問い合わせることになる
	HjUpdater::Instance().StartCheck();
	// HUD用フォント(No.0)を登録。DrawFont(Pos,color,fmt,...)はNo.0を使う。
	KdFontManager::Instance().AddFont(0, "Consolas", 22);
	// 文字流体化(ドリフト演出)用の大フォント(No.1)。太字で垂れ・煙が映える。
	KdFontManager::Instance().AddFont(1, "Arial Black", 130);

	// Drift Project UI：デザイン忠実再現のため Archivo の静的ウェイトを読み込む。
	//   GDIは可変フォントのウェイト軸を選べないので、ウェイトごとの静的TTFを使う。
	//   ※可変版 Archivo.ttf は "Archivo" 名が Regular/Bold と衝突するので読み込まない。
	KdFontManager::Instance().AddFontResource("Asset/Fonts/Archivo-Black.ttf");      // 900 "Archivo Black"
	KdFontManager::Instance().AddFontResource("Asset/Fonts/Archivo-ExtraBold.ttf");  // 800 "Archivo ExtraBold"
	KdFontManager::Instance().AddFontResource("Asset/Fonts/Archivo-SemiBold.ttf");   // 600 "Archivo SemiBold"
	KdFontManager::Instance().AddFontResource("Asset/Fonts/Archivo-Bold.ttf");       // 700 "Archivo"(Bold)
	KdFontManager::Instance().AddFontResource("Asset/Fonts/Archivo-Regular.ttf");    // 400 "Archivo"
	// デザインの各サイズ・ウェイト(1536→1280 の 0.833 倍で換算)。
	//   ※欧文フォントは DEFAULT_CHARSET(1) を指定して別書体への置換を防ぐ(重要)。
	//   ※高さは負値=em高(CSSのfont-size相当)。正値だとセル高扱いで約15%小さくなるため。
	constexpr int LAT = 1;   // DEFAULT_CHARSET
	KdFontManager::Instance().AddFont(2,  "Archivo Black",     -125, 900, LAT);  // DRIFT   (150/900)
	KdFontManager::Instance().AddFont(3,  "Archivo Black",     -88,  900, LAT);  // PROJECT (106/900)
	KdFontManager::Instance().AddFont(4,  "Archivo ExtraBold", -18,  800, LAT);  // メニュー (21/800)
	KdFontManager::Instance().AddFont(5,  "Archivo ExtraBold", -12,  800, LAT);  // タグライン/バッジ (14/800)
	KdFontManager::Instance().AddFont(6,  "Archivo ExtraBold", -11,  800, LAT);  // トップ帯スローガン (13/800)
	KdFontManager::Instance().AddFont(7,  "Archivo",           -11,  700, LAT);  // フッター/小見出し (13/700)
	KdFontManager::Instance().AddFont(8,  "Archivo ExtraBold", -13,  800, LAT);  // NOW PLAYING曲名 (15/800)
	KdFontManager::Instance().AddFont(9,  "Archivo SemiBold",  -10,  600, LAT);  // NOW PLAYINGアーティスト (11/600)
	KdFontManager::Instance().AddFont(10, "Archivo Black",     -17,  900, LAT);  // "///" (20/900)
	KdFontManager::Instance().AddFont(11, "Archivo",           -10,  700, LAT);  // 極小ラベル (12/700)
	KdFontManager::Instance().AddFont(12, "Archivo ExtraBold", -15,  800, LAT);  // ボタン/トグル (15/800)
	KdFontManager::Instance().AddFont(13, "Archivo",           -14,  700, LAT);  // 設定行/バー (16/700)
	KdFontManager::Instance().AddFont(14, "Archivo ExtraBold", -17,  800, LAT);  // タブ/見出し (20/800)
	KdFontManager::Instance().AddFont(15, "Archivo Black",     -47,  900, LAT);  // 画面見出し (56/900)
	KdFontManager::Instance().AddFont(16, "Archivo Black",     -30,  900, LAT);  // カード見出し (中サイズ)

	// 名前入力など、日本語・中国語が入りうる場所で使う書体。
	// Archivo は欧文専用でCJKのグリフを持たないため、別に用意する。
	// Yu Gothic UI は日本語と中国語(簡体字)の多くを含む。
	//
	// ※実際に出したい大きさで登録すること。
	//   描画側の pxHeight は縦位置を決めるだけで、字の大きさは変えない。
	//   1つのサイズを拡大して使い回すと、そのぶん粗くなる。
	KdFontManager::Instance().AddFont(17, "Yu Gothic UI", -47, 700);   // 名前入力(大)
	KdFontManager::Instance().AddFont(18, "Yu Gothic UI", -12, 700);   // バッジ(小)
	// MODメニュー用。走行中に開くパネルなので、
	// 名前入力(47px)は大きすぎ、バッジ(12px)は小さすぎる。
	// 欧文の行・見出しと同じ見え方になる大きさで登録する
	KdFontManager::Instance().AddFont(19, "Yu Gothic UI", -15, 700);   // 行
	KdFontManager::Instance().AddFont(20, "Yu Gothic UI", -17, 700);   // 見出し

	//===================================================================
	// ゲーム固有の初期化
	//===================================================================
	// 例えばカーソルを消したい場合
	//ShowCursor(false);

	// これまでの走行記録(名前・累計スコア・走行回数)を読み込む。
	// タイトルのバッジと銘板がこれを見て表示を変える。
	HjPlayerProfile::Instance().Load();

	// 画面演出(アウトライン・ハーフトーン)の調整値。
	// シェーダー側が持っている値なので、車やステージとは別に読み込む
	HjPostFxSettings::Instance().Load();

	// 音量の設定。鳴らす側が見に来る形なので、読み込んでおけば効く。
	// 効果音だけはバスに掛ける必要があるので、ここで一度渡す
	HjAudioSettings::Instance().Load();
	HjAudioSpace::Instance().SetSfxVolume(HjAudioSettings::Instance().GetSfx());
	HjAudioSpace::Instance().SetAmbientVolume(HjAudioSettings::Instance().GetAmbient());

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション実行
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Execute()
{
	// 窓の大きさ。
	//
	// 読めなかったときは既定で開く。
	// 行が無いのに sizeData[0] を触ると、起動した瞬間に落ちる。
	// 配布ビルドで1つ入れ忘れただけで、何も出ないまま終わることになる
	int winW = WindowConst::DefaultW;
	int winH = WindowConst::DefaultH;
	{
		KdCSVData windowData("Asset/Data/WindowSettings.csv");
		const std::vector<std::string>& sizeData = windowData.GetLine(0);

		if (sizeData.size() >= 2)
		{
			const int w = atoi(sizeData[0].c_str());
			const int h = atoi(sizeData[1].c_str());

			if (w > 0 && h > 0) { winW = w; winH = h; }
		}
	}

	//===================================================================
	// 初期設定(ウィンドウ作成、Direct3D初期化など)
	//===================================================================
	if (Application::Instance().Init(winW, winH) == false) {
		return;
	}

	//===================================================================
	// ゲームループ
	//===================================================================

	// 時間
	m_fpsController.Init();

	// ループ
	while (1)
	{
		// 処理開始時間Get
		m_fpsController.UpdateStartTime();

		// ゲーム終了指定があるときはループ終了
		if (m_endFlag)
		{
			break;
		}

		//=========================================
		//
		// ウィンドウ関係の処理
		//
		//=========================================

		// ウィンドウのメッセージを処理する。WM_QUIT(メニューのQUIT=PostQuitMessage等)で
		// falseが返るのでループ終了。
		if (m_window.ProcessMessage() == false)
		{
			break;
		}

		// ウィンドウが破棄されてるならループ終了
		if (m_window.IsCreated() == false)
		{
			break;
		}

		// ※ESCでアプリ終了はしない（ESCはUIの「戻る」に使うため）。
		//   ゲーム終了はタイトルの QUIT メニュー（PostQuitMessage）で行う。

		//=========================================
		//
		// アプリケーション更新処理
		//
		//=========================================

		KdBeginUpdate();
		{
			PreUpdate();

			Update();

			PostUpdate();
		}
		KdPostUpdate();

		//=========================================
		//
		// アプリケーション描画処理
		//
		//=========================================

		KdBeginDraw();
		{
			PreDraw();

			Draw();

			PostDraw();

			DrawSprite();
		}
		KdPostDraw();

		//=========================================
		//
		// フレームレート制御
		//
		//=========================================

		m_fpsController.Update();
		//ウィンドウにdpsを表示
		// タイトルバーは作品名で始める。
		// ここは毎秒書き換わるので、生成時の名前が残らない
		std::string title = "DRIFT PROJECT  -  FPS " + std::to_string(m_fpsController.m_nowfps);
		SetWindowTextA(m_window.GetWndHandle(), title.c_str());
	}

	//===================================================================
	// アプリケーション解放
	//===================================================================
	Release();
}

// アプリケーション終了
void Application::Release()
{
	KdInputManager::Instance().Release();

	KdShaderManager::Instance().Release();

	KdAudioManager::Instance().Release();

	KdDirect3D::Instance().Release();

	// ウィンドウ削除
	m_window.Release();
}
