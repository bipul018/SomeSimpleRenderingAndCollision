#include "common-stuff.h"

#include <stdbool.h>
#include <string.h>
#include <windows.h>

int test_vulkan_layer_presence(struct VulkanLayer* to_test,
                               const VkLayerProperties* avail_layers,
                               int avail_count) {
    to_test->available = 0;
    for (int i = 0; i < avail_count; ++i) {
        if (strcmp(to_test->layer_name, avail_layers[i].layerName) ==
            0) {
            to_test->available = 1;
            break;
        }
    }
    return (to_test->available) || !(to_test->required);
}

int test_vulkan_extension_presence(
    struct VulkanExtension* to_test,
    const VkExtensionProperties* avail_extensions, int avail_count) {
    to_test->available = 0;
    for (int i = 0; i < avail_count; ++i) {
        if (strcmp(to_test->extension_name,
                   avail_extensions[i].extensionName) == 0) {
            to_test->available = 1;
            break;
        }
    }
    return (to_test->available) || !(to_test->required);
}

StackAllocator alloc_stack_allocator(size_t reserve, size_t commit) {
    StackAllocator allocr = { 0 };
    allocr.page_size = SIZE_KB(4);

    SYSTEM_INFO sys_info = { 0 };
    GetSystemInfo(&sys_info);
    allocr.page_size =
        align_up_(allocr.page_size, sys_info.dwPageSize);

    reserve = align_up_(reserve, allocr.page_size);
    commit = align_up_(commit, allocr.page_size);
    allocr.base_memory =
        VirtualAlloc(nullptr, reserve, MEM_RESERVE, PAGE_READWRITE);
    if (!allocr.base_memory)
        return allocr;
    allocr.reserved_memory = reserve;
    if (NULL ==
        VirtualAlloc(allocr.base_memory, commit, MEM_COMMIT,
                     PAGE_READWRITE))
        return allocr;
    allocr.committed_memory = commit;
    return allocr;
}

void dealloc_stack_allocator(StackAllocator* allocr) {
    if (allocr)
        VirtualFree(allocr->base_memory, 0, MEM_RELEASE);
    allocr->base_memory = NULL;
    allocr->committed_memory = allocr->reserved_memory = allocr->page_size = 0;
}

// alignment should be a power of 2
void* stack_allocate(StackAllocator* data, size_t* curr_offset,
                     size_t size, size_t alignment) {

    if (!data || !size || !curr_offset)
        return NULL;

    uintptr_t next_mem =
        ((uintptr_t)data->base_memory) + *curr_offset;

    next_mem = align_up_(next_mem, alignment);

    size_t next_offset = next_mem - (uintptr_t)data->base_memory +
        align_up_(size, alignment);

    if (next_offset > data->reserved_memory)
        return NULL;

    if (next_offset > data->committed_memory) {
        VirtualAlloc(data->committed_memory + data->base_memory,
                     align_up_(next_offset - data->committed_memory,
                               data->page_size),
                     MEM_COMMIT, PAGE_READWRITE);
        data->committed_memory += align_up_(
            next_offset - data->committed_memory, data->page_size);
    }

    *curr_offset = next_offset;
    return (void*)next_mem;
}

enum CreateInstanceCodes {
    CREATE_INSTANCE_FAILED = -0x7fff,
    CREATE_INSTANCE_EXTENSION_NOT_FOUND,
    CREATE_INSTANCE_EXTENSION_CHECK_ALLOC_ERROR,
    CREATE_INSTANCE_LAYER_NOT_FOUND,
    CREATE_INSTANCE_LAYER_CHECK_ALLOC_ERROR,
    CREATE_INSTANCE_OK = 0
};

