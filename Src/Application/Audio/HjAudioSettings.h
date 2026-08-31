#pragma once

//==========================================================
// HjAudioSettings
//   音量の設定。設定画面のAUDIOタブが触る値。
//
//   ■ なぜ音源側で持たないのか
//   音を出しているのはエンジン・タイヤ・BGMとバラバラで、
//   それぞれが自分の音量を持っていると、設定画面から全部へ
//   配って回ることになる。増やすたびに配り先が増える。
//   値はここ1か所に置いて、鳴らす側が見に来る形にする。
//
//   ■ 種類の分け方
//   「どういう鳴り方をする音か」で分ける。
//     効果音 … 出来事として鳴る音。タイヤの鳴き、衝突、UIの音
//     音楽   … メニューのBGM
//     環境音 … 鳴り続ける背景。エンジン音、風、遠くの街
//
//   エンジン音を環境音の側に置いているのは、あれが止まらずに
//   鳴り続ける音だから。うるさいと感じて下げたくなる理由が、
//   タイヤの鳴きとは違う。
//   「エンジンだけ、タイヤだけ」という細かい調整は
//   遊ぶ人が求めていない(調整パネルの仕事)。
//==========================================================
class HjAudioSettings
{
public:
	static HjAudioSettings& Instance()
	{
		static HjAudioSettings inst;
		return inst;
	}

	// 起動時に1回。無ければ既定値のまま
	void Load();
	void Save() const;

	//===== 0〜1 =====
	float GetSfx()     const { return m_sfx; }
	float GetMusic()   const { return m_music; }
	float GetAmbient() const { return m_ambient; }

	// 設定画面から呼ぶ。触った時点で保存する
	void SetSfx(float v);
	void SetMusic(float v);
	void SetAmbient(float v);

private:
	HjAudioSettings() = default;
	HjAudioSettings(const HjAudioSettings&) = delete;
	void operator=(const HjAudioSettings&) = delete;

	// 保存対象(名前→値の場所)。SaveとLoadが同じ一覧を使うので、
	// 項目を足すときにどちらかを直し忘れることがない
	std::vector<std::pair<const char*, float*>> ParamList();

	float m_sfx     = 1.0f;
	float m_music   = 1.0f;
	float m_ambient = 1.0f;
};
