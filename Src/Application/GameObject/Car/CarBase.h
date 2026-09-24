#pragma once

#include "../../Const/CarConst.h"        // 既定値(規約上constヘッダはinclude可)
#include "HjCarRigid.h"                 // 6自由度の剛体で走らせる側
#include "../../Const/AlignmentConst.h"  // アライメント/サスセッティング既定値
#include "../Effect/DriftSmoke.h"        // ドリフトスモーク(後輪の煙)
#include "../Effect/DriftNeon.h"         // タイヤ周りのネオン線画(Unbound風)
#include "../Effect/SkidMark.h"          // 路面に残るタイヤ痕
#include "../../Input/HjGamePad.h"       // コントローラー入力(XInput)
#include "../../Mod/HjModLoader.h"      // 見た目の差し替え(MOD)
class HjHeightField;
class HjRoad;
#include "../../Audio/HjEngineAudio.h"   // エンジン音
#include "../../Audio/HjTireAudio.h"     // タイヤのスキール音・ブレーキ鳴き

//==========================================================
// CarBase
//   アーケード・FRドリフト車両の共通基底。
//   挙動(物理)・描画(車体+4輪)・調整パネルを持つ。
//   各車種は CarBase を継承し、コンストラクタで
//   「モデルパス」「性能ステータス」「タイヤ配置」を設定する。
//
//   操作: W=前進 / S=ブレーキ・後退 / A,D=ステア / Space=ハンドブレーキ
//==========================================================
class CarBase : public KdGameObject
{
public:
	//===== 1フレームの運転操作 =====
	// キーボードとコントローラーを合成した結果。
	//
	// CPUが運転する車は、ここを自分で埋めて返す。
	// 追走では、前を走る車が出した値を手本として借りるので、
	// 外から見える所に置いてある
	struct DriveInput
	{
		float throttle  = 0.0f;   // -1(ブレーキ/後退) 〜 +1(アクセル)
		float steer     = 0.0f;   // -1(左) 〜 +1(右)
		bool  handbrake = false;
		bool  clutch    = false;
		bool  shiftUp   = false;  // 押した瞬間のみ
		bool  shiftDown = false;
	};

	// 直前に出した運転操作。追走のCPUが手本として読む
	const DriveInput& GetLastInput() const { return m_lastInput; }

	void Init()    override;
	void Update()  override;
	// 増えたタイヤ痕を焼き付けマップへ書き込む。
	// レンダーターゲットを差し替えるので、シーンの描画パスに入る前に行う必要がある。
	void PreDraw() override;
	void DrawLit() override;
	void DrawEffect() override;        // ドリフトスモーク(UnLitパス)
	void DrawOverlayEffect() override; // ネオン線画(煙の輪郭処理を通さず加算合成で重ねる)
	void DrawBright() override;   // ネオンのグロー(ポストプロセスでぼかされて光が滲む)

