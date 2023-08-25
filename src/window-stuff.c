#include "window-stuff.h"
#include <stdbool.h>

int create_swapchain(StackAllocator* stk_allocr, size_t stk_offset,
                     VkAllocationCallbacks* alloc_callbacks,
                     CreateSwapchainParam param) {

    VkResult result = VK_SUCCESS;
    param.p_curr_swapchain_data->swapchain_life = 0;

    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    uint32_t min_img_count;
    VkSurfaceTransformFlagBitsKHR transform_flags;
    VkExtent2D curr_extent;

    if (choose_swapchain_details(
          stk_allocr, stk_offset,
          (ChooseSwapchainDetsParam){
            .surface = param.surface,
            .phy_device = param.device.phy_device,
            .p_transform_flags = &transform_flags,
            .p_img_format = &surface_format,
            .p_min_img_count = &min_img_count,
            .p_present_mode = &present_mode,
            .p_surface_extent = &curr_extent }) < 0)
        return CREATE_SWAPCHAIN_CHOOSE_DETAILS_FAIL;
            
    // Choose swap extent
    //  ++++
    //   TODO:: make dpi aware, ..., maybe not needed
    //  ++++
    // Now just use set width and height, also currently not checked
    // anything from capabilities Also be aware of max and min extent
    // set to numeric max of uint32_t
    if (curr_extent.width != -1 &&
        curr_extent.height != -1) {

        *param.p_img_swap_extent = curr_extent;

    } else {
        param.p_img_swap_extent->width = param.win_width;
        param.p_img_swap_extent->height = param.win_height;
    }

    if (!param.p_img_swap_extent->width ||
        !param.p_img_swap_extent->height) {
        return CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE;
    }

    // An array of queue family indices used
    uint32_t indices_array[] = {
        param.device.graphics_family_inx,
        param.device.present_family_inx
    };

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = param.surface,
        .minImageCount = min_img_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = *param.p_img_swap_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = transform_flags,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE, // This should be false only if pixels
        // have to re read
        .oldSwapchain = param.old_swapchain,
        .imageSharingMode = VK_SHARING_MODE_CONCURRENT,
        // Here , for exclusive sharing mode it is  optional; else for
        // concurrent, there has to be at least two different queue
        // families, and all should be specified to share the images
        // amoong
        .queueFamilyIndexCount = COUNT_OF(indices_array),
        .pQueueFamilyIndices = indices_array
    };

    if (indices_array[0] == indices_array[1]) {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    result = vkCreateSwapchainKHR(
        param.device.device, &create_info, alloc_callbacks,
        &(param.p_curr_swapchain_data->swapchain));

    if (result != VK_SUCCESS)
        return CREATE_SWAPCHAIN_FAILED;

    result = vkGetSwapchainImagesKHR(
        param.device.device, param.p_curr_swapchain_data->swapchain,
        &(param.p_curr_swapchain_data->img_count), NULL);

    if ((result != VK_SUCCESS) ||
        (param.p_curr_swapchain_data->img_count == 0))
        return CREATE_SWAPCHAIN_IMAGE_LOAD_FAIL;

    param.p_curr_swapchain_data->images =
        malloc(
            param.p_curr_swapchain_data->img_count * sizeof(VkImage));

    if (!param.p_curr_swapchain_data->images)
        return CREATE_SWAPCHAIN_IMAGE_ALLOC_FAIL;
    vkGetSwapchainImagesKHR(param.device.device,
                            param.p_curr_swapchain_data->swapchain,
                            &(param.p_curr_swapchain_data->img_count),
                            param.p_curr_swapchain_data->images);


    param.p_curr_swapchain_data->img_views = malloc(
        param.p_curr_swapchain_data->img_count * sizeof(VkImageView));
    if (!param.p_curr_swapchain_data->img_views)
        return CREATE_SWAPCHAIN_IMAGE_VIEW_ALLOC_FAIL;
    memset(param.p_curr_swapchain_data->img_views, 0,
           param.p_curr_swapchain_data->img_count *
           sizeof(VkImageView));
    for (size_t i = 0; i < param.p_curr_swapchain_data->img_count;
         ++i) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = param.p_curr_swapchain_data->images[i],

            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface_format.format,

            .components =
            (VkComponentMapping){
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY },

            .subresourceRange =
            (VkImageSubresourceRange){ .aspectMask =
                                       VK_IMAGE_ASPECT_COLOR_BIT,
                                       .baseMipLevel = 0,
                                       .levelCount = 1,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1 }

        };

        result = vkCreateImageView(
            param.device.device, &create_info, alloc_callbacks,
            param.p_curr_swapchain_data->img_views + i);
        if (result != VK_SUCCESS)
            break;
    }

    if (result != VK_SUCCESS)
        return CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL;

    param.p_curr_swapchain_data->depth_imgs =
        malloc(
            param.p_curr_swapchain_data->img_count * sizeof(VkImage));
    if (!param.p_curr_swapchain_data->depth_imgs)
        return CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL;

    bool format_changed =
      (param.p_surface_format->format != surface_format.format) ||
      (param.p_surface_format->colorSpace !=
       surface_format.colorSpace);
    bool min_count_changed =
      *(param.p_min_img_count) != min_img_count;

    *(param.p_surface_format) = surface_format;
    *(param.p_min_img_count) = min_img_count;
    param.p_curr_swapchain_data->swapchain_life =
      (1 << param.p_curr_swapchain_data->img_count) - 1;

    if (format_changed)
        return CREATE_SWAPCHAIN_SURFACE_FORMAT_CHANGED;
    if (min_count_changed)
        return CREATE_SWAPCHAIN_MIN_IMG_COUNT_CHANGED;
    return CREATE_SWAPCHAIN_OK;
}

