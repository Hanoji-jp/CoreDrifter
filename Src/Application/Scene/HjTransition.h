#pragma once

#include "SceneManager.h"

//==========================================================
// HjTransition
//   パネルワイプのシーン遷移(自作=Hj接頭辞, シングルトン)。
//   COVER: アシッドのパネルが左→右へ伸びて旧シーンを覆う。
//   REVEAL: パネルが右へ抜けて新シーンが現れる。次シーン名がパネルに乗る。
//   使い方: HjTransition::Instance().Go(遷移先) を呼ぶだけ。カバー完了時に
//           自動で SceneManager::SetNextScene し、リベールで新シーンを見せる。
//==========================================================
class HjTransition
{
public:
	static HjTransition& Instance() { static HjTransition inst; return inst; }

	bool Busy() const { return m_phase != Phase::Idle; }
	// モーフ遷移中はシーン更新を止める(遷移中に動くゲームと遷移後の動きが二重になるのを防ぐ)
	bool FreezesScene() const { return m_phase != Phase::Idle && m_morph; }

	// ワイプ開始(遷移中や同一シーンなら無視)
	void Go(SceneManager::SceneType target);
	// モーフ開始(コンテナ・トランスフォーム)。指定矩形(デザイン座標)が伸びて全画面化→新画面へ。
	//   PLAY→プレイモード選択のように「既存の要素が次のレイアウトへ変形する」表現に使う。
	void GoMorph(SceneManager::SceneType target, float dx, float dy, float w, float h);

	void Update(float dt);   // 毎フレーム(SceneManager::Updateから)
	void Draw();             // 最前面に重ねる(SceneManager::DrawSpriteの最後)

private:
	enum class Phase { Idle, Cover, Reveal };
	Phase m_phase = Phase::Idle;
	float m_t     = 0.0f;    // 0..1(現フェーズ内)
	float m_dur   = 0.40f;   // 1フェーズの秒数
	SceneManager::SceneType m_target = SceneManager::SceneType::Title;

	bool  m_morph = false;   // true=モーフ / false=ワイプ
	float m_srcX = 0.0f, m_srcY = 0.0f, m_srcW = 0.0f, m_srcH = 0.0f;   // モーフ元の矩形
	int   m_skipFrames = 0;   // 切替＋ロードの重いフレームを進めずに待つ回数

	HjTransition() {}
	static float Ease(float x);
	const char* TargetName() const;
};
