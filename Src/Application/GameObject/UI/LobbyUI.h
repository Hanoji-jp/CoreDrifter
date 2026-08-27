#pragma once

#include "MultiUIBase.h"

//==========================================================
// LobbyUI
//   部屋の一覧。
//
//   ■ 今は一覧に出るものが無い
//   直接IPで繋ぐ方式なので、他人が立てた部屋を探す手段が無い。
//   マッチングサーバーを入れたときにここが埋まる。
//   それまでは空であることをはっきり出して、代わりに何ができるかを示す。
//   偽の行を並べると、押しても入れない部屋がずっと居座ることになる。
//==========================================================
class LobbyUI : public MultiUIBase
{
public:
	// 一覧に出す部屋。マッチングが入るまでは空のまま
	struct Room
	{
		const char* name    = "";
		const char* mode    = "";
		const char* track   = "";
		int         players = 0;
		int         capacity = 0;
		int         pingMs  = 0;
	};

	void Update()     override;
	void DrawSprite() override;

	// 一覧を差し替える(通信側が持ってきた結果を入れる)
	void SetRooms(const std::vector<Room>& rooms) { m_rooms = rooms; }

	// 押された操作(1回だけ真)
	bool ConsumeQuickJoin() { const bool a = m_quickJoin; m_quickJoin = false; return a; }
	bool ConsumeCreate()    { const bool a = m_create;    m_create    = false; return a; }
	bool ConsumeBack()      { const bool a = m_back;      m_back      = false; return a; }

private:
	void DrawFilters();
	void DrawTable();
	void DrawEmpty(float tableTop);
	void DrawFooter(float tableBottom);

	std::vector<Room> m_rooms;
	int  m_selected = 0;
	int  m_filter   = 0;

	bool m_quickJoin = false;
	bool m_create    = false;
	bool m_back      = false;
};
