//
// Created by ianpo on 22/12/2025.
//

#include "MVT/VulkanAllocator.hpp"
#include <stdexcept>
#include <string>

using namespace std::string_literals;


namespace MVT {
	VulkanAllocator::SubAllocation::SubAllocation() = default;

	VulkanAllocator::SubAllocation::~SubAllocation() {
		if (counter) {
			std::atomic_fetch_sub_explicit(counter, 1, std::memory_order_release);
			id = 0;
			size = 0;
			offset = 0;
			counter = nullptr;
		}
	}

	VulkanAllocator::SubAllocation::SubAllocation(const SubAllocation &o) : id(o.id), size(o.size), offset(o.offset), counter(o.counter) {
		if (counter) {
			std::atomic_fetch_add_explicit(counter, 1, std::memory_order_release);
		}
	}

	VulkanAllocator::SubAllocation &VulkanAllocator::SubAllocation::operator=(const SubAllocation &o) {
		ChangeAllocation(o.id, o.size, o.offset, o.counter);
		return *this;
	}

	VulkanAllocator::SubAllocation::SubAllocation(SubAllocation &&o) noexcept {
		swap(o);
	}

	VulkanAllocator::SubAllocation &VulkanAllocator::SubAllocation::operator=(SubAllocation &&o) noexcept {
		swap(o);
		return *this;
	}

	void VulkanAllocator::SubAllocation::swap(SubAllocation &o) noexcept {
		std::swap(id, o.id);
		std::swap(size, o.size);
		std::swap(offset, o.offset);
		std::swap(counter, o.counter);
	}

	VulkanAllocator::SubAllocation::SubAllocation(const uint64_t _id, const uint64_t _size, const uint64_t _offset, std::atomic_uint64_t *_counter) : id(_id), size(_size), offset(_offset), counter(_counter) {
		if (counter) {
			std::atomic_fetch_add_explicit(counter, 1, std::memory_order_release);
		}
	}

	void VulkanAllocator::SubAllocation::ChangeAllocation(const uint64_t _id, const uint64_t _size, const uint64_t _offset, std::atomic_uint64_t *_counter) {
		if (counter) {
			std::atomic_fetch_sub_explicit(counter, 1, std::memory_order_release);
		}

		id = _id;
		size = _size;
		offset = _offset;
		counter = _counter;

		if (counter) {
			std::atomic_fetch_add_explicit(counter, 1, std::memory_order_release);
		}
	}

	uint64_t VulkanAllocator::SubAllocation::get_id() const {
		return id;
	}

	uint64_t VulkanAllocator::SubAllocation::get_size() const {
		return size;
	}

	uint64_t VulkanAllocator::SubAllocation::get_offset() const {
		return offset;
	}

	void VulkanAllocator::SubAllocation::initialize() {
		if (counter) {
			std::atomic_fetch_add_explicit(counter, 1, std::memory_order_release);
		}
	}

	VulkanAllocator::SubAllocations::SubAllocations() = default;

	VulkanAllocator::SubAllocations::~SubAllocations() {
		if (pAllocation) {
			delete[] pAllocation;
			pAllocation = nullptr;
			count = 0;
		}
	}

	VulkanAllocator::SubAllocations::SubAllocations(SubAllocations &&o) noexcept {
		swap(o);
	}

	VulkanAllocator::SubAllocations & VulkanAllocator::SubAllocations::operator=(SubAllocations &&o) noexcept {
		swap(o);
		return *this;
	}

	void VulkanAllocator::SubAllocations::swap(SubAllocations &o) noexcept {
		std::swap(pAllocation, o.pAllocation);
		std::swap(count, o.count);
	}

	const VulkanAllocator::SubAllocation &VulkanAllocator::SubAllocations::operator[](const uint64_t i) const {
		return pAllocation[i];
	}

	const VulkanAllocator::SubAllocation &VulkanAllocator::SubAllocations::at(const uint64_t i) const {
		if (count && i < count) {
			return pAllocation[i];
		}

		throw std::runtime_error("Cannot fetch index '"s + std::to_string(i) + "' as there is "s + std::to_string(count) + " Sub Allocations."s);
		return pAllocation[i];
	}

	VulkanAllocator::SubAllocation &VulkanAllocator::SubAllocations::operator[](const uint64_t i) {
		return pAllocation[i];
	}

	VulkanAllocator::SubAllocation &VulkanAllocator::SubAllocations::at(const uint64_t i) {
		if (count && i < count) {
			return pAllocation[i];
		}

		throw std::runtime_error("Cannot fetch index '"s + std::to_string(i) + "' as there is "s + std::to_string(count) + " Sub Allocations."s);
		return pAllocation[i];
	}

	uint64_t VulkanAllocator::SubAllocations::size() const {
		return count;
	}

	VulkanAllocator::VulkanAllocator(vk::raii::Device* device) : device(device) {
		name = "#"s + std::to_string((uint64_t) this);
	}

	VulkanAllocator::VulkanAllocator(vk::raii::Device* device, std::string name) : device(device), name(std::move(name)) {
	}

	VulkanAllocator::~VulkanAllocator() {
		uint64_t total_count = 0;
		for (auto &[id, alloc]: m_Memories) {
			const uint64_t count = alloc->m_SubAllocation.load(std::memory_order_acquire);
			if (count) {
				++total_count;
				const std::string name = alloc->m_Name.value_or("#"s + std::to_string(id));
				const std::string err = name + " has still "s + std::to_string(count) + " allocations";
				std::cerr << err << std::endl;
			}
		}
	}

	VulkanAllocator::SubAllocations VulkanAllocator::Allocate(const Allocation *pAllocation, const uint64_t count, const uint32_t memoryType) {
		SubAllocations allocations{};
		allocations.pAllocation = new SubAllocation[count];
		allocations.count = count;

		// Using a sorted multimap to have the values with the same alignment next to each others.
		std::multimap<uint64_t, uint64_t> alignments{};

		// Inserting every allocation
		for (uint64_t i = 0; i < count; ++i) {
			alignments.emplace(pAllocation[count].alignment, i);
		}

		// Prepare the next allocation
		VkAllocation allocation = nullptr;
		uint64_t id = 0;
		uint64_t alignment = 0;
		uint64_t size = 0;

		auto it = alignments.begin();
		uint64_t i = 0;
		do {
			// Reset the allocation when changing the
			if (it->first != alignment) {
				if (allocation) {
					vk::MemoryAllocateInfo info {
						.allocationSize = size,
						.memoryTypeIndex = memoryType,
					};
					allocation->m_Memory = vk::raii::DeviceMemory{*device, info};
					m_Memories.emplace(id, std::move(allocation));
				}
				id = GenerateIndex();
				alignment = it->first;
				size = 0;
				allocation.reset(new T_VkAllocation());
			}

			if (size % alignment != 0) {
				size = ((size / alignment) + 1) * alignment;
			}

			allocations[i].counter = &allocation->m_SubAllocation;
			allocations[i].offset = size;
			allocations[i].size = pAllocation[it->second].size;
			allocations[i].id = id;
			allocations[i].initialize();
			size += allocations[i].size;

			++i;
		} while (++it != alignments.end());

		if (allocation) {
			vk::MemoryAllocateInfo info {
				.allocationSize = size,
				.memoryTypeIndex = memoryType,
			};
			allocation->m_Memory = vk::raii::DeviceMemory{*device, info};
			m_Memories.emplace(id, std::move(allocation));
		}

		return std::move(allocations);
	}
} // MVT
