#pragma once

#include "../../Const/SkidMarkConst.h"

//==========================================================
// SkidMark
//   路面に残るタイヤ痕(スキッドマーク)。
//
//   コース全体を真上から見た1枚のテクスチャ(マークマップ)に痕を「書き溜める」。
//   クリアしないので書いたものはずっと残り、毎フレーム描くのは
//   「新しく増えた数センチぶんの帯」だけで済む。
//   路面側は Lit シェーダーがワールドXZでこのテクスチャを引き、色を暗くする。
//
//   痕をポリゴンとして毎フレーム描き直さないので、
//     ・何本走っても、どれだけ長く残しても負荷が変わらない
//     ・坂やバンクでも路面に完全に密着する(浮き・埋まり・Zファイティングが無い)
//     ・痕の長さに上限がない(1周ぶん残せる)
//
//   使い方(所有者=CarBase):
//     Init()                              … 起動時に1回(マップ生成)
//     Emit(trail, pos, right, strength)   … 毎フレーム、タイヤごとに接地点を渡す
//     Cut(trail)                          … 空中・停止など、痕を途切れさせる時
//     BakePending()                       … 毎フレーム、増えたぶんをマップへ焼く
//     ApplyToShader()                     … 路面を描く直前に呼ぶ
//==========================================================
class SkidMark
{
public:
	// マークマップはコースに1枚。全車がここへ書き込み、路面が1回引くだけで済む。
	// (マルチプレイでも他車の痕が同じ1枚に乗るので、台数が増えても負荷は変わらない)
	static SkidMark& Instance()
	{
		static SkidMark inst;
		return inst;
	}

	void Init();

	// 1台ぶんの痕の枠(後輪2＋前輪2)を確保し、その先頭番号を返す。
	// マップは共有だが「直前に打った点」は車ごとに持つ必要があるため、
	// 車が増えるたびにここで枠を取る。
	int AllocTrails();

	// trail=痕の番号(AllocTrailsで得た先頭番号 + 0..TrailCount-1) / pos=接地点 / right=タイヤの右方向(帯の幅方向)
	// strength=スリップの強さ(0〜1)。SlipMinFor未満なら痕を残さず自動で切る
	void Emit(int trail, const Math::Vector3& pos, const Math::Vector3& right, float strength);
	// 痕を途切れさせる(次の点と線で繋がないようにする)
	void Cut(int trail);

	// 前回から増えたぶんの帯をマークマップへ焼き込む(毎フレーム1回)
	void BakePending();

	// 路面を描く直前に呼ぶ。マップと範囲情報をシェーダーへ渡す
	void ApplyToShader() const;

	void SetColor(const Math::Vector3& c) { m_color = c; }

private:
	// 帯を1区間ぶん(四角形=三角形2枚)、マップ用の頂点列へ積む
	void PushSegment(const Math::Vector3& a, const Math::Vector3& b,
	                 const Math::Vector3& rightA, const Math::Vector3& rightB,
	                 float halfW, float strengthA, float strengthB);

	struct Trail
	{
		Math::Vector3 lastPos   = Math::Vector3::Zero;   // 直前に打った点
		Math::Vector3 lastRight = Math::Vector3::Right;
		float         lastStr   = 0.0f;
		bool          hasLast   = false;   // 直前の点があるか(falseなら痕の始まり)
		float         widthMul  = 1.0f;    // 痕の幅の倍率(前輪は少し細い)
	};

	// カメラを追って窓(マップが覆う範囲)を動かす。動かした時は前の内容をずらして写す
	void Recenter(float camX, float camZ);

	std::vector<Trail>             m_trails;
	std::vector<KdPolygon::Vertex> m_pending;   // 今フレーム焼き込む帯の頂点

	KdRenderTargetPack m_map;      // 痕の蓄積先(カメラ周りを真上から見たもの)
	KdRenderTargetPack m_mapPrev;  // 窓を動かす時に前の内容を写す元(ピンポン用)
	float m_originX = 0.0f;        // 窓の隅のワールドX
	float m_originZ = 0.0f;        // 窓の隅のワールドZ
	bool  m_hasOrigin = false;     // 一度でも窓を置いたか
	bool  m_ready = false;

	Math::Vector3 m_color = { SkidMarkConst::ColorR, SkidMarkConst::ColorG, SkidMarkConst::ColorB };
};
