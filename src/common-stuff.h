#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <stdlib.h>

#define COUNT_OF(x)                                                  \
    ((sizeof(x) / sizeof(0 [x])) /                                   \
     ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

struct VulkanLayer {
    const char *layer_name;
    int required;
    int available;
};

struct VulkanExtension {
    const char *extension_name;
    int required;
    int available;
    struct VulkanLayer *layer;
};

struct StackAllocator {
    uint8_t *base_memory;
    size_t reserved_memory;
    size_t committed_memory;
    size_t page_size;
};
typedef struct StackAllocator StackAllocator;

#define align_up_(size, alignment)                                   \
    ((((unsigned long long)size) + ((unsigned long long)alignment) - \
      1ULL) &                                                        \
     ~(((unsigned long long)alignment) - 1ULL))

#define align_down_(size, alignment)                                 \
    (~((unsigned long long)(alignment)-1ULL) &                       \
     ((unsigned long long)size))

#define SIZE_KB(size) (((unsigned long long)size) << 10ULL)
#define SIZE_MB(size) (((unsigned long long)size) << 20ULL)
#define SIZE_GB(size) (((unsigned long long)size) << 30ULL)

StackAllocator alloc_stack_allocator(size_t reserve, size_t commit);

void dealloc_stack_allocator(StackAllocator *allocr);

// alignment should be a power of 2
void *stack_allocate(StackAllocator *data, size_t *curr_offset,
                     size_t size, size_t alignment);

int create_instance(StackAllocator *stk_allocr,
                    size_t stk_allocr_offset,
                    const VkAllocationCallbacks *alloc_callbacks,
                    VkInstance *p_vk_instance,
                    struct VulkanLayer *instance_layers,
                    size_t layers_count,
                    struct VulkanExtension *instance_extensions,
                    size_t extensions_count);

void clear_instance(const VkAllocationCallbacks *alloc_callbacks,
                    VkInstance *p_vk_instance, int err_codes);

typedef struct {
    VkPhysicalDevice phy_device;
    VkSurfaceKHR surface;

    //Optional, if NULL, then skips assigning{used to check validity of GPU}
    VkSurfaceFormatKHR *p_img_format;
    VkPresentModeKHR *p_present_mode;
    uint32_t *p_min_img_count;
    VkSurfaceTransformFlagBitsKHR *p_transform_flags;
    VkExtent2D *p_surface_extent;

} ChooseSwapchainDetsParam;

int choose_swapchain_details(StackAllocator *stk_allocr,
                             size_t stk_offset,
                             ChooseSwapchainDetsParam param);

struct VulkanDevice {
    VkPhysicalDevice phy_device;
    VkDevice device;

    VkQueue graphics_queue;
    uint32_t graphics_family_inx;

    VkQueue present_queue;
    uint32_t present_family_inx;
};

typedef struct {
    VkInstance vk_instance;
    VkSurfaceKHR chosen_surface;

    VkFormat *p_depth_stencil_format;
    VkSurfaceFormatKHR *p_img_format;
    uint32_t *p_min_img_count;
    struct VulkanDevice *p_vk_device;
} CreateDeviceParam;

int create_device(StackAllocator *stk_allocr,
                  size_t stk_allocr_offset,
                  VkAllocationCallbacks *alloc_callbacks,
                  CreateDeviceParam param,
                  struct VulkanLayer *device_layers,
                  size_t layers_count,
                  struct VulkanExtension *device_extensions, size_t extensions_count);

typedef struct {
    VkDevice *p_device;
    VkPhysicalDevice *p_phy_device;
} ClearDeviceParam;
void clear_device(const VkAllocationCallbacks *alloc_callbacks,
                         ClearDeviceParam param, int err_codes);

typedef struct {
    VkCommandPool *p_cmd_pool;
    uint32_t queue_family_inx;
    VkDevice device;
} CreateCommandPoolParam;

int create_command_pool(StackAllocator *stk_allocr, size_t stk_offset,
                        const VkAllocationCallbacks *alloc_callbacks,
                        CreateCommandPoolParam param);

typedef struct {
    VkCommandPool *p_cmd_pool;
    VkDevice device;
} ClearCommandPoolParam;
void clear_command_pool(const VkAllocationCallbacks *alloc_callbacks,
                        ClearCommandPoolParam param, int err_code);


//Choose an image format if supported, returns VK_FORMAT_UNDEFINED if fails
VkFormat choose_image_format(VkPhysicalDevice phy_device, VkFormat *format_candidates,
                             size_t count,
                             VkImageTiling img_tiling, VkFormatFeatureFlags format_flags);
