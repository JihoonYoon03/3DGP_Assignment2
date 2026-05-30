// 게임 객체의 정보를 위한 상수 버퍼
cbuffer cbGameObjectInfo : register(b0)
{
    matrix gmtxWorld : packoffset(c0);
};

cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView : packoffset(c0);
    matrix gmtxProjection : packoffset(c4);
};

// 방향광 + 환경광, 16바이트 정렬 패딩
cbuffer cbLightInfo : register(b2)
{
    float3 gLightDirWorld : packoffset(c0);
    float  gLightPad0     : packoffset(c0.w);
    float3 gLightColor    : packoffset(c1);
    float  gLightPad1     : packoffset(c1.w);
    float3 gAmbientColor  : packoffset(c2);
    float  gLightPad2     : packoffset(c2.w);
};

// 피격 비네트 강도 (PS 전용)
cbuffer cbScreenFx : register(b3)
{
    float gHitFlash : packoffset(c0.x);
};

// 디퓨즈 라이팅 정점 셰이더 (slot 0: POS+COLOR, slot 1: NORMAL)
struct VS_INPUT
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
};

struct VS_OUTPUT
{
    float4 position    : SV_POSITION;
    float4 color       : COLOR;
    float3 normalWorld : NORMAL;
};

VS_OUTPUT VSDiffused(VS_INPUT input)
{
    VS_OUTPUT output;

    // WVP
    output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxWorld), gmtxView), gmtxProjection);

    // 노멀을 월드 공간으로 변환
    output.normalWorld = mul(input.normal, (float3x3) gmtxWorld);

    output.color = input.color;
    return output;
}

float4 PSDiffused(VS_OUTPUT input) : SV_TARGET
{
    float3 N = normalize(input.normalWorld);
    float3 L = normalize(-gLightDirWorld);
    float  ndotl = saturate(dot(N, L));

    float3 lit = input.color.rgb * (gAmbientColor + gLightColor * ndotl);
    return float4(lit, input.color.a);
}

// HUD 셰이더 (정점이 이미 NDC 좌표, 월드/뷰/투영 통과 안 함)
struct VS_HUD_INPUT
{
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct VS_HUD_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VS_HUD_OUTPUT VSHud(VS_HUD_INPUT input)
{
    VS_HUD_OUTPUT output;
    output.position = float4(input.position.x, input.position.y, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSHud(VS_HUD_OUTPUT input) : SV_TARGET
{
    return input.color;
}

// 피격 비네트 오버레이 (외곽으로 갈수록 짙은 빨강, gHitFlash 로 강도 조절)
struct VS_OVERLAY_OUTPUT
{
    float4 position : SV_POSITION;
    float2 ndc      : TEXCOORD0;
};

VS_OVERLAY_OUTPUT VSHitOverlay(VS_HUD_INPUT input)
{
    VS_OVERLAY_OUTPUT output;
    output.position = float4(input.position.x, input.position.y, 0.0f, 1.0f);
    output.ndc      = float2(input.position.x, input.position.y);
    return output;
}

float4 PSHitOverlay(VS_OVERLAY_OUTPUT input) : SV_TARGET
{
    float r = length(input.ndc) * 0.7071f;
    float mask = smoothstep(0.45f, 1.0f, r);
    float a    = mask * gHitFlash;
    if (a < 0.005f) discard;
    return float4(0.85f, 0.05f, 0.05f, a);
}
