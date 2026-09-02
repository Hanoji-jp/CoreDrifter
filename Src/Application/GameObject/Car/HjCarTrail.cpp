#include "HjCarTrail.h"

namespace CH = ChaseConst;

namespace
{
	// 角度を繋ぐ。近いほうの回り方を選ぶ。
	// 179度と-179度を素直に混ぜると、反対側を1周することになる
	float LerpDeg(float a, float b, float t)
	{
		float d = b - a;
		while (d >  180.0f) { d -= 360.0f; }
		while (d < -180.0f) { d += 360.0f; }
		return a + d * t;
	}

	float LerpRad(float a, float b, float t)
	{
		constexpr float Pi    = 3.14159265f;
		constexpr float TwoPi = Pi * 2.0f;

		float d = b - a;
		while (d >  Pi) { d -= TwoPi; }
		while (d < -Pi) { d += TwoPi; }
		return a + d * t;
	}
}

//----------------------------------------------------------
void HjCarTrail::Push(float now, const Point& p)
{
	// 間引く。毎フレーム入れても、間は繋げば足りる
	if (m_lastPush >= 0.0f && (now - m_lastPush) < CH::TrailStep) { return; }
	m_lastPush = now;

	Point q = p;
	q.time  = now;
	m_points.push_back(q);

	// 古いものを捨てる。
	// 追う遅れより十分に長く持っていれば、それ以上は使い道がない
	const float cut = now - CH::TrailSec;
	size_t drop = 0;
	while (drop < m_points.size() && m_points[drop].time < cut) { ++drop; }

	// 1つは残す。全部消すと、次のフレームで手本が無くなる
	if (drop > 0 && drop < m_points.size())
	{
		m_points.erase(m_points.begin(), m_points.begin() + drop);
	}
}

//----------------------------------------------------------
float HjCarTrail::NewestTime() const
{
	return m_points.empty() ? 0.0f : m_points.back().time;
}

//----------------------------------------------------------
bool HjCarTrail::SampleAt(float time, Point& out) const
{
	if (m_points.empty()) { return false; }

	// 覚えている範囲より前。まだ溜まっていないので、一番古いものを使う
	if (time <= m_points.front().time) { out = m_points.front(); return true; }

	// 範囲より後。まだそこまで走っていないので、一番新しいものを使う。
	// ここで速度を使って伸ばすと、相手がまだ行っていない場所を
	// 目標にすることになり、壁の中を狙いかねない
	if (time >= m_points.back().time) { out = m_points.back(); return true; }

	for (size_t i = 1; i < m_points.size(); ++i)
	{
		const Point& a = m_points[i - 1];
		const Point& b = m_points[i];
		if (time > b.time) { continue; }

		const float span = b.time - a.time;
		if (span <= 1e-6f) { out = b; return true; }

		const float t = std::clamp((time - a.time) / span, 0.0f, 1.0f);

		out = b;   // 真偽値は新しいほうに合わせる
		out.time = time;
		out.pos  = Math::Vector3::Lerp(a.pos, b.pos, t);
		out.vel  = Math::Vector3::Lerp(a.vel, b.vel, t);
		out.yaw      = LerpRad(a.yaw, b.yaw, t);
		out.driftDeg = LerpDeg(a.driftDeg, b.driftDeg, t);
		out.throttle = a.throttle + (b.throttle - a.throttle) * t;
		out.steer    = a.steer    + (b.steer    - a.steer)    * t;
		return true;
	}

	out = m_points.back();
	return true;
}

//----------------------------------------------------------
bool HjCarTrail::NearestTo(const Math::Vector3& pos, Point& out, float& outDist) const
{
	if (m_points.empty()) { return false; }

	// 総当たり。数百点しかないので、木にまとめる必要はない
	float best = 1e18f;
	size_t bestI = 0;

	for (size_t i = 0; i < m_points.size(); ++i)
	{
		const Math::Vector3 d = m_points[i].pos - pos;
		const float d2 = d.LengthSquared();
		if (d2 < best) { best = d2; bestI = i; }
	}

	out     = m_points[bestI];
	outDist = sqrtf(best);
	return true;
}
