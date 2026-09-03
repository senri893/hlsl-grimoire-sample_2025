#include "stdafx.h"
#include "system/system.h"
#include <time.h>
#include <Windows.h>
#include <sstream>
#include <fstream>
#include "tkFile/TkmFile.h"

// 指定した .tkm ファイルを解析して、マテリアル一覧を返す
static std::string BuildTkmMaterialList(const std::string& tkmFilePath)
{
    std::stringstream ss;
    TkmFile tkm;
    tkm.Load(tkmFilePath.c_str());
    ss << "TKM: " << tkmFilePath << "\n";
    const auto& meshes = tkm.GetMeshParts();
    for (size_t mi = 0; mi < meshes.size(); ++mi) {
        const auto& mesh = meshes[mi];
        ss << "  Mesh[" << mi << "] materials: " << mesh.materials.size() << "\n";
        for (size_t matIdx = 0; matIdx < mesh.materials.size(); ++matIdx) {
            const auto& mat = mesh.materials[matIdx];
            ss << "    Material[" << matIdx << "]: ";
            ss << "albedo=" << (mat.albedoMapFileName.empty() ? "(none)" : mat.albedoMapFileName) << ", ";
            ss << "normal=" << (mat.normalMapFileName.empty() ? "(none)" : mat.normalMapFileName) << ", ";
            ss << "specular=" << (mat.specularMapFileName.empty() ? "(none)" : mat.specularMapFileName) << ", ";
            ss << "reflection=" << (mat.reflectionMapFileName.empty() ? "(none)" : mat.reflectionMapFileName) << ", ";
            ss << "refraction=" << (mat.refractionMapFileName.empty() ? "(none)" : mat.refractionMapFileName) << "\n";
        }
    }
    ss << "\n";
    return ss.str();
}

// CSV 向けに値をエスケープして引用符で囲む
static std::string EscapeCsv(const std::string& v)
{
    std::string out = v;
    // ダブルクォートを2つにする
    size_t pos = 0;
    while ((pos = out.find('"', pos)) != std::string::npos) {
        out.insert(pos, "\"");
        pos += 2;
    }
    return std::string("\"") + out + "\"";
}

// 指定した .tkm を CSV 形式で返す（ヘッダー含む）
static std::string BuildTkmMaterialCSV(const std::string& tkmFilePath)
{
    std::stringstream ss;
    ss << "TKM Path,MeshIndex,MaterialIndex,Albedo,Normal,Specular,Reflection,Refraction\n";
    TkmFile tkm;
    tkm.Load(tkmFilePath.c_str());
    const auto& meshes = tkm.GetMeshParts();
    for (size_t mi = 0; mi < meshes.size(); ++mi) {
        const auto& mesh = meshes[mi];
        for (size_t matIdx = 0; matIdx < mesh.materials.size(); ++matIdx) {
            const auto& mat = mesh.materials[matIdx];
            ss << EscapeCsv(tkmFilePath) << "," << mi << "," << matIdx << ",";
            ss << EscapeCsv(mat.albedoMapFileName) << "," << EscapeCsv(mat.normalMapFileName) << ",";
            ss << EscapeCsv(mat.specularMapFileName) << "," << EscapeCsv(mat.reflectionMapFileName) << ",";
            ss << EscapeCsv(mat.refractionMapFileName) << "\n";
        }
    }
    return ss.str();
}

const int NUM_DIRECTIONAL_LIGHT = 4; // ディレクションライトの数

/// <summary>
/// ディレクションライト
/// </summary>
struct DirectionalLight
{
    Vector3 direction;  // ライトの方向
    float pad0;         // パディング
    Vector4 color;      // ライトのカラー
};

/// <summary>
/// ライト構造体
/// </summary>
struct Light
{
    DirectionalLight directionalLight[NUM_DIRECTIONAL_LIGHT]; // ディレクションライト
    Vector3 eyePos;             // カメラの位置
    float specPow;              // スペキュラの絞り
    Vector3 ambinetLight;       // 環境光
};

#define USE_UNITY_CHAN 0
#define USE_NINJA 1
#define USE_ROBO 2

#define USE_MODEL 2