int create_instance(StackAllocator* stk_allocr,
                    size_t stk_allocr_offset,
                    const VkAllocationCallbacks* alloc_callbacks,
                    VkInstance* p_vk_instance,
                    struct VulkanLayer* instance_layers,
                    size_t layers_count,
                    struct VulkanExtension* instance_extensions,
                    size_t extensions_count) {
    VkResult result = VK_SUCCESS;

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Application",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Null Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    // Setup instance layers , + check for availability

    uint32_t avail_layer_count;
    vkEnumerateInstanceLayerProperties(&avail_layer_count, NULL);

    // VkLayerProperties* available_layers = malloc(avail_layer_count
    // * sizeof * available_layers);
    VkLayerProperties* available_layers =
        stack_allocate(stk_allocr, &stk_allocr_offset,
                       avail_layer_count * sizeof *available_layers,
                       8);

    if (!available_layers)
        return CREATE_INSTANCE_LAYER_CHECK_ALLOC_ERROR;

    vkEnumerateInstanceLayerProperties(&avail_layer_count,
                                       available_layers);
    int found = 1;
    for (int i = 0; i < layers_count; ++i) {
        found = found &&
            test_vulkan_layer_presence(instance_layers + i,
                                       available_layers,
                                       avail_layer_count);
        if (!instance_layers[i].available) {
            struct VulkanLayer layer = instance_layers[i];
            instance_layers[i--] = instance_layers[layers_count - 1];
            instance_layers[--layers_count] = layer;
        }
    }

    if (!found)
        return CREATE_INSTANCE_LAYER_NOT_FOUND;

    const char** layers_list = available_layers;
    for (int i = 0; i < layers_count; ++i)
        layers_list[i] = instance_layers[i].layer_name;

    // Setup instance extensions {opt, check for availability}

    uint32_t avail_extension_count;
    vkEnumerateInstanceExtensionProperties(
        NULL, &avail_extension_count, NULL);

    // VkExtensionProperties* available_extensions =
    // malloc(avail_extension_count
    // * sizeof * available_extensions);
    VkExtensionProperties* available_extensions = stack_allocate(
        stk_allocr, &stk_allocr_offset,
        (avail_extension_count * sizeof *available_extensions), 8);
    if (!available_extensions)
        return CREATE_INSTANCE_EXTENSION_CHECK_ALLOC_ERROR;

    vkEnumerateInstanceExtensionProperties(
        NULL, &avail_extension_count, available_extensions);
    found = 1;
    for (int i = 0; i < extensions_count; ++i) {
        found = found &&
            test_vulkan_extension_presence(instance_extensions + i,
                                           available_extensions,
                                           avail_extension_count);
        if (!instance_extensions[i].available) {
            struct VulkanExtension extension = instance_extensions[i];
            instance_extensions[i--] =
                instance_extensions[extensions_count - 1];
            instance_extensions[--extensions_count] = extension;
        }
    }
    // free(available_extensions);

    if (!found)
        return CREATE_INSTANCE_EXTENSION_NOT_FOUND;

    const char** extension_list = available_extensions;
    for (int i = 0; i < extensions_count; ++i)
        extension_list[i] = instance_extensions[i].extension_name;

    // VkInstanceCreateInfo create_info = {
    //	.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    //	.pApplicationInfo = &app_info,
    // };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extensions_count,
        .ppEnabledExtensionNames = extension_list,
        .enabledLayerCount = layers_count,
        .ppEnabledLayerNames = layers_list,
    };

    result =
        vkCreateInstance(&create_info, alloc_callbacks,
                         p_vk_instance);

    if (result != VK_SUCCESS)
        return CREATE_INSTANCE_FAILED;

    return CREATE_INSTANCE_OK;
}

void clear_instance(const VkAllocationCallbacks* alloc_callbacks,
                    VkInstance* p_vk_instance, int err_codes) {
    switch (err_codes) {
    case CREATE_INSTANCE_OK:
        vkDestroyInstance(*p_vk_instance, alloc_callbacks);
    case CREATE_INSTANCE_LAYER_CHECK_ALLOC_ERROR:
    case CREATE_INSTANCE_LAYER_NOT_FOUND:
    case CREATE_INSTANCE_EXTENSION_CHECK_ALLOC_ERROR:
    case CREATE_INSTANCE_EXTENSION_NOT_FOUND:
    case CREATE_INSTANCE_FAILED:
        *p_vk_instance = VK_NULL_HANDLE;
    }
}

enum ChooseSwapchainDetailsCodes {
    CHOOSE_SWAPCHAIN_DETAILS_INT_ALLOC_ERR = -0x7fff,
    CHOOSE_SWAPCHAIN_DETAILS_NO_SURFACE_FORMAT,
    CHOOSE_SWAPCHAIN_DETAILS_NO_PRESENT_MODES,
    CHOOSE_SWAPCHAIN_DETAILS_CAPABILITY_GET_FAIL,
    CHOOSE_SWAPCHAIN_DETAILS_OK = 0
};