	// 車はオブジェクト単位の視錐台カリングの対象外。
	// 車体そのものは小さいが、煙・タイヤ痕・ネオンは車から遠く離れた位置まで
	// 広がっており、車が画面外に出た瞬間にそれらが丸ごと消えてしまう。
	// (これらは各エフェクト側で粒・区間ごとにカリングしている)
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }
	void DrawDebug()  override;   // 当たり判定の可視化(F1でトグル)

	// 追従カメラ等から参照
	Math::Vector3 GetPos()     const override { return m_pos; }

	// 位置を直に置く。
	//
	// ■ なぜ上書きが要るか
	// 枠組みの SetPos はワールド行列へ書くだけで、車が実際に使う
	// m_pos には触らない。GetPos は上書きして m_pos を返すので、
	// 対にしておかないと「読んで足して書く」が噛み合わない。
	//
	// すり抜け(ノークリップ)がまさにそれで、読みは m_pos、
	// 書きはワールド行列という形になっていて動かなかった。
	//
	// 剛体で走らせているときは、そちらの位置も合わせる。
	// 合わせないと、次のフレームに剛体の位置へ引き戻される
	void SetPos(const Math::Vector3& pos) override;
	float         GetYaw()     const          { return m_yaw; }
	Math::Vector3 GetForward() const          { return Math::Vector3(sinf(m_yaw), 0.0f, cosf(m_yaw)); }
	Math::Vector3 GetVel()     const          { return m_vel; }   // ドリフトカメラ用(進行方向)

	// HUDが読む値。計算はここで完結させて、表示側は並べるだけにする
	float GetSpeedKmh() const { return m_vel.Length() * CarConst::HudMsToKmh; }
	int   GetGear()     const { return m_reverse ? 0 : m_gear; }   // 0=リバース
	float GetRpm()      const { return m_engineRPM; }
	// 回転数の割合(0〜1)。回転計の目盛りを光らせる量に使う
	float GetRpmRatio() const
	{
		return std::clamp(m_engineRPM / CarConst::MaxRPM, 0.0f, 1.0f);
	}
	// 横滑り角(度)。進行方向と車体前方のなす角＝ドリフトの深さ。
	// 符号付き：正=右へ滑っている / 負=左へ滑っている。
	// どちらへ滑っているかは切り返しの瞬間に一番知りたい情報なので、
	// 絶対値だけを返すと表示側で作り直せなくなる。
	float GetDriftAngleDegSigned() const
	{
		const Math::Vector3 fwd(sinf(m_yaw), 0.0f, cosf(m_yaw));
		const Math::Vector3 rgt(cosf(m_yaw), 0.0f, -sinf(m_yaw));
		const float vLong = m_vel.Dot(fwd);
		const float vLat  = m_vel.Dot(rgt);
		return atan2f(vLat, fabsf(vLong) + 1.0f) * 57.29578f;
	}
	float GetDriftAngleDeg() const { return fabsf(GetDriftAngleDegSigned()); }

	//===== 通信で相手へ送る値 =====
	// 前輪の切れ角(rad)。相手の画面でもタイヤが同じ向きを向くように送る
	// ヨー角速度(rad/s)。回っている速さ。
	// 追走のCPUが、行き過ぎる前に舵を戻すのに要る
	float GetYawRate() const { return m_yawRate; }

	// 前輪の最大切れ角(rad)。
	// CPUが出す舵を割合へ直すのに要る(この角度で頭打ち)
	float GetMaxSteerAngle() const { return m_maxSteerAngle; }

	//===== 地形(高さマップ) =====
	// 高さを聞く先。渡されていればこちらを使う。
	//
	// メッシュへ光線を飛ばすのに比べて桁違いに軽い。
	// サスペンションを入れると4輪ぶんを細かい刻みで解くので、
	// 1フレームに何十回も聞くことになる。光線では持たない。
	//
	// 借りているだけ。持ち主は場面の側
	void SetHeightField(const HjHeightField* field) { m_pHeightField = field; }

	// 道。地形より先にこちらを聞く。
	//
	// 高さマップは真上から見た格子なので、路面のカントや
	// 中央の盛り上がりを表現できない。道はスプラインの断面から
	// 厳密に求まるので、道の上ならそちらが正しい
	void SetRoad(const HjRoad* road) { m_pRoad = road; }

	float GetSteerAngle() const { return m_steer; }
	// サイドブレーキ中か。相手側で後輪の転がりを止めるのに使う
	bool  IsHandbrake()   const { return m_handbrakeNow; }

	// 車体の傾き(rad)。地形に沿った傾きと、サスによる傾きは別物なので分けて返す。
	// 受け取り側では作れない値なので、そのまま送る
	// 後輪/前輪の滑り量(0〜1)。煙とタイヤ痕の量を決めている値そのもの。
	// 通信で相手へ送り、相手の画面でも同じ量の煙と痕が出るようにする
	float GetSlipRear01()  const { return m_slipRear01; }
	float GetSlipFront01() const { return m_slipFront01; }
	bool  IsOnGround()     const { return m_onGround; }

	// 直前に壁として当たった地形ノードの番号。-1=当たっていない。
	//
	// 地形の当たり判定を詰めるときに使う。名前が自動出力で
	// 種類が読み取れないモデルでも、「今ぶつかっている物」が分かれば
	// その場で名指しで外せる。
	int   GetLastWallNode() const { return m_lastWallNode; }

	float GetTerrainPitch() const { return m_terrainPitch; }
	float GetTerrainRoll()  const { return m_terrainRoll; }
	float GetBodyPitch()    const { return m_pitchAngle; }
	float GetBodyRoll()     const { return m_rollAngle; }

	// 当たり判定対象(地形など)を登録する。車はこれらへレイ/球判定を飛ばす。
	void AddCollisionTarget(const std::weak_ptr<KdGameObject>& obj) { m_wpHitList.push_back(obj); }

	// スポーン(初期配置)：位置・向きを与え、速度など運動状態をリセット。
	// 与えた値はリスポーン地点として記憶する。
	void SetSpawn(const Math::Vector3& pos, float yaw);
	// 走行を止める(観戦中など)。
	//
	// 入力を切るだけでは惰性で走り続けるので、見ていない間に
	// 崖から落ちたり壁に刺さったりする。速度ごと止める。
	// コントローラーの状況を出す(F2のパネル)。
	// 実際に操作へ使っているものをそのまま覗く。
	// 別に用意すると、使われているのと違うものを見ることになる
	//===== 見た目の差し替え(MOD) =====
	// 利用者が置いた .gltf に替える。
	//
	// 失敗しても見た目はそのまま残る。読み込めなかった理由は
	// 戻り値で返すので、画面へそのまま出せる。
	// 何も返さないと「押したのに変わらない」だけになり、
	// 置き場所が違うのか形式が違うのか分からない。
	//
	// 空文字か ModConst::StockMark を渡すと標準へ戻る。
	HjModLoader::Result SetBodyModel(const std::string& path);
	HjModLoader::Result SetWheelModel(const std::string& path);

	// いま何を使っているか。標準なら StockMark
	const std::string& GetBodyModPath()  const { return m_bodyModPath; }
	const std::string& GetWheelModPath() const { return m_wheelModPath; }

	// いま実際に使っているモデルの場所。
	//
	// このゲームに「標準の車体」という区分は無い。
	// 最初から積んであるモデルも、あとから足したモデルも、
	// 同じ「そのモデル」として扱う。
	//
	// 合わせ込みの記録もこれで引く。区分を分けると、
	// 最初のモデルだけ別の仕組みで管理することになる
	// 車が持っているモデルの名前(拡張子なし)。
	//
	// MOD の候補と同じ形で並べるために使う。
	// 「標準」でも「最初のモデル」でもなく、
	// この車のモデルというだけ。継承している車種そのもの
	std::string GetOwnBodyName()  const { return AssetName(m_bodyPath); }
	std::string GetOwnWheelName() const { return AssetName(m_wheelPath); }

	const std::string& GetBodyAsset() const
	{
		return (m_bodyModPath == ModConst::StockMark) ? m_bodyPath : m_bodyModPath;
	}
	const std::string& GetWheelAsset() const
	{
		return (m_wheelModPath == ModConst::StockMark) ? m_wheelPath : m_wheelModPath;
	}

	// 選んだものの保存 / 読込。
	// 車の調整(CarTune)とは別のファイルにする。
	// あちらは数値だけを並べる作りなので、パスを混ぜると読み込みが壊れる
	// 見た目合わせの1項目。
	//
	// 保存に使う名前と、画面に出す名前を分けて持つ。
	// 画面の文言は読みやすさで直したくなるが、そのたびに
	// 保存済みの値が読めなくなるのでは困る。
	struct AppearanceParam
	{
		const char* key;    // 保存に使う名前(一度決めたら変えない)
		const char* label;  // 画面に出す名前(いつ変えてもよい)
		float*      value;  // 動かす先。持ち主は車なので借りるだけ
	};

	// 見た目合わせの調整値。MODメニューが左右で動かす。
	//
	// TuneParamList は物理まで全部返すので、そのまま見せると
	// 走行中のメニューから車の性能まで変えられてしまう。
	// 見た目に関わるものだけを別に返す。
	//
	// ※項目を足すのはここだけでよい。
	//   保存も画面も、この一覧を舐めて作る
	std::vector<AppearanceParam> AppearanceParamList();

	// ※見た目の調整をここから書き出す口は置かない。
	//   差し替えたモデルの合わせ込みを車の調整へ書くと、
	//   モデルを替えるたびに前の値が消え、標準へ戻したときには
	//   標準の値まで壊れている。モデルごとの記録(HjModProfile)へ持つ。

	void SaveModChoice() const;
	void LoadModChoice();

	void DrawPadImGui() { m_pad.DrawImGui(); }

	// コントローラー。飛ばすときなど、車の外から入力を見たい所がある。
	// 読むだけ。持ち主は車のまま
	// 車種の鍵。保存を車ごとに分けるのに使う
	const std::string& GetSaveKey() const { return m_saveKey; }

	//===== 性能の値を読む =====
	// 車庫の画面が棒グラフに使う。
	//
	// 画面側に数字を書き写すと、車を触るたびに食い違う。
	// 設定を当てた車から直接読ませる
	//===== 車庫の見せ札 =====
	// 止まった姿だけを描く。走行中の演出は乗せない
	// 車庫の絵。outlineMul に 0 を渡すと輪郭を描かない。
	// col は掛ける色。映り込みを薄く出すのに使う
	void DrawPortrait(const Math::Matrix& world, float outlineMul = 1.0f,
	                  const Math::Color& col = kWhiteColor);

	// 車体と車輪をそのまま描くだけ。色も輪郭も付けない。
	//
	// 影の元になる深度を書くのに使う。
	// 深度マップの生成では色も縁取りも要らない
	void DrawPortraitPlain(const Math::Matrix& world);

	// 絵を出すのに要るモデルだけ読む(Init は呼ばない)
	void LoadPreviewModels();

	// 車体が占める大きさ。車庫のカメラを合わせるのに使う。
	// 中心は車のローカル(原点=接地面の中央)、半径はそれを包む球。
	// モデルが読めていなければ false
	bool GetBodyBounds(Math::Vector3& outCenter, float& outRadius) const;

	float GetMaxSpeedSpec()    const { return m_maxSpeed; }
	float GetEnginePowerSpec() const { return m_enginePower; }
	float GetBrakePowerSpec()  const { return m_brakePower; }
	float GetMuFrontSpec()     const { return m_muFront; }
	float GetMuRearSpec()      const { return m_muRear; }

	// 寸法。車庫の諸元表が出す。半分で持っているので倍にして使う
	float GetTrackSpec() const { return m_track; }
	float GetBaseSpec()  const { return m_base; }

	const HjGamePad& GetPad() const { return m_pad; }

	void SetHalted(bool halted) { m_halted = halted; }

	// 地形をすり抜ける。見回るときだけ。
	//
	// 止めるだけでは足りない。止めている間も接地は続けているので、
	// 持ち上げた車が毎フレーム地面へ引き戻されて浮かない
	//===== 剛体で走らせるか =====
	//
	// 旧モデルは3自由度の平面モデルで、姿勢は4輪のレイから
	// 推定していた。片輪が浮く・転倒する、が原理的に出せない。
	//
	// 切り替えられるようにしてあるのは、詰め終わるまで
	// 見比べる必要があるため。落ち着いたら旧側を消す
	void SetRigid(bool on);
	bool IsRigid() const { return m_useRigid; }

	// 剛体側へ地面を渡す。場面が持っているものを借りる
	void SetRigidGround(const HjHeightField* field, const HjRoad* road)
	{
		m_rigid.SetGround(field, road);
	}

	void SetNoClip(bool on) { m_noClip = on; }
	bool IsNoClip() const { return m_noClip; }
	bool IsHalted() const { return m_halted; }

	// 記憶したスポーン地点へ戻す(Rキーのリスポーン)
	void Respawn() { SetSpawn(m_spawnPos, m_spawnYaw); }

	// 調整パネルを外部(シーン)から描画するための公開窓口
	void DrawImGui() { DrawTuningImGui(); }
	// Hierarchy に並べる名前(車種ごとに Silvia などを設定する)
	const std::string& GetTuningName() const { return m_tuningName; }

	//===== 見た目の色 =====
	// 調整パネルで設定した色。通信で相手へ送り、
	// 相手の画面でも同じ車に見せる
	const Math::Vector3& GetOutlineColor() const { return m_outlineColor; }
	const Math::Vector3& GetSmokeColorA()  const { return m_smokeColor; }
	const Math::Vector3& GetSmokeColorB()  const { return m_smokeColorB; }
	const Math::Vector3& GetAccentColor()  const { return m_driftTintColor; }
	const Math::Vector3& GetNeonColorA()   const { return m_neonColorA; }
	const Math::Vector3& GetNeonColorB()   const { return m_neonColorB; }
	const Math::Vector3& GetSmokeHiColor() const { return m_smokeHiColor; }
	float                GetSmokeGradDist() const { return m_smokeGradDist; }

	// 受け取った色を反映する(他人の車用)。
	// 自分の車は調整パネルの値をそのまま使うので、これは呼ばない
	void ApplyLookColors(const Math::Vector3& outline,
	                     const Math::Vector3& smokeA, const Math::Vector3& smokeB,
	                     const Math::Vector3& accent,
	                     const Math::Vector3& neonA,  const Math::Vector3& neonB,
	                     const Math::Vector3& smokeHi, float smokeGradDist);

	// ブースト(ニトロ)演出を発動：車体に一瞬だけアクセントカラーが乗り、
	// 同時にネオンの線画が全方向へ弾ける。将来ニトロ機能から呼ぶ。
	void TriggerBoost();

