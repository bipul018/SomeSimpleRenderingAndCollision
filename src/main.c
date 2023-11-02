#include <Windows.h>
#include <windowsx.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "common-stuff.h"
#include "device-mem-stuff.h"
#include "model_gen.h"
#include "render-stuff.h"
#include "vectors.h"
#include "window-stuff.h"


#include "misc_tools.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "imgui/cimgui.h"

#include "imgui/imgui/imgui_impl_vulkan.h"
#include "imgui/imgui/imgui_impl_win32.h"
#include "main.h"
#include "LooplessSizeMove.h"
#include "collides.h"

#include <extra_c_macros.h>
#include <extra_c_helper_fxns.h>


int ImGui_ImplWin32_CreateVkSurface(ImGuiViewport *viewport, ImU64 vk_instance,
                                    const void *vk_allocator, ImU64 *out_vk_surface) {

    VkWin32SurfaceCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = GetModuleHandle(NULL),
            .hwnd = viewport->PlatformHandleRaw,
    };

    return VK_SUCCESS ==
           vkCreateWin32SurfaceKHR((VkInstance) vk_instance, &create_info, vk_allocator,
                                   (VkSurfaceKHR *) out_vk_surface);
}

void *VKAPI_PTR cb_allocation_fxn(void *user_data, size_t size, size_t align,
                                  VkSystemAllocationScope scope) {

    size_t *ptr = user_data;
    (*ptr) += size;
    printf("%zu : %zu Alloc\t", *ptr, align);
    return _aligned_offset_malloc(size, 8, 0);
}

void *VKAPI_PTR cb_reallocation_fxn(void *user_data, void *p_original, size_t size, size_t align,
                                    VkSystemAllocationScope scope) {
    size_t *ptr = user_data;
    printf("%zu : %zu Realloc\t", *ptr, align);
    return _aligned_offset_realloc(p_original, size, 8, 0);
}


void VKAPI_PTR cb_free_fxn(void *user_data, void *p_mem) {
    size_t *ptr = user_data;
    //(*ptr) -= _aligned_msize(p_mem, 8, 0);
    printf("%zu Free\t", *ptr);
    _aligned_free(p_mem);
}


// Letter unit
struct CharacterModel {
    int codepoint;
    struct Model model;
    struct CharacterModel *next;
};

struct CharacterModel *search_character_model(struct CharacterModel *head, int codepoint) {
    while (head) {
        if (head->codepoint == codepoint)
            return head;
        head = head->next;
    }
    return head;
}

bool push_character_model(StackAllocator *stk_allocr, size_t *stk_offset,
                          VkAllocationCallbacks *alloc_callbacks, GPUAllocator *p_gpu_mem_allocr,
                          VkDevice device, struct CharacterModel **phead, int codepoint) {


    struct CharacterModel *model =
            stack_allocate(stk_allocr, stk_offset, sizeof *model, sizeof *model);

    GenerateModelOutput out =
            load_text_character(stk_allocr, *stk_offset, codepoint, (Vec3) {0.f, 0.f, 200.f});
    if (!out.vertices || !out.indices)
        return false;


    if (create_model(alloc_callbacks,
                     (CreateModelParam) {
                             .device = device,
                             .p_allocr = p_gpu_mem_allocr,
                             .index_count = out.index_count,
                             .indices_list = out.indices,
                             .vertex_count = out.vertex_count,
                             .vertices_list = out.vertices,
                     },
                     &(model->model)) < 0)
        return false;

    model->next = *phead;
    model->codepoint = codepoint;
    *phead = model;
    return true;
}


// Forward declare message handler from imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


#pragma warning(push, 3)
#pragma warning(default : 4703 4700)
enum InitGlobalContextCodes {
    INIT_GLOBAL_CONTEXT_TOP_FAIL = -0x7fff,
    INIT_GLOBAL_CONTEXT_GPU_ALLOC_FAIL,
    INIT_GLOBAL_CONTEXT_VK_DEVICE_FAIL,
    INIT_GLOBAL_CONTEXT_DUMMY_WINDOW_SURFACE_CREATE_FAIL,
    INIT_GLOBAL_CONTEXT_VK_INSTANCE_FAIL,
    INIT_GLOBAL_CONTEXT_ARENA_ALLOC_FAIL,
    INIT_GLOBAL_CONTEXT_OK = 0,
};

int init_global_context(HINSTANCE *h_instance, VkInstance *vk_instance, struct VulkanDevice *device,
                        VkAllocationCallbacks *ptr_alloc_callbacks, StackAllocator *stk_allocr,
                        size_t arena_reserve, GPUAllocator *gpu_mem_allocr, size_t gpu_mem_size,
                        VkSurfaceFormatKHR *surface_img_format, VkFormat *depth_img_format,
                        uint32_t *min_surface_img_count, int *err_code) {

    *err_code = 0;
    *h_instance = GetModuleHandle(NULL);

    size_t allocation_amt = 0;

    if (ptr_alloc_callbacks)
        *ptr_alloc_callbacks = (VkAllocationCallbacks) {
                .pfnAllocation = cb_allocation_fxn,
                .pfnReallocation = cb_reallocation_fxn,
                .pfnFree = cb_free_fxn,
                .pUserData = &allocation_amt,
        };


    *stk_allocr = alloc_stack_allocator(arena_reserve, SIZE_MB(100));
    size_t stk_offset = 0;
    if (stk_allocr->reserved_memory == 0)
        return INIT_GLOBAL_CONTEXT_ARENA_ALLOC_FAIL;

    *vk_instance = VK_NULL_HANDLE;
    // Create instance
    {
        struct VulkanLayer inst_layers[] = {
#ifndef NDEBUG
                {
                        .layer_name = "VK_LAYER_KHRONOS_validation",
                        .required = 1,
                },
#endif
                {0}
        };

        struct VulkanExtension inst_exts[] = {
#ifndef NDEBUG
                {
                        .extension_name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                        .required = 1,
                },

#endif
                {
                        .extension_name = VK_KHR_SURFACE_EXTENSION_NAME,
                        .required = 1,
                },
                {
                        .extension_name = VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
                        .required = 1,
                },

                {0}
        };

        *err_code =
                create_instance(stk_allocr, stk_offset, ptr_alloc_callbacks, vk_instance, inst_layers,
                                COUNT_OF(inst_layers) - 1, inst_exts, COUNT_OF(inst_exts) - 1);

        if (*err_code < 0)
            return INIT_GLOBAL_CONTEXT_VK_INSTANCE_FAIL;
    }


    LPWSTR class_name = L"Window Class";

    WNDCLASS wnd_class = {0};
    wnd_class.hInstance = *h_instance;
    wnd_class.lpszClassName = class_name;
    wnd_class.lpfnWndProc = DefWindowProc;
    RegisterClass(&wnd_class);

    HANDLE handle = NULL;
    if (!(handle = CreateWindowEx(0, class_name, L"Tmp", WS_OVERLAPPEDWINDOW, 20, 10, 800, 700,
                                  NULL, NULL, *h_instance, NULL))) {
        UnregisterClass(class_name, *h_instance);
        return INIT_GLOBAL_CONTEXT_DUMMY_WINDOW_SURFACE_CREATE_FAIL;
    }

    // Create surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    {
        VkWin32SurfaceCreateInfoKHR create_info = {
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hinstance = *h_instance,
                .hwnd = handle,
        };

        if (vkCreateWin32SurfaceKHR(*vk_instance, &create_info, ptr_alloc_callbacks, &surface) !=
            VK_SUCCESS) {
            DestroyWindow(handle);
            UnregisterClass(class_name, *h_instance);
            return INIT_GLOBAL_CONTEXT_DUMMY_WINDOW_SURFACE_CREATE_FAIL;
        }
    }


    *surface_img_format = (VkSurfaceFormatKHR) {0};
    *depth_img_format = VK_FORMAT_UNDEFINED;
    *min_surface_img_count = 0;


    *device = (struct VulkanDevice) {0};
    {
        CreateDeviceParam param = {
                .vk_instance = *vk_instance,
                .chosen_surface = surface,
                .p_vk_device = device,
                .p_min_img_count = min_surface_img_count,
                .p_img_format = surface_img_format,
                .p_depth_stencil_format = depth_img_format,
        };

        struct VulkanLayer dev_layers[] = {
#ifndef NDEBUG
                {
                        .layer_name = "VK_LAYER_KHRONOS_validation",
                        .required = 1,
                },
#endif
                {0}
        };

        struct VulkanExtension dev_extensions[] = {
                {.extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME, .required = 1},
                {.extension_name = VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, .required = 1},
                {0}
        };

        *err_code =
                create_device(stk_allocr, stk_offset, ptr_alloc_callbacks, param, dev_layers,
                              COUNT_OF(dev_layers) - 1, dev_extensions, COUNT_OF(dev_extensions) - 1);

        vkDestroySurfaceKHR(*vk_instance, surface, ptr_alloc_callbacks);
        DestroyWindow(handle);
        UnregisterClass(class_name, *h_instance);


        if (*err_code < 0)
            return INIT_GLOBAL_CONTEXT_VK_DEVICE_FAIL;
    }


    // Info to collect for total memory

    // For now , required memory types will be hardcoded, later must
    // be properly evaluated, maybe after textures are implemented
    uint32_t memory_type_flags = 255;
    size_t total_gpu_memory = gpu_mem_size;
    *gpu_mem_allocr = (GPUAllocator) {0};
    {
        *err_code = allocate_device_memory(
                ptr_alloc_callbacks,
                (AllocateDeviceMemoryParam) {.device = device->device,
                        .phy_device = device->phy_device,
                        .p_gpu_allocr = gpu_mem_allocr,
                        .allocation_size = total_gpu_memory,
                        .memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                        .memory_type_flag_bits = memory_type_flags});
        if (*err_code < 0)
            return INIT_GLOBAL_CONTEXT_GPU_ALLOC_FAIL;
    }
    return INIT_GLOBAL_CONTEXT_OK;
}

