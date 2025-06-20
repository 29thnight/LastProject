#include "EffectComponent.h"

void EffectComponent::Awake()
{
	ImGui::ContextRegister("EffectEdit", false, [&]() {

        RenderMainEditor();

	});


	//ImGui::GetContext("EffectEdit").Close();
}

void EffectComponent::Update(float tick)
{
	for (auto& emitter : m_emitters) {
		emitter->Update(tick);
	}
}

void EffectComponent::OnDistroy()
{
	m_emitters.clear();
	m_tempParticleSystem = nullptr;
}

void EffectComponent::RemoveEmitter(int index)
{
	if (index >= 0 && index < m_emitters.size()) {
		m_emitters.erase(m_emitters.begin() + index);
	}
}

void EffectComponent::PlayAll()
{
	for (auto& e : m_emitters)
	{
		e->Play();
	}
}

void EffectComponent::StopAll()
{
	for (auto& e : m_emitters)
	{
		e->Stop();
	}
}

void EffectComponent::PlayEmitter(int index)
{
	if (index >= 0 && index < m_emitters.size()) {
		m_emitters[index]->Play();
	}
}

void EffectComponent::StopEmitter(int index)
{
	if (index >= 0 && index < m_emitters.size()) {
		m_emitters[index]->Stop();
	}
}


void EffectComponent::SetTexture(Texture* tex)
{
    m_textures.push_back(tex);
}


void EffectComponent::RenderMainEditor()
{
	if (!m_isEditingEmitter) {
		if (ImGui::Button("Create New Emitter")) {
			StartCreateEmitter();
		}

		// 기존 에미터들 표시
		for (int i = 0; i < m_emitters.size(); ++i) {
			ImGui::Text("Emitter %d", i);
			ImGui::SameLine();
			if (ImGui::Button(("Play##" + std::to_string(i)).c_str())) {
				PlayEmitter(i);
			}
			ImGui::SameLine();
			if (ImGui::Button(("Stop##" + std::to_string(i)).c_str())) {
				StopEmitter(i);
			}
			ImGui::SameLine();
			if (ImGui::Button(("Remove##" + std::to_string(i)).c_str())) {
				RemoveEmitter(i);
			}
		}
	}
	else {
		RenderEmitterEditor();
	}
}

void EffectComponent::RenderEmitterEditor()
{
    if (!m_tempParticleSystem) return;

    ImGui::Text("Creating New Emitter");
    ImGui::Separator();

    // 모듈 선택 및 추가
    ImGui::Text("Add Modules:");
    const char* currentModuleName = m_selectedModuleIndex < m_availableModules.size() ?
        m_availableModules[m_selectedModuleIndex].name : "None";

    if (ImGui::BeginCombo("Select Module", currentModuleName)) {
        for (size_t i = 0; i < m_availableModules.size(); i++) {
            bool isSelected = (m_selectedModuleIndex == i);
            if (ImGui::Selectable(m_availableModules[i].name, isSelected)) {
                m_selectedModuleIndex = static_cast<int>(i);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Add Module")) {
        AddSelectedModule();
    }

    ImGui::Separator();

    // 렌더 모듈 선택 및 추가
    ImGui::Text("Add Render Module:");
    const char* currentRenderName = m_selectedRenderIndex < m_availableRenders.size() ?
        m_availableRenders[m_selectedRenderIndex].name : "None";

    if (ImGui::BeginCombo("Select Render", currentRenderName)) {

        for (size_t i = 0; i < m_availableRenders.size(); i++) {

            bool isSelected = (m_selectedRenderIndex == i);
            if (ImGui::Selectable(m_availableRenders[i].name, isSelected)) {
                m_selectedRenderIndex = static_cast<int>(i);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Add Render Module")) {
        AddSelectedRender();
    }

    ImGui::Separator();

    // 기존 모듈들 표시
    RenderExistingModules();

    ImGui::Separator();

    // 저장/취소 버튼
    if (ImGui::Button("Save Emitter")) {
        SaveCurrentEmitter();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        CancelEditing();
    }
}

void EffectComponent::StartCreateEmitter()
{
	m_tempParticleSystem = std::make_shared<ParticleSystem>(1000, ParticleDataType::Standard);
	m_isEditingEmitter = true;
}

void EffectComponent::SaveCurrentEmitter()
{
    if (m_tempParticleSystem) {
        m_emitters.push_back(m_tempParticleSystem);
        m_tempParticleSystem = nullptr;
        m_isEditingEmitter = false;
    }
}

void EffectComponent::CancelEditing()
{
    m_tempParticleSystem = nullptr;
    m_isEditingEmitter = false;
}

void EffectComponent::RenderExistingModules()
{
    if (!m_tempParticleSystem) return;

    if (ImGui::Button("Show Module List")) {
        ImGui::OpenPopup("Module List");
    }

    if (ImGui::BeginPopup("Module List")) {
        ImGui::Text("Current Particle Modules");
        ImGui::Separator();

        int index = 0;
       
        auto& moduleList = m_tempParticleSystem->GetModuleList();
        for (auto it = moduleList.begin(); it != moduleList.end(); ++it) {
            ParticleModule& module = *it;
            ImGui::Text("Module %d: %s", index++, typeid(module).name());

            ImGui::Indent();
            ImGui::Text("Easing: %s", module.IsEasingEnabled() ? "Enabled" : "Disabled");

            // 모듈별 세부 설정 UI
            if (auto* spawnModule = dynamic_cast<SpawnModuleCS*>(&module)) {
                ImGui::Text("Spawn Rate: %.2f", spawnModule->GetSpawnRate());
            }
            else if (auto* movementModule = dynamic_cast<MovementModuleCS*>(&module)) {
                ImGui::Text("Use Gravity: %s", movementModule->GetUseGravity() ? "Yes" : "No");
            }
            // ... 다른 모듈 타입들

            ImGui::Unindent();
            ImGui::Separator();
        }
      
        // 임시로 추가된 모듈 개수만 표시
        ImGui::Text("Total modules: %d", index);

        // 렌더 모듈들도 표시
        ImGui::Separator();
        ImGui::Text("Render Modules");
        // 렌더 모듈 리스트도 비슷하게...

        ImGui::EndPopup();
    }

}

void EffectComponent::AddSelectedModule()
{
    if (!m_tempParticleSystem || m_selectedModuleIndex >= m_availableModules.size()) return;

    EffectModuleType type = m_availableModules[m_selectedModuleIndex].type;

    switch (type) {
    case EffectModuleType::SpawnModule:
        m_tempParticleSystem->AddModule<SpawnModuleCS>();
        break;
    case EffectModuleType::MeshSpawnModule:
        m_tempParticleSystem->AddModule<MeshSpawnModuleCS>();
        break;
    case EffectModuleType::MovementModule:
        m_tempParticleSystem->AddModule<MovementModuleCS>();
        break;
    case EffectModuleType::ColorModule:
        m_tempParticleSystem->AddModule<ColorModuleCS>();
        break;
    case EffectModuleType::SizeModule:
        m_tempParticleSystem->AddModule<SizeModuleCS>();
        break;
    }
}

void EffectComponent::AddSelectedRender()
{
    if (!m_tempParticleSystem || m_selectedRenderIndex >= m_availableRenders.size()) return;

    RenderType type = m_availableRenders[m_selectedRenderIndex].type;

    switch (type) {
    case RenderType::Billboard:
        m_tempParticleSystem->AddRenderModule<BillboardModuleGPU>();
        break;
    case RenderType::Mesh:
        m_tempParticleSystem->AddRenderModule<MeshModuleGPU>();
        break;
    }
}