protected:
	//===== Update() の分割 =====
	// 1フレームの運転操作を作る。
	//
	// 人が運転するときはキーボードとパッドを読む。
	// CPUが運転する車は、ここを差し替えて自分で作った値を返す。
	// 物理側はどちらから来た値かを知らないので、
	// 「CPUだけ挙動が違う」ということが起きない
	virtual DriveInput ReadInput();
	void UpdateDebugKeys();        // F1〜F4のデバッグトグル(運転とは無関係)

	// 車体アライン：アクセルオフで車体を進行方向へ寄せるアシスト。
	// 物理の積分結果(m_yaw)を直接書き換えるので、アシスト群として切り離してある。
	void UpdateBodyAlignAssist(float dt, float vLong0, bool handbrake);

	//===== ドリフトの手ざわりを作る処理 =====
	// 旧モデルと剛体の両方から呼ぶ。
	//
	// 挙動そのものなので、片方だけ書き換えると乗り味がずれる。
	// 下回り(平面モデルか剛体か)を替えても、ここが同じなら
	// ドリフトの感触は変わらない

	// 舵角(m_steer)を決める。オートカウンターを含む
	void UpdateSteerAngle(float dt, float steerInput, float throttle,
	                      bool handbrake, float vLong0, float vLat0, float speedNow);

	// 振り返しの後押し。切った向きへ足すヨー加速度(rad/s^2)を返す
	float TransitionYawBoost(float steerInput, float vLong0, float vLat0) const;

	// スピン防止。抑えるべきヨーの割合(1/秒)を返す。0なら効かない
	float SpinAssistRate(float steerInput, bool handbrake,
	                     float vLong, float vLat, float yawRate) const;

	// 後退ギア(R)の断続
	void UpdateReverseGear(float dt, float throttle, bool handbrake, float vLong0);

	// 調整値を剛体へ渡す。
	//
	// 毎フレーム渡す。切り替えた時だけだと、調整パネルで値を変えても
	// 走りが変わらず「触っても何も起きない」つまみになる
	void ApplyRigidSetup();

	// サスペンションのロール/ピッチ。見た目だけで挙動には影響しない。
	void UpdateSuspensionVisual(float dt);

	// 通信で受け取った状態を、そのまま見た目へ反映する。
	//
	// 他人の車は自分のPCで物理を回さない。回してしまうと、
	// 同じ操作をしていないので必ず本人の画面とずれていくうえ、
	// 届いた位置で毎回引き戻すことになって car が震える。
	// 位置と向きは「答え」として受け取り、それを描くだけにする。
	//
	// 運動状態(m_yawRate など)は書き換えない。物理を進めないので使われず、
	// 中途半端に入れると「動いているように見える値」が残って読み違える。
	void ApplyVisualState(const Math::Vector3& pos, float yaw, const Math::Vector3& vel,
	                      float steer, float spinFront, float spinRear);

	// 通信で受け取った傾きを反映する。
	// 位置や向きとは別にしてあるのは、こちらは「見た目だけ」で、
	// 当たり判定にも進行方向にも関わらないため。
	void ApplyVisualTilt(float terrainPitch, float terrainRoll,
	                     float bodyPitch, float bodyRoll);

	// 通信で受け取った姿勢(合成済み)。
	//
	// 車体の向きは「坂の傾き × バンク × ヨー」を掛け合わせた回転で、
	// 3軸が同時に動く。これを角度ごとに別々に補間すると、
	// 本来たどるはずの経路と違う道を通って車体が揺れる。
	// 受け取り側で合成してから補間した結果を、そのまま使う。
	void ApplyVisualRotation(const Math::Quaternion& rot);

	// タイヤ痕と煙を出す。
	//
	// 入力は「どれだけ滑っているか」だけ。物理から出しても通信から
	// 受け取っても同じ絵になるので、他人の車でもそのまま使える。
	//
	// ※自分の車は今のところ UpdateMotionFeedback の中で同じことをしている。
	//   あちらは物理の途中の値を大量に使っており、切り出すと挙動側に
	//   触ることになるので分けたままにしてある(まとめるのは今後の課題)。
	void EmitTireFx(float dt, float speed, float slipRear01, float slipFront01,
	                bool onGround);

	// 煙の粒の寿命と移動を進める。放出とは別なので分けてある
	void UpdateEffectParticles(float dt);

	// 音を止める。
	// 他人の車は音を鳴らさない(台数だけエンジンが増えると何を聞いているのか
	// 分からなくなる)。音源そのものは基底が持っているので、入口だけ開ける。
	void StopAudio();

	// エンジン・ギア・クラッチ。結果は m_engineRPM / m_gear / m_clutch /
	// m_driveSpeed / m_driveAccel に入る。
	void UpdateDriveline(float dt, float throttle, bool handbrake,
	                     bool clutchPressed, bool shiftUp, bool shiftDown, float vLong0);

	// 4輪シミュレーション。各輪の荷重・スリップ角・摩擦円からタイヤ力を求め、
	// 車体の速度とヨーへ積分する。挙動の本体。
	void StepTireForces(float dt, float throttle, float steerInput, bool handbrake,
	                    bool clutchPressed, bool accelPressed);

	// 水平移動と壁の押し戻し(サブステップCCD＋リラクゼーション)
	void ResolveWallCollision(float dt);
	// 接地判定と車体の高さ・地形の傾きへの追従
	void UpdateGroundContact(float dt);

	// 物理の結果を見た目へ反映する。タイヤの回転と、走行状態に応じた
	// エフェクト(煙・ネオン・タイヤ痕)の放出。挙動には影響しない。
	void UpdateMotionFeedback(float dt, bool handbrake, float throttle);

	//===== 音 =====
	// ここにまとめてあるのは、旧モデルと剛体で同じものを
	// 鳴らすため。片方にしか無いと、下回りを入れ替えたとたんに
	// 無音になる

	// レブの効き具合(0〜1)。トルクを絞る側と音の両方が読む
	float RevCut() const;

	// エンジン音を進める。throttle は符号付きのまま渡してよい
	void UpdateEngineAudio(float dt, float throttle);

	// 音源と聴取点の位置を渡す。渡さないと距離も方向も出ない
	void PlaceAudio();

	void DrawTuningImGui();
	void DrawModImGui();   // 見た目の差し替え(調整パネルの中の1区画)

	// 調整値の保存/読込（車種ごとのファイルへ）
	void SaveTuning();
	void LoadTuning();
	// 道から、拡張子なしのファイル名だけ取り出す
	static std::string AssetName(const std::string& path);

	// 車体と4輪の行列を組む。DrawLit と DrawPortrait で共有する
	void BuildPose(const Math::Matrix& carWorld,
	               Math::Matrix& outBody, Math::Matrix outWheel[4]) const;

	std::string TuneFilePath() const;
	std::string ModFilePath() const;
	std::vector<std::pair<const char*, float*>> TuneParamList();

	// 車種ごとの設定は、それぞれの車種クラスが持つ。
	//
	// 相手の車は後から車種が決まるので、設定を「作り方」から
	// 切り離してある。切り離すと外から protected を触ることになるので、
	// その2つだけ通す
	friend class Silvia;
	friend class Nsx;

	//===== 派生クラスがコンストラクタで設定する =====
	// モデル
	std::string m_bodyPath  = "Asset/Data/Box.gltf";
	std::string m_wheelPath = "Asset/Data/Box.gltf";

	// 差し替えで選ばれているもの。標準なら ModConst::StockMark。
	// 上の m_bodyPath とは別に持つ。あちらは「標準は何か」を
	// 覚えておく場所で、上書きすると標準へ戻せなくなる
	// 差し替えを読み込むか。
	//
	// 保存ファイルは車種ごと(CarMod_Silvia.txt)なので、
	// 同じ車種を継いだ車は全部これを読んでしまう。
	// 通信で来た他人の車まで自分の差し替えになってしまうので、
	// 読むかどうかを持ち主が決められるようにする。
	//
	// ※他人の車のモデルは、まだ受け取る仕組みが無い。
	//   出来たらここを通して当てる
	bool m_useModChoice = true;

	std::string m_bodyModPath  = "-";
	std::string m_wheelModPath = "-";
	std::string m_tuningName = "Car Tuning";   // Hierarchy に並べるときの名前
	std::string m_saveKey    = "Car";          // 保存ファイルのキー(車種ごと)

	// 性能ステータス
	float m_enginePower     = CarConst::EnginePower;
	float m_brakePower      = CarConst::BrakePower;
	float m_maxSpeed        = CarConst::MaxSpeed;
	float m_drag            = CarConst::Drag;
	float m_scrubDrag       = CarConst::ScrubDrag;   // 横滑りスクラブ抵抗(ドリフト速度の抑制)
	float m_maxSteerAngle   = CarConst::MaxSteerAngle;
	float m_steerSpeed      = CarConst::SteerSpeed;
	float m_steerReturnMul  = CarConst::SteerReturnMul;   // 戻す/逆へ振る時の速さ倍率
	float m_counterRelease  = CarConst::CounterRelease;   // 逆に切った時カウンターを緩める量
	float m_turnRate        = CarConst::TurnRate;
	float m_turnRefSpeed    = CarConst::TurnRefSpeed;
	float m_yawResponse     = CarConst::YawResponse;
	float m_gripTraction    = CarConst::GripTraction;
	float m_driftTraction   = CarConst::DriftTraction;
	float m_throttleGripLoss = CarConst::ThrottleGripLoss;
	float m_minTraction     = CarConst::MinTraction;

	// CarX風スリップアングル・タイヤモデル
	float m_muFront   = CarConst::TireMuFront;
	float m_muRear    = CarConst::TireMuRear;
	float m_tireB     = CarConst::TireStiffB;
	float m_tireC     = CarConst::TireShapeC;
	float m_izz       = CarConst::YawInertia;
	float m_cgHeight  = CarConst::CgHeight;
	float m_rearGripThrottleLoss = CarConst::RearGripThrottleLoss;
	float m_handbrakeGripMul     = CarConst::HandbrakeGripMul;
	float m_slipEps   = CarConst::SlipSpeedEps;
	// CarX系の切り返しを決める3要素
	float m_tireLoadSens  = CarConst::TireLoadSens;      // 荷重感度(荷重が増えるほどμが下がる)
	float m_tireRelaxLen  = CarConst::TireRelaxLength;   // リラクゼーション長(m)
	float m_rollFreq      = CarConst::RollFreq;          // ロールの固有角周波数
	float m_rollDampRatio = CarConst::RollDampRatio;     // ロールの減衰比(1未満で行き過ぎる)
	// 路面の傾き・空力・駆動系
	float m_slopeGravity      = CarConst::SlopeGravity;       // 斜面の重力成分の倍率
	float m_downforceCoef     = CarConst::DownforceCoef;      // ダウンフォース係数
	float m_downforceRearBias = CarConst::DownforceRearBias;  // ダウンフォースの後ろ寄り配分
	float m_lsdLock           = CarConst::LsdLock;            // デフのロック強さ
	float m_bumpLoadGain      = CarConst::BumpLoadGain;       // 段差による荷重変化の強さ
	// ※ここにあった lowSpeedGrip / latSettle / handbrakeBrake / handbrakeYawDamp /
	//   spinRecover / driftRetain / handbrakeSlipEps は、ImGuiと保存には出ていたが
	//   物理側で一度も参照されていなかった(触っても何も起きない)ため削除した。
	//   必要になったら実装と一緒に追加すること。
	float m_yawDamp   = CarConst::YawDamp;

	// 駆動輪の縦スリップ(摩擦円：空転すると横グリップが減って流れる)
	float m_longStiff    = CarConst::LongStiff;
	float m_wheelInertia = CarConst::WheelInertia;
	float m_driveRelax   = CarConst::DriveRelax;

	// サスペンション(バネ・ダンパーで車体をロール/ピッチさせる。見た目)
	float m_suspStiff = CarConst::SuspStiffness;
	float m_suspDamp  = CarConst::SuspDamping;
	float m_rollGain  = CarConst::RollGain;
	float m_pitchGain = CarConst::PitchGain;
	float m_suspMax   = CarConst::SuspMaxAngle;
	float m_accelSmooth = CarConst::SuspAccelSmooth;

	//===== 運転アシスト =====
	// どれも「タイヤ力を経由せずに車体を動かす」処理。
	// CarXもステアリングアシストや安定制御を既定でONにしているので、
	// こちらも既定はONにしておく。個別に切って比較できる。
	//
	// ただしアシストは「プレイヤーがやろうとしていること」を邪魔してはいけない。
	// 事故のスピンは助け、意図した回転(ドーナツ・360度)は邪魔しない、
	// という区別が要る。区別が無いと、狙って回そうとしても止められる。
	bool  m_transitionEnabled = true;   // 振り返しのヨー後押し(物理を経由しない外力)
	bool  m_bodyAlignEnabled  = true;   // アクセルオフで車体を進行方向へ回頭
	bool  m_scrubDragEnabled  = true;   // 横滑り速度の直接減衰(タイヤ力とは別口)

	// オートカウンター(ステアリングアシスト)
	bool  m_counterSteerEnabled = true;
	// アクセルを抜いたときにオートカウンターを抜くか。
	//
	// 抜くと、舵に触っていないのに前輪の角度が変わる。自分が握っている舵が
	// 意図せず動くのが、手ごたえとして一番読めなくなる原因になる。
	// そもそもアクセルを抜けば荷重が前へ移ってリアがグリップを取り戻すので、
	// 挙動の変化は物理側だけで足りている。舵まで動かすと二重に効く。
	bool  m_liftCounterEnabled = false;
	float m_counterAssist  = CarConst::CounterAssist;   // 横滑り角を打ち消す割合(0-1+)
	float m_counterMinSpeed = CarConst::CounterMinSpeed; // これ未満の速度では効かせない
	// 今フレームのカウンター舵角。横滑り角の震えをそのまま舵へ出さないよう、
	// 動ける量に上限を付けて追わせる(前フレームの値が要るので保持する)
	float m_autoCounter = 0.0f;

	// スピン防止アシスト(スタビリティコントロール)
	bool  m_spinAssistEnabled = true;
	float m_spinAssist = CarConst::SpinAssistStrength;
	float m_handbrakeCounterMul = CarConst::HandbrakeCounterMul; // サイド中のカウンター倍率(1=通常)

	// ※gripRelax(グリップ変化の平滑化)と gripCatch(アクセルオフでグリップ復帰)も
	//   物理側で未参照だったため削除。前者の役割は m_tireRelaxLen が担っている。
	// 車体アライン(アクセルオフで車体を進行方向へ回頭＝角度を抜く。カニ歩き防止)
	float m_bodyAlign = CarConst::BodyAlign;
	// トランジション補助(振り返し。ドリフト中に切った方向へヨーを後押し)
	float m_transitionAssist = CarConst::TransitionAssist;

	// アライメント/サスセッティング(実挙動に効く。既定は中立=現状維持)
	float m_toeFront   = AlignmentConst::ToeFront;    // 前トー(rad, 正=トーイン)
	float m_toeRear    = AlignmentConst::ToeRear;     // 後トー
	float m_ackermann  = AlignmentConst::Ackermann;   // アッカーマン(-1〜1, 0=平行)
	float m_camberGrip = AlignmentConst::CamberGrip;  // キャンバーの横グリップ寄与(0=見た目のみ)
	float m_springF    = AlignmentConst::SpringFront; // 前ばね定数(相対)
	float m_springR    = AlignmentConst::SpringRear;  // 後ばね定数(相対)
	float m_arbF       = AlignmentConst::ArbFront;    // 前スタビ(相対)
	float m_arbR       = AlignmentConst::ArbRear;     // 後スタビ(相対)

	// 見た目(タイヤ配置・向き)
	float m_bodyScale  = CarConst::CarModelScale;
	float m_bodyYaw    = CarConst::CarModelYawOffset;
	float m_wheelScale = CarConst::WheelModelScale;
	float m_wheelYaw   = CarConst::WheelModelYawOffset;
	float m_track      = CarConst::WheelTrack;
	float m_base       = CarConst::WheelBase;
	float m_wheelH     = CarConst::WheelHeight;
	float m_camber     = CarConst::CamberAngle;
	// オフセット(全体 / 前輪 / 後輪 を個別に)
	float m_offX = 0.0f,      m_offZ = 0.0f;       // 4輪全体

	// 車体モデルの位置合わせ(車の座標での取り付け位置)。
	//
	// 外から持ってきたモデルは原点の置き方がバラバラで、
	// 床に埋まったり浮いたりする。タイヤは接地したままにしたいので、
	// 車ごと動かすのではなく、車体モデルだけをずらす。
	//
	// 規約どおり、まとめて Vector3 で持つ。
	// x=左右 / y=高さ / z=前後
	Math::Vector3 m_bodyOffset = Math::Vector3::Zero;
	float m_frontOffX = 0.0f, m_frontOffZ = 0.0f;  // 前輪のみ
	float m_rearOffX = 0.0f,  m_rearOffZ = 0.0f;   // 後輪のみ

	// アウトライン(原神式・背面押し出しトゥーン輪郭)
	// 車体の背面押し出しによる縁取り。
	//
	// セルシェードをやめたので既定は切り。
	// 陰影で形を見せる絵に、線で形を囲う描き方を混ぜると
	// どちらの約束で描いているのか分からなくなる
	bool          m_outlineEnabled = false;
	float         m_outlineWidth   = 0.04f;
	Math::Vector3 m_outlineColor    = Math::Vector3(0.0f, 0.0f, 0.0f); // 黒

	// ドリフトスモークの色味(白=通常。NFS Unbound風のカラー煙にもできる)
	// 発生源から離れるほど 色A → 色B へ滑らかにグラデーションする
	Math::Vector3 m_smokeColor    = Math::Vector3(1.0f, 1.0f, 1.0f);  // 手前の色
	Math::Vector3 m_smokeColorB   = Math::Vector3(1.0f, 1.0f, 1.0f);  // 奥の色
	float         m_smokeGradDist = SmokeConst::SmokeGradDist;        // 色Bになりきる距離(m)
	Math::Vector3 m_smokeHiColor  = Math::Vector3(1.0f, 1.0f, 1.0f);  // ハイライトの色

	// ドリフト中に車体をアクセントカラーで塗る演出(NFS Unboundのドライビングエフェクト風)
	Math::Vector3 m_driftTintColor = Math::Vector3(0.78f, 1.0f, 0.16f); // 塗る色
	// 発光中の輪郭の色。車体の塗りとは別に指定できる
	// (縁だけ違う色で光らせたい場合があるため、アクセントカラーとは分ける)
	Math::Vector3 m_boostOutlineColor = Math::Vector3(1.0f, 1.0f, 1.0f);
	float         m_driftTintMax   = 1.0f;   // 最大の塗り具合(0=無効 1=完全に塗り潰す)
	float         m_driftTintSlipDeg = 35.0f;// この横滑り角(度)で塗りが最大になる
	// ネオン線画・粒の色(2色。粒ごとにAとBを混色して散らす)
	Math::Vector3 m_neonColorA = Math::Vector3(NeonFxConst::ColorAR, NeonFxConst::ColorAG, NeonFxConst::ColorAB);
	Math::Vector3 m_neonColorB = Math::Vector3(NeonFxConst::ColorBR, NeonFxConst::ColorBG, NeonFxConst::ColorBB);


