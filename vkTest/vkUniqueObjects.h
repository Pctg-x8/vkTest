#pragma once

#define UnwrappableObjectTraitImpl(ObjectT) private: ObjectT obj; public: auto& get() const noexcept { return this->obj; }

namespace Vulkan
{
	// Non-copyable and Movable Vulkan Object wrapper
	template<typename ObjectT> class UniqueObject
	{
		using DestroyerT = std::function<void(ObjectT, const VkAllocationCallbacks*)>;
		DestroyerT destroyer;

		UnwrappableObjectTraitImpl(ObjectT);

		UniqueObject() : obj(nullptr) {}
		UniqueObject(ObjectT o, DestroyerT d) : obj(o), destroyer(d) {}
		UniqueObject(const UniqueObject&) = delete;
		~UniqueObject() { if (obj != nullptr) destroyer(obj, nullptr); }

		UniqueObject(UniqueObject&& b) : obj(b.obj), destroyer(b.destroyer) { b.obj = nullptr; }
		auto& operator=(UniqueObject&& b)
		{
			obj = b.obj;
			destroyer = b.destroyer;
			b.obj = nullptr;
			return *this;
		}
	};
	template<typename ObjectT> class UniqueObjectWithInstance
	{
		using DestroyerT = std::function<void(VkInstance, ObjectT, const VkAllocationCallbacks*)>;
		DestroyerT destroyer;
		VkInstance instanceRef;

		UnwrappableObjectTraitImpl(ObjectT);

		UniqueObjectWithInstance() : obj(nullptr) {}
		UniqueObjectWithInstance(VkInstance instance, ObjectT o, DestroyerT d) : obj(o), destroyer(d), instanceRef(instance) {}
		UniqueObjectWithInstance(const UniqueObjectWithInstance&) = delete;
		UniqueObjectWithInstance(UniqueObjectWithInstance&& o) : obj(o.obj), destroyer(o.destroyer), instanceRef(o.instanceRef)
		{
			o.obj = nullptr;
		}
		~UniqueObjectWithInstance() { if (obj != nullptr) destroyer(instanceRef, obj, nullptr); }

		auto& operator=(UniqueObjectWithInstance&& b)
		{
			obj = b.obj;
			destroyer = b.destroyer;
			instanceRef = b.instanceRef;
			b.obj = nullptr;
			return *this;
		}
	};
	// Specialization for Non-dispatchable object
	template<> class UniqueObjectWithInstance<uint64_t>
	{
		using DestroyerT = std::function<void(VkInstance, uint64_t, const VkAllocationCallbacks*)>;
		DestroyerT destroyer;
		VkInstance instanceRef;

		UnwrappableObjectTraitImpl(uint64_t);

		UniqueObjectWithInstance() : obj(0) {}
		UniqueObjectWithInstance(VkInstance instance, uint64_t o, DestroyerT d) : obj(o), destroyer(d), instanceRef(instance) {}
		UniqueObjectWithInstance(const UniqueObjectWithInstance&) = delete;
		UniqueObjectWithInstance(UniqueObjectWithInstance&& o) : obj(o.obj), destroyer(o.destroyer), instanceRef(o.instanceRef)
		{
			o.obj = 0;
		}
		~UniqueObjectWithInstance() { if (obj != 0) destroyer(instanceRef, obj, nullptr); }

		auto& operator=(UniqueObjectWithInstance&& b)
		{
			obj = b.obj;
			destroyer = b.destroyer;
			instanceRef = b.instanceRef;
			b.obj = 0;
			return *this;
		}
	};
	template<typename ObjectT> class UniqueObjectWithDevice
	{
		using DestroyerT = std::function<void(VkDevice, ObjectT, const VkAllocationCallbacks*)>;
		VkDevice deviceRef;
		DestroyerT destroyer;

		UnwrappableObjectTraitImpl(ObjectT);

		UniqueObjectWithDevice() : obj(nullptr) {}
		UniqueObjectWithDevice(VkDevice device, ObjectT o, DestroyerT d) : obj(o), destroyer(d), deviceRef(device) {}
		UniqueObjectWithDevice(const UniqueObjectWithDevice&) = delete;
		UniqueObjectWithDevice(UniqueObjectWithDevice&& o) : obj(o.obj), destroyer(o.destroyer), deviceRef(o.deviceRef)
		{
			o.obj = nullptr;
		}
		~UniqueObjectWithDevice() { if (obj != nullptr) destroyer(deviceRef, obj, nullptr); }

		auto& operator=(UniqueObjectWithDevice&& b)
		{
			obj = b.obj;
			destroyer = b.destroyer;
			deviceRef = b.deviceRef;
			b.obj = nullptr;
			return *this;
		}
	};
	// Specialization for Non-dispatchable object
	template<> class UniqueObjectWithDevice<uint64_t>
	{
		using DestroyerT = std::function<void(VkDevice, uint64_t, const VkAllocationCallbacks*)>;
		VkDevice deviceRef;
		DestroyerT destroyer;

		UnwrappableObjectTraitImpl(uint64_t);

		UniqueObjectWithDevice() : obj(0) {}
		UniqueObjectWithDevice(VkDevice device, uint64_t o, DestroyerT d) : obj(o), destroyer(d), deviceRef(device) {}
		UniqueObjectWithDevice(const UniqueObjectWithDevice&) = delete;
		UniqueObjectWithDevice(UniqueObjectWithDevice&& o) : obj(o.obj), destroyer(o.destroyer), deviceRef(o.deviceRef)
		{
			o.obj = 0;
		}
		~UniqueObjectWithDevice() { if (obj != 0) destroyer(deviceRef, obj, nullptr); }

		auto& operator=(UniqueObjectWithDevice&& b)
		{
			obj = b.obj;
			destroyer = b.destroyer;
			deviceRef = b.deviceRef;
			b.obj = 0;
			return *this;
		}
	};
	class CommandBuffers final
	{
		std::unique_ptr<VkCommandBuffer[]> buffers;
		VkDevice deviceRef;
		VkCommandPool poolRef;
		size_t bufferCount;
	public:
		CommandBuffers(VkDevice dref, VkCommandPool cpref, size_t nBuffers)
			: deviceRef(dref), poolRef(cpref), buffers(std::make_unique<VkCommandBuffer[]>(nBuffers)), bufferCount(nBuffers) {}
		CommandBuffers(const CommandBuffers&) = delete;
		~CommandBuffers()
		{
			if (buffers)
			{
				vkFreeCommandBuffers(deviceRef, poolRef, bufferCount, buffers.get());
			}
		}