void clear_global_context(VkInstance *vk_instance, struct VulkanDevice *device,
                          VkAllocationCallbacks *ptr_alloc_callbacks, StackAllocator *stk_allocr,
                          GPUAllocator *gpu_mem_allocr, int err_code) {
    switch (err_code) {


        case INIT_GLOBAL_CONTEXT_OK:


            err_code = 0;
        case INIT_GLOBAL_CONTEXT_GPU_ALLOC_FAIL:
            free_device_memory(
                    ptr_alloc_callbacks,
                    (FreeDeviceMemoryParam) {.device = device->device, .p_gpu_allocr = gpu_mem_allocr},
                    err_code);

            err_code = 0;
        case INIT_GLOBAL_CONTEXT_VK_DEVICE_FAIL:
            clear_device(
                    ptr_alloc_callbacks,
                    (ClearDeviceParam) {.p_device = &device->device, .p_phy_device = &device->phy_device},
                    err_code);

        case INIT_GLOBAL_CONTEXT_DUMMY_WINDOW_SURFACE_CREATE_FAIL:

            err_code = 0;
        case INIT_GLOBAL_CONTEXT_VK_INSTANCE_FAIL:
            clear_instance(ptr_alloc_callbacks, vk_instance, err_code);


            dealloc_stack_allocator(stk_allocr);
            err_code = 0;
        case INIT_GLOBAL_CONTEXT_ARENA_ALLOC_FAIL:

        case INIT_GLOBAL_CONTEXT_TOP_FAIL:
        default:

            break;
    }
}

enum InitRenderSystemCodes {
    INIT_RENDER_SYSTEM_TOP_ERROR = -0x7fff,
    INIT_RENDER_SYSTEM_CREATE_PRIMARY_CMD_BUFFERS_FAIL,
    INIT_RENDER_SYSTEM_CREATE_CMD_POOL_FAIL,
    INIT_RENDER_SYSTEM_CREATE_SEMAPHORES_FAIL,
    INIT_RENDER_SYSTEM_CREATE_FENCES_FAIL,
    INIT_RENDER_SYSTEM_RENDER_PASS_FAIL,
    INIT_RENDER_SYSTEM_OK = 0,
};

int init_render_system(StackAllocator stk_allocr, size_t stk_offset,
                       VkAllocationCallbacks *ptr_alloc_callbacks, struct VulkanDevice device,
                       VkSurfaceFormatKHR surface_img_format, VkFormat depth_img_format,
                       size_t max_frames_in_flight, VkRenderPass *render_pass,
                       VkFence **frame_fences, VkSemaphore **all_semaphores,
                       VkSemaphore **render_finished_semaphores,
                       VkSemaphore **present_done_semaphores, VkCommandPool *rndr_cmd_pool,
                       VkCommandBuffer **rndr_cmd_buffers, int *err_code) {
    *err_code = 0;
    *render_pass = VK_NULL_HANDLE;
    {
        CreateRenderPassParam param = {
                .device = device.device,
                .img_format = surface_img_format.format,
                .depth_stencil_format = depth_img_format,
                .p_render_pass = &*render_pass,
        };
        *err_code = create_render_pass(&stk_allocr, stk_offset, ptr_alloc_callbacks, param);
        if (*err_code < 0)
            return INIT_RENDER_SYSTEM_RENDER_PASS_FAIL;
    }


    *frame_fences = NULL;
    {
        *err_code = create_fences(&stk_allocr, stk_offset, ptr_alloc_callbacks,
                                  (CreateFencesParam) {.device = device.device,
                                          .fences_count = max_frames_in_flight,
                                          .p_fences = &*frame_fences});
        if (*err_code < 0)
            return INIT_RENDER_SYSTEM_CREATE_FENCES_FAIL;
    }

    *all_semaphores = NULL;
    *render_finished_semaphores = NULL;
    *present_done_semaphores = NULL;
    {
        *err_code =
                create_semaphores(&stk_allocr, stk_offset, ptr_alloc_callbacks,
                                  (CreateSemaphoresParam) {.device = device.device,
                                          .semaphores_count = 2 * max_frames_in_flight,
                                          .p_semaphores = &*all_semaphores});
        if (*err_code < 0)
            return INIT_RENDER_SYSTEM_CREATE_SEMAPHORES_FAIL;
    }
    *present_done_semaphores = *all_semaphores;
    *render_finished_semaphores = *all_semaphores + max_frames_in_flight;


    *rndr_cmd_pool = VK_NULL_HANDLE;
    {
        *err_code = create_command_pool(
                &stk_allocr, stk_offset, ptr_alloc_callbacks,
                (CreateCommandPoolParam) {.device = device.device,
                        .queue_family_inx = device.graphics_family_inx,
                        .p_cmd_pool = &*rndr_cmd_pool});
        if (*err_code < 0)
            return INIT_RENDER_SYSTEM_CREATE_CMD_POOL_FAIL;
    }

    *rndr_cmd_buffers = NULL;
    {
        *err_code = create_primary_command_buffers(
                &stk_allocr, stk_offset, ptr_alloc_callbacks,
                (CreatePrimaryCommandBuffersParam) {.device = device.device,
                        .cmd_pool = *rndr_cmd_pool,
                        .cmd_buffer_count = max_frames_in_flight,
                        .p_cmd_buffers = &*rndr_cmd_buffers});
        if (*err_code < 0)
            return INIT_RENDER_SYSTEM_CREATE_PRIMARY_CMD_BUFFERS_FAIL;
    }
    return INIT_RENDER_SYSTEM_OK;
}

void clear_render_system(VkAllocationCallbacks *ptr_alloc_callbacks, struct VulkanDevice device,
                         size_t max_frames_in_flight, VkRenderPass *render_pass,
                         VkFence **frame_fences, VkSemaphore **all_semaphores,
                         VkSemaphore **render_finished_semaphores,
                         VkSemaphore **present_done_semaphores, VkCommandPool *rndr_cmd_pool,
                         VkCommandBuffer **rndr_cmd_buffers, int err_code) {

    switch (err_code) {
        case INIT_RENDER_SYSTEM_OK:

            err_code = 0;
        case INIT_RENDER_SYSTEM_CREATE_PRIMARY_CMD_BUFFERS_FAIL:
            clear_primary_command_buffers(
                    ptr_alloc_callbacks,
                    (ClearPrimaryCommandBuffersParam) {.p_cmd_buffers = rndr_cmd_buffers}, err_code);

            err_code = 0;
        case INIT_RENDER_SYSTEM_CREATE_CMD_POOL_FAIL:
            clear_command_pool(
                    ptr_alloc_callbacks,
                    (ClearCommandPoolParam) {.device = device.device, .p_cmd_pool = rndr_cmd_pool},
                    err_code);

            err_code = 0;
        case INIT_RENDER_SYSTEM_CREATE_SEMAPHORES_FAIL:
            clear_semaphores(ptr_alloc_callbacks,
                             (ClearSemaphoresParam) {.device = device.device,
                                     .semaphores_count = 2 * max_frames_in_flight,
                                     .p_semaphores = all_semaphores},
                             err_code);
            *present_done_semaphores = NULL;
            *render_finished_semaphores = NULL;

            err_code = 0;
        case INIT_RENDER_SYSTEM_CREATE_FENCES_FAIL:
            clear_fences(ptr_alloc_callbacks,
                         (ClearFencesParam) {.device = device.device,
                                 .fences_count = max_frames_in_flight,
                                 .p_fences = frame_fences},
                         err_code);

            err_code = 0;
        case INIT_RENDER_SYSTEM_RENDER_PASS_FAIL:
            clear_render_pass(
                    ptr_alloc_callbacks,
                    (ClearRenderPassParam) {.device = device.device, .p_render_pass = render_pass},
                    err_code);
        case INIT_RENDER_SYSTEM_TOP_ERROR:
        default:
            break;
    }
}

enum InitVkWindowCodes {
    INIT_VK_WINDOW_TOP_ERROR = -0x7fff,
    INIT_VK_WINDOW_CREATE_FRAMEBUFFER_FAIL,
    INIT_VK_WINDOW_CREATE_DEPTHBUFFER_FAIL,
    INIT_VK_WINDOW_CREATE_SWAPCHAIN_FAIL,
    INIT_VK_WINDOW_INCOMPATIBLE_RENDER_PASS,
    INIT_VK_WINDOW_CREATE_SURFACE_FAIL,
    INIT_VK_WINDOW_CREATE_WND_FAIL,
    INIT_VK_WINDOW_OK = 0,
};

