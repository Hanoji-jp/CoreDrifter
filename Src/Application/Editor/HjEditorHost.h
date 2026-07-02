#pragma once

#include "HjEditorController.h"
#include "../GameObject/Camera/HjEditorCamera.h"

//==========================================================
// HjEditorHost
//   エディタ基盤(HjEditorController + HjEditorCamera)の動作デモ兼ホスト。
//   シーンの m_objList に追加するだけで、
//     ・右ドラッグ回転 / 中ドラッグパン / ホイールズーム / WASD のフリーカメラ
//     ・左クリック選択 → X/Y/Z 軸ギズモで操作
//     ・マウス中ボタンのクリックでギズモモード切替（移動 → 拡大 → 回転）
//     ・Ctrl+Z / Ctrl+Y で Undo / Redo、ImGui から「選択をコピー」
//   が動作する。
//
//   配置オブジェクトはデモとして「位置/回転/拡縮を持つ箱」で表現している。
//   実ゲームでは m_objs を自分のゲームオブジェクトに置き換え、
//   RegisterEntries() の setPos/setRot/setScale/dup を各オブジェクトに合わせて書けばよい。
//==========================================================
class HjEditorHost : public KdGameObject
{
public:
	void Init()       override;
	void Update()     override;
	void PreDraw()    override;
	void DrawEffect() override;   // シーンRTに描く＝ImGuiの「Game」ウィンドウに映る

private:
	// デモ配置オブジェクト（位置/回転/拡縮を持つ箱）
	struct DemoObj
	{
		Math::Vector3 pos;
		Math::Vector3 rot   = Math::Vector3::Zero;   // オイラー角(度)
		Math::Vector3 scale = Math::Vector3::One;
	};

	void RegisterEntries();   // 毎フレーム、配置オブジェクトをエディタへ登録
	void DrawBoxes();         // 配置オブジェクトを変形込みのワイヤーボックスで描画

	std::shared_ptr<HjEditorCamera> m_spCam = nullptr;   // エディタ用フリーカメラ
	HjEditorController              m_editor;            // ギズモ/Undo/Redo/コピー
	std::vector<DemoObj>          m_objs;              // 配置オブジェクト(デモ)
};
