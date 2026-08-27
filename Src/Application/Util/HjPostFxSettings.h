#pragma once

//==========================================================
// HjPostFxSettings
//   画面演出(ポストプロセス)の調整値を保存・復元する。
//
//   ■ なぜ別に持つのか
//   これらの値はシェーダー側が持っていて、車やステージのものではない。
//   車の調整ファイルへ混ぜると、車種を変えたときに画面の見た目まで
//   変わってしまう。画面の設定は車種に依らないので別のファイルにする。
//
//   ■ 触った時点で保存する
//   調整パネルで直したのに、次に起動すると戻っている——というのが
//   一番よくある取りこぼし。保存ボタンを押させるのではなく、
//   触り終わった時点で自動的に書き出す。
//
//   使い方:
//     起動時          Load()
//     パネルの最後で  NotifyChanged() … 値を触ったフレームで呼ぶ
//                     Update()        … 触り終わっていれば保存する
//==========================================================
class HjPostFxSettings
{
public:
	static HjPostFxSettings& Instance()
	{
		static HjPostFxSettings inst;
		return inst;
	}

	// ファイルから読み込んでシェーダーへ反映する。起動時に1回
	void Load();
	// 今の値をファイルへ書き出す
	void Save() const;

	// 調整パネルで値が変わったことを伝える
	void NotifyChanged() { m_dirty = true; }
	// 触り終わっていれば保存する。毎フレーム呼んでよい
	void Update();

private:
	HjPostFxSettings() = default;
	HjPostFxSettings(const HjPostFxSettings&) = delete;
	void operator=(const HjPostFxSettings&) = delete;

	// 保存対象(名前→値の場所)。SaveとLoadが同じ一覧を使うので、
	// 項目を足すときにどちらかを直し忘れることがない
	std::vector<std::pair<const char*, float*>> ParamList();

	// ドラッグ中は毎フレーム値が変わる。そのたびに書き出すと無駄なので、
	// 手を離してから1回だけ書く
	bool m_dirty = false;
};
