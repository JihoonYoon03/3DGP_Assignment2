// 게임 객체의 정보를 위한 상수 버퍼 선언
cbuffer cbGameObjectInfo : register(b0)
{
    matrix gmtxWorld : packoffset(c0);
};

cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView : packoffset(c0);
    matrix gmtxProjection : packoffset(c4);
};

// 라이트 상수 (퐁 디퓨즈) — Scene.cpp 의 SetGraphicsRoot32BitConstants(2, ...) 와 일치.
// 각 float3 뒤에 1 float 패딩을 두어 16 byte 정렬을 맞춘다 (총 12 floats = 48 bytes).
cbuffer cbLightInfo : register(b2)
{
    float3 gLightDirWorld : packoffset(c0);   // 라이트가 진행하는 방향 (정규화)
    float  gLightPad0     : packoffset(c0.w);
    float3 gLightColor    : packoffset(c1);   // 디퓨즈 색 (Lambert 항에 곱해짐)
    float  gLightPad1     : packoffset(c1.w);
    float3 gAmbientColor  : packoffset(c2);   // 환경광 (그림자 측에도 최소 밝기 보장)
    float  gLightPad2     : packoffset(c2.w);
};

// =============================================================
// 디퓨즈 라이팅용 정점 셰이더
// slot 0: POSITION + COLOR
// slot 1: NORMAL (병렬 정점 버퍼)
// =============================================================
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

    // 정점을 변환(월드-카메라-투영)한다.
    output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxWorld), gmtxView), gmtxProjection);

    // 노멀을 월드 공간으로 변환한다. 균등 스케일/회전만 사용한다는 가정하에 world 의 상위 3x3 사용.
    output.normalWorld = mul(input.normal, (float3x3) gmtxWorld);

    output.color = input.color;
    return output;
}

// =============================================================
// 픽셀 셰이더 — Lambert 디퓨즈 + 환경광
// =============================================================
float4 PSDiffused(VS_OUTPUT input) : SV_TARGET
{
    // 보간된 노멀은 일반적으로 단위 벡터가 아니므로 픽셀 단위로 재정규화.
    float3 N = normalize(input.normalWorld);
    // 라이트가 진행하는 방향의 역이 표면에서 빛 쪽으로 향하는 벡터.
    float3 L = normalize(-gLightDirWorld);
    float  ndotl = saturate(dot(N, L));

    // 디퓨즈 = baseColor * (ambient + lightColor * Lambert)
    float3 lit = input.color.rgb * (gAmbientColor + gLightColor * ndotl);
    return float4(lit, input.color.a);
}

// =============================================================
// 스크린 스페이스 HUD 셰이더 (조준점 십자선 / 라이프 바)
// 입력 정점을 그대로 NDC(클립 공간) 좌표로 사용한다.
// 월드/뷰/투영 행렬을 거치지 않으므로 카메라 회전과 무관하게
// 화면 정중앙에 회전 없이 고정 렌더링된다.
// 정점 z 는 0, w 는 1 로 채워 깊이 비교에서 항상 통과하도록 한다.
// =============================================================
// HUD 입력은 NORMAL 슬롯이 없으므로(슬롯 1 미바인딩) 별도 구조체로 분리한다.
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
    // input.position.xy 는 NDC([-1,1]) 범위로 미리 정해진 값.
    output.position = float4(input.position.x, input.position.y, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSHud(VS_HUD_OUTPUT input) : SV_TARGET
{
    return input.color;
}
