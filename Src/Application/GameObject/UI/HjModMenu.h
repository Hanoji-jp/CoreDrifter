#pragma once

#include "../../Const/ModMenuConst.h"
#include "../../Mod/HjModLoader.h"

class CarBase;

//==========================================================
// HjModMenu
//   走行中に TAB で開く、見た目の差し替えメニュー。自作なので Hj 接頭辞。
//
//   ■ なぜ専用の画面にするか
//   差し替えは「選んで終わり」ではない。外から持ってきたモデルは
//   向きも大きさもバラバラで、置いた直後は車の形にならない。
//   選ぶ → 車を見る → 直す、を何度も繰り返すことになる。
//
//   開発用のパネル(F2)でもできるが、あれはマウスが要る。
//   走りながら片手で合わせられないと、走行中の見え方が確かめられない。
//
//   ■ 階層で持つ理由
//   1枚に全部並べると、モデルの候補と調整値が混ざって長くなる。
//   細いパネルに収めたいので、種類ごとに潜る形にした。
//==========================================================
class HjModMenu : public KdGameObject
{
public:
	void Init() override;
	void Update() override;
	void DrawSprite() override;

	// 触る対象の車。持ち主は場面の側なので、こちらは借りるだけ
	void SetCar(const std::weak_ptr<CarBase>& car) { m_wpCar = car; }

	// 開いているか。開いている間は車の操作を止めたいので、外から見る
	bool IsOpen() const { return m_open; }

private:
	// いまどの階層にいるか
	enum class Page
	{
		Root,        // 入口
		BodyList,    // 車体のモデル候補
		WheelList,   // ホイールのモデル候補
		Adjust,      // 向き・大きさ・位置合わせ
	};

	// 1行の種類。
	// 種類で描き方と、決定・左右を押したときの動きが変わる
	enum class RowKind
	{
		Submenu,   // 潜る
		Choice,    // モデルを選ぶ
		Slider,    // 数値を左右で動かす
		Action,    // その場で何かする
	};

	// Action の行が何をするか。
	// ラベルの文字で分岐させると、文言を直した瞬間に動かなくなる
	enum class ActionKind
	{
		None,
		Rescan,     // 一覧を読み直す
		SaveProf,   // いまの合わせ込みを書き出す
		RevertProf, // 書き出した所まで戻す
		ResetProf,  // 合わせ込みを捨てて、置いたままの状態へ
	};

	// 画面に並べる1行。
	// 毎フレーム組み直す。状態を持たないので、
	// 候補が増減しても表示と中身がずれない
	struct Row
	{
		RowKind     kind = RowKind::Action;
		std::string label;
		std::string value;      // 右へ出す文字(選択中のモデル名・数値)

		// 潜る先(Submenu のとき)
		Page        to = Page::Root;

		// 動かす対象(Slider のとき)。
		// 持ち主は車なので、ここは借りるだけ。
		// 車の調整値一覧が同じ形なので、作法を合わせている
		float*      target = nullptr;
		float       min = 0.0f, max = 1.0f, step = 0.01f;

		// 候補の何番目か(Choice のとき)。-1 は「標準へ戻す」
		int         choice = -1;

		// Action のとき何をするか
		ActionKind act = ActionKind::None;
	};

	// いまの階層の行を組む
	void BuildRows();
	void BuildRoot(std::shared_ptr<CarBase>& car);
	void BuildList(std::shared_ptr<CarBase>& car, bool body);
	void BuildAdjust(std::shared_ptr<CarBase>& car);

	// 入力
	void UpdateInput();
	void Decide();                 // 決定(潜る・選ぶ・実行する)
	void StepValue(int dir);       // 左右(数値を動かす)
	void GoBack();                 // 一段戻る

	//===== モデルごとの合わせ込み =====
	// 車の調整(CarTune)とは別に持つ。
	// あちらへ書くと、モデルを替えるたびに前の値が消え、
	// 標準へ戻したときには標準の値まで壊れる
	void ApplyProfile(const std::string& path);   // 記録 → 車へ
	void CaptureProfile();                        // 車 → 記録(手元だけ)
	void SaveProfile();                           // 記録 → ファイル
	void RevertProfile();                         // ファイル → 車

	// 合わせ込みの鍵。標準のときは記録しない(車の調整が受け持つ)
	std::string ProfileKey() const;

	// 押しっぱなしを一定の間隔で拾う。
	// 押した瞬間だけだと、100倍のモデルを合わせるのに時間がかかりすぎる
	bool Repeat(int vk, float& timer, bool pressed, bool down);

	// いまいる階層の名前
	const char* PageTitle() const;

	std::weak_ptr<CarBase> m_wpCar;

	std::vector<Row> m_rows;
	Page m_page  = Page::Root;
	int  m_index = 0;    // 選んでいる行
	int  m_top   = 0;    // 送っている先頭の行

	bool  m_open  = false;
	float m_slide = 0.0f;   // 開閉の進み(0=閉, 1=開)

	// 左右の押しっぱなし用
	float m_holdL = 0.0f;
	float m_holdR = 0.0f;

	// 直前に読み込めなかった理由。次に何か選ぶまで出しておく
	HjModLoader::Result m_last = HjModLoader::Result::Ok;
};
