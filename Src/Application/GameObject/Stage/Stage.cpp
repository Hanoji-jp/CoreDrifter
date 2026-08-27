#include "Stage.h"
#include "../Effect/SkidMark.h"   // 路面へタイヤ痕の焼き付けマップを適用する

void Stage::Init()
{
	m_drawType = eDrawTypeLit;

	// マップモデル読み込み
	m_model.SetModelData(StageConst::ModelPath);

	// 広大なマップなのでカリングで消えないよう半径を大きく
	m_cullingRadius = StageConst::CullingRadius;

	// 保存済みの配置(大きさ・座標・向き)とスポーンがあれば読み込む(既定値を上書き)
	LoadConfig();

	// 配置行列を作成
	RebuildMatrix();

	// 手動で外したノードの一覧(あれば)
	LoadNoCollision();

	// 当たり判定形状を登録
	RebuildCollision();
}

//----------------------------------------------------------
// 当たり判定に使うノードを選び直して登録する。
//
// モデルメッシュを「地形(乗れる)＋壁」として使う。
//   TypeGround … 車が下方レイで接地判定に使う
//   TypeBump   … 車が球判定で壁の押し戻しに使う
//   ※1枚メッシュ兼用だが、車側で「面法線がほぼ垂直＝壁」だけ押し戻すよう
//     フィルタするので、走る路面(水平面)は壁扱いにならない。
//
// ただし草や葉は判定から外す。見た目のための物であって、
// 乗る物でも当たる物でもない。残すと葉の上に車が乗ったり、
// 草むらで押し返されたりする。面の枚数も多いので判定が重くなる。
//----------------------------------------------------------
void Stage::RebuildCollision()
{
	auto data = m_model.GetData();
	if (!data) { return; }

	// 判定に使えるノードの中から、除外対象を抜いた一覧を作る。
	// フィルタは「使うノードの並び」を渡す形なので、
	// 残すほうを集める(空で渡すと全部使う扱いになる)。
	const auto& nodes = data->GetOriginalNodes();
	std::vector<int> use;

	for (int index : data->GetCollisionMeshNodeIndices())
	{
		if (index < 0 || index >= static_cast<int>(nodes.size())) { continue; }
		if (IsNoCollisionNode(nodes[index].m_name)) { continue; }

		// 小さい物は当たり判定から外す。
		// 草や小石に当たって止まる必要はないし、数が多いので判定も重い
		if (m_excludeSmallerThan > 0.0f
		 && NodeWorldSize(index) < m_excludeSmallerThan) { continue; }
		use.push_back(index);
	}

	auto shape = std::make_unique<KdModelCollision>(
		data, KdCollider::TypeGround | KdCollider::TypeBump);

	// 全部残った場合は指定しない。
	// 空でない一覧を渡すのと結果は同じだが、
	// 「絞っていない」ことがコード上で分かるようにしておく
	if (use.size() != data->GetCollisionMeshNodeIndices().size())
	{
		shape->SetNodeFilter(use);
	}

	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape("StageCollision", std::move(shape));
}

//----------------------------------------------------------
// このノードを当たり判定から外すか。
//----------------------------------------------------------
bool Stage::IsNoCollisionNode(const std::string& name) const
{
	// 手動で外したもの
	for (const std::string& n : m_manualExclude)
	{
		if (n == name) { return true; }
	}

	return MatchesKeyword(name);
}

//----------------------------------------------------------
// そのノードのワールドでの大きさ(一番長い辺, m)。
//
// メッシュが持つ境界ボックスに、ステージの拡大率を掛けて出す。
// 回転は考えない(向きが変わっても「だいたいの大きさ」は変わらない)。
//----------------------------------------------------------
float Stage::NodeWorldSize(int nodeIndex) const
{
	auto data = m_model.GetData();
	if (!data) { return 0.0f; }

	const auto& nodes = data->GetOriginalNodes();
	if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) { return 0.0f; }

	const auto& mesh = nodes[nodeIndex].m_spMesh;
	if (!mesh) { return 0.0f; }

	// Extents は中心からの半分の長さなので、辺の長さは2倍
	const auto& ex = mesh->GetBoundingBox().Extents;
	const float longest = std::max(ex.x, std::max(ex.y, ex.z)) * 2.0f;

	return longest * m_scale;
}

