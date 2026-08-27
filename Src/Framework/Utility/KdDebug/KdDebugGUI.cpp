#include "../../../Application/main.h"

#include "KdDebugGUI.h"
#include "../../Direct3D/KdDirect3D.h"
#include "../../Direct3D/KdTexture.h"
#include "../../Shader/KdShaderManager.h"
#include "../../Shader/PostProcessShader/KdPostProcessShader.h"

KdDebugGUI::KdDebugGUI()
{}
KdDebugGUI::~KdDebugGUI()
{ 
	GuiRelease(); 
}

void KdDebugGUI::GuiInit(int w, int h)
{
	// 初期化済みなら動作させない
	if (m_uqLog) return;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Setup Dear ImGui style
	// ImGui::StyleColorsDark();
	ImGui::StyleColorsClassic();
	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(Application::Instance().GetWindowHandle());
	ImGui_ImplDX11_Init(KdDirect3D::Instance().WorkDev(), KdDirect3D::Instance().WorkDevContext());

	// ゲームウィンドウ外へのフロート + ドッキング（ドラッグ結合）を有効化
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#include "imgui/ja_glyph_ranges.h"
	ImGuiIO& io = ImGui::GetIO();
	// 見やすさ重視で大きめに読み込む（仮想解像度＋FramebufferScaleで小さく見えるため）
	constexpr float kFontPx = 19.0f;
	// ベースフォントを明示的なサイズで追加（MergeModeと競合しないように）
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msgothic.ttc", kFontPx, nullptr, io.Fonts->GetGlyphRangesDefault());
	// 日本語グリフをMergeModeで追加
	ImFontConfig config;
	config.MergeMode = true;
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msgothic.ttc", kFontPx, &config, glyphRangesJapanese);

	// ウィジェット（余白・スクロールバー・角丸など）も合わせて拡大して全体を見やすく
	ImGui::GetStyle().ScaleAllSizes(1.4f);

	m_uqLog = std::make_unique<ImGuiAppLog>();
}

void KdDebugGUI::GuiProcess()
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	// 「Game」ウィンドウ用に、UIまで描き終えたバックバッファを複製する。
	// この時点(ImGuiが何か描く前)のバックバッファが、UI込みの最終画そのもの。
	if (m_gameViewport) { CaptureGameView(); }

	//===========================================================
	// ImGui開始
	//===========================================================
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	// 仮想解像度対応：内部描画(バックバッファ)がウィンドウのクライアントサイズと違う場合、
	// ImGui のレンダー出力だけをバックバッファ基準へ縮尺する。
	// FramebufferScale を使うので、座標系(DisplaySize)・マウスはウィンドウのまま＝
	// マルチビューポート（ウィンドウ外ドラッグ）有効でもクリック位置がずれない。
	{
		ImGuiIO& io = ImGui::GetIO();
		const auto& bb = KdDirect3D::Instance().GetBackBuffer();
		if (bb && io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f)
		{
			const float bw = static_cast<float>(bb->GetInfo().Width);
			const float bh = static_cast<float>(bb->GetInfo().Height);
			io.DisplayFramebufferScale = ImVec2(bw / io.DisplaySize.x, bh / io.DisplaySize.y);
		}
	}

	ImGui::NewFrame();

	// 画面全体を覆う DockSpace を作成（エディタ表示中のみ）。
	// DockSpace が無いとウィンドウ同士を結合する受け皿が存在しないので、
	// 調整パネルを出すときは必ずこれも作る。
	// 中央は透過なので、ゲーム画面の見た目は変わらない。
	if (m_gameViewport)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGuiWindowFlags dockFlags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("##DockSpaceRoot", nullptr, dockFlags);
		ImGui::PopStyleVar();
		// 中央は常に透過（裏のシーン描画／VR映像が透けて見える）
		ImGui::DockSpace(ImGui::GetID("##MainDockSpace"), ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::End();
	}

	// （背景は灰色クリアしない。裏のシーン描画／VR映像をそのまま透過表示する）

	// ── ゲーム画面ビューポート：シーンRT(SRV)を「Game」ウィンドウに表示 ──
	if (m_gameViewport)
	{
		// 既定で中央ドックスペースに入れる（別ウィンドウ化して見失わないように）
		ImGui::SetNextWindowDockID(ImGui::GetID("##MainDockSpace"), ImGuiCond_FirstUseEver);
		ImGui::Begin("Game");
		// UI込みの複製(m_gameCapture)を表示する。ポストプロセス直後のシーンRTだと、
		// その後に描くHUD/UIが映らない。
		const auto& gameTex = m_gameCapture;
		if (gameTex && gameTex->WorkSRView())
		{
			ImVec2 avail = ImGui::GetContentRegionAvail();
			const float texAspect = 16.0f / 9.0f;   // 16:9固定でレターボックス
			float w = avail.x, h = avail.x / texAspect;
			if (h > avail.y) { h = avail.y; w = avail.y * texAspect; }
			const ImVec2 cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + (avail.x - w) * 0.5f, cur.y + (avail.y - h) * 0.5f));
			ImGui::Image((ImTextureID)(intptr_t)gameTex->WorkSRView(), ImVec2(w, h));

			// 描画したゲーム画像の矩形内マウス座標を正規化して保持（シーン側のピッキングに使う）
			const ImVec2 rmin = ImGui::GetItemRectMin();
			const ImVec2 rmax = ImGui::GetItemRectMax();
			const ImVec2 mp   = ImGui::GetIO().MousePos;
			const float  gw   = rmax.x - rmin.x;
			const float  gh   = rmax.y - rmin.y;
			m_gameHovered = ImGui::IsItemHovered();
			m_gameUV[0]   = (gw > 0.0f) ? (mp.x - rmin.x) / gw : 0.0f;
			m_gameUV[1]   = (gh > 0.0f) ? (mp.y - rmin.y) / gh : 0.0f;
		}
		else
		{
			m_gameHovered = false;
		}
		ImGui::End();
	}

	//===========================================================
	// 以下にImGui描画処理を記述
	//===========================================================

	// デバッグウィンドウ(日本語を表示したい場合はこう書く)
