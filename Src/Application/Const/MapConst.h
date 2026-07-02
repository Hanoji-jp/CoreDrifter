#pragma once

// マップエディター・マップ配置に関わる定数
namespace MapConst
{
    // セーブ・ロードするJSONファイルのパス
    constexpr const char* SaveFilePath   = "Asset/Data/mapdata.json";

    // エディターで選択できるモデルのリスト
    constexpr const char* ModelListPath  = "Asset/Data/";

    // 配置オブジェクトの移動スナップ単位
    constexpr float SnapSize             = 1.0f;

    // エディターカメラ移動速度
    constexpr float EditorCameraMoveSpeed = 0.2f;

    // エディターカメラ回転速度
    constexpr float EditorCameraRotSpeed  = 0.005f;
}

