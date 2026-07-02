#pragma once

class KdTexture;

struct ImGuiAppLog
{
	ImGuiTextBuffer     Buf;
	ImGuiTextFilter     Filter;
	ImVector<int>       LineOffsets;        // Index to lines offset. We maintain this with AddLog() calls, allowing us to have a random access on lines
	bool                AutoScroll;
	bool                ScrollToBottom;

	ImGuiAppLog()
	{
		AutoScroll = true;
		ScrollToBottom = false;
		Clear();
	}

	void    Clear()
	{
		Buf.clear();
		LineOffsets.clear();
		LineOffsets.push_back(0);
	}

	void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
	{
		int old_size = Buf.size();
		va_list args;
		va_start(args, fmt);
		Buf.appendfv(fmt, args);
		va_end(args);
		for (int new_size = Buf.size(); old_size < new_size; old_size++)
			if (Buf[old_size] == '\n')
				LineOffsets.push_back(old_size + 1);
		if (AutoScroll)
			ScrollToBottom = true;
	}

	void    Draw(const char* title, bool* p_open = NULL)
	{
		if (!ImGui::Begin(title, p_open))
		{
			ImGui::End();
			return;
		}

		// Options menu
		if (ImGui::BeginPopup("Options"))
		{
			if (ImGui::Checkbox("Auto-scroll", &AutoScroll))
				if (AutoScroll)
					ScrollToBottom = true;
			ImGui::EndPopup();
		}

		// Main window
		if (ImGui::Button("Options"))
			ImGui::OpenPopup("Options");
		ImGui::SameLine();
		bool clear = ImGui::Button("Clear");
		ImGui::SameLine();
		bool copy = ImGui::Button("Copy");
		ImGui::SameLine();
		Filter.Draw("Filter", -100.0f);

		ImGui::Separator();
		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		if (clear)
			Clear();
		if (copy)
			ImGui::LogToClipboard();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		const char* buf = Buf.begin();
		const char* buf_end = Buf.end();
		if (Filter.IsActive())
		{
			for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
			{
				const char* line_start = buf + LineOffsets[line_no];
				const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
				if (Filter.PassFilter(line_start, line_end))
					ImGui::TextUnformatted(line_start, line_end);
			}
		}
		else
		{
			ImGuiListClipper clipper;
			clipper.Begin(LineOffsets.Size);
			while (clipper.Step())
			{
				for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
				{
					const char* line_start = buf + LineOffsets[line_no];
					const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
					ImGui::TextUnformatted(line_start, line_end);
				}
			}
			clipper.End();
		}
		ImGui::PopStyleVar();

		if (ScrollToBottom)
			ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;
		ImGui::EndChild();
		ImGui::End();
	}
};

class KdDebugGUI
{
public:
	void GuiInit(int w, int h);
	void GuiProcess();

	void AddLog(const char* fmt, ...);
	void ClearLog();

	// ImGui描画コールバックを登録する
	void SetGuiCallback(const std::function<void()>& _callback) { m_guiCallback = _callback; }
	void ClearGuiCallback() { m_guiCallback = nullptr; }

	// 常時ImGui描画コールバック（エディタ画面ON/OFFやシーンに関係なく毎フレーム呼ぶ）
	void SetPersistentGuiCallback(const std::function<void()>& _callback) { m_persistentGuiCallback = _callback; }
	void ClearPersistentGuiCallback() { m_persistentGuiCallback = nullptr; }

	// ゲーム画面を別ウィンドウ(ImGui)に表示するか（動画編集風レイアウト用。エディタ時に有効化）
	void SetGameViewport(bool enable) { m_gameViewport = enable; }
	bool IsGameViewport() const { return m_gameViewport; }

	// Game ウィンドウ(ImGui::Image)上のマウス情報（GuiProcess で更新、シーン側が次フレームで参照）
	// マウスが Game 画像の上にあるか
	bool IsGameHovered() const { return m_gameHovered; }
	// Game 画像内のマウス正規化座標（左上0,0 〜 右下1,1）
	void GetGameUV(float& u, float& v) const { u = m_gameUV[0]; v = m_gameUV[1]; }

private:
	void GuiRelease();

	// ImGui
	std::unique_ptr<ImGuiAppLog> m_uqLog = nullptr;

	std::function<void()> m_guiCallback = nullptr;
	std::function<void()> m_persistentGuiCallback = nullptr;   // 常時呼ぶImGui描画

	// ゲーム画面ビューポート（バックバッファをコピーして ImGui::Image で表示）
	bool m_gameViewport = false;
	std::shared_ptr<KdTexture> m_gameCapture = nullptr;

	// Game 画像上のマウス状態（GuiProcess で更新）
	bool  m_gameHovered = false;
	float m_gameUV[2]   = { 0.0f, 0.0f };

//=====================================================
// シングルトンパターン
//=====================================================
private:
	KdDebugGUI();
	~KdDebugGUI();

public:
	static KdDebugGUI& Instance() {
		static KdDebugGUI Instance;
		return Instance;
	}
};
