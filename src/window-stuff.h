#pragma once
#include "common-stuff.h"

struct SwapchainEntities {
    VkSwapchainKHR swapchain;

    uint32_t img_count;
    // All following have this img_count elements if successfully
    // created

    VkImage *images;
    VkImageView *img_views;
    //Properly record all images status from these fences
    VkFence *img_fences;

    VkDeviceMemory device_mem;
    VkImage *depth_imgs;
    VkImageView *depth_views;

    VkFramebuffer *framebuffers;
};


typedef struct {
    struct VulkanDevice device;
    int win_width;
    int win_height;
    VkSurfaceKHR surface;
    VkSwapchainKHR old_swapchain;

    //These two values, if changes upon query, will return a success code that is not 0
    VkSurfaceFormatKHR *p_surface_format;
    uint32_t *p_min_img_count;

    VkExtent2D *p_img_swap_extent;
    struct SwapchainEntities *p_curr_swapchain_data;
} CreateSwapchainParam;

int create_swapchain(StackAllocator *stk_allocr, size_t stk_offset,
                     VkAllocationCallbacks *alloc_callbacks,
                     CreateSwapchainParam param);

typedef struct {
    VkDevice device;
    struct SwapchainEntities *p_swapchain_data;
} ClearSwapchainParam;

void clear_swapchain(const VkAllocationCallbacks *alloc_callbacks,
                     ClearSwapchainParam param, int err_codes);

typedef struct {
    VkDevice device;
    VkPhysicalDevice phy_device;
    size_t depth_count;
    VkExtent2D img_extent;
    VkFormat depth_img_format;


    VkImage **p_depth_buffers;
    VkImageView **p_depth_buffer_views;
    VkDeviceMemory *p_depth_memory;
} CreateDepthbuffersParam;

int create_depthbuffers(StackAllocator *stk_allocr, size_t stk_offset,
                        const VkAllocationCallbacks *alloc_callbacks,
                        CreateDepthbuffersParam param);

typedef struct {
    VkDevice device;
    size_t depth_count;

    VkImage **p_depth_buffers;
    VkImageView **p_depth_buffer_views;
    VkDeviceMemory *p_depth_memory;
}ClearDepthbuffersParam;

void clear_depthbuffers(const VkAllocationCallbacks *alloc_callbacks,
                        ClearDepthbuffersParam param, int err_code);

typedef struct {
    VkDevice device;

    VkExtent2D framebuffer_extent;
    VkRenderPass compatible_render_pass;
    uint32_t framebuffer_count;
    VkImageView *img_views;
    VkImageView *depth_views;

    VkFramebuffer **p_framebuffers;

} CreateFramebuffersParam;

int create_framebuffers(StackAllocator *stk_allocr, size_t stk_offset,
                        const VkAllocationCallbacks *alloc_callbacks,
                        CreateFramebuffersParam param);

typedef struct {
    VkDevice device;
    uint32_t framebuffer_count;

    VkFramebuffer **p_framebuffers;
} ClearFramebuffersParam;
void clear_framebuffers(const VkAllocationCallbacks *alloc_callbacks,
                        ClearFramebuffersParam param, int err_codes);

typedef struct {
    struct VulkanDevice device;
    int new_win_width;
    int new_win_height;
    VkSurfaceKHR surface;
    VkRenderPass framebuffer_render_pass;
    VkFormat depth_img_format;
    

    // These two values, if changes upon query, will return a success
    // code that is not 0
    VkSurfaceFormatKHR *p_surface_format;
    uint32_t *p_min_img_count;

    VkExtent2D *p_img_swap_extent;
    struct SwapchainEntities *p_old_swapchain_data;
    struct SwapchainEntities *p_new_swapchain_data;
} RecreateSwapchainParam;

int recreate_swapchain(StackAllocator *stk_allocr, size_t stk_offset,
                       VkAllocationCallbacks *alloc_callbacks,
                       RecreateSwapchainParam param);
