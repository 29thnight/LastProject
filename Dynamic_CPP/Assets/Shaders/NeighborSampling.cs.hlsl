RWTexture2D<float4> TargetTexture : register(u0); // Ÿ�� �ؽ�ó (UAV)
Texture2D<float4> InputTexture : register(t0);
// sobel filter algorithm

float2 Arrow[4] =
{
    { 1, 0 },
    { 0, 1 },
    { -1, 0 },
    { 0, -1 }
};

// Sobel ���� (���� ����)
float SobelEdgeDetection(Texture2D<float4> tex, int2 uv)
{
    float4 Gx =
        -tex.Load(int3(uv + int2(-1, -1), 0)) - 2 * tex.Load(int3(uv + int2(-1, 0), 0)) - tex.Load(int3(uv + int2(-1, 1), 0)) +
         tex.Load(int3(uv + int2(1, -1), 0)) + 2 * tex.Load(int3(uv + int2(1, 0), 0)) + tex.Load(int3(uv + int2(1, 1), 0));

    float4 Gy =
        -tex.Load(int3(uv + int2(-1, -1), 0)) - 2 * tex.Load(int3(uv + int2(0, -1), 0)) - tex.Load(int3(uv + int2(1, -1), 0)) +
         tex.Load(int3(uv + int2(-1, 1) , 0)) + 2 * tex.Load(int3(uv + int2(0, 1) , 0)) + tex.Load(int3(uv + int2(1, 1) , 0));

    return length(Gx.rgb + Gy.rgb);
}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int2 uv = DTid.xy;
    
    //float edge = SobelEdgeDetection(InputTexture, uv);
    
    float4 color = InputTexture[uv];

    //// ���� �ȼ��̶�� �ֺ� �ȼ� �������� (��â ȿ��)
    //if (edge > 0.1)
    //{
    //    for (int y = -1; y <= 1; y++)
    //    {
    //        for (int x = -1; x <= 1; x++)
    //        {
    //            float4 neighborColor = InputTexture[uv + int2(x, y)];
    //            if (neighborColor.a > 0.8)
    //            {
    //                color = neighborColor;
    //                break;
    //            }
    //        }
    //    }
    //    //color /= 9.0;
    //}
    //TargetTexture[uv] = color;
    
    
    //for (int i = 0; i < 4; i++)
    //{
    //    
    //}
    if (color.a > 0)
    {
        TargetTexture[uv] = color;
        return;
    }
    
    //TargetTexture[uv] = color;
    //TargetTexture[uv] = InputTexture.Load(float3(uv, 0)) / 9.0;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            if (x == 0 && y == 0)
                continue;
            float4 neighborColor = InputTexture[uv + int2(x, y)];
            if (neighborColor.a == 1)
            {
                color = neighborColor;
                break;
            }
        }
    }
    TargetTexture[uv] = color;
}