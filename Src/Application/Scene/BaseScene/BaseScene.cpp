#include "BaseScene.h"
#include "../../Const/CullingConst.h"

void BaseScene::PreUpdate()
{
	// Updateの前の更新処理
	// オブジェクトリストの整理 ・・・ 無効なオブジェクトを削除
	auto it = m_objList.begin();

	while (it != m_objList.end())
	{
		if ((*it)->IsExpired())	// IsExpired() ・・・ 無効ならtrue
		{
			// 無効なオブジェクトをリストから削除
			it = m_objList.erase(it);
		}
		else
		{
			++it;	// 次の要素へイテレータを進める
		}
	}

	// ↑の後には有効なオブジェクトだけのリストになっている

	for (auto& obj : m_objList)
	{
		obj->PreUpdate();
	}
}

void BaseScene::Update()
{
	// シーン毎のイベント処理
	Event();

	// KdGameObjectを継承した全てのオブジェクトの更新 (ポリモーフィズム)
	for (auto& obj : m_objList)
	{
		obj->Update();
	}
}

void BaseScene::PostUpdate()
{
	for (auto& obj : m_objList)
	{
		obj->PostUpdate();
	}
}

void BaseScene::PreDraw()
{
	for (auto& obj : m_objList)
	{
		obj->PreDraw();
	}
}

void BaseScene::Draw()
{
	// 視錐台カリング用。画面に映らないオブジェクトは色を描く各パスで飛ばす。
	// ただし影の生成パスには使わない。画面外の物も画面内へ影を落とすため、
	// カメラの視錐台で弾くと影だけが消えて不自然になる。
	DirectX::BoundingFrustum frustum;
	{
		const auto& cam = KdShaderManager::Instance().GetCameraCB();
		DirectX::BoundingFrustum local;
		DirectX::BoundingFrustum::CreateFromMatrix(local, cam.mProj);
		local.Transform(frustum, cam.mView.Invert());
	}
	auto visible = [&](const std::shared_ptr<KdGameObject>& o)
	{
		return !CullingConst::ObjectFrustumCull || o->CheckInScreen(frustum);
	};
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 光を遮るオブジェクト(影を生み出す要因となるオブジェクト)をBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_StandardShader.BeginGenerateDepthMapFromLight();
	{
		for (auto& obj : m_objList)
		{
			obj->GenerateDepthMapFromLight();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndGenerateDepthMapFromLight();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 陰影のないオブジェクト(背景など)はBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_StandardShader.BeginUnLit();
	{
		for (auto& obj : m_objList)
		{
			if (!visible(obj)) { continue; }
			obj->DrawUnLit();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndUnLit();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 陰影のあるオブジェクト(光源の影響を受けるオブジェクト)はBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_StandardShader.BeginLit();
	{
		for (auto& obj : m_objList)
		{
			if (!visible(obj)) { continue; }
			obj->DrawLit();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndLit();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 不透明シーンへ画面エッジ検出アウトラインを適用(トゥーン輪郭)
	// この後に描くエフェクト・光源オブジェクトには線が乗らない
	KdShaderManager::Instance().m_postProcessShader.ApplySceneOutline();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 陰影のないオブジェクト(エフェクトなど)はBeginとEndの間にまとめてDrawする。
	// 煙などは専用RTへ描き、塊全体のシルエット外周に輪郭を乗せてシーンへ合成する
	// (1粒ごとではなく合成後のシルエットに線を引くので内部が線だらけにならない)。
	// ※煙輪郭OFF時は Begin/End がスルーされ、従来通りシーンへ直描きされる。
	KdShaderManager::Instance().m_postProcessShader.BeginSmoke();
	KdShaderManager::Instance().m_StandardShader.BeginUnLit();
	{
		for (auto& obj : m_objList)
		{
			if (!visible(obj)) { continue; }
			obj->DrawEffect();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndUnLit();
	KdShaderManager::Instance().m_postProcessShader.EndSmokeAndComposite();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 煙の合成が済んだ後、シーンへ直接重ねるエフェクト(ネオンの線画など)。
	// 煙専用RTを通さないので、シルエット輪郭に塗り潰されず加算合成の光がそのまま出る。
	KdShaderManager::Instance().m_StandardShader.BeginUnLit();
	{
		for (auto& obj : m_objList)
		{
			if (!visible(obj)) { continue; }
			obj->DrawOverlayEffect();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndUnLit();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 光源オブジェクト(自ら光るオブジェクトやエフェクト)はBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_postProcessShader.BeginBright();
	KdShaderManager::Instance().m_StandardShader.BeginUnLit();
	{
		for (auto& obj : m_objList)
		{
			if (!visible(obj)) { continue; }
			obj->DrawBright();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndUnLit();
	KdShaderManager::Instance().m_postProcessShader.EndBright();
}

void BaseScene::DrawSprite()
{
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 2Dの描画はこの間で行う
	KdShaderManager::Instance().m_spriteShader.Begin();
	{
		for (auto& obj : m_objList)
		{
			obj->DrawSprite();
		}
	}
	KdShaderManager::Instance().m_spriteShader.End();

	// 文字流体化(ドリフト演出)：スプライトEndの後にバックバッファへ合成＝最前面に出す。
	// ゲーム中のスコア演出なので、それを使うシーンだけで描く(タイトル等には出さない)。
	if (UsesFluidText())
	{
		KdShaderManager::Instance().m_postProcessShader.DrawFluidText(KdFPSController::GetDt());
	}
}

void BaseScene::DrawDebug()
{
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// デバッグ情報の描画はこの間で行う
	KdShaderManager::Instance().m_StandardShader.BeginUnLit();
	{
		for (auto& obj : m_objList)
		{
			obj->DrawDebug();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndUnLit();
}

void BaseScene::Event()
{
	// 各シーンで必要な内容を実装(オーバーライド)する
}

void BaseScene::Init()
{
	// 各シーンで必要な内容を実装(オーバーライド)する
}
