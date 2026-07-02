#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

// ============================================================
// KdThreadPool
// シンプルなスレッドプール（シングルトン）
//
// 使い方:
//   // ジョブを投げて future で結果を受け取る
//   auto f = KdThreadPool::Instance().Enqueue([]{ DoWork(); });
//   f.get(); // 完了まで待機
//
//   // 複数ジョブを投げてまとめて待つ
//   std::vector<std::future<void>> jobs;
//   for (auto& obj : list)
//       jobs.push_back(KdThreadPool::Instance().Enqueue([&obj]{ obj->ParallelUpdate(); }));
//   for (auto& f : jobs) f.get();
// ============================================================
class KdThreadPool
{
public:
	static KdThreadPool& Instance()
	{
		static KdThreadPool s_instance;
		return s_instance;
	}

	// スレッドプールを初期化（ゲーム起動時に呼ぶ）
	// threadCount = 0 のとき hardware_concurrency - 1 本（メインスレッド分を引く）
	void Init(size_t threadCount = 0)
	{
		if (m_running) { return; }

		if (threadCount == 0)
		{
			const size_t hw = std::thread::hardware_concurrency();
			threadCount = (hw > 1) ? (hw - 1) : 1;
		}
		// 上限 8 本
		threadCount = std::min(threadCount, static_cast<size_t>(8));

		m_running = true;
		m_workers.reserve(threadCount);
		for (size_t i = 0; i < threadCount; ++i)
		{
			m_workers.emplace_back([this]
			{
				while (true)
				{
					std::function<void()> job;
					{
						std::unique_lock<std::mutex> lock(m_mutex);
						m_cv.wait(lock, [this]{ return !m_jobs.empty() || !m_running; });
						if (!m_running && m_jobs.empty()) { return; }
						job = std::move(m_jobs.front());
						m_jobs.pop();
					}
					job();
				}
			});
		}
	}

	// ジョブをキューに積む。std::future<T> で結果/完了を待てる
	template<class F>
	auto Enqueue(F&& func) -> std::future<decltype(func())>
	{
		using RetType = decltype(func());
		auto task = std::make_shared<std::packaged_task<RetType()>>(std::forward<F>(func));
		std::future<RetType> fut = task->get_future();
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_jobs.emplace([task]{ (*task)(); });
		}
		m_cv.notify_one();
		return fut;
	}

	// スレッド数を返す
	size_t ThreadCount() const { return m_workers.size(); }

	// シャットダウン（アプリ終了時に呼ぶ）
	void Release()
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_running = false;
		}
		m_cv.notify_all();
		for (auto& t : m_workers) { if (t.joinable()) { t.join(); } }
		m_workers.clear();
	}

	~KdThreadPool() { Release(); }

private:
	KdThreadPool() = default;
	KdThreadPool(const KdThreadPool&) = delete;
	KdThreadPool& operator=(const KdThreadPool&) = delete;

	std::vector<std::thread>          m_workers;
	std::queue<std::function<void()>> m_jobs;
	std::mutex                        m_mutex;
	std::condition_variable           m_cv;
	bool                              m_running = false;
};
