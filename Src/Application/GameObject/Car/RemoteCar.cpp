#include "RemoteCar.h"
#include "../UI/HjUI.h"
#include "../../Const/NamePlateConst.h"

void RemoteCar::Init()
{
	Silvia::Init();

	// 音は鳴らさない。
	// 他人の車ぶんまで自前で音を作ると、台数だけエンジンが増えて
	// 何を聞いているのか分からなくなる。距離で小さくするだけでは足りない。
	// (鳴らすなら、位置に応じた音量と定位を入れたうえで別途調整が要る)
	StopAudio();
}

//----------------------------------------------------------
// 届いた状態を積む。
//----------------------------------------------------------
void RemoteCar::PushState(const HjNetStatePacket& state)
{
	Sample s;
	s.time  = m_time;   // 送り主の時刻ではなく、受け取った自分の時刻で並べる
	s.state = state;
	m_history.push_back(s);

	// 古いものから捨てる。描くのに使うのは直近の数個だけなので、
	// 残し続けても増えるだけで得が無い
	while (static_cast<int>(m_history.size()) > NetConst::HistoryMax)
	{
		m_history.erase(m_history.begin());
	}

	m_hasState = true;
}

//----------------------------------------------------------
// 角度の補間。近いほうの回り方を選ぶ。
//----------------------------------------------------------
float RemoteCar::LerpAngle(float a, float b, float t)
{
	constexpr float Pi    = 3.14159265f;
	constexpr float TwoPi = Pi * 2.0f;

	float d = b - a;
	// 差を -π〜+π に収める。こうすると必ず近いほうを回る
	while (d >  Pi) { d -= TwoPi; }
	while (d < -Pi) { d += TwoPi; }

	return a + d * t;
}

//----------------------------------------------------------
// 指定した時刻の状態を作る。
//   ・その時刻を挟む2つがあれば、その間を繋ぐ
//   ・一番古いものより前なら、一番古いものをそのまま使う
//   ・一番新しいものより後なら、速度で少しだけ伸ばす
//----------------------------------------------------------
bool RemoteCar::SampleAt(float renderTime, HjNetStatePacket& out) const
{
	if (m_history.empty()) { return false; }

	// まだ届き始めたばかりで、描くべき時刻が最初の状態より前。
	// 補間する材料が無いので、持っているものをそのまま置く
	if (renderTime <= m_history.front().time)
	{
		out = m_history.front().state;
		return true;
	}

	// 挟んでいる2つを探す
	for (size_t i = 1; i < m_history.size(); ++i)
	{
		const Sample& a = m_history[i - 1];
		const Sample& b = m_history[i];
		if (renderTime > b.time) { continue; }

		const float span = b.time - a.time;
		// 同時刻に2つ届いた場合は割り算ができないので、新しいほうを使う
		if (span <= 1e-6f) { out = b.state; return true; }

		const float t = std::clamp((renderTime - a.time) / span, 0.0f, 1.0f);

		out = b.state;   // 数値以外(番号・フラグ)は新しいほうに合わせる
		out.px = a.state.px + (b.state.px - a.state.px) * t;
		out.py = a.state.py + (b.state.py - a.state.py) * t;
		out.pz = a.state.pz + (b.state.pz - a.state.pz) * t;
		out.vx = a.state.vx + (b.state.vx - a.state.vx) * t;
		out.vy = a.state.vy + (b.state.vy - a.state.vy) * t;
		out.vz = a.state.vz + (b.state.vz - a.state.vz) * t;
		out.steer = a.state.steer + (b.state.steer - a.state.steer) * t;
		out.yaw   = LerpAngle(a.state.yaw, b.state.yaw, t);

		// 傾きも繋ぐ。ここが飛ぶと車体がガクッと揺れて目立つ。
		// 傾きは小さい範囲しか動かないが、向きと同じ扱いにしておけば
		// 繋ぎ目で逆へ回ることが無い
		out.terrainPitch = LerpAngle(a.state.terrainPitch, b.state.terrainPitch, t);
		out.terrainRoll  = LerpAngle(a.state.terrainRoll,  b.state.terrainRoll,  t);
		out.bodyPitch    = LerpAngle(a.state.bodyPitch,    b.state.bodyPitch,    t);
		out.bodyRoll     = LerpAngle(a.state.bodyRoll,     b.state.bodyRoll,     t);
		return true;
	}

	// 最新より先。パケットが途切れている。
	// その場で止めるとカクッと止まって、復帰した瞬間に飛ぶので、
	// 速度の向きへ少しだけ伸ばして繋ぐ。
	// 伸ばしすぎると壁を抜けた位置を描くので、上限を決めてある。
	const Sample& last = m_history.back();
	out = last.state;

	const float ahead = std::min(renderTime - last.time, NetConst::ExtrapolateMax);
	out.px += last.state.vx * ahead;
	out.py += last.state.vy * ahead;
	out.pz += last.state.vz * ahead;
	return true;
}

//----------------------------------------------------------
// 見た目のタイヤの回転。速度から進める。
//----------------------------------------------------------
void RemoteCar::SpinWheels(float dt, float speed, bool handbrake)
{
	const float radius = std::max(m_wheelH, 0.01f);
	// 進んだ距離 ÷ 半径 ＝ 回った角度
	const float turn = (speed / radius) * dt;

	m_spinFront += turn;
	// サイドブレーキ中は後輪がロックして回らない。
	// ここを回してしまうと、止まっているのに転がって見える
	if (!handbrake) { m_spinRear += turn; }
}

