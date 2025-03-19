#include "Snow.hlsli"

//--------------------------------------------------------------------------------------
// Snow Vertex Shader
//--------------------------------------------------------------------------------------

// 간단한 패스스루 버텍스 셰이더
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;
    output.Position = float4(0, 0, 0, 1);
    return output;
}