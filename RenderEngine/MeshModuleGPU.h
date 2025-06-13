#pragma once
#include "RenderModules.h"

enum class MeshType
{
    Cube,
    Sphere,
    Custom
};                

struct MeshVertex
{
    Mathf::Vector4 position;
    Mathf::Vector3 normal;
    Mathf::Vector2 texCoord;
};

// 메쉬 불러서 쓰기 (물어볼것) 일단 해보기
struct MeshConstantBuffer
{
    Mathf::Matrix world;
    Mathf::Matrix view;
    Mathf::Matrix projection;
    Mathf::Vector3 cameraPosition;
    float padding;
};



class MeshModuleGPU
{
};

