#include "Canvas.h"
#include "GameObject.h"
#include "ImageComponent.h"
#include "UIManager.h"
#include "TextComponent.h"
#include "UIButton.h"

Canvas::Canvas()
{
	m_typeID = TypeTrait::GUIDCreator::GetTypeID<Canvas>();
}

void Canvas::OnDestroy()
{
	
	Scene* scene = SceneManagers->GetActiveScene();
	if (scene != nullptr && m_pOwner->IsDestroyMark())
	{
		if (UIManagers->CurCanvas == m_pOwner)
			UIManagers->CurCanvas = nullptr;
		UIManagers->DeleteCanvas(m_pOwner->ToString());
	}
	

}

void Canvas::AddUIObject(GameObject* obj)
{
	
	auto image = obj->GetComponent<ImageComponent>();
	if(image)
		image->SetCanvas(this);
	auto text = obj->GetComponent<TextComponent>();
	if (text)
		text->SetCanvas(this);
	auto btn = obj->GetComponent<UIButton>();
	if (btn)
		btn->SetCanvas(this);
	UIObjs.push_back(obj);
}


void Canvas::Update(float tick)
{
	if (PreCanvasOrder != CanvasOrder)
	{
		UIManagers->needSort = true;
		PreCanvasOrder = CanvasOrder;
	}
}
