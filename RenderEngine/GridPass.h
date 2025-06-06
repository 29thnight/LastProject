#pragma once
#include "IRenderPass.h"
#include "Texture.h"

struct alignas(16) GridConstantBuffer
{
    Mathf::Matrix world;
    Mathf::Matrix view;
    Mathf::Matrix projection;
};

// Vertex 구조체 정의
struct alignas(16) GridVertex
{
    XMFLOAT3 pos;
	XMFLOAT2 uv0;
};

struct alignas(16) GridUniformBuffer
{
    float4 gridColor{ XMFLOAT4(0.45f, 0.44f, 0.43f, 0.5f) };
    float4 checkerColor{ XMFLOAT4(0.41f, 0.38f, 0.36f, 0.f) };
    float fadeStart{ 300.f };
    float fadeEnd{ 1000.f };
    float unitSize{ 10.f };
    int subdivisions{ 10 };
    float3 centerOffset{ 0, 0, 0 };
    float majorLineThickness{ 2.f };
    float minorLineThickness{ 0.5f };
    float minorLineAlpha{ 1.f };
};

class Camera;
class GridPass final : public IRenderPass
{
public:
	GridPass();
	~GridPass();
	void PrepareCameraType(Camera* camera);
	void Execute(RenderScene& scene, Camera& camera) override;
	void ControlPanel() override;
    void GridSetting();
    virtual void Resize(uint32_t width, uint32_t height) override;

    ID3D11ShaderResourceView* GetGridSRV() const { return m_gridTexture->m_pSRV; }

private:
	Camera* m_pEditorCamera;
    Texture* m_colorTexture;
    Texture* m_gridTexture;
    GridConstantBuffer m_gridConstant{};
    GridUniformBuffer m_gridUniform{};
    ComPtr<ID3D11Buffer> m_pGridConstantBuffer;
    ComPtr<ID3D11Buffer> m_pVertexBuffer;
	ComPtr<ID3D11Buffer> m_pIndexBuffer;
    ComPtr<ID3D11Buffer> m_pUniformBuffer;
    ComPtr<ID3D11Buffer> m_pCamPosBuffer;
};
