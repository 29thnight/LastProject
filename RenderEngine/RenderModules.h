#pragma once
#include "Core.Minimal.h"
#include "PSO.h"
#include "DeviceState.h"
#include "GameObject.h"

enum class BillBoardType
{
    Basic,
    YAxs,
    AxisAligned
};

struct BillboardVertex
{
    Mathf::Vector4 Position;  // Position in clip space
    Mathf::Vector2 TexCoord;  // Texture coordinates
    UINT TexIndex;            // Texture array index
    float Padding;            // Padding for alignment
    Mathf::Vector4 Color;     // Color/tint for the billboard
};

struct BillboardInstance
{
    Mathf::Vector3 Position;  // Position in world space
    float Padding1;           // Padding for 16-byte alignment
    Mathf::Vector2 Scale;     // Width and height scale
    UINT TexIndex;            // Texture array index
    float Padding2;           // Padding for 16-byte alignment
    Mathf::Vector4 Color;     // Color/tint for the billboard
};

struct CameraConstants
{
    Mathf::Matrix view;
    Mathf::Matrix projection;
    Mathf::Vector3 cameraRight;
    float padding1;
    Mathf::Vector3 cameraUp;
    UINT billboardCount;
};

struct ModelConstantBuffer
{
    Mathf::Matrix world;
    Mathf::Matrix view;
    Mathf::Matrix projection;
};

class RenderModules
{
public:
    virtual void Initialize() {}
    virtual void Render(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection) {}
    void movePSO(std::unique_ptr<PipelineStateObject> pso) { m_pso.swap(pso); }
    PipelineStateObject* GetPSO() { return m_pso.get(); }
    virtual void SetupInstancing(void* instanceData, UINT count) = 0;

    void CleanupRenderState();
    void SaveRenderState();
    void RestoreRenderState();

protected:
    std::unique_ptr<PipelineStateObject> m_pso;

private:
    ID3D11DepthStencilState* m_prevDepthState = nullptr;
    UINT m_prevStencilRef = 0;
    ID3D11BlendState* m_prevBlendState = nullptr;
    float m_prevBlendFactor[4] = { 0 };
    UINT m_prevSampleMask = 0;
    ID3D11RasterizerState* m_prevRasterizerState = nullptr;

};


class BillboardModule : public RenderModules
{
public:


public:

    void Initialize() override;
    void ProcessBillboards(const std::vector<BillboardInstance>& instance);
    void Render(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection) override;

    BillBoardType GetBillboardType() const { return m_BillBoardType; }
    PipelineStateObject* GetPSO() { return m_pso.get(); }
    void SetupInstancing(BillboardInstance* instanceData, UINT count);
    void SetBillboardType(BillBoardType type) { m_BillBoardType = type; }

    void DebugPrintComputeBufferContent();

    virtual void SetupInstancing(void* instanceData, UINT count) override {
        SetupInstancing(static_cast<BillboardInstance*>(instanceData), count);
    }

private:
    BillBoardType m_BillBoardType;
    UINT m_instanceCount;
    UINT m_maxCount;
    BillboardVertex* mVertex;

    Microsoft::WRL::ComPtr<ID3D11Buffer> billboardVertexBuffer;
    ComPtr<ID3D11Buffer> billboardIndexBuffer;
    ComPtr<ID3D11Buffer> instanceBuffer;
    ComPtr<ID3D11ShaderResourceView> instanceSRV;
    ComPtr<ID3D11UnorderedAccessView> vertexBufferUAV;
    ComPtr<ID3D11Buffer> billboardComputeBuffer;
    ComPtr<ID3D11Buffer> cameraConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_ModelBuffer;

    ModelConstantBuffer m_ModelConstantBuffer;

    std::vector<BillboardVertex> Quad
    {
        { { -1.0f, 1.0f, 0.0f, 1.0f}, { 0.0f, 0.0f } },  // 좌상단
        { { 1.0f,  1.0f, 0.0f, 1.0f}, { 1.0f, 0.0f} },   // 우상단
        { { 1.0f, -1.0f, 0.0f, 1.0f}, { 1.0f, 1.0f} },   // 우하단
        { {-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },    // 좌하단

    };
    std::vector<uint32> Indices = { 0, 1, 2, 0, 2, 3 };

    std::vector<BillboardVertex> m_vertices;
    std::vector<uint32> m_indices;
};

class GameObject;
class MeshModule : public RenderModules
{
public:
    virtual void Initialize();
    virtual void Render(Mathf::Matrix world, Mathf::Matrix view, Mathf::Matrix projection);
    virtual void SetupInstancing(void* instanceData, UINT count);

    void SetGameObject(GameObject* obj) { m_gameObject = obj; }
    
private:
    GameObject* m_gameObject;
};