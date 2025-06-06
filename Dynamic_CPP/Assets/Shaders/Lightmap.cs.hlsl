#include "Sampler.hlsli"
#define MAX_LIGHTS 20
#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

#define LIGHT_DISABLED 0
#define LIGHT_ENABLED 1
#define LIGHT_ENABLED_W_SHADOWMAP 2

#define FLT_MAX 3.402823466E+38f

struct SurfaceInfo
{
    float4 posW;
    float3 N;
    float3 T;
    float3 B;
};

struct Light
{
    float4 position;
    float4 direction;
    float4 color;

    float constantAtt;
    float linearAtt;
    float quadAtt;
    float spotAngle;

    int lightType;
    int status;
    float range;
    float intencity;
};

struct Vertex
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float4 boneIds : BLENDINDICES;
    float4 boneWeight : BLENDWEIGHT;
};
struct Triangle
{
    float4 v0, v1, v2;
    float4 n0, n1, n2;
    float2 uv0, uv1, uv2;
    float2 lightmapUV0, lightmapUV1, lightmapUV2;
    int lightmapIndex;
    float3 pad;
};

struct BVHNode
{
    float3 boundsMin;
    int pad1;
    float3 boundsMax;
    int pad2;
    int left; // child index
    int right; // child index
    int start; // index range for triangles
    int end;
    bool isLeaf;
    int3 pad3;
};

struct Ray
{
    float3 origin;
    float3 dir;
};
// s, u, t, b
RWTexture2D<float4> TargetTexture : register(u0); // 타겟 텍스처 (UAV)
//RWTexture2D<float4> TargetEnvironmentTexture : register(u7); // 타겟 텍스처 (UAV)
RWTexture2D<float4> TargetEnvironmentTexture : register(u1);
RWTexture2D<float4> TargetDirectionalMap : register(u2); // 타겟 텍스처 (UAV)


Texture2D<float4> positionMapTexture : register(t1); // 소스 텍스처 (SRV)
StructuredBuffer<Light> g_Lights : register(t2);
Texture2D<float4> normalMapTexture : register(t3); // 노멀맵까지 적용된 텍스쳐로 받기보단 따로 받아서 계산해서 사용하기.

TextureCube EnvMap : register(t4);  // 리플렉션 프로브처럼 lightmap pass에서 추가 계산하여 사용하는 것이 좋아보임.
Texture2D AoMap : register(t5);

StructuredBuffer<Triangle> triangles : register(t10);
StructuredBuffer<int> TriIndices : register(t11); // BVH에서 삼각형 인덱스
StructuredBuffer<BVHNode> BVHNodes : register(t12);


cbuffer lightMapSetting : register(b0)
{
    float4 globalAmbient;
    float bias;
    int lightSize;
    int useEnvMap;
    int pad;
}

cbuffer CB : register(b1)
{
    int2 Offset; // 타겟 텍스처에서 그릴 위치
    int2 Size; // 복사할 영역 크기
    int useAO;
};

cbuffer transform : register(b2)
{
    matrix worldMat;
};

float4 Sampling(Texture2D tex, SamplerState state, float2 uv, float2 textureSize)
{
    float2 texel = float2(1, 1) / textureSize;
    float4 color = { 0, 0, 0, 1 };
    
    for (int x = -2; x < 3; ++x)
    {
        //[unroll]
        for (int y = -2; y < 3; ++y)
        {
            color += tex.SampleLevel(state, uv + (float2(x, y) * texel), 0);
        }
    }
    color /= 25.0;
    return color;
}
float3 LinearToGamma(float3 color)
{
    return pow(color, 1.0 / 2.2);
}

