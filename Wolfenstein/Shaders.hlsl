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

// 라이트 정보 (방향광 + 앰비언트) — PS 에서 사용.
cbuffer cbLight : register(b2)
{
    float3 gLightDir;   float gLightPad0;   // 정규화된 빛 진행 방향
    float3 gLightColor; float gLightPad1;   // 빛 색상
    float3 gAmbient;    float gLightPad2;   // 환경광
};

// 매쉬 삼각형마다의 face normal (로컬 좌표) — PS 가 SV_PrimitiveID 로 조회.
// float4 배열로 정렬해 cbuffer 한 슬롯에 담는다. 큐브 = 12 triangle.
cbuffer cbFaceNormals : register(b3)
{
    float4 gFaceNormals[12];
};

// 정점 셰이더를 정의한다.

// 정점 셰이더의 입력을 위한 구조체를 선언한다.
struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

// 정점 셰이더의 출력(픽셀 셰이더의 입력)을 위한 구조체를 선언한다.
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

// 정점 셰이더를 정의한다.
VS_OUTPUT VSDiffused(VS_INPUT input)
{
    VS_OUTPUT output;

    // 정점을 변환(월드-카메라-투영)한다.
    output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxWorld), gmtxView), gmtxProjection);
    output.color = input.color;

    return output;
}

// 픽셀 셰이더 — 퐁 모델의 디퓨즈 항 + 앰비언트.
// primId(SV_PrimitiveID) 로 face normal 을 조회하므로 flat shading 결과가 된다.
float4 PSDiffused(VS_OUTPUT input, uint primId : SV_PrimitiveID) : SV_TARGET
{
    // 1) 로컬 face normal → 월드 변환 (균등 스케일 가정).
    float3 nLocal = gFaceNormals[primId].xyz;
    float3 nWorld = normalize(mul(nLocal, (float3x3)gmtxWorld));

    // 2) Lambert 디퓨즈: max(N · (-L), 0). L 은 빛이 진행하는 방향.
    float ndl = saturate(dot(nWorld, -normalize(gLightDir)));
    float3 diffuse = gLightColor * ndl;

    // 3) 앰비언트 + 디퓨즈 합산 후 정점 색상에 변조.
    float3 lit = (gAmbient + diffuse) * input.color.rgb;
    return float4(lit, input.color.a);
}

// =============================================================
// 스크린 스페이스 HUD 셰이더 (조준점 십자선용)
// 입력 정점을 그대로 NDC(클립 공간) 좌표로 사용한다.
// 월드/뷰/투영 행렬을 거치지 않으므로 카메라 회전과 무관하게
// 화면 정중앙에 회전 없이 고정 렌더링된다.
// 정점 z 는 0, w 는 1 로 채워 깊이 비교에서 항상 통과하도록 한다.
// =============================================================
struct VS_HUD_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VS_HUD_OUTPUT VSHud(VS_INPUT input)
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