int init_vk_window(StackAllocator *stk_allocr, size_t stk_offset,
                   VkAllocationCallbacks *ptr_alloc_callbacks, HINSTANCE h_instance,
                   VkInstance vk_instance, struct VulkanDevice device,
                   VkSurfaceFormatKHR surface_img_format, VkFormat depth_img_format,
                   uint32_t min_surface_img_count, VkRenderPass render_pass, LPCTSTR wndclass_name,
                   LPCTSTR window_name, HWND *p_wnd_handle, void *winproc_data,
                   WNDPROC wnd_proc_func, VkSurfaceKHR *surface,
                   struct SwapchainEntities *swapchain_data, VkExtent2D *swapchain_extent,
                   int *err_code) {
    WNDCLASS wnd_class = {0};
    wnd_class.hInstance = h_instance;
    wnd_class.lpszClassName = wndclass_name;
    wnd_class.lpfnWndProc = wnd_proc_func;
    RegisterClass(&wnd_class);

    *p_wnd_handle = CreateWindowEx(0, wndclass_name, window_name, WS_OVERLAPPEDWINDOW | WS_MAXIMIZE, 20, 10, 800,
                                   700, NULL, NULL, h_instance, winproc_data);
    if (!(*p_wnd_handle))
        return INIT_VK_WINDOW_CREATE_WND_FAIL;

    *surface = VK_NULL_HANDLE;
    {
        VkWin32SurfaceCreateInfoKHR create_info = {
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hinstance = h_instance,
                .hwnd = *p_wnd_handle,
        };

        if (vkCreateWin32SurfaceKHR(vk_instance, &create_info, ptr_alloc_callbacks, &*surface) !=
            VK_SUCCESS)
            return INIT_VK_WINDOW_CREATE_SURFACE_FAIL;
    }

    ShowWindow(*p_wnd_handle, SW_SHOW);
    RECT rc;
    GetClientRect(*p_wnd_handle, &rc);

    // Some additional surface properties required, prewritten also from device
    // create
    *swapchain_data = (struct SwapchainEntities) {0};
    {
        *err_code = create_swapchain(stk_allocr, stk_offset, ptr_alloc_callbacks,
                                     (CreateSwapchainParam) {
                                             .device = device,
                                             .win_height = rc.bottom,
                                             .win_width = rc.right,
                                             .surface = *surface,
                                             .p_surface_format = &surface_img_format,
                                             .p_min_img_count = &min_surface_img_count,
                                             .p_img_swap_extent = swapchain_extent,
                                             .p_curr_swapchain_data = swapchain_data,
                                     });
        if (*err_code > 0)
            return INIT_VK_WINDOW_INCOMPATIBLE_RENDER_PASS;
        if (*err_code < 0)
            return INIT_VK_WINDOW_CREATE_SWAPCHAIN_FAIL;
    }

    // Create depth image and image views
    {
        *err_code = create_depthbuffers(stk_allocr, stk_offset, ptr_alloc_callbacks,
                                        (CreateDepthbuffersParam) {
                                                .device = device.device,
                                                .phy_device = device.phy_device,
                                                .depth_count = swapchain_data->img_count,
                                                .img_extent = *swapchain_extent,
                                                .depth_img_format = depth_img_format,
                                                .p_depth_buffers = &swapchain_data->depth_imgs,
                                                .p_depth_buffer_views = &swapchain_data->depth_views,
                                                .p_depth_memory = &swapchain_data->device_mem,
                                        });
        if (*err_code < 0)
            return INIT_VK_WINDOW_CREATE_DEPTHBUFFER_FAIL;
    }

    // Create framebuffers on swapchain
    {
        *err_code = create_framebuffers(stk_allocr, stk_offset, ptr_alloc_callbacks,
                                        (CreateFramebuffersParam) {
                                                .device = device.device,
                                                .compatible_render_pass = render_pass,
                                                .framebuffer_count = swapchain_data->img_count,
                                                .framebuffer_extent = *swapchain_extent,
                                                .img_views = swapchain_data->img_views,
                                                .depth_views = swapchain_data->depth_views,
                                                .p_framebuffers = &swapchain_data->framebuffers,
                                        });
        if (*err_code < 0)
            return INIT_VK_WINDOW_CREATE_FRAMEBUFFER_FAIL;
    }
    return INIT_VK_WINDOW_OK;
}

void clear_vk_window(const VkAllocationCallbacks *ptr_alloc_callbacks, HINSTANCE h_instance,
                     VkInstance vk_instance, LPCTSTR wnd_class_name, HWND *wnd_handle,
                     VkSurfaceKHR *surface, struct VulkanDevice device,
                     struct SwapchainEntities *curr_swapchain_data,
                     struct SwapchainEntities *old_swapchain_data, int err_code) {

    switch (err_code) {

        case INIT_VK_WINDOW_OK:
            err_code = 0;
        case INIT_VK_WINDOW_CREATE_FRAMEBUFFER_FAIL:
            clear_framebuffers(
                    ptr_alloc_callbacks,
                    (ClearFramebuffersParam) {.device = device.device,
                            .framebuffer_count = old_swapchain_data->img_count,
                            .p_framebuffers = &old_swapchain_data->framebuffers},
                    0);
            clear_framebuffers(
                    ptr_alloc_callbacks,
                    (ClearFramebuffersParam) {.device = device.device,
                            .framebuffer_count = curr_swapchain_data->img_count,
                            .p_framebuffers = &curr_swapchain_data->framebuffers},
                    err_code);


            err_code = 0;
        case INIT_VK_WINDOW_CREATE_DEPTHBUFFER_FAIL:
            clear_depthbuffers(ptr_alloc_callbacks,
                               (ClearDepthbuffersParam) {
                                       .device = device.device,
                                       .depth_count = old_swapchain_data->img_count,
                                       .p_depth_buffers = &old_swapchain_data->depth_imgs,
                                       .p_depth_buffer_views = &old_swapchain_data->depth_views,
                                       .p_depth_memory = &old_swapchain_data->device_mem,
                               },
                               0);
            clear_depthbuffers(ptr_alloc_callbacks,
                               (ClearDepthbuffersParam) {
                                       .device = device.device,
                                       .depth_count = curr_swapchain_data->img_count,
                                       .p_depth_buffers = &curr_swapchain_data->depth_imgs,
                                       .p_depth_buffer_views = &curr_swapchain_data->depth_views,
                                       .p_depth_memory = &curr_swapchain_data->device_mem,
                               },
                               err_code);

        case INIT_VK_WINDOW_INCOMPATIBLE_RENDER_PASS:

            err_code = 0;
        case INIT_VK_WINDOW_CREATE_SWAPCHAIN_FAIL:
            clear_swapchain(
                    ptr_alloc_callbacks,
                    (ClearSwapchainParam) {.device = device.device, .p_swapchain_data = curr_swapchain_data},
                    err_code);
            clear_swapchain(
                    ptr_alloc_callbacks,
                    (ClearSwapchainParam) {.device = device.device, .p_swapchain_data = old_swapchain_data},
                    0);


            err_code = 0;

            err_code = 0;
            vkDestroySurfaceKHR(vk_instance, *surface, ptr_alloc_callbacks);
        case INIT_VK_WINDOW_CREATE_SURFACE_FAIL:
            *surface = VK_NULL_HANDLE;

            DestroyWindow(*wnd_handle);
            err_code = 0;
        case INIT_VK_WINDOW_CREATE_WND_FAIL:
            *wnd_handle = NULL;

            UnregisterClass(wnd_class_name, h_instance);
        case INIT_VK_WINDOW_TOP_ERROR:
        default:
            break;
    }
}


struct WinProcData {
    UINT_PTR timer_id;
    int width;
    int height;
    HANDLE win_handle;

    POINT mouse_pos;
    bool init_success;

    // Data that are UI controlled
    Vec3 *ptr_translate;
    Vec3 *ptr_rotate;
    Vec3 *ptr_scale;
    bool char_pressed;
    int char_codepoint;
};

timespec nctimer;

LRESULT CALLBACK wnd_proc(HWND h_wnd, UINT msg, WPARAM wparam, LPARAM lparam) {

    struct WinProcData *pdata = NULL;

    if (msg == WM_CREATE) {
        CREATESTRUCT *cr_data = (CREATESTRUCT *) lparam;
        pdata = (struct WinProcData *) cr_data->lpCreateParams;
        SetWindowLongPtr(h_wnd, GWLP_USERDATA, (LONG_PTR) pdata);

        pdata->win_handle = h_wnd;

        RECT client_area;
        GetClientRect(h_wnd, &client_area);
        pdata->width = client_area.right;
        pdata->height = client_area.bottom;

        pdata->timer_id = SetTimer(h_wnd, 111, 20, NULL);

    } else {
        pdata = (struct WinProcData *) GetWindowLongPtr(h_wnd, GWLP_USERDATA);
    }
    if (ImGui_ImplWin32_WndProcHandler(h_wnd, msg, wparam, lparam))
        return 1;
    if (pdata) {
        if (msg == WM_DESTROY) {
            KillTimer(h_wnd, pdata->timer_id);
            PostQuitMessage(0);
            return 0;
        }

        if (msg == WM_PAINT) {

        }
        if (msg == WM_TIMER) {
            if (wparam == pdata->timer_id) {
                InvalidateRect(h_wnd, NULL, true);
            }
        }
        if (msg == WM_SIZE) {

            RECT client_area;
            GetClientRect(h_wnd, &client_area);
            pdata->width = client_area.right;
            pdata->height = client_area.bottom;
            //recreate_swapchain(pdata);
            return 1;
        }
        if(msg == WM_MOVE){
        }
        if(msg == WM_WINDOWPOSCHANGED ){
        }
        if(msg == WM_SIZING){
            return TRUE;
        }
        if(msg == WM_NCLBUTTONDOWN){
            //return 1;
        }

        if (!pdata->init_success)
            return DefWindowProc(h_wnd, msg, wparam, lparam);

        if (!igGetIO()->WantCaptureKeyboard) {
            if (msg == WM_KEYDOWN) {
                if (pdata->ptr_translate) {
                    switch (wparam) {
                        case VK_NEXT:
                            pdata->ptr_translate->z -= 10.f;
                            break;
                        case VK_PRIOR:
                            pdata->ptr_translate->z += 10.f;
                            break;
                        case VK_LEFT:
                            pdata->ptr_translate->x -= 10.f;
                            break;
                        case VK_RIGHT:
                            pdata->ptr_translate->x += 10.f;
                            break;
                        case VK_UP:
                            pdata->ptr_translate->y -= 10.f;
                            break;
                        case VK_DOWN:
                            pdata->ptr_translate->y += 10.f;
                            break;
                    }
                }
            }
            if (msg == WM_CHAR) {
                pdata->char_pressed = true;
                pdata->char_codepoint = wparam;
            }
        }
        if (!igGetIO()->WantCaptureMouse) {
            if (msg == WM_SETCURSOR) {
                HCURSOR cursor = LoadCursor(NULL, IDC_UPARROW);
                SetCursor(cursor);
            }
            if (msg == WM_MOUSEWHEEL) {
                if (pdata->ptr_scale) {
                    *(pdata->ptr_scale) = vec3_scale_fl(
                            *(pdata->ptr_scale),
                            powf(1.1f, (float) GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA));
                }
            }
            if (msg == WM_MOUSEMOVE) {
                POINT mouse_pos = {.x = GET_X_LPARAM(lparam), .y = GET_Y_LPARAM(lparam)};
                if ((wparam & MK_LBUTTON) && pdata->ptr_rotate) {

                    Mat4 rot_org = mat4_rotation_XYZ(*pdata->ptr_rotate);


                    Mat4 rot_new = mat4_rotation_XYZ(
                            (Vec3) {.y = (mouse_pos.x - pdata->mouse_pos.x) / (-20 * M_PI),
                                    .x = (mouse_pos.y - pdata->mouse_pos.y) / (20 * M_PI)});

                    Mat4 res = mat4_multiply_mat(&rot_new, &rot_org);

                    if (fabsf(fabsf(res.cols[0].comps[2]) - 1.f) > 0.0001f) {
                        pdata->ptr_rotate->y = -asinf(res.mat[0][2]);
                        pdata->ptr_rotate->x = atan2f(res.mat[1][2] / cosf(pdata->ptr_rotate->y),
                                                      res.mat[2][2] / cosf(pdata->ptr_rotate->y));
                        pdata->ptr_rotate->z = atan2f(res.mat[0][1] / cosf(pdata->ptr_rotate->y),
                                                      res.mat[0][0] / cosf(pdata->ptr_rotate->y));
                    } else {
                        pdata->ptr_rotate->z = 0.f;
                        pdata->ptr_rotate->y = res.mat[0][2] * M_PI / -2.f;
                        pdata->ptr_rotate->x =
                                atan2f(-res.mat[1][0] * res.mat[0][2], -res.mat[2][0] * res.mat[0][2]);
                    }
                }
                pdata->mouse_pos = mouse_pos;
            }
        }
    }

    return LSMProc(h_wnd,msg,wparam,lparam);
    //return DefWindowProc(h_wnd, msg, wparam, lparam);
}