void clear_swapchain(const VkAllocationCallbacks* alloc_callbacks,
                     ClearSwapchainParam param, int err_codes) {

    switch (err_codes) {

    case CREATE_SWAPCHAIN_MIN_IMG_COUNT_CHANGED:
    case CREATE_SWAPCHAIN_SURFACE_FORMAT_CHANGED:
    case CREATE_SWAPCHAIN_OK:
    case CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL:
        if (param.p_swapchain_data->img_views) {
            for (int i = 0; i < param.p_swapchain_data->img_count;
                 ++i) {
                if (param.p_swapchain_data->img_views[i])
                    vkDestroyImageView(
                        param.device,
                        param.p_swapchain_data->img_views[i],
                        alloc_callbacks);
            }

            free(param.p_swapchain_data->img_views);
        }
    case CREATE_SWAPCHAIN_IMAGE_VIEW_ALLOC_FAIL:
        param.p_swapchain_data->img_views = NULL;

        free(param.p_swapchain_data->images);

    case CREATE_SWAPCHAIN_IMAGE_ALLOC_FAIL:
        param.p_swapchain_data->images = NULL;
        param.p_swapchain_data->img_count = 0;

    case CREATE_SWAPCHAIN_IMAGE_LOAD_FAIL:
        vkDestroySwapchainKHR(param.device,
                              param.p_swapchain_data->swapchain,
                              alloc_callbacks);

    case CREATE_SWAPCHAIN_FAILED:
        param.p_swapchain_data->swapchain = VK_NULL_HANDLE;
    case CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE:
    case CREATE_SWAPCHAIN_CHOOSE_DETAILS_FAIL:
        param.p_swapchain_data->swapchain_life = 0;
        break;
    }
}

enum CreateDepthbuffersCodes {
    CREATE_DEPTHBUFFERS_BASE_FAIL = -0x7fff,
    CREATE_DEPTHBUFFERS_IMAGE_VIEW_FAIL,
    CREATE_DEPTHBUFFERS_IMAGE_VIEW_MEM_ALLOC_FAIL,
    CREATE_DEPTHBUFFERS_IMAGE_DEVICE_MEM_BIND_FAIL,
    CREATE_DEPTHBUFFERS_DEVICE_MEM_ALLOC_FAIL,
    CREATE_DEPTHBUFFERS_DEVICE_MEM_INAPPROPRIATE,
    CREATE_DEPTHBUFFERS_IMAGE_FAIL,
    CREATE_DEPTHBUFFERS_IMAGE_MEM_ALLOC_FAIL,
    CREATE_DEPTHBUFFERS_OK = 0,
};

