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
	// 送り主の時計へ直す。
	//
	// 受け取った時刻で並べてはいけない。送り主は一定の間隔で
	// 送っているのに、届く間隔は回線の都合でばらつく。
	// 受信時刻で並べると、そのばらつきがそのまま点の間隔になり、
	// 区間ごとに速さが変わって見える。
	//
	// 連番は毎秒 SendRate 回ぶん増えるので、そこから送信時刻が出る
	const float senderTime =
		static_cast<float>(state.seq) / std::max(NetConst::SendRate, 1.0f);

	// 自分の時間軸へ移すためのずれ
	const float ideal = m_time - senderTime;

	if (!m_clockSet)
	{
		m_clockOffset = ideal;
		m_clockSet    = true;
	}
	else if (fabsf(ideal - m_clockOffset) > NetConst::ClockResync)
	{
		// 相手が入り直して連番が振り直された、または長く止まっていた。
		// 少しずつ寄せていては追いつかないので合わせ直す
		m_clockOffset = ideal;
		m_history.clear();
	}
	else
	{
		// 時計は少しずつずれるので合わせ続ける。
		// 急に合わせると、その瞬間に相手の車が飛ぶ
		m_clockOffset += (ideal - m_clockOffset) * NetConst::ClockBlend;
	}

	Sample s;
	s.time  = senderTime + m_clockOffset;
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
// 1つの状態から車体の姿勢を作る。
//
// 掛ける順番は描画側と揃える。ここがずれると、
// 通信で来た車だけ傾き方が違うことになる。
//----------------------------------------------------------
Math::Quaternion RemoteCar::MakeRotation(const HjNetStatePacket& st)
{
	return Math::Quaternion::CreateFromAxisAngle(Math::Vector3::UnitX, -st.terrainPitch)
	     * Math::Quaternion::CreateFromAxisAngle(Math::Vector3::UnitZ, -st.terrainRoll)
	     * Math::Quaternion::CreateFromAxisAngle(Math::Vector3::UnitY,  st.yaw);
}