int choose_swapchain_details(StackAllocator* stk_allocr,
                             size_t stk_offset,
                             ChooseSwapchainDetsParam param) {

    // Query for the swap_support of swapchain and window surface
    VkSurfaceCapabilitiesKHR capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          param.phy_device, param.surface, &capabilities) !=
        VK_SUCCESS)
        return CHOOSE_SWAPCHAIN_DETAILS_CAPABILITY_GET_FAIL;

    uint32_t formats_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        param.phy_device, param.surface, &formats_count, NULL);
    if (!formats_count)
        return CHOOSE_SWAPCHAIN_DETAILS_NO_SURFACE_FORMAT;

    
    uint32_t present_modes_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      param.phy_device, param.surface, &present_modes_count, NULL);

    if (!present_modes_count)
        return CHOOSE_SWAPCHAIN_DETAILS_NO_PRESENT_MODES;

    VkSurfaceFormatKHR* surface_formats = NULL;
    if (param.p_img_format) {
        param.p_img_format->format = VK_FORMAT_UNDEFINED;
        surface_formats = stack_allocate(
          stk_allocr, &stk_offset,
          formats_count * sizeof(VkSurfaceFormatKHR), 1);
        if (!surface_formats)
            return CHOOSE_SWAPCHAIN_DETAILS_INT_ALLOC_ERR;
    }


    VkPresentModeKHR* present_modes = NULL;
    if (param.p_present_mode) {
        present_modes = stack_allocate(
          stk_allocr, &stk_offset,
          present_modes_count * sizeof(VkPresentModeKHR), 1);
        if (!present_modes)
            return CHOOSE_SWAPCHAIN_DETAILS_INT_ALLOC_ERR;
    }

    if (param.p_img_format) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(
         param.phy_device, param.surface, &formats_count,
         surface_formats);
        // Choose the settings
        // Choose best if availabe else the first format
        *(param.p_img_format) = surface_formats[0];
        for (uint32_t i = 0; i < formats_count; ++i) {
            if (surface_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
                surface_formats[i].colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                *(param.p_img_format) = surface_formats[i];
                break;
                }
        }
    }

    
    if (param.p_present_mode) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(
         param.phy_device, param.surface, &present_modes_count,
         present_modes);


        // Choose present mode
        *(param.p_present_mode) = VK_PRESENT_MODE_FIFO_KHR;
        for (uint32_t i = 0; i < present_modes_count; ++i) {
            //if(present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
            if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                *(param.p_present_mode) = present_modes[i];
        }
    }

    if (param.p_min_img_count) {
        // Choose a img count
        *(param.p_min_img_count) = capabilities.minImageCount + 3;
        if (capabilities.maxImageCount)
            *(param.p_min_img_count) =
              min(*(param.p_min_img_count), capabilities.maxImageCount);
    }

    if (param.p_transform_flags)
        *(param.p_transform_flags) = capabilities.currentTransform;

    if (param.p_surface_extent)
        *(param.p_surface_extent) = capabilities.currentExtent;
    

    return CHOOSE_SWAPCHAIN_DETAILS_OK;
}