int create_depthbuffers(StackAllocator* stk_allocr, size_t stk_offset,
                        const VkAllocationCallbacks* alloc_callbacks,
                        CreateDepthbuffersParam param) {
    VkResult result = VK_SUCCESS;

    *param.p_depth_buffers =
        malloc(param.depth_count * sizeof(VkImage));
    if (!(*param.p_depth_buffers))
        return CREATE_DEPTHBUFFERS_IMAGE_MEM_ALLOC_FAIL;

    VkImageCreateInfo img_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .extent =
        (VkExtent3D){
            .depth = 1,
            .width = param.img_extent.width,
            .height = param.img_extent.height,
        },
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = param.depth_img_format,
        .imageType = VK_IMAGE_TYPE_2D,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    for (size_t i = 0; i < param.depth_count; ++i) {
        (*param.p_depth_buffers)[i] = VK_NULL_HANDLE;
        result =
            vkCreateImage(param.device, &img_create_info,
                          alloc_callbacks,
                          *param.p_depth_buffers + i);
        if (result != VK_SUCCESS)
            return CREATE_DEPTHBUFFERS_IMAGE_FAIL;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(
        param.device, param.p_depth_buffers[0][0], &mem_reqs);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(param.phy_device, &mem_props);

    uint32_t mem_type_inx;
    for (mem_type_inx = 0; mem_type_inx < mem_props.memoryTypeCount;
         ++mem_type_inx) {
        if ((mem_reqs.memoryTypeBits & (1 << mem_type_inx)) &&
            (mem_props.memoryTypes[mem_type_inx].propertyFlags &
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            break;
    }

    if (mem_type_inx == mem_props.memoryTypeCount)
        return CREATE_DEPTHBUFFERS_DEVICE_MEM_INAPPROPRIATE;

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = mem_type_inx,
        .allocationSize =
        align_up_(mem_reqs.size, mem_reqs.alignment) *
        param.depth_count,
    };

    result = vkAllocateMemory(param.device, &alloc_info,
                              alloc_callbacks, param.p_depth_memory);
    if (result != VK_SUCCESS)
        return CREATE_DEPTHBUFFERS_DEVICE_MEM_ALLOC_FAIL;

    for (size_t i = 0; i < param.depth_count; ++i) {
        result = vkBindImageMemory(
            param.device, (*param.p_depth_buffers)[i],
            *param.p_depth_memory,
            i *
            align_up_(mem_reqs.size,
                      mem_reqs.alignment));
        if (result != VK_SUCCESS)
            return CREATE_DEPTHBUFFERS_IMAGE_DEVICE_MEM_BIND_FAIL;
    }

    *param.p_depth_buffer_views =
        malloc(param.depth_count * sizeof(VkImageView));
    if (!*param.p_depth_buffer_views)
        return CREATE_DEPTHBUFFERS_IMAGE_VIEW_MEM_ALLOC_FAIL;

    for (size_t i = 0; i < param.depth_count; ++i) {
        (*param.p_depth_buffer_views)[i] = VK_NULL_HANDLE;
        VkImageViewCreateInfo view_create = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = param.depth_img_format,
            .components = {
                0
            },
            .image = (*param.p_depth_buffers)[i],
            .subresourceRange = {
                .baseArrayLayer = 0,
                .levelCount = 1,
                .baseMipLevel = 0,
                .layerCount = 1,
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            }

        };

        result = vkCreateImageView(param.device, &view_create,
                                   alloc_callbacks,
                                   param.p_depth_buffer_views[0] + i);
        if (result != VK_SUCCESS)
            return CREATE_DEPTHBUFFERS_IMAGE_VIEW_FAIL;
    }


    return CREATE_DEPTHBUFFERS_OK;
}

void clear_depthbuffers(const VkAllocationCallbacks* alloc_callbacks,
                        ClearDepthbuffersParam param, int err_code) {

    switch (err_code) {

    case CREATE_DEPTHBUFFERS_OK:


    case CREATE_DEPTHBUFFERS_IMAGE_VIEW_FAIL:
        if (param.p_depth_buffer_views[0])
            for (size_t i = 0; i < param.depth_count; ++i)
                if ((*param.p_depth_buffer_views)[i])
                    vkDestroyImageView(
                        param.device,
                        (*param.p_depth_buffer_views)[i],
                        alloc_callbacks);
        if (param.p_depth_buffer_views[0])
            free(*param.p_depth_buffer_views);
    case CREATE_DEPTHBUFFERS_IMAGE_VIEW_MEM_ALLOC_FAIL:
        *param.p_depth_buffer_views = NULL;

    case CREATE_DEPTHBUFFERS_IMAGE_DEVICE_MEM_BIND_FAIL:


    case CREATE_DEPTHBUFFERS_DEVICE_MEM_ALLOC_FAIL:
        vkFreeMemory(param.device, *param.p_depth_memory,
                     alloc_callbacks);

    case CREATE_DEPTHBUFFERS_DEVICE_MEM_INAPPROPRIATE:


    case CREATE_DEPTHBUFFERS_IMAGE_FAIL:
        if (param.p_depth_buffers[0])
            for (size_t i = 0; i < param.depth_count; ++i) {
                if (param.p_depth_buffers[0][i])
                    vkDestroyImage(param.device,
                                   param.p_depth_buffers[0][i],
                                   alloc_callbacks);
            }

        if (param.p_depth_buffers[0])
            free(*param.p_depth_buffers);
    case CREATE_DEPTHBUFFERS_IMAGE_MEM_ALLOC_FAIL:
        *param.p_depth_buffers = NULL;

    case CREATE_DEPTHBUFFERS_BASE_FAIL:

        break;
    }
}

enum CreateFramebuffersCodes {
    CREATE_FRAMEBUFFERS_FAILED = -0x7fff,
    CREATE_FRAMEBUFFERS_INT_ALLOC_FAILED,
    CREATE_FRAMEBUFFERS_OK = 0,
};

int create_framebuffers(StackAllocator* stk_allocr, size_t stk_offset,
                        const VkAllocationCallbacks* alloc_callbacks,
                        CreateFramebuffersParam param) {

    VkResult result = VK_SUCCESS;

    param.p_framebuffers[0] =
        malloc(param.framebuffer_count * sizeof(VkFramebuffer));

    if (!param.p_framebuffers[0])
        return CREATE_FRAMEBUFFERS_INT_ALLOC_FAILED;

    memset(param.p_framebuffers[0], 0,
           param.framebuffer_count * sizeof(VkFramebuffer));
    for (int i = 0; i < param.framebuffer_count; ++i) {
        VkImageView img_attachments[] = { param.img_views[i],
                                          param.depth_views[i] };
        VkFramebufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = param.compatible_render_pass,
            .attachmentCount = COUNT_OF(img_attachments),
            .pAttachments = img_attachments,
            .width = param.framebuffer_extent.width,
            .height = param.framebuffer_extent.height,
            .layers = 1,
        };
        result = vkCreateFramebuffer(param.device, &create_info,
                                     alloc_callbacks,
                                     param.p_framebuffers[0] + i);

        if (result != VK_SUCCESS)
            break;
    }

    if (result != VK_SUCCESS)
        return CREATE_FRAMEBUFFERS_FAILED;

    return CREATE_FRAMEBUFFERS_OK;
}