int main(int argc, char *argv[]) {


    enum MainFailCodes {
        MAIN_FAIL_ALL = -0x7fff,

        MAIN_FAIL_MODEL_LOAD,

        MAIN_FAIL_IMGUI_INIT,

        MAIN_FAIL_GRAPHICS_MESH_PIPELINE,
        MAIN_FAIL_GRAPHICS_PIPELINE,
        MAIN_FAIL_GRAPHICS_PIPELINE_LAYOUT,

        MAIN_FAIL_DESCRIPTOR_ALLOC_AND_BIND,
        MAIN_FAIL_DESCRIPTOR_MEM_ALLOC,
        MAIN_FAIL_DESCRIPTOR_BUFFER,
        MAIN_FAIL_DESCRIPTOR_BUFFER_MEM_ALLOC,
        MAIN_FAIL_DESCRIPTOR_POOL,
        MAIN_FAIL_DESCRIPTOR_LAYOUT,

        MAIN_FAIL_WINDOW,
        MAIN_FAIL_RENDER_SYSTEM,
        MAIN_FAIL_GLOBAL_CONTEXT,

        MAIN_FAIL_OK = 0,
    } failure = MAIN_FAIL_OK;

#define return_main_fail(fail_code)                                                                \
    {                                                                                              \
        DebugBreak();                                                                              \
        failure = fail_code;                                                                       \
        goto cleanup_phase;                                                                        \
    }
    int err_code = 0;

    HINSTANCE h_instance;
    VkInstance vk_instance;
    struct VulkanDevice device;
    VkAllocationCallbacks alloc_callbacks;
    StackAllocator stk_allocr;
    size_t stk_offset = 0;
    GPUAllocator gpu_mem_allocr;
    VkSurfaceFormatKHR surface_img_format;
    VkFormat depth_img_format;
    uint32_t min_surface_img_count;
    VkAllocationCallbacks *ptr_alloc_callbacks = NULL;
    int init_global_err_code = 0;
    err_code =
            init_global_context(&h_instance, &vk_instance, &device, ptr_alloc_callbacks, &stk_allocr,
                                SIZE_GB(2), &gpu_mem_allocr, SIZE_MB(50), &surface_img_format,
                                &depth_img_format, &min_surface_img_count, &init_global_err_code);


    // Determine the _max frames in flight
    size_t max_frames_in_flight = 3;
    size_t curr_frame_in_flight = 0;

    VkRenderPass render_pass;
    VkFence *frame_fences;
    VkSemaphore *all_semaphores;
    VkSemaphore *render_finished_semaphores;
    VkSemaphore *present_done_semaphores;
    VkCommandPool rndr_cmd_pool;
    VkCommandBuffer *rndr_cmd_buffers;
    int init_render_err_code = 0;
    err_code =
            init_render_system(stk_allocr, stk_offset, ptr_alloc_callbacks, device, surface_img_format,
                               depth_img_format, max_frames_in_flight, &render_pass, &frame_fences,
                               &all_semaphores, &render_finished_semaphores, &present_done_semaphores,
                               &rndr_cmd_pool, &rndr_cmd_buffers, &init_render_err_code);


    LPWSTR wndclass_name = L"Window Class";
    HWND wnd_handle = NULL;
    struct WinProcData winproc_data = {0};
    VkSurfaceKHR surface;
    struct SwapchainEntities curr_swapchain_data = {0};
    struct SwapchainEntities old_swapchain_data = {0};
    VkExtent2D swapchain_extent;
    int init_vk_wnd_err_code = 0;
    winproc_data.init_success = false;
    err_code =
            init_vk_window(&stk_allocr, stk_offset, ptr_alloc_callbacks, h_instance, vk_instance, device,
                           surface_img_format, depth_img_format, min_surface_img_count, render_pass,
                           wndclass_name, L"Window", &wnd_handle, &winproc_data, wnd_proc, &surface,
                           &curr_swapchain_data, &swapchain_extent, &init_vk_wnd_err_code);
    winproc_data.init_success = true;

    // Create descriptor layouts
    VkDescriptorSetLayout g_matrix_layout;
    VkDescriptorSetLayout g_lights_layout;
    {
        err_code = create_descriptor_layouts(
                ptr_alloc_callbacks,
                (CreateDescriptorLayoutsParam) {.device = device.device,
                        .p_lights_set_layout = &g_lights_layout,
                        .p_matrix_set_layout = &g_matrix_layout});
        if (err_code < 0) return_main_fail(MAIN_FAIL_DESCRIPTOR_LAYOUT);
    }

    // Create descriptor pool
    VkDescriptorPool g_descriptor_pool = VK_NULL_HANDLE;
    {
        err_code = create_descriptor_pool(
                ptr_alloc_callbacks,
                (CreateDescriptorPoolParam) {.device = device.device,
                        .no_matrix_sets = max_frames_in_flight,
                        .no_light_sets = max_frames_in_flight,
                        .p_descriptor_pool = &g_descriptor_pool});
        if (err_code < 0) return_main_fail(MAIN_FAIL_DESCRIPTOR_POOL);
    }

    // Create buffers for descriptors
    GpuAllocrAllocatedBuffer *g_matrix_uniform_buffers = NULL;
    GpuAllocrAllocatedBuffer *g_lights_uniform_buffers = NULL;
    {
        GpuAllocrAllocatedBuffer *p_buffs = stack_allocate(
                &stk_allocr, &stk_offset, sizeof(GpuAllocrAllocatedBuffer) * 2 * max_frames_in_flight, 1);
        if (!p_buffs) return_main_fail(MAIN_FAIL_DESCRIPTOR_BUFFER_MEM_ALLOC);
        ZeroMemory(p_buffs, 2 * max_frames_in_flight * sizeof *p_buffs);
        g_matrix_uniform_buffers = p_buffs;
        g_lights_uniform_buffers = p_buffs + max_frames_in_flight;
    }
    {
        for (size_t i = 0; i < max_frames_in_flight; ++i) {
            err_code =
                    create_and_allocate_buffer(ptr_alloc_callbacks, &gpu_mem_allocr, device.device,
                                               (CreateAndAllocateBufferParam) {
                                                       .p_buffer = g_matrix_uniform_buffers + i,
                                                       .share_mode = VK_SHARING_MODE_EXCLUSIVE,
                                                       .size = sizeof(struct DescriptorMats),
                                                       .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                               });
            if (err_code < 0) return_main_fail(MAIN_FAIL_DESCRIPTOR_BUFFER);
            err_code =
                    create_and_allocate_buffer(ptr_alloc_callbacks, &gpu_mem_allocr, device.device,
                                               (CreateAndAllocateBufferParam) {
                                                       .p_buffer = g_lights_uniform_buffers + i,
                                                       .share_mode = VK_SHARING_MODE_EXCLUSIVE,
                                                       .size = sizeof(struct DescriptorLight),
                                                       .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                               });
            if (err_code < 0) return_main_fail(MAIN_FAIL_DESCRIPTOR_BUFFER);
        }
    }
    VkDescriptorSet *g_matrix_descriptor_sets = NULL;
    VkDescriptorSet *g_lights_descriptor_sets = NULL;
    {
        VkDescriptorSet *ptr =
                stack_allocate(&stk_allocr, &stk_offset,
                               sizeof(VkDescriptorSet) * max_frames_in_flight * 2, sizeof(uintptr_t));
        if (!ptr) return_main_fail(MAIN_FAIL_DESCRIPTOR_MEM_ALLOC);
        ZeroMemory(ptr, 2 * max_frames_in_flight * sizeof *ptr);

        g_matrix_descriptor_sets = ptr;
        g_lights_descriptor_sets = ptr + max_frames_in_flight;
    }
    {


        err_code = allocate_and_bind_descriptors(&stk_allocr, stk_offset, ptr_alloc_callbacks,
                                                 (AllocateAndBindDescriptorsParam) {
                                                         .device = device.device,
                                                         .pool = g_descriptor_pool,
                                                         .lights_sets_count = max_frames_in_flight,
                                                         .lights_set_layout = g_lights_layout,
                                                         .p_light_buffers = g_lights_uniform_buffers,
                                                         .p_light_sets = g_lights_descriptor_sets,
                                                         .matrix_sets_count = max_frames_in_flight,
                                                         .matrix_set_layout = g_matrix_layout,
                                                         .p_matrix_buffers = g_matrix_uniform_buffers,
                                                         .p_matrix_sets = g_matrix_descriptor_sets,
                                                 });
        if (err_code < 0) return_main_fail(MAIN_FAIL_DESCRIPTOR_ALLOC_AND_BIND);
    }

    // Now create the graphics pipeline layout, with only push
    // constant of a float for now
    VkPipelineLayout graphics_pipeline_layout = VK_NULL_HANDLE;
    {

        VkDescriptorSetLayout des_layouts[] = {
                g_matrix_layout,
                g_lights_layout,
        };

        VkPipelineLayoutCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pushConstantRangeCount = push_range_count,
                .pPushConstantRanges = push_ranges,
                .setLayoutCount = COUNT_OF(des_layouts),
                .pSetLayouts = des_layouts,

        };

        if (vkCreatePipelineLayout(device.device, &create_info, ptr_alloc_callbacks,
                                   &graphics_pipeline_layout) != VK_SUCCESS) return_main_fail(
                MAIN_FAIL_GRAPHICS_PIPELINE_LAYOUT);
    }


    // Now create graphics pipeline
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;
    {
        GraphicsPipelineCreationInfos create_infos = default_graphics_pipeline_creation_infos();

        create_infos.vertex_input_state.vertexAttributeDescriptionCount = COUNT_OF(attrib_descs),
                create_infos.vertex_input_state.pVertexAttributeDescriptions = attrib_descs;
        create_infos.vertex_input_state.vertexBindingDescriptionCount = COUNT_OF(binding_descs);
        create_infos.vertex_input_state.pVertexBindingDescriptions = binding_descs;
        create_infos.rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
        /*
          VK_POLYGON_MODE_LINE;
          VK_POLYGON_MODE_POINT;
        */
        create_infos.rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
        /*
        create_infos.rasterization_state.cullMode = VK_CULL_MODE_NONE;
        */
        err_code = create_graphics_pipeline(
                &stk_allocr, stk_offset, ptr_alloc_callbacks,
                (CreateGraphicsPipelineParam) {.create_infos = create_infos,
                        .compatible_render_pass = render_pass,
                        .device = device.device,
                        .pipe_layout = graphics_pipeline_layout,
                        .subpass_index = 0,
                        .vert_shader_file = vert_file_name,
                        .frag_shader_file = frag_file_name,
                        .p_pipeline = &graphics_pipeline});
        if (err_code < 0) return_main_fail(MAIN_FAIL_GRAPHICS_PIPELINE);
    }

    // Now create graphics pipeline for drawing meshes
    VkPipeline graphics_mesh_pipeline = VK_NULL_HANDLE;
    {
        GraphicsPipelineCreationInfos create_infos = default_graphics_pipeline_creation_infos();

        create_infos.vertex_input_state.vertexAttributeDescriptionCount = COUNT_OF(attrib_descs),
                create_infos.vertex_input_state.pVertexAttributeDescriptions = attrib_descs;
        create_infos.vertex_input_state.vertexBindingDescriptionCount = COUNT_OF(binding_descs);
        create_infos.vertex_input_state.pVertexBindingDescriptions = binding_descs;
        create_infos.rasterization_state.polygonMode = VK_POLYGON_MODE_LINE;
        /*
          VK_POLYGON_MODE_POINT;
          VK_POLYGON_MODE_FILL;
        */
        create_infos.rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
        /*
        create_infos.rasterization_state.cullMode = VK_CULL_MODE_NONE;
        */
        err_code = create_graphics_pipeline(
                &stk_allocr, stk_offset, ptr_alloc_callbacks,
                (CreateGraphicsPipelineParam) {.create_infos = create_infos,
                        .compatible_render_pass = render_pass,
                        .device = device.device,
                        .pipe_layout = graphics_pipeline_layout,
                        .subpass_index = 0,
                        .vert_shader_file = vert_file_name,
                        .frag_shader_file = frag_file_name,
                        .p_pipeline = &graphics_mesh_pipeline});
        if (err_code < 0) return_main_fail(MAIN_FAIL_GRAPHICS_MESH_PIPELINE);
    }

    ShowWindow(winproc_data.win_handle, SW_SHOW);

    // Setup imgui
    ImGui_ImplVulkan_InitInfo imgui_init_info;
    {
        igCreateContext(NULL);
        // set docking
        ImGuiIO *ioptr = igGetIO();
        ioptr->ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable |
                              ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard
        // Controls
        // ioptr->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //
        // Enable Gamepad Controls


        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(winproc_data.win_handle);
        igGetPlatformIO()->Platform_CreateVkSurface = ImGui_ImplWin32_CreateVkSurface;

        imgui_init_info = (ImGui_ImplVulkan_InitInfo) {
                .Instance = vk_instance,
                .PhysicalDevice = device.phy_device,
                .Device = device.device,
                .QueueFamily = device.graphics_family_inx,
                .Queue = device.graphics_queue,
                .PipelineCache = VK_NULL_HANDLE,
                .DescriptorPool = g_descriptor_pool,
                .Subpass = 0,
                .MinImageCount = min_surface_img_count,
                .ImageCount = (uint32_t) min_surface_img_count,
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .Allocator = ptr_alloc_callbacks,
                .CheckVkResultFn = NULL,
        };
        ImGui_ImplVulkan_Init(&imgui_init_info, render_pass);

        igStyleColorsDark(NULL);

        // Upload Fonts
        // Use any command queue
        VkCommandPool command_pool = rndr_cmd_pool;
        VkCommandBuffer command_buffer = rndr_cmd_buffers[curr_frame_in_flight];
        VkResult err;
        err = vkResetCommandPool(device.device, command_pool, 0);

        if (err != VK_SUCCESS) return_main_fail(MAIN_FAIL_IMGUI_INIT);

        VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        err = vkBeginCommandBuffer(command_buffer, &begin_info);

        if (err != VK_SUCCESS) return_main_fail(MAIN_FAIL_IMGUI_INIT);

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        VkSubmitInfo end_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffer,
        };
        err = vkEndCommandBuffer(command_buffer);

        if (err != VK_SUCCESS) return_main_fail(MAIN_FAIL_IMGUI_INIT);

        err = vkQueueSubmit(device.graphics_queue, 1, &end_info, VK_NULL_HANDLE);

        if (err != VK_SUCCESS) return_main_fail(MAIN_FAIL_IMGUI_INIT);

        err = vkDeviceWaitIdle(device.device);

        if (err != VK_SUCCESS) return_main_fail(MAIN_FAIL_IMGUI_INIT);

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }


    // Now create the vertex buffers, that are constant across frames
    struct Model plane_model = {0};
    struct Model sphere_model = {0};
    struct Model cube_model = {0};

    {
        GenerateModelOutput out;
        out = load_cuboid_aa(&stk_allocr, stk_offset, (Vec3) {20000.f, 0.f, 20000.f});
        if (!out.vertices || !out.indices) return_main_fail(MAIN_FAIL_MODEL_LOAD);

        if (create_model(ptr_alloc_callbacks,
                         (CreateModelParam) {
                                 .device = device.device,
                                 .p_allocr = &gpu_mem_allocr,
                                 .index_count = out.index_count,
                                 .indices_list = out.indices,
                                 .vertex_count = out.vertex_count,
                                 .vertices_list = out.vertices,
                         },
                         &plane_model) < 0) return_main_fail(MAIN_FAIL_MODEL_LOAD);

        out = load_cuboid_aa(&stk_allocr, stk_offset, (Vec3) {100.f, 100.f, 100.f});
        if (!out.vertices || !out.indices) return_main_fail(MAIN_FAIL_MODEL_LOAD);

        if (create_model(ptr_alloc_callbacks,
                         (CreateModelParam) {
                                 .device = device.device,
                                 .p_allocr = &gpu_mem_allocr,
                                 .index_count = out.index_count,
                                 .indices_list = out.indices,
                                 .vertex_count = out.vertex_count,
                                 .vertices_list = out.vertices,
                         },
                         &cube_model) < 0) return_main_fail(MAIN_FAIL_MODEL_LOAD);

        out = load_sphere_uv(&stk_allocr, stk_offset, 10, 50);
        if (!out.vertices || !out.indices) return_main_fail(MAIN_FAIL_MODEL_LOAD);

        if (create_model(ptr_alloc_callbacks,
                         (CreateModelParam) {
                                 .device = device.device,
                                 .p_allocr = &gpu_mem_allocr,
                                 .index_count = out.index_count,
                                 .indices_list = out.indices,
                                 .vertex_count = out.vertex_count,
                                 .vertices_list = out.vertices,
                         },
                         &sphere_model) < 0) return_main_fail(MAIN_FAIL_MODEL_LOAD);


    }

    // Miscellaneous data + object data
    enum {
        SCENE_MAX_OBJ = 100,
    };
    struct Object3D scene_objs[SCENE_MAX_OBJ]= {0};
    bool object_solid_mode[SCENE_MAX_OBJ] = {0};
    Vec3 velocities[SCENE_MAX_OBJ] = {0};
    Vec3 accelerations[SCENE_MAX_OBJ] = {0};
    float masses[SCENE_MAX_OBJ] = {0};
    int obj_count = 0;
    int active_obj = -1;
    float animation_step = 0.001f;
    double residual_time = 0.f;

    //Collection of all valid model pointers
    struct Model* model_ptrs[] = {nullptr, &plane_model, &cube_model, &sphere_model};
