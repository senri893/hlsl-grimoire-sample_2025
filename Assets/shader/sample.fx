// sample.fx - 簡易レイトレースシェーダ（透過率と屈折率のパラメータを追加）
// 注意: このシェーダはプロジェクト内の既存のルートシグネチャやリソース配置に合わせて
// 非破壊で動作するよう最小限の実装を行っています。

// DXR HLSL
// 必須のエントリポイント:
//   rayGen  - レイ生成
//   miss    - ミスシェーダ（背景色を返す）
//   chs     - 近接ヒットシェーダ（マテリアル計算、アルファ取得）
//   shadowChs, shadowMiss - 影用（簡易実装）

struct RayPayload {
    float4 color;            // 出力色
    float4 reflectionColor;  // reflectionColor.w を opacity として利用
    float4 hit_depth;        // 予備
};

struct Attributes {
    float2 barycentrics : BARYCENTRICS;
};

// グローバルリソース（PSO側のルートシグネチャにマッチする想定）
RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// 簡易定数バッファ（グローバル）: 背景色など
cbuffer GlobalCB : register(b0)
{
    float4 gBackgroundColor; // 背景色
    float4 gPad;
}

// マテリアル用の追加パラメータ（main.cpp で渡した拡張定数バッファの想定レイアウト）
cbuffer MaterialParamsCB : register(b1)
{
    float gOpacity; // 0.0 = fully transparent, 1.0 = opaque
    float gIOR;     // 屈折率（今回の簡易実装では未使用）
    float2 gPad2;
}

// シンプルなフェイク・マテリアル色計算
float3 ComputeSurfaceColor() {
    // 本来はテクスチャや法線を参照するが、簡易実装として固定色を返す
    return float3(0.8f, 0.4f, 0.2f);
}

[shader("raygeneration")]
void rayGen()
{
    uint2 dispatchIndex = DispatchRaysIndex();
    uint2 dispatchDim = DispatchRaysDimensions();

    // カメラレイの生成は簡易: スクリーンスペースをワールドに変換する実装は省略
    // このサンプルではレイ生成ロジックを簡単化し、TraceRay を直接呼ぶ最小構成にします。

    RayDesc ray;
    ray.Origin = float3(0.0f, 0.0f, 0.0f);
    ray.Direction = normalize(float3((dispatchIndex.x / (float)dispatchDim.x) - 0.5f,
                                   (dispatchIndex.y / (float)dispatchDim.y) - 0.5f,
                                   1.0f));
    ray.TMin = 0.001f;
    ray.TMax = 1e6;

    RayPayload payload;
    payload.color = float4(0,0,0,0);
    payload.reflectionColor = float4(0,0,0,1); // default opacity = 1
    payload.hit_depth = float4(0,0,0,0);

    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

    // payload.reflectionColor.w に書かれた opacity を使って背景色と線形補間
    float opacity = saturate(payload.reflectionColor.w);
    float3 finalColor = lerp(gBackgroundColor.xyz, payload.color.xyz, opacity);

    gOutput[dispatchIndex] = float4(finalColor, 1.0f);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
    // ミスは背景色を返す
    payload.color = gBackgroundColor;
    payload.reflectionColor.w = 1.0f; // ミスは不透明扱い
}

[shader("closesthit")]
void chs(inout RayPayload payload, in Attributes attr)
{
    // 簡易的にサーフェス色を計算
    float3 surf = ComputeSurfaceColor();

    // アルファ（透過率）は以下の順で決める:
    // 1) マテリアル用定数バッファ gOpacity が 0 or 1 ではない場合はそれを使う
    // 2) それ以外は不透明
    float opacity = saturate(gOpacity);

    // 設定された opacity を payload の予約フィールドに格納（rayGen で参照される）
    payload.color = float4(surf, 1.0f);
    payload.reflectionColor.w = opacity;
}

[shader("closesthit")]
void shadowChs(inout RayPayload payload, in Attributes attr)
{
    // 簡易シャドウヒットは不透明として扱う
    payload.reflectionColor.w = 1.0f;
}

[shader("miss")]
void shadowMiss(inout RayPayload payload)
{
    // シャドウミスは影がないので 1.0（光が届く）
    payload.reflectionColor.w = 1.0f;
}
