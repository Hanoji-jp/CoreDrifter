#pragma once

#include "../../Const/CarChoiceConst.h"

class CarBase;

//==========================================================
// HjCarPortrait
//   車庫の3D空間。床・回す台・車。
//
//   ■ 画面いっぱいに置く
//   はじめは別の絵(レンダーターゲット)へ描いて、UIの緑の窓へ
//   貼っていた。覗き穴から見る形になり、せっかく作った空間が
//   小さくしか見えない。
//
//   スプライトは3Dより後に描かれるので、画面へ直に描いても
//   UIは上に乗る。窓を切る必要がそもそも無かった。
//
//   ■ カメラと光はここが決める
//   車庫は画面全体がこの空間なので、場面のカメラを奪ってよい。
//   走行中の空とは別に、絵として当てる光を置く
//==========================================================
class HjCarPortrait : public KdGameObject
{
public:
	// 明るさの抽出のしきい値を元へ戻す。
	//
	// 車庫では輪が白へ飛ばないように下げている。
	// 戻さないと、走行の画面まで同じしきい値で滲む
	~HjCarPortrait();
	void Init()    override;
	void Update()  override;

	// 手で回す。車庫の画面が引きずった量を渡す。
	//
	// その場で足さずに溜める。離したときの勢いを出すのに、
	// 「このフレームでどれだけ回されたか」が要る
	void AddYaw(float rad) { m_yawInput += rad; }
	// カメラと光を置く。描くのはこの後
	void PreDraw() override;

	// 影の元になる深度を書く。
	//
	// 影は枠組みの深度マップに任せる。自前で車を床へ潰して
	// 描いていたときは、潰した面どうしが深度を取り合って
	// ちらつき、裏面まで重なって汚れていた
	void GenerateDepthMapFromLight() override;

	// 光るものだけをもう一度描く。
	//
	// 明るさの抽出パス。蛍光灯と回す台の輪だけを出す。
	// ここへ出したものが滲む
	void DrawBright() override;

	// 部屋と車。
	//
	// 影を受けるので、床まわりも光の当たる側で描く
	void DrawLit() override;

	// 見せる車を決める。同じ車なら読み直さない。
	//
	// すぐには差し替えない。カメラを振る演出を挟んで、
	// 一番速く動いている所で入れ替える
	void SetCar(CarChoiceConst::Kind kind);


private:
	// 車を絵の中に納める行列を作る
	void SetupCamera();

	// 車を実際に読み込んで差し替える。
	// 演出の途中から呼ばれるので、SetCar とは分けてある
	void ApplyCar(CarChoiceConst::Kind kind);

	std::shared_ptr<CarBase> m_car;
	CarChoiceConst::Kind     m_kind = CarChoiceConst::Kind::Count;   // 未設定

	KdCamera m_cam;

	// 見せる向き。止まった絵だと模型に見えるのでゆっくり回す
	// 車の向き(rad)。カメラではなく車を回している
	float m_spin = 0.0f;

	// 入れ替えの進み(0〜1)。負なら何も起きていない
	float m_swapT = -1.0f;

	// 入れ替え先。演出の真ん中で m_kind になる
	CarChoiceConst::Kind m_pendingKind = CarChoiceConst::Kind::Count;

	// もう差し替えたか。1回の演出で2度読み込まないため
	bool m_swapped = false;

	// 差し替わった瞬間の明るさ(0〜1)。台の縁と床の筋に乗せる
	float m_flash = 0.0f;

	// 直前に組んだカメラ。接地面の位置を出すのに使う。
	// 0 なら、まだモデルを測れていない
	// 車が立っている場所。床・線・奥の壁・影。
	//
	// 車の大きさに合わせて作るので、車を替えたら作り直す
	void BuildStage();

	// 壁に刷る文字。フォントの絵を板へ貼って並べる
	void BuildWallText(float radius);
	void DrawWallText() const;
	void DrawStage();

	// 車を置く行列。向きと、台の上への持ち上げ
	Math::Matrix CarMatrix() const;

	// 場所を組んだときの車の大きさ(包む球の半径)
	float m_stageRadius = 0.0f;
	// 渡された寸法(メートル)に掛ける比。
	//
	// 車の包む球を測って、実車ならこれくらいという値で割ったもの。
	// 1 なら渡された寸法そのまま
	float m_scale = 1.0f;

	//===== 部屋 =====
	std::vector<KdPolygon::Vertex> m_floor;
	std::vector<KdPolygon::Vertex> m_wall;

	//===== 奥の壁に付くもの =====
	std::vector<KdPolygon::Vertex> m_slat;     // シャッターの羽根
	std::vector<KdPolygon::Vertex> m_slatGap;  // 羽根の隙間(奥の面)
	std::vector<KdPolygon::Vertex> m_frame;    // シャッターと通用口の枠
	std::vector<KdPolygon::Vertex> m_doorIn;   // 引っ込めた通用口の面
	std::vector<KdPolygon::Vertex> m_tube;     // 蛍光灯

	//===== 床に付くもの =====
	std::vector<KdPolygon::Vertex> m_line;     // 駐車枠の白線
	std::vector<KdPolygon::Vertex> m_stop;     // 車止め
	std::vector<KdPolygon::Vertex> m_disc;     // 回す台の円盤
	std::vector<KdPolygon::Vertex> m_ring;     // 縁の光る輪

	// 壁の文字。1文字ぶんの板と、壁の上での位置
	struct WallGlyph
	{
		std::shared_ptr<KdSquarePolygon> poly;
		Math::Vector3 pos;
	};

	std::vector<WallGlyph> m_wallText;

	// 既定の向きを入れたか。
	// 毎フレーム入れ直すと、手で回しても戻ってしまう
	bool m_yawInit = false;

	// このフレームに手で回された量(rad)。Update で使って空にする
	float m_yawInput = 0.0f;

	// 手を離した後に残る勢い(rad/秒)。
	// 0.4秒かけて0へ落として、ひとりでに回る速さへ戻す
	float m_extraVel = 0.0f;
};
