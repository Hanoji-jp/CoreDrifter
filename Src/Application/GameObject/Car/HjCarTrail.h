#pragma once

#include "../../Const/ChaseConst.h"

//==========================================================
// HjCarTrail
//   車の通ってきた跡。自作なので Hj 接頭辞。
//
//   ■ 何のためか
//   追走で後追いのCPUが追いかける「手本」になる。
//
//   追走の目標は録画では代用できない。前を走るのが人なら、
//   どこを走るかは毎回変わるので、その場で溜めるしかない。
//
//   ■ 操作も一緒に覚える
//   位置と向きだけでは、CPUは釣り合う踏み量を自分で探すことになる。
//   前を走っている人が既に正解を出しているので、それを借りる。
//   (一人用なので同じプログラムの中にいて、そのまま読める)
//==========================================================
class HjCarTrail
{
public:
	// 1点。手本として要るものだけ持つ
	struct Point
	{
		float time = 0.0f;

		Math::Vector3 pos = Math::Vector3::Zero;
		Math::Vector3 vel = Math::Vector3::Zero;
		float yaw       = 0.0f;
		float driftDeg  = 0.0f;   // 横滑り角(度)。符号付き

		// この地点で相手が出していた操作
		float throttle  = 0.0f;
		float steer     = 0.0f;
		bool  handbrake = false;
	};

	// 毎フレーム呼ぶ。間隔を空けて間引く
	void Push(float now, const Point& p);

	// 指定した時刻の点を作る。間は繋ぐ。
	// 覚えている範囲の外なら false
	bool SampleAt(float time, Point& out) const;

	// 一番近い点。押し出されたときに戻る先を探す。
	//
	// 時刻で追うと、押し出された瞬間に目標が先へ逃げていき、
	// CPUは追いつこうとして全開になる。
	// 位置で探せば、遅れは遅れとして受け入れられる
	bool NearestTo(const Math::Vector3& pos, Point& out, float& outDist) const;

	bool  Empty() const { return m_points.empty(); }
	float NewestTime() const;
	void  Clear() { m_points.clear(); m_lastPush = -1.0f; }

private:
	std::vector<Point> m_points;
	float m_lastPush = -1.0f;
};