//----------------------------------------------------------
// 指定した時刻の状態を作る。
//   ・その時刻を挟む2つがあれば、その間を繋ぐ
//   ・一番古いものより前なら、一番古いものをそのまま使う
//   ・一番新しいものより後なら、速度で少しだけ伸ばす
//----------------------------------------------------------
bool RemoteCar::SampleAt(float renderTime, HjNetStatePacket& out,
                         Math::Quaternion& outRot) const
{
	if (m_history.empty()) { return false; }

	// まだ届き始めたばかりで、描くべき時刻が最初の状態より前。
	// 補間する材料が無いので、持っているものをそのまま置く
	if (renderTime <= m_history.front().time)
	{
		out    = m_history.front().state;
		outRot = MakeRotation(out);
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
		if (span <= 1e-6f) { out = b.state; outRot = MakeRotation(out); return true; }

		const float t = std::clamp((renderTime - a.time) / span, 0.0f, 1.0f);

		out = b.state;   // 数値以外(番号・フラグ)は新しいほうに合わせる

		// 位置は「速度を接線に使った曲線」で繋ぐ(エルミート補間)。
		//
		// 2点を直線で結ぶと、旋回中に内側を通ってしまう。
		// 毎秒20回では点の間隔が広いので、コーナーで車が
		// 実際より内側を走って見える。速度が分かっているなら、
		// その向きへ出て その向きへ入る曲線を引ける。
		{
			const float t2 = t * t;
			const float t3 = t2 * t;
			// エルミート基底。始点・終点の位置と、両端の接線の重み
			const float h00 =  2.0f * t3 - 3.0f * t2 + 1.0f;
			const float h10 =         t3 - 2.0f * t2 + t;
			const float h01 = -2.0f * t3 + 3.0f * t2;
			const float h11 =         t3 -        t2;

			// 接線は「速度 × 区間の長さ」。
			// 秒速のままだと区間の長さと単位が合わず、曲がりすぎる
			const Math::Vector3 p0(a.state.px, a.state.py, a.state.pz);
			const Math::Vector3 p1(b.state.px, b.state.py, b.state.pz);
			const Math::Vector3 m0 = Math::Vector3(a.state.vx, a.state.vy, a.state.vz) * span;
			const Math::Vector3 m1 = Math::Vector3(b.state.vx, b.state.vy, b.state.vz) * span;

			const Math::Vector3 p = p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11;
			out.px = p.x;  out.py = p.y;  out.pz = p.z;
		}
		out.vx = a.state.vx + (b.state.vx - a.state.vx) * t;
		out.vy = a.state.vy + (b.state.vy - a.state.vy) * t;
		out.vz = a.state.vz + (b.state.vz - a.state.vz) * t;
		out.steer = a.state.steer + (b.state.steer - a.state.steer) * t;
		out.yaw = LerpAngle(a.state.yaw, b.state.yaw, t);

		// 車体の姿勢は合成してから球面で繋ぐ(Slerp)。
		//
		// 坂の傾き・バンク・向きは掛け合わさった1つの回転なので、
		// 角度ごとに別々に繋ぐと、3軸が同時に動いたときに
		// 本来たどるはずの経路と違う道を通る。バンクの付いた
		// コーナーで車体がわずかに揺れて見えるのがこれ。
		// 球面で繋げば、常に最短の回り方で一定の速さで回る。
		outRot = Math::Quaternion::Slerp(MakeRotation(a.state), MakeRotation(b.state), t);

		// サスの傾きは車体へ後から掛ける小さな角度なので、そのまま繋ぐ。
		// 合成へ含めても差が出ないうえ、分けておけばタイヤは接地したままにできる
		out.bodyPitch = LerpAngle(a.state.bodyPitch, b.state.bodyPitch, t);
		out.bodyRoll  = LerpAngle(a.state.bodyRoll,  b.state.bodyRoll,  t);
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

	// 姿勢は伸ばさない。回る速さを送っていないので、
	// 推測で回すと止まった相手が回り続けることになる
	outRot = MakeRotation(last.state);
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
	Math::Quaternion rot;
	if (!SampleAt(renderTime, s, rot)) { return; }

	const Math::Vector3 pos(s.px, s.py, s.pz);
	const Math::Vector3 vel(s.vx, s.vy, s.vz);
	const bool handbrake = (s.flags & HjNetFlag::Handbrake) != 0;

	// 先に回してから渡す。渡したあとに回すと、常に1フレーム古い角度が描かれる
	SpinWheels(dt, vel.Length(), handbrake);
	ApplyVisualState(pos, s.yaw, vel, s.steer, m_spinFront, m_spinRear);

	// 車体の姿勢は球面で繋いだものをそのまま渡す。
	// サスの傾きだけは、車体へ後から掛ける小さな角度として別に渡す。
	//
	// ここで UpdateSuspensionVisual を呼んでも、加速度を持っていないので
	// 常に水平のままになる。地形の傾きに至っては、レイを飛ばしていないので
	// そもそも求まらない。
	ApplyVisualRotation(rot);
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

	// 観戦中の印。名前の上に帯で出す。
	// 遠くの車でも、どれを見ているのかが一目で分かる
	if (m_spectated)
	{
		const float tagY = plateY - NP::TagH;
		Math::Color tagBg = NP::TagColor; tagBg.w *= alpha;
		Math::Color tagTx = NP::TagText;  tagTx.w *= alpha;

		HjUI::RectTL(plateX, tagY, plateW, NP::TagH, tagBg);
		HjUI::TextAtC(UIConst::FontTiny, cx,
		              tagY + UIConst::CenterInBox(NP::TagH, UIConst::FontPx(UIConst::FontTiny)),
		              NP::SpectateLabel, tagTx);
	}

	// 後ろの板。明るい路面や空の上でも読めるようにする
	HjUI::RectTL(plateX, plateY, plateW, plateH,
	             Math::Color(0.0f, 0.0f, 0.0f, NP::PlateAlpha * alpha));

	// どの車のものかを指す小さな三角(細い矩形2枚で代用せず、幅を詰めた板1枚)
	HjUI::RectTL(cx - NP::PointerW * 0.5f, plateY + plateH, NP::PointerW, NP::PointerH,
	             Math::Color(0.0f, 0.0f, 0.0f, NP::PlateAlpha * alpha));

	HjUI::TextAtC(NP::FontId, cx, plateY + NP::PadY, name,
	              Math::Color(1.0f, 1.0f, 1.0f, alpha));
}
