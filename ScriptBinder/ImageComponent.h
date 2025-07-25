#pragma once
#include "../Utility_Framework/Core.Minimal.h"
#include "Component.h"
#include "IRenderable.h"
#include "IRegistableEvent.h"
#include "UIComponent.h"
#include "ImageComponent.generated.h"
#include <DirectXTK/SpriteBatch.h>

struct alignas(16) ImageInfo
{
	Mathf::xMatrix world;
	float2 size;
	float2 screenSize;
 
};


//모든 2d이미지 기본?
class Texture;
class UIMesh;
class Canvas;
class ImageComponent : public UIComponent, public RegistableEvent<ImageComponent>
{
public:
   ReflectImageComponent
    [[Serializable(Inheritance:UIComponent)]]
	ImageComponent();
	~ImageComponent() = default;

	void Load(Texture* ptr);
	virtual void Update(float tick) override;
    [[Method]]
	void UpdateTexture();
	void SetTexture(int index);
	void Draw(SpriteBatch* sBatch);
	
	ImageInfo uiinfo;
	Texture* m_curtexture{};
    [[Property]]
	int curindex = 0;

	//ndc좌표 {-1,1}
	Mathf::Vector3 trans{ 0,0,0 };
	Mathf::Vector3 rotat{ 0,0,0 };
private:
	float rotate =0;
	std::vector<Texture*> textures;
	XMFLOAT2 origin{};

};

