//
// Created by ianpo on 05/11/2025.
//

#include "MVT/VmaBuffer.hpp"

namespace MVT {
	VmaBuffer::VmaBuffer(VmaAllocator allocator, const VkBufferCreateInfo* pBufferInfo, const VmaAllocationCreateInfo* pAllocInfo) {
		vmaCreateBuffer(allocator, pBufferInfo, pAllocInfo, reinterpret_cast<VkBuffer*>(&buffer), &allocation, nullptr);
	}

	VmaBuffer::~VmaBuffer() {
		if (allocator) {
			vmaDestroyBuffer(allocator, buffer, allocation);
		}
	}

	VmaBuffer::VmaBuffer(VmaBuffer &&o) noexcept {
		swap(o);
	}

	VmaBuffer & VmaBuffer::operator=(VmaBuffer &&o) noexcept {
		swap(o);
		return *this;
	}

	void VmaBuffer::swap(VmaBuffer &o) noexcept {
		std::swap(allocator, o.allocator);
		std::swap(buffer, o.buffer);
		std::swap(allocation, o.allocation);
	}

	VmaAllocationInfo VmaBuffer::GetAllocationInfo() {
		VmaAllocationInfo allocationInfo;

		vmaGetAllocationInfo(allocator, allocation, &allocationInfo);

		return allocationInfo;
	}
} // MVT