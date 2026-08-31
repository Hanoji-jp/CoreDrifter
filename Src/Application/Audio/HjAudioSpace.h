#pragma once

#include <xaudio2fx.h>

//==========================================================
// HjAudioSpace
//   車の音をまとめる「ミキサーと空間」。FMODの考え方を持ち込んだもの。
//
//   ① バス構成
//        エンジン ─┐
//                  ├─→ 車両バス(反射・残響) ─→ マスター
//        タイヤ  ─┘
//      カテゴリごとに音量とエフェクトを一括で扱える。
//      各クラスが個別に音量を持っていると、全体の바ランスが取れない。
//
//   ② 3D(距離減衰・定位・遠くの音の鈍り)
//      音が常に耳元で同じ大きさで鳴っていると、距離と方向の手がかりが無い。
//      現実の音には必ずそれが入っているので、無いと「実在しない音」に聞こえる。
//      ・距離で減衰させる
//      ・左右へ振る(カメラの向きに対してどちらにいるか)
//      ・遠いほど高音が失われる(空気による吸収)
//
//   使い方:
//     Init()                        … 起動時に1回
//     RouteVoice(voice, Bus)        … 音源ボイスをバスへ通す
//     UpdateListener()              … 毎フレーム(カメラから聴取点を取る)
//     ApplySource(voice, worldPos)  … 音源の位置を反映
//==========================================================
class HjAudioSpace
{
public:
	// 音のカテゴリ。FMODのバスにあたる
	enum class Bus
	{
		Engine,
		Tire,
	};

	// 走っている場所。反射の量と残り方が変わる。
	enum class Preset
	{
		OpenRoad,     // 開けた道。反射は弱く短い
		MountainPass, // 峠。片側の斜面で反射が返る
		Tunnel,       // トンネル。長く尾を引く
		City,         // 市街地。建物の反射が多い
	};

	static HjAudioSpace& Instance()
	{
		static HjAudioSpace inst;
		return inst;
	}

	void Init();
	void Release();

	// 音源ボイスの出力先を、指定したバスへ差し替える
	void RouteVoice(IXAudio2SourceVoice* voice, Bus bus);

	// 聴取点をカメラから取る(毎フレーム)
	void UpdateListener();
	// 音源の位置を反映する。距離減衰・定位・遠くの音の鈍りが掛かる
	void ApplySource(IXAudio2SourceVoice* voice, const Math::Vector3& worldPos);

	//===== 設定画面から来る音量 =====
	// バス(サブミックス)に掛けるので、そのバスへ流している音源が
	// 増えても、ここを通れば必ず効く。
	//
	// エンジン音を環境音の側に置いているのは、あれが
	// 「鳴り続けている背景」だから。タイヤの鳴きや衝突音のような
	// 出来事として鳴る音とは、下げたい理由が違う。
	void SetSfxVolume(float v);       // タイヤなど、出来事として鳴る音
	void SetAmbientVolume(float v);   // エンジンなど、鳴り続ける音

	void SetPreset(Preset p);
	void SetWetness(float w);

	void DrawImGui();
	bool IsValid() const { return m_vehicle != nullptr; }

private:
	HjAudioSpace() = default;
	~HjAudioSpace() { Release(); }

	void ApplyReverb();
	IXAudio2SubmixVoice* GetBus(Bus b);

	// バス。車両バスに反射をまとめて掛ける
	IXAudio2SubmixVoice* m_vehicle = nullptr;   // 反射・残響を持つ
	IXAudio2SubmixVoice* m_engine  = nullptr;
	IXAudio2SubmixVoice* m_tire    = nullptr;

	// 聴取点(カメラ)
	Math::Vector3 m_listenPos = Math::Vector3::Zero;
	Math::Vector3 m_listenFwd = Math::Vector3::Backward;
	Math::Vector3 m_listenRight = Math::Vector3::Right;

	Preset m_preset  = Preset::MountainPass;
	float  m_wetness = 0.22f;

	// バスごとの音量。全体のバランスをここで取る
	float m_volEngine = 1.0f;
	float m_volTire   = 1.0f;
	float m_volMaster = 1.0f;
	// 設定画面から来る音量。調整パネルの値とは別に持つ。
	// 一緒にすると、遊ぶ人が下げた値を開発中の調整で上書きしてしまう
	float m_userSfx     = 1.0f;
	float m_userAmbient = 1.0f;

	// バスへ実際の音量を流し込む(調整値 × 設定値)
	void ApplyBusVolumes();

	// 3D
	float m_refDistance = 6.0f;    // この距離までは減衰しない(m)
	float m_maxDistance = 90.0f;   // ここで聞こえなくなる(m)
	float m_panWidth    = 0.8f;    // 左右へ振る強さ(0=中央固定)
	float m_airAbsorb   = 0.7f;    // 遠いほど高音が失われる量

	// ボイスを作ったときのXAudio2エンジン。
	//
	// KdAudioManager::Release() はエンジンごと破棄し、ぶら下がっている
	// ボイスもそこで全て解放される。こちらが持っているのは生ポインタなので、
	// その後に触ると解放済みメモリへのアクセスになって落ちる。
	// 静的オブジェクトの破棄はエンジンの破棄より後に走ることがあるため、
	// デストラクタから解放するだけでは防げない。
	//
	// 毎回「今のエンジンが当時と同じか」を確かめ、違えば
	// ボイスは既に解放済みとみなしてポインタを捨てるだけにする。
	IXAudio2* m_xa = nullptr;
	bool IsVoiceAlive() const;

	HjAudioSpace(const HjAudioSpace&) = delete;
	void operator=(const HjAudioSpace&) = delete;
};