//----------------------------------------------------------
// 毎フレーム。物理は回さず、届いた答えを繋いで置くだけ。
//----------------------------------------------------------
void RemoteCar::Update()
{
	const float dt = KdFPSController::GetDt();
	if (dt <= 0.0f) { return; }

	m_time += dt;

	if (!m_hasState) { return; }

	// わざと少し過去を描く。
	// こうすると手元に必ず「次の位置」があるので、間を繋ぐだけで済む
	const float renderTime = m_time - NetConst::InterpDelay;

	HjNetStatePacket s;
	if (!SampleAt(renderTime, s)) { return; }

	const Math::Vector3 pos(s.px, s.py, s.pz);
	const Math::Vector3 vel(s.vx, s.vy, s.vz);
	const bool handbrake = (s.flags & HjNetFlag::Handbrake) != 0;

	// 先に回してから渡す。渡したあとに回すと、常に1フレーム古い角度が描かれる
	SpinWheels(dt, vel.Length(), handbrake);
	ApplyVisualState(pos, s.yaw, vel, s.steer, m_spinFront, m_spinRear);

	// 傾きは送り主が出した答えをそのまま使う。
	// ここで UpdateSuspensionVisual を呼んでも、加速度を持っていないので
	// 常に水平のままになる。地形の傾きに至っては、レイを飛ばしていないので
	// そもそも求まらない。
	ApplyVisualTilt(s.terrainPitch, s.terrainRoll, s.bodyPitch, s.bodyRoll);

	// 煙とタイヤ痕。送られてきた滑り量をそのまま使う
	const bool onGround = (s.flags & HjNetFlag::OnGround) != 0;
	EmitTireFx(dt, vel.Length(),
	           s.slipRear / 255.0f, s.slipFront / 255.0f, onGround);

	// 粒の寿命と移動。自分の車は UpdateMotionFeedback の最後で回している
	UpdateEffectParticles(dt);
}

//----------------------------------------------------------
// ワールド座標を、UIが使うデザイン座標へ直す。
//
// シェーダーへ渡しているカメラ行列を借りて、自分で変換する。
// 画面に出す位置は毎フレーム変わるので、描く直前に求めるしかない。
//----------------------------------------------------------
bool RemoteCar::WorldToDesign(const Math::Vector3& world, float& outX, float& outY)
{
	const auto& cam = KdShaderManager::Instance().GetCameraCB();

	// ワールド → カメラから見た位置 → 画面へ潰した位置
	Math::Vector4 clip = Math::Vector4(world.x, world.y, world.z, 1.0f);
	clip = Math::Vector4::Transform(clip, cam.mView);
	clip = Math::Vector4::Transform(clip, cam.mProj);

	// wが0以下＝カメラの後ろ。割ると符号が反転して、
	// 後ろにあるものが画面の反対側に出てしまう
	if (clip.w <= 0.0001f) { return false; }

	// -1〜+1 の範囲へ収める
	const float ndcX = clip.x / clip.w;
	const float ndcY = clip.y / clip.w;

	// デザイン座標(左上原点)へ。Yは上下が逆
	outX = (ndcX * 0.5f + 0.5f) * UIConst::DesignW;
	outY = (1.0f - (ndcY * 0.5f + 0.5f)) * UIConst::DesignH;
	return true;
}

//----------------------------------------------------------
// 名前札。
// 3Dの板ではなく2Dで描く(向きで潰れず、距離でも読める)。
//----------------------------------------------------------
void RemoteCar::DrawSprite()
{
	if (!m_hasState || m_playerName.empty()) { return; }

	namespace NP = NamePlateConst;

	// カメラからの距離。遠い相手は出さない
	const auto& cam = KdShaderManager::Instance().GetCameraCB();
	const Math::Vector3 camPos = cam.CamPos;
	const float dist = (GetPos() - camPos).Length();
	if (dist > NP::MaxDistance) { return; }

	// 車の少し上に出す
	const Math::Vector3 anchor = GetPos() + Math::Vector3(0.0f, NP::HeightOffset, 0.0f);

	float cx = 0.0f, cy = 0.0f;
	if (!WorldToDesign(anchor, cx, cy)) { return; }

	// 遠いほど薄く。急に消えると、走っているだけで名前が点滅する
	float alpha = 1.0f;
	if (dist > NP::FadeStart)
	{
		const float t = (dist - NP::FadeStart) / std::max(NP::MaxDistance - NP::FadeStart, 0.01f);
		alpha = std::clamp(1.0f - t, 0.0f, 1.0f);
	}

	const char* name = m_playerName.c_str();
	const float textW = HjUI::Measure(NP::FontId, name, 0.0f);
	const float textH = UIConst::FontPx(NP::FontId);

	const float plateW = textW + NP::PadX * 2.0f;
	const float plateH = textH + NP::PadY * 2.0f;
	const float plateX = cx - plateW * 0.5f;
	const float plateY = cy - plateH;   // 指し先が車になるよう、上へ積む

	// 後ろの板。明るい路面や空の上でも読めるようにする
	HjUI::RectTL(plateX, plateY, plateW, plateH,
	             Math::Color(0.0f, 0.0f, 0.0f, NP::PlateAlpha * alpha));

	// どの車のものかを指す小さな三角(細い矩形2枚で代用せず、幅を詰めた板1枚)
	HjUI::RectTL(cx - NP::PointerW * 0.5f, plateY + plateH, NP::PointerW, NP::PointerH,
	             Math::Color(0.0f, 0.0f, 0.0f, NP::PlateAlpha * alpha));

	HjUI::TextAtC(NP::FontId, cx, plateY + NP::PadY, name,
	              Math::Color(1.0f, 1.0f, 1.0f, alpha));
}
