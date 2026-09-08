#pragma once

#include "../Const/UpdaterConst.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

//==========================================================
// HjUpdater
//   GitHub のリリースを見て、新しければ差し替える。
//   自作の再利用クラスなので Hj 接頭辞。
//
//   ■ なぜ別スレッドでやるか
//   通信は相手の都合で何秒も返ってこない。
//   描画と同じ流れで待つと、その間ゲームが固まる。
//
//   ■ なぜバッチに任せるか
//   動いている exe は掴まれていて、自分で自分を置き換えられない。
//   「閉じるのを待ってから入れ替える」役は、別のプロセスでないと務まらない。
//
//   ■ 状態を atomic で持つ理由
//   描画の流れから毎フレーム見に来る。
//   通信の流れが書き換えている最中に読むので、
//   途中の値が見えない形で持つ必要がある。
//==========================================================
class HjUpdater
{
public:
	enum class State
	{
		Idle,        // 何もしていない
		Checking,    // 問い合わせ中
		Available,   // 新しいものがある
		UpToDate,    // 手元が最新
		Downloading, // 受け取り中
		Ready,       // 受け取った。あとは入れ替えるだけ
		Failed,      // 駄目だった
	};

	static HjUpdater& Instance()
	{
		static HjUpdater inst;
		return inst;
	}

	// 新しいものがあるか聞く(別スレッド)
	void StartCheck();
	// 受け取る(別スレッド)
	void StartDownload();
	// 受け取り中にやめる
	void CancelDownload();

	// 入れ替えて開き直す。
	// ここでゲームは閉じる。あとはバッチが引き継ぐ
	void Apply();

	// 自動で先へ進める。タイトル画面から毎フレーム呼ぶ。
	//
	// ■ なぜタイトルからだけか
	// 入れ替えるとゲームは閉じる。走っている最中に閉じられたら
	// たまったものではない。タイトルに居るときだけ進める。
	//
	// ■ 何をするか
	// 新しいものがあれば落とし始め、
	// 支度ができたら少し待ってから入れ替える
	void AutoStep(float dt);

	// あと何秒で入れ替えるか。0以下なら待っていない。
	// 画面に出すと、いきなり閉じたように見えない
	float GetApplyCountdown() const;

	State GetState()    const { return m_state; }
	float GetProgress() const { return m_progress; }   // 0〜1

	// 文字列は通信の流れが書き換えるので、写して返す。
	// 参照で返すと、読んでいる最中に書き換えられる
	std::string GetLatestVersion() const;
	std::string GetErrorMessage()  const;

	// 手元の版。無ければ v0.0.0
	static std::string GetCurrentVersion();

	// 通信中か。二重に走らせないために見る
	bool IsWorking() const
	{
		return m_state == State::Checking || m_state == State::Downloading;
	}

	// 画面へ出す一言。状態ごとに書き分けると各画面で同じ分岐が増える
	const char* StateText() const;

private:
	HjUpdater() = default;
	~HjUpdater();

	void CheckThread();
	void DownloadThread();

	// 前のスレッドを片付けてから新しく始める。
	// 生成に失敗してもゲームを巻き込まない
	bool Launch(void (HjUpdater::*fn)(), const char* failMsg);

	//===== 通信 =====
	static bool HttpGet(const std::string& host, const std::string& path,
	                    std::string& outBody);
	bool DownloadFile(const std::string& url, const std::string& destPath,
	                  int redirectLeft);

	// リリースの応答から、版と受け取り先を取り出す
	static bool ParseRelease(const std::string& json,
	                         std::string& outVersion, std::string& outUrl);

	// "v1.2.3" を比べる。a が b より新しければ true。
	// 文字列の一致で見ると、古いものへ差し戻したときにも
	// 「更新があります」と出てしまう
	static bool IsNewer(const std::string& a, const std::string& b);

	std::atomic<State> m_state    { State::Idle };
	std::atomic<float> m_progress { 0.0f };
	std::atomic<bool>  m_cancel   { false };

	std::string m_latestVersion;
	std::string m_downloadUrl;
	std::string m_errorMsg;

	// 入れ替えるまでの待ち。知らせを見せておく時間
	float m_applyWait = 0.0f;

	std::thread        m_thread;
	mutable std::mutex m_mutex;

	HjUpdater(const HjUpdater&) = delete;
	void operator=(const HjUpdater&) = delete;
};
