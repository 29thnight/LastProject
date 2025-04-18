#pragma once
#include "../Utility_Framework/Core.Minimal.h"

class Texture
{
public:
	Texture() = default;
	Texture(ID3D11Texture2D* texture, const std::string_view& name);
	Texture(const Texture&) = delete;
	Texture(Texture&& texture) noexcept;
	~Texture();
	//texture creator functions (static)
	static Texture* Create(
		_In_ uint32 width, 
		_In_ uint32 height, 
		_In_ const std::string_view& name, 
		_In_ DXGI_FORMAT textureFormat, 
		_In_ uint32 bindFlags, 
		_In_opt_ D3D11_SUBRESOURCE_DATA* data = nullptr
	);

	static Texture* CreateCube(
		_In_ uint32 size,
		_In_ const std::string_view& name,
		_In_ DXGI_FORMAT textureFormat,
		_In_ uint32 bindFlags,
		_In_opt_ uint32 mipLevels = 1,
		_In_opt_ D3D11_SUBRESOURCE_DATA* data = nullptr
	);

	static Texture* CreateArray(
		_In_ uint32 width,
		_In_ uint32 height,
		_In_ const std::string_view& name,
		_In_ DXGI_FORMAT textureFormat,
		_In_ uint32 bindFlags,
		_In_ uint32 arrsize = 3,
		_In_opt_ D3D11_SUBRESOURCE_DATA* data = nullptr
	);

	static Texture* LoadFormPath(_In_ const file::path& path);

	void CreateSRV(
		_In_ DXGI_FORMAT textureFormat,
		_In_opt_ D3D11_SRV_DIMENSION viewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
		_In_opt_ uint32 mipLevels = 1
	);

	void CreateRTV(_In_ DXGI_FORMAT textureFormat);

	void CreateCubeRTVs(
		_In_ DXGI_FORMAT textureFormat,
		_In_opt_ uint32 mipLevels = 1
	);

	void CreateDSV(_In_ DXGI_FORMAT textureFormat);

	void CreateUAV(_In_ DXGI_FORMAT textureFormat);

	ID3D11RenderTargetView* GetRTV(uint32 index = 0);

	ID3D11Texture2D* m_pTexture{};
	ID3D11ShaderResourceView* m_pSRV{};
	ID3D11DepthStencilView* m_pDSV{};
	ID3D11UnorderedAccessView* m_pUAV{};

	std::string m_name;

	float2 GetImageSize() const;

	bool IsTextureAlpha() const
	{
		return m_isTextureAlpha;
	}

	void SetTextureAlpha(bool isAlpha)
	{
		m_isTextureAlpha = isAlpha;
	}

private:
	float2 size{};
	std::vector<ID3D11RenderTargetView*> m_pRTVs;
	bool m_isTextureAlpha{ false };
};

namespace TextureHelper
{
    extern std::function<void(Texture*)> deleter;

	inline std::unique_ptr<Texture, decltype(deleter)> CreateRenderTexture(int width, int height, const std::string name, DXGI_FORMAT format)
	{
		Texture* tex = Texture::Create(
			width, 
			height, 
			name, 
			format, 
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
		);
		tex->CreateRTV(format);
		tex->CreateSRV(format);

        return std::unique_ptr<Texture, decltype(deleter)>(tex, deleter);
	}

	inline std::unique_ptr<Texture, decltype(deleter)> CreateDepthTexture(int width, int height, const std::string name)
	{
		Texture* tex = Texture::Create(
			width,
			height,
			name,
			DXGI_FORMAT_R24G8_TYPELESS,
			D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE
		);
		tex->CreateDSV(DXGI_FORMAT_D24_UNORM_S8_UINT);
		tex->CreateSRV(DXGI_FORMAT_R24_UNORM_X8_TYPELESS);

		return std::unique_ptr<Texture, decltype(deleter)>(tex, deleter);
	}
}

using UniqueTexturePtr = std::unique_ptr<Texture, decltype(TextureHelper::deleter)>;

#ifndef TEXTURE_NULL_INITIALIZER
#define TEXTURE_NULL_INITIALIZER UniqueTexturePtr(nullptr, TextureHelper::deleter)
#endif

inline UniqueTexturePtr MakeUniqueTexturePtr(Texture* texture)
{
    return UniqueTexturePtr(texture, TextureHelper::deleter);
}
