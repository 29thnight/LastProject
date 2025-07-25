#pragma once
#include "IRenderPass.h"
#include "Texture.h"

class Camera;
struct ColorGradingPassSetting;
class ColorGradingPass final : public IRenderPass
{
public:
	ColorGradingPass();
	~ColorGradingPass();

	void Initialize(const std::string_view& fileName);
	void Execute(RenderScene& scene, Camera& camera) override;
	void ControlPanel() override;
        void ApplySettings(const ColorGradingPassSetting& setting);

	UniqueTexturePtr m_pColorGradingTexture{};
private:
	Texture* m_pCopiedTexture{};
	ComPtr<ID3D11Buffer> m_Buffer{};
	float lerp = 0.f;
	float timer = 0.f;
	bool isOn{ true };
};