//	if (ImGui::Begin(U8("えふぴぃえす")))
//	{
		// FPS
//		ImGui::Text("FPS : %d", Application::Instance().GetNowFPS());
//	}
//	ImGui::End();

	// エディタ画面ON時のみ、シーンのImGui・ログを表示（OFFは全ImGui非表示）
	if (m_gameViewport)
	{
		// 登録されたシーンのImGui描画を呼ぶ
		if (m_guiCallback) { m_guiCallback(); }

		// ログウィンドウ
		m_uqLog->Draw("Log Window");
	}

	//=====================================================
	// ログ出力 ・・・ AddLog("～") で追加
	//=====================================================

//	m_uqLog->AddLog("hello world\n");

	//=====================================================
	// 別ソースファイルからログを出力する場合
	//=====================================================

//	KdDebugGUI::Instance().AddLog("TestLog\n");

	// 常時ImGui描画（エディタ画面ON/OFF・シーンに関係なく毎フレーム呼ぶ）
	if (m_persistentGuiCallback) { m_persistentGuiCallback(); }

	//===========================================================
	// ここより上にImGuiの描画はする事
	//===========================================================
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// ゲームウィンドウ外のImGuiウィンドウを更新・描画
	// RenderPlatformWindowsDefault はレンダーターゲットを書き換えるため前後で保存・復元する
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ID3D11RenderTargetView* prevRTV = nullptr;
		ID3D11DepthStencilView* prevDSV = nullptr;
		KdDirect3D::Instance().WorkDevContext()->OMGetRenderTargets(1, &prevRTV, &prevDSV);

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		KdDirect3D::Instance().WorkDevContext()->OMSetRenderTargets(1, &prevRTV, prevDSV);
		if (prevRTV) { prevRTV->Release(); }
		if (prevDSV) { prevDSV->Release(); }
	}
}

void KdDebugGUI::AddLog(const char* fmt,...)
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	char tmpStr[128] = {};
	va_list args;
	va_start(args, fmt);
	vsprintf_s(tmpStr, fmt, args);
	m_uqLog->AddLog(tmpStr);
	va_end(args);
}

void KdDebugGUI::ClearLog()
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	m_uqLog->Clear();
}

//----------------------------------------------------------
// 「Game」ウィンドウ用にバックバッファを複製する。
//
// ポストプロセス直後のシーンRTを直接貼っていると、その後に描くHUD/UIが
// 映らない(spriteShaderはポストプロセスの後、バックバッファへ直接描くため)。
// UIまで含めた最終画像を見せるには、UIを描き終えた直後のバックバッファ
// そのものを複製するしかない。
//----------------------------------------------------------
void KdDebugGUI::CaptureGameView()
{
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	if (!bb || !bb->WorkResource()) { return; }

	D3D11_TEXTURE2D_DESC srcDesc = {};
	bb->WorkResource()->GetDesc(&srcDesc);

	// 初回、またはバックバッファの解像度が変わった(ウィンドウリサイズ等)ときだけ作り直す
	if (!m_gameCapture || !m_gameCapture->WorkResource() ||
	    m_gameCapture->GetInfo().Width  != srcDesc.Width ||
	    m_gameCapture->GetInfo().Height != srcDesc.Height)
	{
		// ※バックバッファの記述子をそのまま流用する。
		//   CopyResource は「フォーマット・サイズ・ミップ数・サンプル数が
		//   完全一致」を要求するため、自前の設定で作ると条件を外して落ちる。
		//   ImGui へ渡すのでシェーダーリソースとしても使えるようにする。
		D3D11_TEXTURE2D_DESC desc = srcDesc;
		desc.Usage          = D3D11_USAGE_DEFAULT;
		desc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags      = 0;

		m_gameCapture = std::make_shared<KdTexture>();
		if (!m_gameCapture->Create(desc, nullptr))
		{
			m_gameCapture = nullptr;
			return;
		}
	}

	// GPU内コピー。条件は上で揃えてある
	KdDirect3D::Instance().WorkDevContext()->CopyResource(
		m_gameCapture->WorkResource(), bb->WorkResource());
}

void KdDebugGUI::GuiRelease()
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	m_gameCapture = nullptr;

	// 終了時にレイアウト（ドッキング配置）を確実に保存する
	if (ImGui::GetCurrentContext() && ImGui::GetIO().IniFilename)
	{
		ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
	}

	m_uqLog = nullptr;

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
