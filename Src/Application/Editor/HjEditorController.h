#pragma once

//==========================================================
// HjEditorController
//   汎用エディタ基盤：オブジェクトの「選択・ギズモ操作(移動/拡大/回転)・Undo/Redo・コピー」を提供する。
//   ゲーム固有の型に依存せず、毎フレーム「編集対象(HjEditEntry)」を登録して使う。
//
//   ギズモのモードはマウス中ボタンの「クリック」で切り替わる（移動→拡大→回転→…）。
//   中ボタンの「ドラッグ」はエディタカメラのパン用なので、移動量で区別している。
//
//   使い方（シーンの Update/Event 内）:
//     m_editor.Begin();
//     for (auto& obj : m_objList) {
//         HjEditEntry e;
//         e.pos   = obj->GetPos();
//         e.rot   = obj->GetRotDeg();      // 回転を使うなら
//         e.scale = obj->GetScale();       // 拡縮を使うなら
//         e.label = "Object";
//         e.setPos   = [..](const Math::Vector3& p){ obj->SetPos(p); };
//         e.setRot   = [..](const Math::Vector3& r){ obj->SetRotDeg(r); };
//         e.setScale = [..](const Math::Vector3& s){ obj->SetScale(s); };
//         e.dup      = [..]{ /* 複製 */ };
//         m_editor.AddEntry(e);
//     }
//     m_editor.Update();      // ピック＋ギズモ＋Undo/Redo＋モード切替
//     （描画フェーズで）
//     m_editor.DrawMarker();  // 選択枠＋モード別ギズモ
//     m_editor.DrawImGui();   // モード/スナップ/コピー/Undo-Redo の UI
//==========================================================

#include "../Const/EditorPickConst.h"

// ギズモの操作モード
enum class HjGizmoMode
{
	Translate,  // 移動（軸方向にドラッグ）
	Scale,      // 拡大縮小（軸方向にドラッグ）
	Rotate,     // 回転（軸まわりにドラッグ）
};

// 1つの編集対象（位置/回転/拡縮の取得・設定・複製を抽象化して全オブジェクトを統一的に扱う）
struct HjEditEntry
{
	Math::Vector3                             pos;                          // 現在位置
	Math::Vector3                             rot   = Math::Vector3::Zero;  // 現在回転(オイラー角・度)
	Math::Vector3                             scale = Math::Vector3::One;   // 現在拡縮
	std::function<void(const Math::Vector3&)> setPos;                       // 位置を設定
	std::function<void(const Math::Vector3&)> setRot;                       // 回転を設定（任意。Rotateモードで使用）
	std::function<void(const Math::Vector3&)> setScale;                     // 拡縮を設定（任意。Scaleモードで使用）
	std::function<void()>                     dup;                          // 複製（任意。コピー機能で使用）
	std::string                               label;                        // 表示名
};

class HjEditorController
{
public:
	// フレーム先頭で呼ぶ：登録エントリをクリア
	void Begin() { m_entries.clear(); }

	// 編集対象を登録（Begin の後、Update の前に毎フレーム呼ぶ）
	void AddEntry(const HjEditEntry& _e) { m_entries.push_back(_e); }

	// コピー／生成の Undo 用：最後に追加したオブジェクトを取り除くハンドラ（任意）
	void SetRemoveLastHandler(const std::function<void()>& _fn) { m_removeLast = _fn; }

	// マウス選択＋軸ギズモ操作＋Undo/Redo＋モード切替（シーンの Event/Update から毎フレーム）
	void Update();

	// 選択中オブジェクトの選択枠＋モード別ギズモを描画（描画フェーズから）
	void DrawMarker();

	// モード/スナップ・選択情報・コピー・Undo/Redo ボタンの ImGui
	void DrawImGui();

	// 手動 Undo/Redo
	void Undo();
	void Redo();

	// 任意のコマンドを積む（undo/redo のペア）。redo は積んだ時点では呼ばれない。
	void PushCommand(const std::function<void()>& _undo, const std::function<void()>& _redo);

	int       GetSelectedIndex() const { return m_selEntry; }
	bool      HasSelection()     const { return m_selEntry >= 0 && m_selEntry < static_cast<int>(m_entries.size()); }
	bool&     SnapEnabled()            { return m_snapEnabled; }
	float&    SnapSize()               { return m_snapSize; }
	HjGizmoMode GetMode()          const { return m_mode; }
	void      SetMode(HjGizmoMode _m)    { m_mode = _m; }
	void      CycleMode();   // Translate → Scale → Rotate → Translate

private:
	// ゲーム画像内の正規化座標(u,v)からワールド空間ピッキングレイを作る
	bool ScreenRayFromGameUV(float _u, float _v, Math::Vector3& _outOrigin, Math::Vector3& _outDir) const;

	// 軸ハンドルのピック（0:X 1:Y 2:Z / 当たらなければ -1）
	int PickGizmoAxis(const Math::Vector3& _ro, const Math::Vector3& _rd, const Math::Vector3& _selPos) const;

	// アンドゥ／リドゥ単位
	struct EditCommand
	{
		std::function<void()> undo;
		std::function<void()> redo;
	};

	std::vector<HjEditEntry> m_entries;          // 毎フレーム再構築
	int   m_selEntry = -1;                     // 選択中（index）
	bool  m_lmbPrev  = false;                  // 左クリックのエッジ検出

	// ギズモのモード
	HjGizmoMode m_mode = HjGizmoMode::Translate;

	// ギズモ（軸ドラッグ）
	int           m_dragAxis     = -1;         // 0:X 1:Y 2:Z -1:none
	Math::Vector3 m_dragStartPos   = {};       // ドラッグ開始時の位置（兼アンドゥ用 旧値）
	Math::Vector3 m_dragStartScale = {};       // ドラッグ開始時の拡縮
	Math::Vector3 m_dragStartRot   = {};       // ドラッグ開始時の回転
	float         m_dragStartS   = 0.0f;       // ドラッグ開始時の軸上パラメータ
	bool          m_moveActive   = false;      // 軸ドラッグ操作中か

	// アンドゥ／リドゥ
	std::vector<EditCommand> m_undoStack;
	std::vector<EditCommand> m_redoStack;
	bool m_ctrlZPrev = false;
	bool m_ctrlYPrev = false;

	// 中ボタン（モード切替）：クリック／ドラッグ判別用
	bool  m_mmbPrev   = false;
	POINT m_mmbDownPos = {};
	bool  m_mmbMoved  = false;

	// グリッドスナップ（カクカク移動。Translateモードのみ）
	bool  m_snapEnabled = false;
	float m_snapSize    = EditorPickConst::DefaultSnapSize;

	// コピー／生成の Undo 用（任意）
	std::function<void()> m_removeLast;
};
