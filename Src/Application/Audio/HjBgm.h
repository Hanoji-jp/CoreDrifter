#pragma once

//==========================================================
// HjBgm
//   メニュー画面のBGM。
//
//   ■ 画面をまたいで鳴らし続ける
//   タイトル→設定→ルームと移っても曲は切らない。
//   画面ごとに鳴らし直すと、そのたびに頭から始まって
//   「別のゲームに移った」ように感じる。
//   走行画面へ入ったときだけ止める。
//
//   ■ 走行中は鳴らさない
//   エンジン音とタイヤの音で既に情報が多く、そこへ曲を重ねると
//   回転の上がり方も滑り出しも聞き取れなくなる。
//   このゲームは音で車の状態を読ませる作りなので、走行中は空ける。
//   (ポーズ中も走行画面の一部なので鳴らさない)
//
//   ■ 音量で出し入れする
//   停止と再生を繰り返すのではなく、音量を上げ下げする。
//   途中まで進んだ位置を保ったまま戻れるので、
//   走行を終えてメニューへ戻ったときに続きから聞こえる。
//
//   使い方(SceneManagerから):
//     SetPlaying(鳴らすか)  … 画面が切り替わったとき
//     Update(dt)            … 毎フレーム
//==========================================================
class HjBgm
{
public:
	static HjBgm& Instance()
	{
		static HjBgm inst;
		return inst;
	}

	// 鳴らすかどうかを伝える。
	// 初めて true を渡されたときに読み込みと再生を始める
	void SetPlaying(bool play);

	// 音量の出し入れを進める
	void Update(float dt);

	// 完全に止める(アプリ終了時など)
	void Stop();

	//===== 遊ぶ人が触る操作 =====
	// 手で止める/再開する。
	// 画面の都合で止まっているのとは別に持つ。
	// 一緒にすると、走行から戻ったときに勝手に鳴り出して
	// 「止めたはずなのに」となる
	void SetMuted(bool muted) { m_muted = muted; }
	bool IsMuted() const { return m_muted; }
	void ToggleMuted() { m_muted = !m_muted; }

	// 今実際に鳴っているか(画面が許していて、かつ止められていない)。
	// タイトルの再生マークと波形の動きに使う
	bool IsSounding() const { return m_wantPlay && !m_muted && m_gain > 0.01f; }

	// 表示用の曲名
	const char* GetTitle()  const;
	const char* GetArtist() const;

	// 調整パネル
	void DrawImGui();

private:
	HjBgm() = default;
	~HjBgm() { Stop(); }
	HjBgm(const HjBgm&) = delete;
	void operator=(const HjBgm&) = delete;

	KdBgmVoice m_voice;

	// 鳴らしたいか。ここへ向けて音量が動く
	bool  m_wantPlay = false;
	// 遊ぶ人が手で止めているか。画面の都合とは別に持つ
	bool  m_muted = false;
	// 今の音量(0〜1の割合。実際の音量はこれに設定値を掛けたもの)
	float m_gain = 0.0f;
	// 読み込み済みか。ファイルを開くのは最初の1回だけ
	bool  m_loaded = false;

	// 調整パネルから触る音量
	float m_volume = 0.0f;
};