#define _PASTE_MACROS_INT_(P1, P2) P1 ## P2
#define _PASTE_MACROS_(P1,P2) _PASTE_MACROS_INT_(P1,P2)
#define ConvertPtrToId(ptr_var) \
    do{                            \
    for_range(_PASTE_MACROS_( inx,__LINE__), 0, _countof(model_ptrs)){\
        if((ptr_var) == model_ptrs[_PASTE_MACROS_(inx,__LINE__)]){\
            ptr_var = (struct Model*)_PASTE_MACROS_(inx,__LINE__);\
            break;\
        }\
    }}                           \
while(0)

#define RecoverPtrFromId(ptr_var)\
    do{\
        (ptr_var) = (struct Model*)model_ptrs[(int)ptr_var]; \
    }                             \
    while(0)



    bool active_changed = false;
    FILE *debug_save_file = nullptr;
    //save into file
#define SaveAllToFile(filename)\
do{ \
    debug_save_file = fopen( filename, "wb");                                  \
    for_range(i,0,SCENE_MAX_OBJ){    ConvertPtrToId(scene_objs[i].ptr_model);}                           \
    fwrite(scene_objs, sizeof(scene_objs), 1, debug_save_file);\
    for_range(i,0,SCENE_MAX_OBJ){    RecoverPtrFromId(scene_objs[i].ptr_model);}                           \
    fwrite(object_solid_mode, sizeof(object_solid_mode), 1, debug_save_file);\
    fwrite(velocities, sizeof(velocities), 1, debug_save_file);\
    fwrite(accelerations, sizeof(accelerations), 1, debug_save_file);\
    fwrite(masses, sizeof(masses), 1, debug_save_file);\
    fwrite(&obj_count, sizeof(obj_count), 1, debug_save_file);\
    fwrite(&active_obj, sizeof(active_obj), 1, debug_save_file);\
    fwrite(&animation_step, sizeof(animation_step), 1, debug_save_file);\
    fwrite(&residual_time, sizeof(residual_time), 1, debug_save_file);\
    fclose(debug_save_file);\
}while(0)

    //load from file