enum CreateDeviceCodes {
    CREATE_DEVICE_FAILED = -0x7fff,
    CREATE_DEVICE_NO_GPU,
    CREATE_DEVICE_PHY_DEVICE_TEST_ALLOC_FAIL,
    CREATE_DEVICE_NO_SUITABLE_GPU,
    CREATE_DEVICE_OK = 0,
};
int create_device(StackAllocator* stk_allocr,
                  size_t stk_allocr_offset,
                  VkAllocationCallbacks* alloc_callbacks,
                  CreateDeviceParam param,
                  struct VulkanLayer* device_layers,
                  size_t layers_count,
                  struct VulkanExtension* device_extensions,
                  size_t extensions_count) {

    VkResult result = VK_SUCCESS;

    param.p_vk_device->phy_device = VK_NULL_HANDLE;

    uint32_t phy_device_count = 0;
    vkEnumeratePhysicalDevices(param.vk_instance, &phy_device_count,
                               NULL);

    if (phy_device_count == 0)
        return CREATE_DEVICE_NO_GPU;

    VkPhysicalDevice* phy_devices =
        stack_allocate(stk_allocr, &stk_allocr_offset,
                       (phy_device_count * sizeof *phy_devices), 1);

    if (!phy_devices)
        return CREATE_DEVICE_PHY_DEVICE_TEST_ALLOC_FAIL;

    vkEnumeratePhysicalDevices(param.vk_instance, &phy_device_count,
                               phy_devices);

    // Inside, will test and fill all required queue families

    uint32_t graphics_family;
    uint32_t present_family;
    for (int i = 0; i < phy_device_count; ++i) {

        size_t local_stk = stk_allocr_offset;

        // test if suitable

        // Check if device extensions are supported
        uint32_t avail_extension_count;
        vkEnumerateDeviceExtensionProperties(
            phy_devices[i], NULL, &avail_extension_count, NULL);

        VkExtensionProperties* available_extensions = stack_allocate(
            stk_allocr, &local_stk,
            (avail_extension_count * sizeof *available_extensions),
            1);

        if (!available_extensions)
            return CREATE_DEVICE_PHY_DEVICE_TEST_ALLOC_FAIL;

        vkEnumerateDeviceExtensionProperties(phy_devices[i], NULL,
                                             &avail_extension_count,
                                             available_extensions);

        int found = 1;
        for (int i = 0; i < extensions_count; ++i) {
            found = found &&
                test_vulkan_extension_presence(device_extensions + i,
                    available_extensions,
                    avail_extension_count);
        }
        if (!found)
            continue;

        // Check if device layers are supported
        uint32_t avail_layers_count;
        vkEnumerateDeviceLayerProperties(phy_devices[i],
                                         &avail_layers_count, NULL);

        VkLayerProperties* available_layers = stack_allocate(
            stk_allocr, &local_stk,
            (avail_layers_count * sizeof *available_layers), 1);

        vkEnumerateDeviceLayerProperties(
            phy_devices[i], &avail_layers_count, available_layers);

        found = 1;
        for (int i = 0; i < layers_count; ++i) {
            found = found &&
                test_vulkan_layer_presence(device_layers + i,
                                           available_layers,
                                           avail_layers_count);
        }
        if (!found)
            continue;

        // Check device properties and features
        VkPhysicalDeviceProperties device_props;
        vkGetPhysicalDeviceProperties(phy_devices[i], &device_props);

        VkPhysicalDeviceFeatures device_feats;
        vkGetPhysicalDeviceFeatures(phy_devices[i], &device_feats);

        // Here test for non queue requirements
        // if (!device_feats.geometryShader)
        //	continue;

        // Check for availability of required queue family / rate here

        uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_devices[i],
            &family_count, NULL);
        VkQueueFamilyProperties* families =
            stack_allocate(stk_allocr, &local_stk,
                           (family_count * sizeof *families), 1);

        vkGetPhysicalDeviceQueueFamilyProperties(
            phy_devices[i], &family_count, families);
        // One by one check for suitability
        bool graphics_avail = 0;
        bool present_avail = 0;

        for (int jj = 0; jj < family_count; ++jj) {
            int j = family_count - jj - 1;
            j = jj;

            VkBool32 present_capable;
            vkGetPhysicalDeviceSurfaceSupportKHR(phy_devices[i], j,
                param.chosen_surface,
                &present_capable);
            if ((families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                !graphics_avail)) {
                graphics_family = j;
                graphics_avail = 1;
            }

            if (present_capable && !present_avail) {
                present_family = j;
                present_avail = 1;
            }

            // break if all is available
            if (graphics_avail && present_avail)
                break;
        }

        // Check if all is filled
        if (!graphics_avail || !present_avail)
            continue;

        // Choose swapchain details
        ChooseSwapchainDetsParam choose_swap = {
            .surface = param.chosen_surface,
            .phy_device = phy_devices[i],
            .p_img_format = param.p_img_format,
            .p_min_img_count = param.p_min_img_count,
        };

        if (choose_swapchain_details(stk_allocr, local_stk,
                                     choose_swap) <
            CHOOSE_SWAPCHAIN_DETAILS_OK)
            continue;

        if ((*param.p_depth_stencil_format = choose_image_format(
               phy_devices[i],
               (VkFormat[]){ VK_FORMAT_D32_SFLOAT,
                             VK_FORMAT_D32_SFLOAT_S8_UINT,
                             VK_FORMAT_D24_UNORM_S8_UINT },
               3, VK_IMAGE_TILING_OPTIMAL,
               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) ==
            VK_FORMAT_UNDEFINED)
            continue;

        param.p_vk_device->phy_device = phy_devices[i];
        break;
    }

    // free(phy_devices);

    if (param.p_vk_device->phy_device == VK_NULL_HANDLE)
        return CREATE_DEVICE_NO_SUITABLE_GPU;

    // Rearrange the layers and extensions
    for (int i = 0; i < layers_count; ++i) {
        if (!device_layers[i].available) {
            struct VulkanLayer layer = device_layers[i];
            device_layers[i--] = device_layers[layers_count - 1];
            device_layers[--layers_count] = layer;
        }
    }

    // This allocation should always succed, as in loop at least one
    // itr alloc > this
    const char** layers_list =
        stack_allocate(stk_allocr, &stk_allocr_offset,
                       layers_count * sizeof(const char*), 1);
    for (int i = 0; i < layers_count; ++i)
        layers_list[i] = device_layers[i].layer_name;

    for (int i = 0; i < extensions_count; ++i) {
        if (!device_extensions[i].available) {
            struct VulkanExtension extension = device_extensions[i];
            device_extensions[i--] =
                device_extensions[extensions_count - 1];
            device_extensions[--extensions_count] = extension;
        }
    }
    const char** extensions_list =
        stack_allocate(stk_allocr, &stk_allocr_offset,
                       extensions_count * sizeof(const char*), 1);
    for (int i = 0; i < extensions_count; ++i)
        extensions_list[i] = device_extensions[i].extension_name;

    float queue_priorities = 1.f;
    VkDeviceQueueCreateInfo queue_create_infos[2];
    uint32_t queue_create_count = 1;
    queue_create_infos[0] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .queueFamilyIndex = graphics_family,
        .pQueuePriorities = &queue_priorities
    };
    // Make a nested if for this case, it's handleable anyways
    if (present_family != graphics_family) {
        queue_create_count++;
        queue_create_infos[1] = queue_create_infos[0];
        queue_create_infos[1].queueFamilyIndex = present_family;
    }

    VkPhysicalDeviceFeatures device_features = {
        .fillModeNonSolid = VK_TRUE,
    };

    // When required, create the device features, layers check and
    // enable code here

    VkDeviceCreateInfo dev_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_infos,
        .queueCreateInfoCount = queue_create_count,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = extensions_count,
        .ppEnabledExtensionNames = extensions_list,
        .enabledLayerCount = layers_count,
        .ppEnabledLayerNames = layers_list,
    };

    result =
        vkCreateDevice(param.p_vk_device->phy_device,
                       &dev_create_info,
                       alloc_callbacks, &param.p_vk_device->device);

    if (result != VK_SUCCESS)
        return CREATE_DEVICE_FAILED;

    vkGetDeviceQueue(param.p_vk_device->device, graphics_family, 0,
                     &param.p_vk_device->graphics_queue);
    vkGetDeviceQueue(param.p_vk_device->device, present_family, 0,
                     &param.p_vk_device->present_queue);

    param.p_vk_device->graphics_family_inx = graphics_family;
    param.p_vk_device->present_family_inx = present_family;

    return 0;
}

