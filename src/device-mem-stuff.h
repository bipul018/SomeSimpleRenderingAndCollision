#pragma once

#include "common-stuff.h"

struct GPUAllocator {
    VkDeviceMemory memory_handle;

    // Will be mapped iff host visible
    void *mapped_memory;
    VkDeviceSize mem_size;
    VkDeviceSize curr_offset;
    size_t atom_size;
    uint32_t memory_type;
};

typedef struct GPUAllocator GPUAllocator;

enum AllocateDeviceMemoryCodes {
    ALLOCATE_DEVICE_MEMORY_FAILED = -0x7fff,
    ALLOCATE_DEVICE_MEMORY_NOT_AVAILABLE_TYPE,
    ALLOCATE_DEVICE_MEMORY_OK = 0,
};

typedef struct {
    VkPhysicalDevice phy_device;
    VkDevice device;
    // like host visible, local
    uint32_t memory_properties;
    uint32_t memory_type_flag_bits;
    size_t allocation_size;

    GPUAllocator *p_gpu_allocr;
} AllocateDeviceMemoryParam;

int allocate_device_memory(const VkAllocationCallbacks *alloc_callbacks,
                           AllocateDeviceMemoryParam param);

typedef struct {
    VkDevice device;

    GPUAllocator *p_gpu_allocr;
} FreeDeviceMemoryParam;

void free_device_memory(const VkAllocationCallbacks *alloc_callbacks,
                        FreeDeviceMemoryParam param, int err_codes);

enum GpuAllocrAllocateBufferCodes {
    GPU_ALLOCR_ALLOCATE_BUFFER_FAILED = -0x7fff,
    GPU_ALLOCR_ALLOCATE_BUFFER_NOT_ENOUGH_MEMORY,
    GPU_ALLOCR_ALLOCTE_BUFFER_OK = 0
};

typedef struct {
    VkBuffer buffer;
    size_t total_amt;
    size_t base_align;
    void *mapped_memory;
} GpuAllocrAllocatedBuffer;

int gpu_allocr_allocate_buffer(
  GPUAllocator *gpu_allocr, VkDevice device,
  GpuAllocrAllocatedBuffer *buffer_info);

// Only for host visible and properly mapped memories
VkResult gpu_allocr_flush_memory(VkDevice device, GPUAllocator allocr,
                                 void *mapped_memory, size_t amount);

//Assumes a compatible memory is allocated
enum CreateAndAllocateBufferCodes {
    CREATE_AND_ALLOCATE_BUFFER_DUMMY_ERR_CODE = -0x7fff,
    CREATE_AND_ALLOCATE_BUFFER_BUFFER_ALLOC_FAILED,
    CREATE_AND_ALLOCATE_BUFFER_INCOMPATIBLE_MEMORY,
    CREATE_AND_ALLOCATE_BUFFER_CREATE_BUFFER_FAILED,


    CREATE_AND_ALLOCATE_BUFFER_OK = 0,
};


typedef struct {
    GpuAllocrAllocatedBuffer *p_buffer;
    VkSharingMode share_mode;
    VkBufferUsageFlags usage;
    size_t size;

}CreateAndAllocateBufferParam;
int create_and_allocate_buffer(const VkAllocationCallbacks * alloc_callbacks,
                               GPUAllocator *p_allocr,
                               VkDevice device,
  CreateAndAllocateBufferParam param);