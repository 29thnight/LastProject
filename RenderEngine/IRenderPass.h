#pragma once
#include "RenderScene.h"
#include "PSO.h"
#include "SwapEvent.h"

enum RTV_Type
{
	BaseColor,
	OccRoughMetal,
	Normal,
	Emissive,
	BitMask,
	RTV_TypeMax
};

struct IViewEvent
{
	virtual ~IViewEvent() = default;

	virtual void ResizeRelease() = 0;
	virtual void Resize(uint32_t width, uint32_t height) = 0;

	Core::DelegateHandle m_onReleaseHandle{};
	Core::DelegateHandle m_onResizeHandle{};
};

using namespace concurrency;
using CommandQueue = concurrent_queue<ID3D11CommandList*>;

class IRenderPass abstract : public IViewEvent
{
public:
	using CommandQueueMap = std::unordered_map<size_t, std::array<CommandQueue, 10>>;
	using FrameQueueArray = std::array<std::array<CommandQueue, 10>, 3>;
public:
	IRenderPass()
	{
		m_swapEventHandle = SwapEvent.AddRaw(this, &IRenderPass::SwapQueue);
	}
	virtual ~IRenderPass()
	{
		SwapEvent -= m_swapEventHandle;

		for (auto& [frame, cmdArr] : m_commandQueueMap)
		{
			for (auto& queue : cmdArr)
			{
				while (!queue.empty())
				{
					ID3D11CommandList* CommandJob;
					if (queue.try_pop(CommandJob))
					{
						Memory::SafeDelete(CommandJob);
					}
				}
			}
		}

		m_commandQueueMap.clear();
	}

	//virtual std::string ToString() abstract;
	virtual void Execute(RenderScene& scene, Camera& camera) abstract;
	virtual void CreateRenderCommandList(ID3D11DeviceContext* deferredContext, RenderScene& scene, Camera& camera) {}
	virtual void ControlPanel() {};
	void ReloadShaders() { m_pso->ReloadShaders(); }
	virtual void ResizeRelease() {};
	virtual void Resize(uint32_t width, uint32_t height) {}

	void PushQueue(size_t key, ID3D11CommandList* command) 
	{ 
		size_t index = m_frame.load(std::memory_order_relaxed) % 3;
		m_frameQueues[index][key].push(command);
	}

	CommandQueue* GetCommandQueue(size_t key)
	{
		size_t prevIndex = (m_frame.load(std::memory_order_relaxed) + 1) % 3;
		return &m_frameQueues[prevIndex][key];
	}

	void SwapQueue()
	{
		m_frame.fetch_add(1, std::memory_order_relaxed);
	}

protected:
	std::unique_ptr<PipelineStateObject> m_pso{ nullptr };
	CommandQueueMap m_commandQueueMap{}; //카메라 별 커멘드 큐
	FrameQueueArray m_frameQueues;
	Core::DelegateHandle m_swapEventHandle{};

	bool m_abled{ true };
	std::atomic_ullong m_frame{};
};