void clear_device(const VkAllocationCallbacks* alloc_callbacks,
                  ClearDeviceParam param, int err_codes) {
    switch (err_codes) {
    case CREATE_DEVICE_OK:
        vkDestroyDevice(*(param.p_device), alloc_callbacks);
    case CREATE_DEVICE_NO_SUITABLE_GPU:
    case CREATE_DEVICE_PHY_DEVICE_TEST_ALLOC_FAIL:
    case CREATE_DEVICE_NO_GPU:
    case CREATE_DEVICE_FAILED:
        *(param.p_phy_device) = *(param.p_device) = nullptr;
    }
}

enum CreateCommandPoolCodes {
    CREATE_COMMAND_POOL_FAILED = -0x7fff,
    CREATE_COMMAND_POOL_OK = 0
};

int create_command_pool(StackAllocator* stk_allocr, size_t stk_offset,
                        const VkAllocationCallbacks* alloc_callbacks,
                        CreateCommandPoolParam param) {
    VkResult result = VK_SUCCESS;

    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = param.queue_family_inx,
    };
    result = vkCreateCommandPool(param.device, &create_info,
                                 alloc_callbacks, param.p_cmd_pool);

    if (result != VK_SUCCESS)
        return CREATE_COMMAND_POOL_FAILED;

    return CREATE_COMMAND_POOL_OK;
}

void clear_command_pool(const VkAllocationCallbacks* alloc_callbacks,
                        ClearCommandPoolParam param, int err_code) {

    switch (err_code) {
    case CREATE_COMMAND_POOL_OK:
        vkDestroyCommandPool(param.device, *(param.p_cmd_pool),
                             alloc_callbacks);
    case CREATE_COMMAND_POOL_FAILED:
        *(param.p_cmd_pool) = VK_NULL_HANDLE;
    }
}

VkFormat choose_image_format(VkPhysicalDevice phy_device,
                             VkFormat* format_candidates,
                             size_t count, VkImageTiling img_tiling,
                             VkFormatFeatureFlags format_flags) {
    for (size_t i = 0; i < count; ++i) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(
          phy_device, format_candidates[i], &properties);
        if (img_tiling == VK_IMAGE_TILING_LINEAR &&
            ((properties.linearTilingFeatures & format_flags) ==
              format_flags))
            return format_candidates[i];
        if (img_tiling == VK_IMAGE_TILING_OPTIMAL &&
            ((properties.optimalTilingFeatures & format_flags) ==
              format_flags))
            return format_candidates[i];
    }
    return VK_FORMAT_UNDEFINED;
}
