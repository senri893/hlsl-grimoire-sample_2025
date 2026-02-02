/*!
 * @brief チェッカーボードワイプ
 */

cbuffer cb : register(b0)
{
    float4x4 mvp; // MVP行列
    float4 mulColor; // 乗算カラー
};

cbuffer NagaCB : register(b1)
{
    float monochromeRate; // ネガポジ反転率
};

struct VSInput
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D<float4> colorTexture : register(t0); // カラーテクスチャ
sampler Sampler : register(s0);

PSInput VSMain(VSInput In)
{
    PSInput psIn;
    psIn.pos = mul(mvp, In.pos);
    psIn.uv = In.uv;
    return psIn;
}

float4 PSMain(PSInput In) : SV_Target0
{
    float4 color = colorTexture.Sample(Sampler, In.uv);

    // =====================
    // グレースケール作成
    // =====================
    float gray = dot(color.rgb, float3(0.299, 0.587, 0.114));
    float3 grayColor = float3(gray, gray, gray);

    // =====================
    // チェッカーワイプ
    // =====================
    float2 uv = In.uv * 20.0; // マスの細かさ

    int cx = (int) floor(uv.x);
    int cy = (int) floor(uv.y);

    float checker = (cx + cy) & 1;

    // negaRate をワイプ進行率として使う
    float mask = step(checker, monochromeRate);

    // =====================
    // 元 → グレー
    // =====================
    float3 result = lerp(color.rgb, grayColor, mask);

    return float4(result, color.a);
}