#define LoadAllFromFile(filename)                                                       \
do {\
    debug_save_file = fopen(filename, "rb");\
    fread(scene_objs, sizeof(scene_objs), 1, debug_save_file);\
    for_range(i,0,SCENE_MAX_OBJ){    RecoverPtrFromId(scene_objs[i].ptr_model);}                           \
    fread(object_solid_mode, sizeof(object_solid_mode), 1, debug_save_file);\
    fread(velocities, sizeof(velocities), 1, debug_save_file);\
    fread(accelerations, sizeof(accelerations), 1, debug_save_file);\
    fread(masses, sizeof(masses), 1, debug_save_file);\
    fread(&obj_count, sizeof(obj_count), 1, debug_save_file);\
    fread(&active_obj, sizeof(active_obj), 1, debug_save_file);\
    fread(&animation_step, sizeof(animation_step), 1, debug_save_file);\
    fread(&residual_time, sizeof(residual_time), 1, debug_save_file);\
    fclose(debug_save_file);                                                            \
    active_changed = true;                                                                                        \
}while(0)

    static const char* debug_names[] = {"debugfile","debugfile1", "debugfile2", "debugfile3", "debugfile4", "debugfile5"};
    static char* curr_name = nullptr;
    if(!curr_name)
        curr_name = debug_names[1];

#define remove_obj_inx(inx)                                                 \
    do {\
        if (inx < 0)\
            break;\
        if (inx >= obj_count)\
            break;\
\
        for_range(i, inx + 1, obj_count) {\
            scene_objs[i - 1] = scene_objs[i];\
            object_solid_mode[i - 1] = object_solid_mode[i];\
            velocities[i - 1] = velocities[i];\
            accelerations[i - 1] = accelerations[i];\
            masses[i - 1] = masses[i];\
        }\
        obj_count--;\
        scene_objs[obj_count] = (struct Object3D) {0};\
        object_solid_mode[obj_count] = false;\
        velocities[obj_count] = (Vec3) {0};\
        accelerations[obj_count] = (Vec3) {0};\
        masses[obj_count] = -1.f;\
        if(active_obj >= obj_count) {   \
            active_obj = obj_count-1; \
            active_changed = true;\
        }\
    }while(0)


    for_range(i, 0, SCENE_MAX_OBJ) {
        masses[i] = -1.f;
    }


    Collidable coll_objs[SCENE_MAX_OBJ + 1] = {0};
    float animation_factor = 1.f;
    bool do_animate = false;
    //Timer and timer info
    double time_samples[25] = {0};
    double freq_samples[65] = {0};
    int curr_time_sample = 0;
    int curr_freq_sample = 0;
    timespec frame_timer;
    frame_timer = start_monotonic_timer();
    double curr_frame_time = 0.0;
    double curr_frame_freq = 0.0;
    double inst_time;
    double inst_freq;

    //vkResetFences(device.device,curr_swapchain_data.img_count,curr_swapchain_data.img_fences);
    //for(int i = 0; i <= (curr_swapchain_data.img_count- min_surface_img_count); ++i){
    //    uint32_t x;
    //    VkResult res = vkAcquireNextImageKHR(device.device,curr_swapchain_data.swapchain,UINT64_MAX,nullptr,curr_swapchain_data.img_fences[i],&x);
    //    int j = 93;
    //}


    active_obj = 0;


    Vec3 world_min = {-300, -300, -1800};
    Vec3 world_max = {300, 300, 1800};
    float fov = M_PI / 3.f;

    Vec3 cam_scale = {1.f, 1.f, 1.f};
    Vec3 cam_translate = {0.f, 0.f, 0.f};
    Vec3 cam_rotate = {0.f, 0.f, 0.f};

    winproc_data.ptr_translate = NULL;
    winproc_data.ptr_rotate = NULL;
    winproc_data.ptr_scale = NULL;

    Vec3 focus_pt = {0.f, 0.f, 0.f};

    bool cam_control = false;
    bool model_control = false;
    bool light_control = false;
    bool focus_control = false;
    bool animate_camera = false;

    Vec3 light_pos = {0.f, 0, -800.f};
    Vec3 light_col = {1.f, 1.f, 1.f};
    Vec3 clear_col = {0.1f, 0.2f, 0.4f};
    winproc_data.init_success = true;


    MSG msg = {0};
    while (1) {


        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
            TranslateMessage(&msg);
            SizingCheck(&msg);
            DispatchMessage(&msg);
        }
        if (msg.message == WM_QUIT)
            break;

        // Setup miscelannous data if needed
        {
            ImGui_ImplVulkan_SetMinImageCount(min_surface_img_count);
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplWin32_NewFrame();
            igNewFrame();

            world_min.x = -winproc_data.width / 2.f;
            world_min.y = -winproc_data.height / 2.f;

            world_max.x = winproc_data.width / 2.f;
            world_max.y = winproc_data.height / 2.f;
            //gnd_obj.translate.y = world_max.y;

            if (animate_camera)
                cam_rotate.y += M_PI / 180.0;

            //Timing
            {
                inst_time = timer_sec(end_monotonic_timer(&frame_timer));
                inst_freq = 1.0 / inst_time;

                curr_frame_time = get_moving_avg(time_samples, &curr_time_sample, _countof(time_samples), inst_time);
                curr_frame_freq = get_moving_avg(freq_samples, &curr_freq_sample, _countof(freq_samples), inst_freq);

            }



            bool true_val = true;

            winproc_data.char_pressed = false;
            {
                igBegin("3D Object Info", &true_val, 0);
                igText("Total objects in scene : %d ", obj_count);
                igText("Current object index : %d ", active_obj);
                if (igButton("Create Sphere", (ImVec2) {0})) {
                    active_obj = obj_count++;
                    object_solid_mode[active_obj] = true;
                    scene_objs[active_obj] = (struct Object3D) {
                            .ptr_model = &sphere_model,
                            .color = (Vec3) {1.0f, 1.0f, 1.0f},
                            .rotate = (Vec3) {0},
                            .scale = (Vec3) {1.0f, 1.0f, 1.0f},
                            .translate = (Vec3) {0},
                    };
                    active_changed = true;
                }
                if (igButton("Create Cube", (ImVec2) {0})) {
                    active_obj = obj_count++;
                    object_solid_mode[active_obj] = true;
                    scene_objs[active_obj] = (struct Object3D) {
                            .ptr_model = &cube_model,
                            .color = (Vec3) {1.f, 1.f, 1.f},
                            .rotate = (Vec3) {0},
                            .translate = (Vec3) {0},
                            .scale = (Vec3) {1.f, 1.f, 1.f},
                    };
                    active_changed = true;
                }
                if (igButton("Create Plane", (ImVec2) {0})) {
                    active_obj = obj_count++;
                    object_solid_mode[active_obj] = true;
                    scene_objs[active_obj] = (struct Object3D) {
                            .ptr_model = &plane_model,
                            .color = (Vec3) {1.f, 1.f, 1.f},
                            .rotate = (Vec3) {0},
                            .translate = (Vec3) {0},
                            .scale = (Vec3) {1.f, 1.f, 1.f},
                    };
                    active_changed = true;
                }


                if (igButton("Next Object", (ImVec2) {0})) {
                    active_obj = (active_obj + 1) % obj_count;
                    active_changed = true;
                }
                if (igButton("Previous Object", (ImVec2) {0})) {

                    active_obj += (active_obj == 0) * obj_count;
                    active_obj = (active_obj - 1);
                    active_changed = true;
                }
                if(igButton("Remove Current Object", (ImVec2){0})) {
                    remove_obj_inx(active_obj);
                }

                igSliderFloat("Scene Animation Factor", &animation_factor, 0.f, 30.f, "%.3f", 0);
                igSliderFloat("Scene Animation Step", &animation_step, 0.001f, 3.f, "%.3f", 0);
                igCheckbox("Animate",&do_animate);

                igEnd();
            }

            {
                igBegin("Current Model", &true_val, 0);

                if (obj_count > 0) {

                    igColorEdit3("Object Color", scene_objs[active_obj].color.comps, 0);
                    igInputFloat3("Object Position", scene_objs[active_obj].translate.comps, "%.2f", 0);
                    Vec3 dummy = vec3_to_degrees(scene_objs[active_obj].rotate);
                    igInputFloat3("Object rotations Degree", dummy.comps, "%.2f", 0);
                    scene_objs[active_obj].rotate = vec3_to_radians(dummy);
                    igInputFloat3("Object scale factor", scene_objs[active_obj].scale.comps, "%.2f", 0);
                    igSliderFloat3("Object Velocity", velocities + active_obj, -150.f, 150.f, "%.3f", 0);
                    igSliderFloat3("Object Acceleration", accelerations + active_obj, -6.f, 6.f, "%.3f", 0);
                    igSliderFloat("Object Mass", masses + active_obj, -1.f, 500.f, "%.3f", 0);
                    igSelectable_BoolPtr("Draw Surface", object_solid_mode + active_obj, 0,
                                         (ImVec2) {0});
                }

                igEnd();
            }

            {
                igBegin("Scene Info", &true_val, 0);
                igText("FPS : %.2lf, Frame Time : %.3lf ms", curr_frame_freq, curr_frame_time IMILLI);
                igColorEdit3("Light Color", light_col.comps, 0);
                igInputFloat3("Light Position", light_pos.comps, "%.3f", 0);
                igColorEdit3("Sky Color", clear_col.comps, 0);
                igSliderFloat("FOV : ", &fov, M_PI / 18.f, M_PI, NULL, 0);
                //igColorEdit3("Ground Color", gnd_obj.color.comps, 0);

                if (igButton("Exit", (ImVec2) {0}))
                    PostMessage(wnd_handle, WM_CLOSE, 0, 0);

                igCheckbox("Animate Camera", &animate_camera);

                igInputFloat3("Camera Position", cam_translate.comps, "%.2f", 0);
                igInputFloat3("Camera Rotation", cam_rotate.comps, "%.2f", 0);
                igInputFloat3("Camera Zoom", cam_scale.comps, "%.2f", 0);

                if(igButton("Save to file",(ImVec2){0})) {
                    SaveAllToFile(curr_name);
                }
                if(igButton("Load from file", (ImVec2){0})) {
                    LoadAllFromFile(curr_name);
                }

                if(igBeginCombo("Debug file to save/load",curr_name,0)) {
                    for_range(i, 0, _countof(debug_names)) {
                        if (igButton(debug_names[i], (ImVec2) {0}))
                            curr_name = debug_names[i];
                    }
                    igEndCombo();
                }

                {
                    int control =
                            model_control * 0 + cam_control * 1 + light_control * 2 + focus_control * 3;

                    active_changed |= igRadioButton_IntPtr("Control Model", &control, 0);
                    active_changed |= igRadioButton_IntPtr("Control Camera", &control, 1);
                    active_changed |= igRadioButton_IntPtr("Control Light", &control, 2);
                    active_changed |= igRadioButton_IntPtr("Control Focus", &control, 3);

                    model_control = control == 0;
                    cam_control = control == 1;
                    light_control = control == 2;
                    focus_control = control == 3;
                }
                igEnd();
            }

            if (active_changed) {
                if (cam_control) {
                    winproc_data.ptr_translate = &cam_translate;
                    winproc_data.ptr_rotate = &cam_rotate;
                    winproc_data.ptr_scale = &cam_scale;
                } else {
                    winproc_data.ptr_translate = &scene_objs[active_obj].translate;
                    winproc_data.ptr_rotate = &scene_objs[active_obj].rotate;
                    winproc_data.ptr_scale = &scene_objs[active_obj].scale;
                }
            }
            active_changed = false;

            igRender();


            // Update and Render additional Platform Windows
            if (igGetIO()->ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                igUpdatePlatformWindows();
                igRenderPlatformWindowsDefault(NULL, NULL);
            }
        }