///////////////////////////////////////////////////////////////////
// ウィンドウプログラムのメイン関数
///////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // ゲームの初期化
    InitGame(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));

    srand(time(nullptr));
    //////////////////////////////////////
    // ここから初期化を行うコードを記述する
    //////////////////////////////////////

    // 3Dモデルを作成
    Model model, bgModel;
    ModelInitData initData;

    initData.m_tkmFilePath = "Assets/modelData/sample.tkm";
    // ここで透過率と屈折率用の拡張定数バッファを渡す。
    // Sample.fx 側で同じレイアウトの定数バッファを参照します。
    struct MaterialParams {
        float opacity; // 0.0 = 完全透明, 1.0 = 不透明
        float ior;     // 屈折率（未使用だが将来利用可能）
        float pad[2];
    };
    static MaterialParams s_defaultMaterialParams = { 1.0f, 1.0f, {0.0f, 0.0f} };

    // デフォルトのモデルは不透明。透過を確認したいモデルはここで値を変える。
    initData.m_fxFilePath = "Assets/shader/NoAnimModel_PBR.fx";
    initData.m_expandConstantBuffer = &s_defaultMaterialParams;
    initData.m_expandConstantBufferSize = sizeof(s_defaultMaterialParams);

    // materialのリストを出力
    std::string materialList = BuildTkmMaterialList(initData.m_tkmFilePath);
    MessageBoxA(nullptr, materialList.c_str(), "TKM Material List", MB_OK);
    // CSV 出力（カレントパス）
    {
        std::string csv = BuildTkmMaterialCSV(initData.m_tkmFilePath);
        // ファイル名を作成（sample.tkm -> sample_materials.csv）
        std::string path = initData.m_tkmFilePath;
        auto pos = path.find_last_of("\\/");
        std::string fname = (pos == std::string::npos) ? path : path.substr(pos + 1);
        auto dot = fname.find_last_of('.');
        std::string base = (dot == std::string::npos) ? fname : fname.substr(0, dot);
        std::string outName = base + "_materials.csv";
        std::ofstream ofs(outName, std::ios::binary);
        if (ofs) ofs << csv;
    }

    model.Init(initData);

    // sample.tkm のモデルを透過状態にするサンプル: s_defaultMaterialParams を変更して
    // 透過を有効にする（開発中はここで値を変更して動作確認を行ってください）。
    // 例: 透過にする
    // s_defaultMaterialParams.opacity = 0.5f;
    // model は既に初期化済みなので、m_expandConstantBuffer を利用する場合は
    // Model 側で再設定が必要なケースがあります。簡便のため、ここでは初期化時に
    // 渡すパターンを使っています。

    initData.m_tkmFilePath = "Assets/modelData/bg/bg.tkm";
    bgModel.Init(initData);

    // materialのリストを出力
    materialList = BuildTkmMaterialList(initData.m_tkmFilePath);
    MessageBoxA(nullptr, materialList.c_str(), "TKM Material List", MB_OK);
    // CSV 出力（カレントパス）
    {
        std::string csv = BuildTkmMaterialCSV(initData.m_tkmFilePath);
        std::string path = initData.m_tkmFilePath;
        auto pos = path.find_last_of("\\/");
        std::string fname = (pos == std::string::npos) ? path : path.substr(pos + 1);
        auto dot = fname.find_last_of('.');
        std::string base = (dot == std::string::npos) ? fname : fname.substr(0, dot);
        std::string outName = base + "_materials.csv";
        std::ofstream ofs(outName, std::ios::binary);
        if (ofs) ofs << csv;
    }

    //////////////////////////////////////
    // 初期化を行うコードを書くのはここまで！！！
    //////////////////////////////////////
    auto& renderContext = g_graphicsEngine->GetRenderContext();

    g_camera3D->SetPosition(0.0f, 50.0f, 120.0f);
    g_camera3D->SetTarget(0.0f, 50.0f, 200.0f);

    g_graphicsEngine->RegistModelToRaytracingWorld(model);
    g_graphicsEngine->RegistModelToRaytracingWorld(bgModel);
    g_graphicsEngine->BuildRaytracingWorld(renderContext);

    // ここからゲームループ
    while (DispatchWindowMessage())
    {
        // レンダリング開始
        g_engine->BeginFrame();

        if (g_pad[0]->IsPress(enButtonLB1))
        {
            g_camera3D->MoveUp(g_pad[0]->GetLStickYF());
        }
        else
        {
            g_camera3D->MoveForward(-g_pad[0]->GetLStickYF());
        }
        g_camera3D->MoveRight(g_pad[0]->GetLStickXF());
        Quaternion qRotX, qRotY;
        qRotX.SetRotationX(g_pad[0]->GetRStickYF() * -0.005f);
        g_camera3D->RotateOriginTarget(qRotX);
        qRotY.SetRotationY(g_pad[0]->GetRStickXF() * 0.005f);
        g_camera3D->RotateOriginTarget(qRotY);

        //////////////////////////////////////
        // ここから絵を描くコードを記述する
        //////////////////////////////////////
        g_graphicsEngine->DispatchRaytracing(renderContext);

        //////////////////////////////////////
        // 絵を描くコードを書くのはここまで！！！
        //////////////////////////////////////
        // レンダリング終了
        g_engine->EndFrame();
    }
    return 0;
}