void clear_framebuffers(const VkAllocationCallbacks* alloc_callbacks,
                        ClearFramebuffersParam param, int err_codes) {

    switch (err_codes) {
    case CREATE_FRAMEBUFFERS_OK:
    case CREATE_FRAMEBUFFERS_FAILED:
        if (param.p_framebuffers[0]) {
            for (int i = 0; i < param.framebuffer_count; ++i) {
                if (param.p_framebuffers[0][i])
                    vkDestroyFramebuffer(param.device,
                                         param.p_framebuffers[0][i],
                                         alloc_callbacks);
            }

            free(param.p_framebuffers[0]);
        }
    case CREATE_FRAMEBUFFERS_INT_ALLOC_FAILED:
        param.p_framebuffers[0] = NULL;
    }
}

int recreate_swapchain(StackAllocator* stk_allocr, size_t stk_offset,
                       VkAllocationCallbacks* alloc_callbacks,
                       RecreateSwapchainParam param) {

    if (param.p_old_swapchain_data->swapchain) {
        if (param.p_old_swapchain_data->swapchain_life > 0)
            vkDeviceWaitIdle(param.device.device);

        clear_framebuffers(
          alloc_callbacks,
          (ClearFramebuffersParam){
            .device = param.device.device,
            .framebuffer_count =
              param.p_old_swapchain_data->img_count,
            .p_framebuffers =
              &param.p_old_swapchain_data->framebuffers },
          0);

        clear_depthbuffers(
          alloc_callbacks,
          (ClearDepthbuffersParam){
            .device = param.device.device,
            .depth_count = param.p_old_swapchain_data->img_count,
            .p_depth_buffers =
              &param.p_old_swapchain_data->depth_imgs,
            .p_depth_buffer_views =
              &param.p_old_swapchain_data->depth_views,
            .p_depth_memory = &param.p_old_swapchain_data->device_mem,
          },
          0);

        clear_swapchain(
          alloc_callbacks,
          (ClearSwapchainParam){
            .device = param.device.device,
            .p_swapchain_data = param.p_old_swapchain_data,
          },
          0);
    }

    param.p_old_swapchain_data[0] = param.p_new_swapchain_data[0];

    int err_code = 0;

    err_code = create_swapchain(
      stk_allocr, stk_offset, alloc_callbacks,
      (CreateSwapchainParam){
        .device = param.device,
        .win_width = param.new_win_width,
        .win_height = param.new_win_height,
        .surface = param.surface,
        .old_swapchain = param.p_old_swapchain_data->swapchain,
        .p_surface_format = param.p_surface_format,
        .p_min_img_count = param.p_min_img_count,
        .p_img_swap_extent = param.p_img_swap_extent,
        .p_curr_swapchain_data = param.p_new_swapchain_data,
      });

    if ((err_code < 0) || (err_code == CREATE_SWAPCHAIN_SURFACE_FORMAT_CHANGED)) {
        // Return if any error, like zero framebuffer size, with
        // swapchain unchanged Also clear the swapchain, and reassign
        // old swapchain to current again
        
        clear_swapchain(
          alloc_callbacks,
          (ClearSwapchainParam){
            .device = param.device.device,
            .p_swapchain_data = param.p_new_swapchain_data,
          },
          err_code);
        *(param.p_new_swapchain_data) = *(param.p_old_swapchain_data);
        *(param.p_old_swapchain_data) =
            (struct SwapchainEntities){ 0 };

        return err_code ? RECREATE_SWAPCHAIN_CHANGE_RENDERPASS_ERR
                        : RECREATE_SWAPCHAIN_CREATE_SWAPCHAIN_ERR;
    }
    
    err_code = create_depthbuffers(
      stk_allocr, stk_offset, alloc_callbacks,
      (CreateDepthbuffersParam){
        .device = param.device.device,
        .phy_device = param.device.phy_device,
        .depth_count = param.p_new_swapchain_data->img_count,
        .img_extent = *param.p_img_swap_extent,
        .depth_img_format = param.depth_img_format,
        .p_depth_buffers = &param.p_new_swapchain_data->depth_imgs,
        .p_depth_buffer_views =
          &param.p_new_swapchain_data->depth_views,
        .p_depth_memory = &param.p_new_swapchain_data->device_mem,
      });

    if (err_code < 0) {
        // Return if any error, like zero framebuffer size, with
        // swapchain unchanged Also clear the swapchain, and reassign
        // old swapchain to current again
        clear_depthbuffers(
            alloc_callbacks,
            (ClearDepthbuffersParam){
                .device = param.device.device,
                .depth_count = param.p_new_swapchain_data->img_count,
                .p_depth_buffers =
                &param.p_new_swapchain_data->depth_imgs,
                .p_depth_buffer_views =
                &param.p_new_swapchain_data->depth_views,
                .p_depth_memory = &param.p_new_swapchain_data->
                                         device_mem,
            },
            err_code);
        
        clear_swapchain(
          alloc_callbacks,
          (ClearSwapchainParam){
            .device = param.device.device,
            .p_swapchain_data = param.p_new_swapchain_data,
          },
          0);
        *(param.p_new_swapchain_data) = *(param.p_old_swapchain_data);
        *(param.p_old_swapchain_data) =
            (struct SwapchainEntities){ 0 };
        return RECREATE_SWAPCHAIN_CREATE_SWAPCHAIN_ERR;
    }

    err_code = create_framebuffers(
      stk_allocr, stk_offset, alloc_callbacks,
      (CreateFramebuffersParam){
        .device = param.device.device,
        .compatible_render_pass = param.framebuffer_render_pass,
        .framebuffer_count = param.p_new_swapchain_data->img_count,
        .framebuffer_extent = param.p_img_swap_extent[0],
        .img_views = param.p_new_swapchain_data->img_views,
        .depth_views = param.p_new_swapchain_data->depth_views,
        .p_framebuffers = &(param.p_new_swapchain_data->framebuffers),
      });

    if (err_code < 0) {
        // Return if any error, like zero framebuffer size, with
        // swapchain unchanged Also clear the swapchain, and reassign
        // old swapchain to current again
        clear_framebuffers(
          alloc_callbacks,
          (ClearFramebuffersParam){
            .device = param.device.device,
            .framebuffer_count =
              param.p_new_swapchain_data->img_count,
            .p_framebuffers =
              &(param.p_new_swapchain_data->framebuffers),
          },
          err_code);

        clear_depthbuffers(
          alloc_callbacks,
          (ClearDepthbuffersParam){
            .device = param.device.device,
            .depth_count = param.p_new_swapchain_data->img_count,
            .p_depth_buffers =
              &param.p_new_swapchain_data->depth_imgs,
            .p_depth_buffer_views =
              &param.p_new_swapchain_data->depth_views,
            .p_depth_memory = &param.p_new_swapchain_data->device_mem,
          },
          0);

        
        clear_swapchain(
          alloc_callbacks,
          (ClearSwapchainParam){
            .device = param.device.device,
            .p_swapchain_data = param.p_new_swapchain_data,
          },
          0);

        *(param.p_new_swapchain_data) = *(param.p_old_swapchain_data);
        *(param.p_old_swapchain_data) =
            (struct SwapchainEntities){ 0 };
        return RECREATE_SWAPCHAIN_CREATE_FRAMEBUFFER_ERR;
    }

    return RECREATE_SWAPCHAIN_OK;
}
