#pragma once

#include "../../Const/StageConst.h"

//==========================================================
// Stage
//   コースマップ本体。gltfモデルを読み込み、当たり判定形状(モデル
//   コリジョン)を TypeGround(乗れる) + TypeBump(壁) として登録する。
//   車はこのオブジェクトへ下方レイ/球判定を飛ばして接地・壁押し戻しをする。
//
//   ※当たり判定は「当てられる側(=地形)」がコリジョン形状を持つ。
//     当てる側(=車)が Intersects() を実行する。
//==========================================================
class Stage : public KdGameObject
{
public:
	void Init()      override;
	void DrawLit()   override;
	void DrawDebug() override;

	// 位置合わせ用の調整パネル(ImGui)
	// hitNode = 車が今ぶつかっている地形ノードの番号(-1=なし)。
	// 名前から種類が読み取れないモデルでも、ぶつかった物を
	// その場で名指しで外せるようにするために受け取る。
	void DrawTuningImGui(int hitNode = -1);

	// プレイヤースポーン(シーンが起動時に車へ適用する)
	const Math::Vector3& GetSpawnPos() const { return m_spawnPos; }
	float                GetSpawnYaw() const { return m_spawnYaw; }
	// 現在の車位置などをスポーンとして設定(GameSceneのボタンから呼ぶ)
	void SetSpawn(const Math::Vector3& pos, float yaw) { m_spawnPos = pos; m_spawnYaw = yaw; }

	// マップ配置(大きさ・座標・向き)＋スポーンをファイルへ保存/読込
	void SaveConfig();
	void LoadConfig();

private:
	// m_offset / m_scale / m_yaw から m_mWorld を作り直す
	void RebuildMatrix();

	//===== 当たり判定に使うノードの選別 =====
	// 草や葉は見た目だけの物なので判定から外す。
	// モデル全体をそのまま判定に使うと、葉の上に乗ったり
	// 草むらに押し返されたりする。
	void RebuildCollision();
	// ノード名が除外対象か(キーワード一致 or 手動で外したもの)
	bool IsNoCollisionNode(const std::string& name) const;

	// 手動で外したノード名。キーワードで拾えないものを個別に指定する
	std::vector<std::string> m_manualExclude;

	// 実行中に足したキーワード。
	// 草木は数が多く、1つずつ外していられない。名前に共通部分があれば
	// ここへ登録して一括で外す。定数側を直さずに済むので、
	// モデルを差し替えても対応できる
	std::vector<std::string> m_extraKeywords;

	// この大きさ(m)より小さいノードを当たり判定から外す。0で無効。
	//
	// 草木は種類も配置数も多く、1つずつ外していられない。
	// しかも名前が自動出力で、木なのか建物なのか読み取れない。
	// 「小さい物には当たらなくてよい」という基準なら、
	// 種類を特定しなくても機械的に切れる。
	float m_excludeSmallerThan = 0.0f;

	// 名前がこのキーワードのどれかを含むか(大文字小文字は区別しない)
	bool MatchesKeyword(const std::string& name) const;

	// 名前から "mdl_037" のような一区切りを取り出す。
	// この地形のノード名は自動出力されたもので、
	//   unit_034_inst_027_mdl_037_lod_000
	// のように区切りが並ぶ形をしている。
	// prefix に "mdl_" を渡すと "mdl_037" が返る。見つからなければ空。
	static std::string ExtractTag(const std::string& name, const char* prefix);

	// そのノードの、ワールドでの大きさ(一番長い辺, m)。
	// 草や小物は小さく、建物や地形は大きい。名前が自動出力で
	// 種類が分からないモデルでも、大きさなら機械的に判別できる。
	float NodeWorldSize(int nodeIndex) const;
	void SaveNoCollision() const;
	void LoadNoCollision();

	// 保存対象(名前→float*)。SaveConfig/LoadConfigが共通で使う
	std::vector<std::pair<const char*, float*>> ConfigParamList();

	KdModelWork   m_model;

	// 配置(gltfの実寸・原点に合わせてImGuiで調整)
	Math::Vector3 m_offset = Math::Vector3(StageConst::OffsetX, StageConst::OffsetY, StageConst::OffsetZ);
	float         m_scale  = StageConst::ModelScale;
	float         m_yaw    = StageConst::YawOffset;

	// プレイヤーの初期スポーン(位置＋向き)
	Math::Vector3 m_spawnPos = Math::Vector3(StageConst::SpawnX, StageConst::SpawnY, StageConst::SpawnZ);
	float         m_spawnYaw = StageConst::SpawnYaw;
};