#define is_almost_zero(x)   (((x) >= -0.0001) && ((x) <= 0.0001))

        if (do_animate && (animation_factor > 0.001f)) {
            //Simulate

            residual_time += inst_time * animation_factor;
            for_range(i, 0, obj_count) {
                coll_objs[i].mass = masses[i];
                coll_objs[i].force = vec3_scale_fl(accelerations[i], masses[i]);
                coll_objs[i].vel = velocities[i];
                coll_objs[i].pos = scene_objs[i].translate;
                if(scene_objs[i].ptr_model == &plane_model){
                    coll_objs[i].shape_type = COLL_SHAPE_PLANE;
                    Mat4 rot_mat = mat4_rotation_XYZ(scene_objs[i].rotate);
                    coll_objs[i].shape.plane_normal = vec3_from_vec4( mat4_multiply_vec(&rot_mat, (Vec4){0.f,-1.f,0.f,0.f}));
                }
                else {
                    coll_objs[i].shape_type = COLL_SHAPE_SPHERE;
                    coll_objs[i].shape.sphere_radius = scene_objs[i].scale.x * 50;
                }
            }

            //For debugging
            //Find total energy, {no momentum for now as things with infinite mass complicate}
            int steps_count = 0;
            double debug_residual_time = residual_time;

            while (residual_time > animation_step) {
                steps_count++;
                resolve_collision(coll_objs, animation_step, obj_count ,nullptr);

                //Test conditions, won't break while testres == true
                bool testres = true;

		//Confine balls to inside of box made by first 6 planes
                for_range(i,6,obj_count) {
                    for_range(j,0,6){
		                testres =testres && ((  vec3_dot(coll_objs[j].shape.plane_normal,
					        vec3_sub(coll_objs[i].pos, coll_objs[j].pos))-
					        coll_objs[i].shape.sphere_radius) >= 0.f);
					    if(!testres)
		                break;
		     
                    }
                    if(!testres)
		            break;
                }
		        //Let's confine balls' position and velocity to xy plane
		        //for_range(i,6,obj_count){
		        //  bool poszero = is_almost_zero(coll_objs[i].pos.z);
		        //  bool velzero = is_almost_zero(coll_objs[i].vel.z * 10.f);
		        //  testres = testres && poszero && velzero;
                //  if(!testres)
		        //    break;
		        //}

                //Now make sure no balls are inside other balls
                for_range(i,6,obj_count) {
                    for_range(j,i+1,obj_count) {
                        float dist = vec3_magnitude(vec3_sub(coll_objs[i].pos,coll_objs[j].pos));
                        dist = dist - coll_objs[i].shape.sphere_radius - coll_objs[j].shape.sphere_radius;
                        testres = testres && (dist > 0.f);
                        if(!testres)
                            break;
                    }
                    if(!testres)
                        break;
                }

		        if(!testres){
		          //DebugBreak();
		          SaveAllToFile(curr_name);
		        }

                for_range(i, 0, obj_count) {
                    velocities[i] = coll_objs[i].vel;
                    scene_objs[i].translate = coll_objs[i].pos;
                }
                residual_time -= animation_step;
            }

        }