private:
	// モデル
	KdModelWork m_body;
	KdModelWork m_wheel;

	// ランタイム状態
	Math::Vector3 m_pos = Math::Vector3::Zero;
	Math::Vector3 m_vel = Math::Vector3::Zero;
	Math::Vector3 m_spawnPos = Math::Vector3::Zero;   // リスポーン地点(SetSpawnで記憶)
	float         m_spawnYaw = 0.0f;
	float         m_yaw     = 0.0f;
	float         m_yawRate = 0.0f;   // ヨー角速度(rad/s)
	float         m_mzFilt  = 0.0f;   // 平滑化したヨーモーメント(タイヤリラクゼーション)
	float         m_steer       = 0.0f;
	// 今フレームのサイドブレーキ。操作の読み取りは局所変数なので、
	// 外へ伝えるために覚えておく(通信で相手へ送る)
	bool          m_handbrakeNow = false;
	// 走行を止めているか(観戦中など)
	bool          m_halted = false;

	// 地形をすり抜けているか。接地を取らなくなる
	bool          m_noClip = false;

	//===== 剛体 =====
	// 旧モデルと並べて持つ。切り替えて見比べるため
	bool       m_useRigid = false;
	HjCarRigid m_rigid;

	// 剛体で1フレーム進める。旧モデルの代わりに呼ぶ
	void UpdateRigid(float dt);
	// 通信で受け取った姿勢。設定されていれば角度より優先する
	Math::Quaternion m_netRotation;
	bool             m_useNetRotation = false;
	// 後退へ入るまでSを踏み続けている時間(秒)。
	// 一瞬の誤判定でギアが切り替わらないようにするためのもの
	float         m_reverseHold  = 0.0f;
	// 直前に壁として当たった地形ノードの番号(調整用)
	int           m_lastWallNode = -1;
	// 今フレームの滑り量。演出の量を決めている値で、これも相手へ送る
	float         m_slipRear01   = 0.0f;
	float         m_slipFront01  = 0.0f;
	float         m_playerSteer = 0.0f;   // プレイヤー入力ぶんの舵角(平滑化)
	float         m_hbCounterFactor = 1.0f; // サイド中カウンター倍率の平滑化(段差カクつき防止)
	float         m_liftCounterFactor = 1.0f; // アクセルオフ時カウンター減衰の平滑化(急スリップ防止)
	float         m_wheelSpinFront = 0.0f;   // 前輪の転がり角(rad)
	float         m_wheelSpinRear  = 0.0f;   // 後輪の転がり角(rad, サイド中はロック)
	float         m_driveSpeed     = 0.0f;   // 駆動輪の接地面速度(m/s, 空転で路面速度を超える)

	// エンジン / ギア / クラッチ
	float         m_engineRPM = CarConst::IdleRPM;
	int           m_gear      = 1;
	float         m_clutch    = 1.0f;        // 1=接続 / 0=切断(サイドで自動的に切れる)
	float         m_driveAccel = 0.0f;       // 今フレームの駆動加速(クラッチ・トルク込み)
	bool          m_reverse    = false;      // 後退ギア(R)に入っているか

	// サスペンション状態(車体のロール/ピッチ)
	float         m_rollAngle  = 0.0f, m_rollVel  = 0.0f;
	float         m_pitchAngle = 0.0f, m_pitchVel = 0.0f;
	float         m_accelLong  = 0.0f, m_accelLat = 0.0f;   // 直近の車体座標加速度
	float         m_accelLongF = 0.0f, m_accelLatF = 0.0f;  // 平滑化した加速度(サス入力)
	float         m_dLongF = 0.0f, m_dLatF = 0.0f;          // 平滑化した荷重移動(急なリフトオフ防止)
	float         m_rollV = 0.0f;        // ロール(横荷重移動)の速度。ばね-ダンパの状態
	float         m_wheelFy[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // 各輪の横力(リラクゼーションで遅らせた値)
	float         m_bumpLoad[4] = { 0.0f, 0.0f, 0.0f, 0.0f };// 段差による各輪の荷重の偏り(-1〜1)
	float         m_driveDiff = 0.0f;    // 左右の駆動輪の回転差の半分(デフ)

	// ドリフトスモーク(後輪の煙)
	DriftSmoke    m_smoke;
	float         m_smokeCarry = 0.0f;   // 放出数の端数を蓄積(毎秒レート→整数枚)
	float         m_smokeCarryFront = 0.0f;   // 同・前輪ぶん(こちらはごく少量)
	float         m_driftTint  = 0.0f;   // 車体のアクセントカラー塗り(0〜1, 平滑化済み)

	// タイヤ周りのネオン線画(Unbound風。リング＋スパーク)
	DriftNeon     m_neon;
	float         m_neonRingCarry  = 0.0f;   // 放出数の端数(リング)
	float         m_neonSparkCarry = 0.0f;   // 放出数の端数(スパーク)

	// 路面に残るタイヤ痕。マップはコースに1枚の共有(SkidMark::Instance)で、
	// ここに持つのは自車ぶんの枠の先頭番号だけ。
	int m_skidBase = -1;

	// コントローラー入力(接続時のみアナログ操作を反映)
	HjGamePad     m_pad;

	// 地形の格子。渡されていれば、接地はここから取る。
	// 無ければ従来どおりメッシュへ光線を飛ばす
	const HjHeightField* m_pHeightField = nullptr;

	// 道。地形より先に聞く。借りているだけ
	const HjRoad* m_pRoad = nullptr;

	// 直前に出した運転操作。
	//
	// 追走で後追いのCPUが手本として借りる。
	// 位置と向きだけ渡しても、CPUは釣り合う踏み量を自分で探すことになる。
	// 前を走っている人が既に正解を出しているので、それをそのまま貸す
	DriveInput m_lastInput;

	// エンジン音。RPMとアクセル開度から波形を組み立てて鳴らす
	HjEngineAudio m_engineAudio;
	// タイヤのスキール音とブレーキ鳴き。滑り量と制動力から合成する
	HjTireAudio   m_tireAudio;

	// マニュアルシフトのキーボード用エッジ検出(押した瞬間だけ1段送る)
	bool          m_prevKeyShiftUp   = false;
	bool          m_prevKeyShiftDown = false;

	// 当たり判定：地形などの対象(接地レイ・壁球を飛ばす相手)
	std::vector<std::weak_ptr<KdGameObject>> m_wpHitList;
	Math::Vector3 m_groundNormal = Math::Vector3::Up;   // 接地面の法線(後で重力相対に使う)
	bool          m_onGround      = false;              // 今フレーム接地したか
	float         m_terrainPitch  = 0.0f;              // 地形の前後傾き(rad, 4輪レイから推定)
	float         m_terrainRoll   = 0.0f;              // 地形の左右傾き(rad, 4輪レイから推定)

	// ジャンプ/滞空の手触り調整(ImGuiで生調整可)
	float         m_airGravityMul  = CarConst::AirGravityMul; // 滞空重力倍率(大=ズシッと速い/小=フワッと)
	float         m_jumpLaunch     = 1.0f;                    // ランプの打ち上げ強さ倍率
	float         m_landBounce     = CarConst::LandBounce;    // 着地の跳ね返り

	// ジャンプ/滞空(エビス風ジャンプドリフト)：垂直速度・重力・着地を扱う
	bool          m_airborne       = false;   // 滞空中(タイヤ力なし=横向き/スピンを保持して飛ぶ)
	float         m_velY           = 0.0f;    // 垂直速度(m/s, 上+)
	float         m_groundYFilt    = 0.0f;    // 支持面の高さ(低域通過。メッシュ継ぎ目のガタつき除去)
	float         m_prevGroundY    = 0.0f;    // 前フレームの支持面高さ(上昇速度の算出用)
	float         m_supportVelY    = 0.0f;    // 支持面の上昇速度(平滑化。クレストで打ち上がる勢い)
	bool          m_prevGroundValid = false;  // 前フレームに支持面高さが有効だったか
	// 空中の剛体回転(角運動量)：ランプで付いた回転を空中で保持し空力で弾道へ収束
	float         m_pitchRate      = 0.0f;    // ピッチ角速度(rad/s)
	float         m_rollRate       = 0.0f;    // ロール角速度(rad/s)

	// 当たり判定の可視化(F1トグル)：壁プローブ球＋接地レイをワイヤ表示
	bool          m_debugDraw    = false;
	bool          m_prevDebugKey = false;              // F1のエッジ検出
	bool          m_prevOutlineKey = false;            // F2(煙輪郭トグル)のエッジ検出
	float         m_boostFlash    = -1.0f;            // ブースト演出の経過秒(負=発動していない)
	bool          m_prevTintKey   = false;            // F4のエッジ検出
	bool          m_prevStyleKey   = false;            // F3(文字エフェクト切替)のエッジ検出
};