		CommandBuffers(CommandBuffers&& b)
			: deviceRef(b.deviceRef), poolRef(b.poolRef), buffers(std::move(b.buffers)), bufferCount(b.bufferCount) {}
		auto& operator=(CommandBuffers&& b)
		{
			deviceRef = b.deviceRef;
			poolRef = b.poolRef;
			buffers = std::move(b.buffers);
			bufferCount = b.bufferCount;
			return *this;
		}

		auto data() const noexcept { return this->buffers.get(); }
		auto& operator[](size_t idx) const noexcept { return this->buffers[idx]; }
	};

	// Typedefs
	using Instance = UniqueObject<VkInstance>;
	using Surface = UniqueObjectWithInstance<VkSurfaceKHR>;
	using CommandPool = UniqueObjectWithDevice<VkCommandPool>;
	using Swapchain = UniqueObjectWithDevice<VkSwapchainKHR>;
	using ImageView = UniqueObjectWithDevice<VkImageView>;
	using RenderPass = UniqueObjectWithDevice<VkRenderPass>;
	using Framebuffer = UniqueObjectWithDevice<VkFramebuffer>;
	using Buffer = UniqueObjectWithDevice<VkBuffer>;
	using DeviceMemory = UniqueObjectWithDevice<VkDeviceMemory>;
	using ShaderModule = UniqueObjectWithDevice<VkShaderModule>;
	using PipelineLayout = UniqueObjectWithDevice<VkPipelineLayout>;
	using PipelineCache = UniqueObjectWithDevice<VkPipelineCache>;
	using Pipeline = UniqueObjectWithDevice<VkPipeline>;
	using Fence = UniqueObjectWithDevice<VkFence>;

	// Fixed and Unique Array
	template<typename Element> using UniqueArray = std::pair<std::unique_ptr<Element[]>, uint32_t>;

	template<typename ElementT> constexpr auto size(const UniqueArray<ElementT>& a) { return a.second; }
}