//----------------------------------------------------------
// 名前から "mdl_037" のような一区切りを取り出す。
//
// 区切りは英字の接頭辞＋数字で、次の区切りは "_" で始まる。
// 数字の桁数はモデルによって違うので、固定長では取れない。
//----------------------------------------------------------
std::string Stage::ExtractTag(const std::string& name, const char* prefix)
{
	if (!prefix) { return ""; }

	const size_t at = name.find(prefix);
	if (at == std::string::npos) { return ""; }

	// 接頭辞の直後から、数字が続く間だけを取る
	size_t end = at + strlen(prefix);
	while (end < name.size() && name[end] >= '0' && name[end] <= '9') { ++end; }

	// 数字が1つも無ければ区切りとして扱わない
	if (end == at + strlen(prefix)) { return ""; }

	return name.substr(at, end - at);
}

//----------------------------------------------------------
// 名前がキーワードのどれかを含むか。
// 大文字小文字は揃えてから比べる(モデルによって書き方が違う)。
//----------------------------------------------------------
bool Stage::MatchesKeyword(const std::string& name) const
{
	std::string lower = name;
	std::transform(lower.begin(), lower.end(), lower.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	// 最初から入れてあるもの
	for (const char* key : StageConst::NoCollisionKeywords)
	{
		if (lower.find(key) != std::string::npos) { return true; }
	}

	// 実行中に足したもの。草木は数が多く1つずつ外していられないので、
	// 名前の共通部分で一括して外せるようにしてある
	for (const std::string& key : m_extraKeywords)
	{
		if (key.empty()) { continue; }
		std::string k = key;
		std::transform(k.begin(), k.end(), k.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (lower.find(k) != std::string::npos) { return true; }
	}
	return false;
}

//----------------------------------------------------------
// 手動で外したノードの保存/読込。
// 配置(数値だけ)とは形式が違うので別ファイルにしてある。
//----------------------------------------------------------
void Stage::SaveNoCollision() const
{
	std::ofstream ofs(StageConst::NoCollisionPath);
	if (!ofs) { return; }
	// キーワードは行頭に # を付けて区別する。
	// ノード名に空白が入ることがあるので、区切りではなく行頭で見分ける
	for (const std::string& k : m_extraKeywords) { ofs << "#" << k << "\n"; }
	for (const std::string& n : m_manualExclude) { ofs << n << "\n"; }
}

void Stage::LoadNoCollision()
{
	m_manualExclude.clear();
	m_extraKeywords.clear();

	std::ifstream ifs(StageConst::NoCollisionPath);
	if (!ifs) { return; }

	std::string line;
	while (std::getline(ifs, line))
	{
		// ノード名に空白が入ることがあるので行ごと読む
		if (!line.empty() && line.back() == '\r') { line.pop_back(); }
		if (line.empty()) { continue; }

		// 行頭が # ならキーワード、そうでなければノード名
		if (line[0] == '#') { m_extraKeywords.push_back(line.substr(1)); }
		else                { m_manualExclude.push_back(line); }
	}
}

void Stage::RebuildMatrix()
{
	m_mWorld =
		Math::Matrix::CreateScale(m_scale) *
		Math::Matrix::CreateRotationY(m_yaw) *
		Math::Matrix::CreateTranslation(m_offset);
}

void Stage::DrawLit()
{
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);   // 裏面カリング有効

	// タイヤ痕の焼き付けマップを路面へ適用する。
	// 痕はポリゴンとして描かず、ここでワールドXZから引いて色を暗くするだけなので、
	// 痕が何本あっても、どれだけ長く残っても路面の描画コストは変わらない。
	// DrawModelは描画後に定数バッファを既定へ戻すため、描画の直前に呼ぶこと。
	SkidMark::Instance().ApplyToShader();

	KdShaderManager::Instance().m_StandardShader.DrawModel(m_model, m_mWorld);
	KdShaderManager::Instance().UndoRasterizerState();
}

void Stage::DrawDebug()
{
	// 当たり判定形状のワイヤ表示など(必要になれば追加)
}

void Stage::DrawTuningImGui(int hitNode)
{
	// ※ウィンドウは開かない。Hierarchy の Inspector の中へ描く。

	bool changed = false;
	changed |= ImGui::DragFloat(U8("スケール(大きさ)"), &m_scale, StageConst::ScaleStep,
	                            StageConst::ScaleMin, StageConst::ScaleMax);
	changed |= ImGui::DragFloat3(U8("オフセット(座標XYZ)"), &m_offset.x, StageConst::OffsetStep);
	changed |= ImGui::DragFloat(U8("向き(Y回転 rad)"), &m_yaw, StageConst::YawStep);

	if (changed) { RebuildMatrix(); }   // 動かすと当たり判定(m_mWorld)も一緒に動く

	ImGui::Separator();
	ImGui::Text(U8("プレイヤースポーン"));
	ImGui::DragFloat3(U8("スポーン座標(XYZ)"), &m_spawnPos.x, StageConst::SpawnStep);
	ImGui::DragFloat(U8("スポーン向き(Y回転 rad)"), &m_spawnYaw, StageConst::YawStep);

	ImGui::Separator();
	// 大きさ・座標・向き＋スポーンをまとめてファイルへ保存/読込
	if (ImGui::Button(U8("保存(マップ配置＋スポーン)"))) { SaveConfig(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("読込(ファイルから)")))         { LoadConfig(); RebuildMatrix(); }


	//===== 当たり判定に使うノード =====
	// 草や葉が判定に入っていると、葉の上に乗ったり草むらで押し返されたりする。
	// モデルによって名前の付け方が違うので、実際の名前を並べて選べるようにする。
	if (ImGui::CollapsingHeader(U8("当たり判定のノード")))
	{
		auto data = m_model.GetData();
		if (!data)
		{
			ImGui::TextDisabled(U8("モデルが読み込まれていません"));
		}
		else
		{
			const auto& nodes   = data->GetOriginalNodes();
			const auto& indices = data->GetCollisionMeshNodeIndices();

			int on = 0;
			for (int index : indices)
			{
				if (index >= 0 && index < static_cast<int>(nodes.size())
				 && !IsNoCollisionNode(nodes[index].m_name)) { ++on; }
			}
			ImGui::Text(U8("判定に使用: %d / %d"), on, static_cast<int>(indices.size()));
			ImGui::TextWrapped(
				U8("チェックを外すと当たり判定から除外されます。"
				   "草・葉などは名前から自動で外れます(その場合はチェックが灰色)。"));
			
			//----- キーワードで一括除外 -----
			// 草木は数が多く、1つずつ外していられない。
			// 名前に共通部分があれば、ここへ入れて一度に外す。
			ImGui::SeparatorText(U8("キーワードで一括除外"));
			
			static char s_keyword[64] = "";
			ImGui::InputText(U8("名前に含む文字"), s_keyword, sizeof(s_keyword));
			
			// 何個に効くのかを先に見せる。
			// 押してから「多すぎた」と気づくと戻すのが面倒
			int hit = 0;
			if (s_keyword[0])
			{
				std::string k = s_keyword;
				std::transform(k.begin(), k.end(), k.begin(),
				               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				for (int index : indices)
				{
					if (index < 0 || index >= static_cast<int>(nodes.size())) { continue; }
					std::string lower = nodes[index].m_name;
					std::transform(lower.begin(), lower.end(), lower.begin(),
					               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					if (lower.find(k) != std::string::npos) { ++hit; }
				}
				ImGui::Text(U8("このキーワードに %d 個が一致します"), hit);
			}
			
			ImGui::BeginDisabled(s_keyword[0] == '\0' || hit == 0);
			if (ImGui::Button(U8("このキーワードで外す")))
			{
				m_extraKeywords.push_back(s_keyword);
				s_keyword[0] = '\0';
				RebuildCollision();
				SaveNoCollision();
			}
			ImGui::EndDisabled();
			
			// 登録済みのキーワード。効きすぎたときに個別で取り消せる
			for (size_t ki = 0; ki < m_extraKeywords.size(); ++ki)
			{
				ImGui::PushID(static_cast<int>(ki) + 10000);
				if (ImGui::SmallButton(U8("取消")))
				{
					m_extraKeywords.erase(m_extraKeywords.begin() + ki);
					RebuildCollision();
					SaveNoCollision();
					ImGui::PopID();
					break;
				}
				ImGui::SameLine();
				ImGui::Text(U8("%s を含むノードを除外中"), m_extraKeywords[ki].c_str());
				ImGui::PopID();
			}
			ImGui::Separator();

			//----- LODで一括除外 -----
			// LODは同じ物の詳細度違いなので、同じ場所に形が何枚も重なっている。
			// 判定は全部拾ってしまうため、三角形の数が段数ぶん増えているのに
			// 見た目は何も変わらない。一番詳しいもの(lod_000)だけ残す。
			//----- 大きさで一括除外 -----
			// この地形のノード名は自動出力で、木なのか建物なのか読み取れない。
			// しかも1つの区画(unit)の中に別々のモデルが大量に混ざっているので、
			// 名前で種類をまとめることができない。
			//
			// ただ「小さい物には当たらなくてよい」という基準なら、
			// 種類を特定しなくても機械的に切れる。
			// 草・小石・落ち葉は小さく、建物や地形は大きい。
			//----- 今ぶつかっている物を外す -----
			// 一番確実なやり方。草へ突っ込んで、出た名前をそのまま外す。
			// 名前から種類が読み取れないモデルでは、これが一番早い。
			ImGui::SeparatorText(U8("今ぶつかっている物"));
			{
				if (hitNode < 0 || hitNode >= static_cast<int>(nodes.size()))
				{
					ImGui::TextDisabled(U8("(何にも当たっていません。壁や草へ寄せてください)"));
				}
				else
				{
					const std::string& hitName = nodes[hitNode].m_name;
					ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "%s", hitName.c_str());
					ImGui::Text(U8("大きさ %.2f m"), NodeWorldSize(hitNode));
					
					// このノード1つだけ外す
					if (ImGui::Button(U8("これを外す")))
					{
						m_manualExclude.push_back(hitName);
						RebuildCollision();
						SaveNoCollision();
					}
					
					// 同じモデルを全部外す。
					// 草は同じ物が何百と置かれているので、1つ外しても切りがない
					const std::string mdl = ExtractTag(hitName, "mdl_");
					if (!mdl.empty())
					{
						int same = 0;
						for (int index : indices)
						{
							if (index < 0 || index >= static_cast<int>(nodes.size())) { continue; }
							if (ExtractTag(nodes[index].m_name, "mdl_") == mdl) { ++same; }
						}
						
						ImGui::SameLine();
						if (ImGui::Button(U8("同じモデルを全部外す")))
						{
							m_extraKeywords.push_back(mdl);
							RebuildCollision();
							SaveNoCollision();
						}
						ImGui::Text(U8("%s は %d 個置かれています"), mdl.c_str(), same);
					}
				}
			}
			
			ImGui::SeparatorText(U8("大きさで一括除外"));
			{
				float th = m_excludeSmallerThan;
				if (ImGui::DragFloat(U8("この大きさ(m)より小さい物を外す"), &th, 0.05f, 0.0f, 20.0f))
				{
					m_excludeSmallerThan = std::max(th, 0.0f);
					RebuildCollision();
					SaveConfig();
				}
				ImGui::TextDisabled(U8("0 で無効。車の全長が約 %.1fm なので、それより小さい物が目安"),
				                    2.0f);
				
				// 今の設定で何個が外れるかを出す。
				// 押してから確かめる作りだと、行き過ぎたときに気づけない
				if (m_excludeSmallerThan > 0.0f)
				{
					// ※変数名に small は使えない。Windowsのヘッダが
					//   #define small char としており、型名に置き換えられてしまう
					int smallCount = 0;
					for (int index : indices)
					{
						if (NodeWorldSize(index) < m_excludeSmallerThan) { ++smallCount; }
					}
					ImGui::Text(U8("この大きさ未満: %d 個"), smallCount);
				}
			}
			
			ImGui::SeparatorText(U8("LOD"));
			{
				// 名前に含まれる lod_### を集めて、0番以外を数える
				std::set<std::string> lodTags;
				for (int index : indices)
				{
					if (index < 0 || index >= static_cast<int>(nodes.size())) { continue; }
					const std::string tag = ExtractTag(nodes[index].m_name, "lod_");
					if (!tag.empty() && tag != "lod_000") { lodTags.insert(tag); }
				}
				
				if (lodTags.empty())
				{
					ImGui::TextDisabled(U8("(lod_000 以外は見つかりませんでした)"));
				}
				else
				{
					std::string list;
					for (const std::string& t : lodTags) { list += t + " "; }
					ImGui::Text(U8("見つかったLOD: %s"), list.c_str());
					if (ImGui::Button(U8("lod_000 以外をまとめて外す")))
					{
						for (const std::string& t : lodTags) { m_extraKeywords.push_back(t); }
						RebuildCollision();
						SaveNoCollision();
					}
				}
			}
			
			//----- モデル別に外す -----
			// この地形の名前は自動出力されたもので、tree や grass のような
			// 意味が入っていない。種類を表しているのは mdl_### で、
			// 同じ木を何本置いても mdl は同じ番号になる。
			// つまり mdl 単位で外せば、その木を全部まとめて外せる。
			//
			// 配置数が多いものほど「散らして置いた草木」である可能性が高い。
			// 路面や建物は数が少ないので、多い順に並べれば上から順に見ていける。
			ImGui::SeparatorText(U8("まとめて外す"));
			
			// どの区切りでまとめるか。
			//   unit … プロップの定義。木や岩といった「種類」にあたる。
			//          1本の木が幹と葉に分かれていても、まとめて外れる。
			//   mdl  … その中の部品。幹だけ残して葉だけ外す、ができる。
			//   inst … 配置ごとの通し番号。種類ではないので普通は使わない。
			static int s_groupBy = 0;
			const char* groupPrefix[] = { "unit_", "mdl_", "inst_" };
			const char* groupLabel[]  = { U8("unit(種類)"), U8("mdl(部品)"), U8("inst(配置)") };
			ImGui::Combo(U8("まとめる単位"), &s_groupBy, groupLabel, IM_ARRAYSIZE(groupLabel));
			const char* prefix = groupPrefix[s_groupBy];
			{
				std::map<std::string, int> counts;
				for (int index : indices)
				{
					if (index < 0 || index >= static_cast<int>(nodes.size())) { continue; }
					const std::string tag = ExtractTag(nodes[index].m_name, prefix);
					if (!tag.empty()) { counts[tag]++; }
				}
				
				if (counts.empty())
				{
					ImGui::TextDisabled(U8("(%s を含む名前が見つかりませんでした)"), prefix);
				}
				else
				{
					// 配置数の多い順に並べ替える
					std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
					std::sort(sorted.begin(), sorted.end(),
					          [](const auto& a, const auto& b) { return a.second > b.second; });
					
					ImGui::Text(U8("%d 種類。配置数の多い順"), static_cast<int>(sorted.size()));
					ImGui::BeginChild("StageGroup", ImVec2(0, 180), true);
					for (const auto& e : sorted)
					{
						ImGui::PushID(e.first.c_str());
						
						// すでに外しているかを見て、押せるかを変える
						bool already = false;
						for (const std::string& k : m_extraKeywords)
						{
							if (k == e.first) { already = true; break; }
						}
						
						ImGui::BeginDisabled(already);
						if (ImGui::SmallButton(U8("外す")))
						{
							m_extraKeywords.push_back(e.first);
							RebuildCollision();
							SaveNoCollision();
						}
						ImGui::EndDisabled();
						
						ImGui::SameLine();
						if (already) { ImGui::TextDisabled(U8("%s  x%d  (除外中)"), e.first.c_str(), e.second); }
						else         { ImGui::Text(U8("%s  x%d"), e.first.c_str(), e.second); }
						
						ImGui::PopID();
					}
					ImGui::EndChild();
				}
			}
			ImGui::Separator();
			
			ImGui::BeginChild("StageNodes", ImVec2(0, 220), true);
			for (int index : indices)
			{
				if (index < 0 || index >= static_cast<int>(nodes.size())) { continue; }
				const std::string& name = nodes[index].m_name;

				// キーワードで自動的に外れているものは、触らせても意味が薄い。
				// なぜ外れているのかが分かるよう、区別して出す
				const bool byKeyword = MatchesKeyword(name);

				ImGui::PushID(index);
				if (byKeyword)
				{
					ImGui::TextDisabled(U8("[自動で除外] %s"), name.c_str());
				}
				else
				{
					// 手動リストに入っていなければ判定に使う
					bool use = true;
					for (const std::string& n : m_manualExclude)
					{
						if (n == name) { use = false; break; }
					}

					if (ImGui::Checkbox(name.c_str(), &use))
					{
						if (use)
						{
							// 使う＝手動リストから外す
							for (size_t k = 0; k < m_manualExclude.size(); ++k)
							{
								if (m_manualExclude[k] == name)
								{
									m_manualExclude.erase(m_manualExclude.begin() + k);
									break;
								}
							}
						}
						else
						{
							m_manualExclude.push_back(name);
						}
						RebuildCollision();
						SaveNoCollision();
					}
				}
				ImGui::PopID();
			}
			ImGui::EndChild();

			if (ImGui::Button(U8("手動の除外をすべて戻す")))
			{
				m_manualExclude.clear();
				m_extraKeywords.clear();
				RebuildCollision();
				SaveNoCollision();
			}
		}
	}
}

//----------------------------------------------------------
// 配置(大きさ・座標・向き)＋スポーンの保存 / 読込
//   CarTune_*.txt と同じ「key value」の簡易テキスト形式。
//----------------------------------------------------------
std::vector<std::pair<const char*, float*>> Stage::ConfigParamList()
{
	return {
		{ "scale", &m_scale },
		{ "offX", &m_offset.x }, { "offY", &m_offset.y }, { "offZ", &m_offset.z },
		{ "yaw",  &m_yaw },
		{ "spawnX", &m_spawnPos.x }, { "spawnY", &m_spawnPos.y }, { "spawnZ", &m_spawnPos.z },
		{ "spawnYaw", &m_spawnYaw }, { "excludeSmallerThan", &m_excludeSmallerThan },
	};
}

void Stage::SaveConfig()
{
	std::ofstream ofs(StageConst::ConfigPath);
	if (!ofs) { return; }
	for (const auto& p : ConfigParamList()) { ofs << p.first << " " << *p.second << "\n"; }
}

void Stage::LoadConfig()
{
	std::ifstream ifs(StageConst::ConfigPath);
	if (!ifs) { return; }
	auto params = ConfigParamList();
	std::string key;
	float val = 0.0f;
	while (ifs >> key >> val)
	{
		for (const auto& p : params) { if (key == p.first) { *p.second = val; break; } }
	}
}