// 이 함수는 큐브맵 샘플링하는 것이 아님. 정육면체 전개로 샘플링 하는거인듯?
static const float2 invAtan = float2(0.1591, 0.3183);
float2 SampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), -asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}
inline bool RayTriangleIntersect(float3 orig, float3 dir, Triangle tri, out float t, out float3 bary)
{
    //float3 origin = orig + dir * 0.001; // 자신을 때리는 경우 방지.
    
    float3 v0 = tri.v0.xyz, v1 = tri.v1.xyz, v2 = tri.v2.xyz;
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 pvec = cross(dir, edge2);    // 평행한 직선은 외적이 0이 되므로 오류 발생 가능성 있음.
    float det = dot(edge1, pvec);       // 크기가 0인 벡터와의 내적은 0임. 아래에서 처리하므로 문제없음.
    if (abs(det) < 1e-6)                    // 빛이 오는 방향에서 막는 삼각형 검출.
        return false;

    float invDet = 1.0 / det;
    float3 tvec = orig - v0;
    float u = dot(tvec, pvec) * invDet;
    if (u < 0 || u > 1)
        return false;

    float3 qvec = cross(tvec, edge1);
    float v = dot(dir, qvec) * invDet;
    if (v < 0 || u + v > 1)
        return false;

    t = dot(edge2, qvec) * invDet;
    if (t < 0.0001)
        return false;

    bary = float3(1 - u - v, u, v);
    return true;
}
bool IntersectAABB(Ray ray, float3 bmin, float3 bmax)
{
    float3 t1 = (bmin - ray.origin) / ray.dir;
    float3 t2 = (bmax - ray.origin) / ray.dir;
    float3 tmin = min(t1, t2);
    float3 tmax = max(t1, t2);
    float tEnter = max(max(tmin.x, tmin.y), tmin.z);
    float tExit = min(min(tmax.x, tmax.y), tmax.z);
    return tEnter <= tExit && tExit > 0 && tEnter < tExit;
   
}

bool IntersectAABBWithT(float3 rayOrigin, float3 rayDir, float3 minB, float3 maxB, out float tNear)
{
    float3 invDir = 1.0 / rayDir;
    float3 t0 = (minB - rayOrigin) * invDir;
    float3 t1 = (maxB - rayOrigin) * invDir;

    float3 tmin = min(t0, t1);
    float3 tmax = max(t0, t1);

    float tEnter = max(max(tmin.x, tmin.y), tmin.z);
    float tExit = min(min(tmax.x, tmax.y), tmax.z);

    tNear = tEnter;
    return tExit >= tEnter && tExit > 0;
}
bool TraceShadow(float3 origin, float3 dir, float maxDist, out float3 color)
{
    uint stack[64];
    int stackPtr = 0;
    stack[stackPtr++] = 0; // root node

    float hitT;
    float3 bary;

    while (stackPtr > 0)
    {
        uint nodeIndex = stack[--stackPtr];
        BVHNode node = BVHNodes[nodeIndex];
        Ray ray = { origin, dir };

        if (!IntersectAABB(ray, node.boundsMin, node.boundsMax))
            continue;

        if (node.isLeaf)
        {
            for (int i = node.start; i <= node.end; ++i)
            {
                int triIndex = TriIndices[i];
                Triangle tri = triangles[triIndex];
                if (RayTriangleIntersect(origin, dir, tri, hitT, bary) && hitT < maxDist)
                {
                    color = tri.pad;
                    return true;
                }
            }
        }
        else
        {
            //stack[stackPtr++] = node.left;
            //stack[stackPtr++] = node.right;
            
            float tLeft = 0, tRight = 0;
            bool hitLeft = IntersectAABBWithT(ray.origin, ray.dir, BVHNodes[node.left].boundsMin, BVHNodes[node.left].boundsMax, tLeft);
            bool hitRight = IntersectAABBWithT(ray.origin, ray.dir, BVHNodes[node.right].boundsMin, BVHNodes[node.right].boundsMax, tRight);

            if (hitLeft && hitRight)
            {
                stack[stackPtr++] = (tLeft < tRight) ? node.right : node.left;
                stack[stackPtr++] = (tLeft < tRight) ? node.left : node.right;
            }
            else if (hitLeft)
            {
                stack[stackPtr++] = node.left;
            }
            else if (hitRight)
            {
                stack[stackPtr++] = node.right;
            }
        }
    }

    return false;
}

