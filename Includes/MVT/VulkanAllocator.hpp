//
// Created by ianpo on 22/12/2025.
//

#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <optional>
#include <cstdint>

namespace MVT {
	class VulkanAllocator {
	private:
		struct T_VkAllocation {
			vk::raii::DeviceMemory m_Memory = nullptr;
			std::atomic_uint64_t m_SubAllocation = 0;
			std::optional<std::string> m_Name = std::nullopt;
		};
		using VkAllocation = std::unique_ptr<T_VkAllocation>;
	public:
		struct Allocation {
			uint64_t size;
			uint64_t alignment;
		};

		class SubAllocation {
		public:
			SubAllocation();
			~SubAllocation();
			SubAllocation(const SubAllocation& o);
			SubAllocation& operator=(const SubAllocation& o);
			SubAllocation(SubAllocation&& o) noexcept;
			SubAllocation& operator=(SubAllocation&& o) noexcept;
			void swap(SubAllocation& o) noexcept;
		private:
			SubAllocation(uint64_t _id, uint64_t _size, uint64_t _offset, std::atomic_uint64_t* _counter);
			void ChangeAllocation(uint64_t _id, uint64_t _size, uint64_t _offset, std::atomic_uint64_t* _counter);
		public:
			[[nodiscard]] uint64_t get_id() const;
			[[nodiscard]] uint64_t get_size() const;
			[[nodiscard]] uint64_t get_offset() const;
		private:
			void initialize();
		private:
			uint64_t id = 0;
			uint64_t size = 0;
			uint64_t offset = 0;
			std::atomic_uint64_t* counter = nullptr;
		private:
			friend VulkanAllocator;
		};

		class SubAllocations {
		public:
			SubAllocations();
			~SubAllocations();
			SubAllocations(const SubAllocations&) = delete;
			SubAllocations& operator=(const SubAllocations&) = delete;
			SubAllocations(SubAllocations&& o) noexcept;
			SubAllocations& operator=(SubAllocations&& o) noexcept;
			void swap(SubAllocations& o) noexcept;
		public:
			[[nodiscard]] const SubAllocation& operator[](uint64_t index) const;
			[[nodiscard]] const SubAllocation& at(uint64_t index) const;
			[[nodiscard]] uint64_t size() const;
		private:
			[[nodiscard]] SubAllocation& operator[](uint64_t index);
			[[nodiscard]] SubAllocation& at(uint64_t index);
		private:
			SubAllocation* pAllocation = nullptr;
			uint64_t count = 0;
			friend class VulkanAllocator;
		};
	private:
		VulkanAllocator(vk::raii::Device* device);
		VulkanAllocator(vk::raii::Device* device, std::string name);
		~VulkanAllocator();
	public:

		SubAllocations Allocate(const Allocation *pAllocation, uint64_t count, uint32_t memoryType);
	private:
		// ID 0 is null.
		static inline uint64_t index = 0;
		static inline uint64_t GenerateIndex() {return ++index;}
	private:
		// TODO: Replace map with thread safe map
		std::unordered_map<uint64_t, VkAllocation> m_Memories;
		std::string name;
		vk::raii::Device* device = nullptr;
	};
} // MVT