#undef is_almost_zero
        {

            uint32_t img_inx = 0;
            {

                int res = begin_rendering_operations((BeginRenderingOperationsParam) {
                        .device = device.device,
                        .render_pass = render_pass,
                        .framebuffer_render_extent = swapchain_extent,
                        .swapchain = curr_swapchain_data.swapchain,
                        .framebuffers = curr_swapchain_data.framebuffers,
                        .cmd_buffer = rndr_cmd_buffers[curr_frame_in_flight],
                        .render_done_fence = frame_fences[curr_frame_in_flight],
                        .present_done_semaphore = present_done_semaphores[curr_frame_in_flight],
                        .p_img_inx = &img_inx,
                        .clear_value =
                        (VkClearValue) {.color = {clear_col.r, clear_col.g, clear_col.b, 1.0f}},
                });
                if (res < 0)
                    continue;

                if ((res == BEGIN_RENDERING_OPERATIONS_TRY_RECREATE_SWAPCHAIN) ||
                    (res == BEGIN_RENDERING_OPERATIONS_MINIMIZED)) {
                    recreate_swapchain(
                            &stk_allocr, stk_offset, ptr_alloc_callbacks,
                            (RecreateSwapchainParam) {.device = device,
                                    .new_win_height = winproc_data.height,
                                    .new_win_width = winproc_data.width,
                                    .surface = surface,
                                    .framebuffer_render_pass = render_pass,
                                    .depth_img_format = depth_img_format,
                                    .p_surface_format = &surface_img_format,
                                    .p_min_img_count = &min_surface_img_count,
                                    .p_img_swap_extent = &swapchain_extent,
                                    .p_old_swapchain_data = &old_swapchain_data,
                                    .p_new_swapchain_data = &curr_swapchain_data});

                    continue;
                }


                // Here now set viewports, bind pipeline , set
                // constants, bind buffers and draw
                vkCmdSetViewport(rndr_cmd_buffers[curr_frame_in_flight], 0, 1,
                                 &(VkViewport) {.x = 0.f,
                                         .y = 0.f,
                                         .minDepth = 0.f,
                                         .maxDepth = 1.f,
                                         .width = swapchain_extent.width,
                                         .height = swapchain_extent.height});
                vkCmdSetScissor(
                        rndr_cmd_buffers[curr_frame_in_flight], 0, 1,
                        &(VkRect2D) {.offset = {.x = 0, .y = 0}, .extent = swapchain_extent});


                vkCmdBindPipeline(rndr_cmd_buffers[curr_frame_in_flight],
                                  VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
            }

            {
                struct DescriptorMats des_mats;
                struct DescriptorLight des_lights;
                des_mats.view = (Mat4) MAT4_IDENTITIY;
                des_mats.proj = (Mat4) MAT4_IDENTITIY;

                des_mats.proj = mat4_orthographic(world_min, world_max);

                des_mats.proj = mat4_perspective(world_min, world_max, fov);


                Mat4 translate =
                        mat4_translate_3(-cam_translate.x, -cam_translate.y, -cam_translate.z);
                Mat4 rotate = mat4_rotation_ZYX(vec3_neg(cam_rotate));
                Mat4 scale = mat4_scale_3(1.f / cam_scale.x, 1.f / cam_scale.y, 1.f / cam_scale.z);
                des_mats.view = mat4_multiply_mat_3(&scale, &rotate, &translate);


                des_lights.light_col = vec4_from_vec3(light_col, 1.0f);
                des_lights.light_src = vec4_from_vec3(light_pos, 1.f);
                des_lights.view_pos = vec4_from_vec3(cam_translate, 1.0f);

                *(struct DescriptorMats *) (g_matrix_uniform_buffers[curr_frame_in_flight]
                        .mapped_memory) = des_mats;

                *(struct DescriptorLight *) (g_lights_uniform_buffers[curr_frame_in_flight]
                        .mapped_memory) = des_lights;

                vkCmdBindDescriptorSets(rndr_cmd_buffers[curr_frame_in_flight],
                                        VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_layout,
                                        0, 1, g_matrix_descriptor_sets + curr_frame_in_flight, 0,
                                        NULL);

                vkCmdBindDescriptorSets(rndr_cmd_buffers[curr_frame_in_flight],
                                        VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_layout,
                                        1, 1, g_lights_descriptor_sets + curr_frame_in_flight, 0,
                                        NULL);
            }

            bool is_solid_pipeline = true;

            PushConst pushes;

            //pushes = object_process_push_const(gnd_obj);
//
//
            //for (int i = 0; i < push_range_count; ++i) {
            //    vkCmdPushConstants(rndr_cmd_buffers[curr_frame_in_flight], graphics_pipeline_layout,
            //                       push_ranges[i].stageFlags, push_ranges[i].offset,
            //                       push_ranges[i].size, (uint8_t *) &pushes + push_ranges[i].offset);
            //}
            //submit_model_draw(gnd_obj.ptr_model, rndr_cmd_buffers[curr_frame_in_flight]);

            for (int i = 0; i < obj_count; ++i) {
                pushes = object_process_push_const(scene_objs[i]);

                for (int i = 0; i < push_range_count; ++i) {
                    vkCmdPushConstants(rndr_cmd_buffers[curr_frame_in_flight],
                                       graphics_pipeline_layout, push_ranges[i].stageFlags,
                                       push_ranges[i].offset, push_ranges[i].size,
                                       (uint8_t *) &pushes + push_ranges[i].offset);
                }

                if (is_solid_pipeline != object_solid_mode[i]) {
                    is_solid_pipeline = object_solid_mode[i];
                    vkCmdBindPipeline(
                            rndr_cmd_buffers[curr_frame_in_flight], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            (is_solid_pipeline) ? (graphics_pipeline) : (graphics_mesh_pipeline));
                }

                submit_model_draw(scene_objs[i].ptr_model, rndr_cmd_buffers[curr_frame_in_flight]);
            }


            {


                ImGui_ImplVulkan_RenderDrawData(
                        igGetDrawData(), rndr_cmd_buffers[curr_frame_in_flight], VK_NULL_HANDLE);


                VkResult res = end_rendering_operations((EndRenderingOperationsParam) {
                        .device = device.device,
                        .cmd_buffer = rndr_cmd_buffers[curr_frame_in_flight],
                        .graphics_queue = device.graphics_queue,
                        .present_queue = device.present_queue,
                        .swapchain = curr_swapchain_data.swapchain,
                        .img_index = img_inx,
                        .render_done_fence = frame_fences[curr_frame_in_flight],
                        .render_done_semaphore = render_finished_semaphores[curr_frame_in_flight],
                        .present_done_semaphore = present_done_semaphores[curr_frame_in_flight]});

                if (res == END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN) {
                    recreate_swapchain(
                            &stk_allocr, stk_offset, ptr_alloc_callbacks,
                            (RecreateSwapchainParam) {.device = device,
                                    .new_win_height = winproc_data.height,
                                    .new_win_width = winproc_data.width,
                                    .surface = surface,
                                    .framebuffer_render_pass = render_pass,
                                    .depth_img_format = depth_img_format,
                                    .p_surface_format = &surface_img_format,
                                    .p_min_img_count = &min_surface_img_count,
                                    .p_img_swap_extent = &swapchain_extent,
                                    .p_old_swapchain_data = &old_swapchain_data,
                                    .p_new_swapchain_data = &curr_swapchain_data});

                    continue;
                }


                // Decrease old swapchain life one by one
                // TODO:: later maybe implement a feature to add swapchain life
                // in begin rendering
                //if (old_swapchain_data.swapchain_life && (old_swapchain_data.swapchain_life != -1))
                //    old_swapchain_data.swapchain_life &= ~(1 << img_inx);

                // To prevent cases that may occur if swapchain images are very
                // less
                curr_frame_in_flight =
                        (curr_frame_in_flight + 1) % _min(max_frames_in_flight, min_surface_img_count);
            }
        }
    }
    LSMCleanup();

    failure = MAIN_FAIL_OK;

    vkDeviceWaitIdle(device.device);

    cleanup_phase:
    switch (failure) {
        // Those who take in err_code, will be below their fail
        // case label Else they will be above thier fail case
        // label

        case MAIN_FAIL_OK:

            err_code = 0;
        case MAIN_FAIL_MODEL_LOAD:
            clear_model(ptr_alloc_callbacks, device.device, &plane_model);
            clear_model(ptr_alloc_callbacks, device.device, &sphere_model);
            clear_model(ptr_alloc_callbacks, device.device, &cube_model);

            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplWin32_Shutdown();
            igDestroyContext(NULL);
            err_code = 0;
        case MAIN_FAIL_IMGUI_INIT:


            clear_graphics_pipeline(ptr_alloc_callbacks,
                                    (ClearGraphicsPipelineParam) {
                                            .device = device.device, .p_pipeline = &graphics_mesh_pipeline},
                                    err_code);
            err_code = 0;
        case MAIN_FAIL_GRAPHICS_MESH_PIPELINE:

            clear_graphics_pipeline(
                    ptr_alloc_callbacks,
                    (ClearGraphicsPipelineParam) {.device = device.device, .p_pipeline = &graphics_pipeline},
                    err_code);
            err_code = 0;
        case MAIN_FAIL_GRAPHICS_PIPELINE:

            vkDestroyPipelineLayout(device.device, graphics_pipeline_layout, ptr_alloc_callbacks);
            err_code = 0;
        case MAIN_FAIL_GRAPHICS_PIPELINE_LAYOUT:

            err_code = 0;
        case MAIN_FAIL_DESCRIPTOR_ALLOC_AND_BIND:


            err_code = 0;
        case MAIN_FAIL_DESCRIPTOR_MEM_ALLOC:
            g_matrix_descriptor_sets = g_lights_descriptor_sets = NULL;

            err_code = 0;
        case MAIN_FAIL_DESCRIPTOR_BUFFER:
            for (int i = 0; i < max_frames_in_flight; ++i) {
                if (g_matrix_uniform_buffers[i].buffer)
                    vkDestroyBuffer(device.device, g_matrix_uniform_buffers[i].buffer,
                                    ptr_alloc_callbacks);
                if (g_lights_uniform_buffers[i].buffer)
                    vkDestroyBuffer(device.device, g_lights_uniform_buffers[i].buffer,
                                    ptr_alloc_callbacks);
            }


            err_code = 0;
        case MAIN_FAIL_DESCRIPTOR_BUFFER_MEM_ALLOC:
            g_matrix_uniform_buffers = g_lights_uniform_buffers = NULL;

            err_code = 0;
        case MAIN_FAIL_DESCRIPTOR_POOL:
            clear_descriptor_pool(ptr_alloc_callbacks,
                                  (ClearDescriptorPoolParam) {
                                          .device = device.device,
                                          .p_descriptor_pool = &g_descriptor_pool,
                                  },
                                  err_code);

            err_code = 0;
        case MAIN_FAIL_DESCRIPTOR_LAYOUT:
            clear_descriptor_layouts(ptr_alloc_callbacks,
                                     (ClearDescriptorLayoutsParam) {
                                             .device = device.device,
                                             .p_lights_set_layout = &g_lights_layout,
                                             .p_matrix_set_layout = &g_matrix_layout,
                                     },
                                     err_code);

        case MAIN_FAIL_WINDOW:
            clear_vk_window(ptr_alloc_callbacks, h_instance, vk_instance, wndclass_name, &wnd_handle,
                            &surface, device, &curr_swapchain_data, &old_swapchain_data, err_code);


        case MAIN_FAIL_RENDER_SYSTEM:
            clear_render_system(ptr_alloc_callbacks, device, max_frames_in_flight, &render_pass,
                                &frame_fences, &all_semaphores, &render_finished_semaphores,
                                &present_done_semaphores, &rndr_cmd_pool, &rndr_cmd_buffers, err_code);

        case MAIN_FAIL_GLOBAL_CONTEXT:
            clear_global_context(&vk_instance, &device, ptr_alloc_callbacks, &stk_allocr,
                                 &gpu_mem_allocr, init_global_err_code);


        case MAIN_FAIL_ALL:

            break;
    }

    return failure;

#pragma warning(pop)
#undef return_main_fail
}