// sRGB/Rec.709 표준 : 명도 계산 함수
float luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 타겟 텍스처 좌표
    float2 targetPos = float2(DTid.xy); // 0 ~ lightmapSize

    // 범위 내 픽셀인지 체크 
    if (targetPos.x < Offset.x || targetPos.x > (Offset.x + Size.x) ||
        targetPos.y < Offset.y || targetPos.y > (Offset.y + Size.y))
    {
        //TargetTexture[DTid.xy] = float4(0, 0, 0, 0);
        return;
    }

    // 타겟 텍스처 좌표를 0~1로 정규화 // targetpos 0~lightmapSize, offset
    float2 localUV = (targetPos + 0.5f - Offset) / Size; 
    
    // 주변값을 샘플링하게 되면 급격한 위치변화 때문에 그림자가 번지듯이 나옴
    //float4 localpos = Sampling(positionMapTexture, LinearSampler, localUV, float2(2048, 2048));
    float4 localNormal = normalMapTexture.SampleLevel(PointSampler, localUV, 0);
    localNormal.a = 0;
    
    
    float4 localpos = positionMapTexture.SampleLevel(PointSampler, localUV, 0);
    
    if(localpos.a != 1)
    {
        TargetTexture[DTid.xy] = float4(0, 0, 0, 0);
        //TargetEnvironmentTexture[DTid.xy] = float4(1, 0, 0, 1);
        return;
    }
    
    //if (localpos.w == 0)
    //{
    //    float3 temp = TargetTexture[DTid.xy].rgb;
    //    return;
    //}
    //float4 localNormal = normalMapTexture.SampleLevel(PointSampler, localUV, 0);
    
    float3 worldPos = (mul(worldMat, localpos)).xyz; // 라이트맵 PositionMap에서 가져온 월드 좌표
    float3 normal = normalize(mul(worldMat, localNormal).xyz); // 라이트맵 NormalMap 또는 기하 정보에서 유도
    float4 finalColor = float4(0, 0, 0, 0);
    float3 dominantDir = float3(0, 0, 0);
    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        Light light = g_Lights[i];
        if (light.status == LIGHT_DISABLED)
            continue;

        float3 toLight;
        float distance = 1000000000.0;
        float attenuation = 1.0;
        float NdotL = 0.0;

        if (light.lightType == DIRECTIONAL_LIGHT)
        {
            toLight = normalize(-light.direction.xyz);
            NdotL = max(dot(normal, toLight), 0.0);
        }
        else
        {
            toLight = light.position.xyz - worldPos;
            distance = length(toLight);
            
            toLight = normalize(toLight);
            NdotL = max(dot(normal, toLight), 0.0);
            
            if (distance > light.range)
                continue;
            
            // Distance attenuation (Point, Spot)
            float att = light.constantAtt + (light.linearAtt * distance) + (light.quadAtt * distance * distance);
            attenuation = 1.0 / att; //1.0 / max(att, 0.001);
            
            
            //// 부드러운 거리감쇠를 생각했으나 너무 선형적임. 1 - (d^2 / r^2)으로? 위에 이미 있었네.
            //float rangeAtt = saturate(1.0 - (distance * distance / (light.range * light.range)));
            //attenuation *= rangeAtt;
        }

        // Spot light extra check
        if (light.lightType == SPOT_LIGHT)
        {
            float3 lightDir = normalize(-light.direction.xyz);
            float spotCos = dot(toLight, lightDir);
            float minCos = cos(light.spotAngle);
            float maxCos = (minCos + 1.0f) / 2.0f; // squash between [0, 1]
            if (spotCos < minCos)
                continue; // outside cone
            // optional: smoothstep for soft edge
            float smoothFactor = smoothstep(minCos, maxCos, spotCos);
            attenuation *= smoothFactor;
            if (attenuation <= 0)
                continue;
        }
        float3 debugColor = float3(0, 0, 0);
        // Shadow check (BVH traversal)
        bool blocked = TraceShadow(worldPos + normal * bias, toLight, distance, debugColor);
        finalColor.a = 1;
        if (!blocked)
        {
            finalColor.rgb += light.color.rgb * NdotL * attenuation /** light.intencity*/;
            dominantDir += -toLight * luminance(light.color.rgb * NdotL * attenuation) /* * light.intencity*/;
        }
    }
    SurfaceInfo surf;
    surf.N = normal.xyz;
    float3 ambient = globalAmbient.rgb;
    if (useEnvMap)
    {
        float3 irradiance = EnvMap.SampleLevel(LinearSampler, surf.N, 0.0).rgb;
        float3 diffuse = irradiance;
        ambient.rgb = diffuse;
    }
    float ao = useAO ? AoMap.SampleLevel(PointSampler, localUV.xy, 0.0).r : 1.0;
    
    ambient *= ao; // * occlusion;
    //finalColor.rgb += ambient;
    
    TargetTexture[DTid.xy] = finalColor;
    TargetEnvironmentTexture[DTid.xy] = float4(ambient, 1);
    TargetDirectionalMap[DTid.xy] = float4(dominantDir, 1);

}

/*
1. 라이트맵 uv에 해당하는 픽셀을 뽑고
이 픽셀의 uv값으로 positionMap에서 localPosition을 가져옴.
2. 이 localPosition을 lightViewProj로 변환하여 섀도우맵에서 깊이값을 샘플링.
3. 섀도우맵에서 샘플링한 깊이값과 lightSpacePos.z / lightSpacePos.w를 비교하여 그림자 여부를 결정.
4. 그림자 여부에 따라 라이트의 영향을 계산하여 color에 더함.
5. color를 타겟 텍스처에 기록.

*/