#pragma once

//============================================================
// アプリケーションのFPS制御 + 測定
//============================================================
struct KdFPSController
{
	// FPS制御
	int		m_nowfps = 0;		// 現在のFPS値
	int		m_maxFps = 60;		// 最大FPS

	void Init();

	void UpdateStartTime();

	void Update();

	float GetDeltaTime() const { return m_DeltaTime; }

	// どこからでも呼び出せる静的デルタタイム取得関数
	static float GetDt() { return s_deltaTime; }

private:

	void Control();

	void Monitoring();

	DWORD		m_frameStartTime = 0;		// フレームの開始時間

	int			m_fpsCnt = 0;				// FPS計測用カウント
	DWORD		m_fpsMonitorBeginTime = 0;	// FPS計測開始時間

	float m_DeltaTime = 0;  // デルタタイム

	static float s_deltaTime;  // グローバル共有用静的デルタタイム

	const int	kSecond = 1000;				// １秒のミリ秒
};
