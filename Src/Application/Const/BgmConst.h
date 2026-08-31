#pragma once

// メニューで流すBGMの定数。
//
// ■ 走行中は流さない
// エンジン音とタイヤの音で既に情報が多く、そこへ曲を重ねると
// 回転の上がり方も滑り出しも聞き取れなくなる。
// このゲームは音で車の状態を読ませる作りなので、走行中は空けておく。
//
// ■ ポーズ中も鳴らさない
// ポーズは走行画面の一部なので、そこだけ曲が始まると
// 「別の画面に来た」ように感じてしまう。
namespace BgmConst
{
	// メニューで流す曲
	constexpr const char* MenuPath = "Asset/Audio/VOIA - Mochi Moshi (feat. Keii) (Arthur & Medic Remix).mp3";

	// タイトル画面の NOW PLAYING に出す表示。
	// ファイル名から切り出すこともできるが、書き方が曲によって違うので
	// 取り違える。表に出す文字は明示しておく
	constexpr const char* MenuTitle  = "MOCHI MOSHI";
	constexpr const char* MenuArtist = "VOIA (ARTHUR & MEDIC RMX)";

	// 音量(0〜1)。エンジン音より控えめにする
	constexpr float Volume = 0.45f;

	// 音量の上げ下げにかける時間(秒)。
	// 急に鳴らすと驚くし、急に切ると場面が途切れて聞こえる。
	// 画面の切り替え(パネルワイプ)より少し長くすると、
	// 曲が先に消えて画面だけ残る、という間が生まれない
	constexpr float FadeInTime  = 0.8f;
	constexpr float FadeOutTime = 0.5f;
}
