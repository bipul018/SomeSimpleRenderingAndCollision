#pragma once
#include "window-stuff.h"

typedef struct {
    VkDevice device;
    VkFormat img_format;
    VkFormat depth_stencil_format;

    VkRenderPass *p_render_pass;
} CreateRenderPassParam;
int create_render_pass(StackAllocator *stk_allocr, size_t stk_offset,
                       const VkAllocationCallbacks *alloc_callbacks,
                       CreateRenderPassParam param);

typedef struct {
    VkDevice device;

    VkRenderPass *p_render_pass;
} ClearRenderPassParam;
void clear_render_pass(const VkAllocationCallbacks *alloc_callbacks,
                       ClearRenderPassParam param, int err_codes);

uint32_t *read_spirv_in_stk_allocr(StackAllocator *stk_allocr,
                                   size_t *p_stk_offset,
                                   const char *file_name,
                                   size_t *p_file_size);

enum CreateShaderModuleFromFileCodes {
    CREATE_SHADER_MODULE_FROM_FILE_ERROR = -0x7fff,
    CREATE_SHADER_MODULE_FROM_FILE_READ_FILE_ERROR,
    CREATE_SHADER_MODULE_FROM_FILE_OK = 0,
};

typedef struct {
    VkDevice device;
    const char *shader_file_name;

    VkShaderModule *p_shader_module;
} CreateShaderModuleFromFileParam;

int create_shader_module_from_file(
  StackAllocator *stk_allocr, size_t stk_offset,
  const VkAllocationCallbacks *alloc_callbacks,
  CreateShaderModuleFromFileParam param);

struct GraphicsPipelineCreationInfos {
    VkPipelineDynamicStateCreateInfo dynamic_state;
    VkPipelineColorBlendStateCreateInfo color_blend_state;
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
    VkPipelineMultisampleStateCreateInfo multisample_state;
    VkPipelineRasterizationStateCreateInfo rasterization_state;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineTessellationStateCreateInfo tessellation_state;
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
    VkPipelineVertexInputStateCreateInfo vertex_input_state;
    VkPipelineCreateFlags flags;
};
typedef struct GraphicsPipelineCreationInfos
  GraphicsPipelineCreationInfos;

GraphicsPipelineCreationInfos
default_graphics_pipeline_creation_infos(void);

typedef struct {
    VkDevice device;
    GraphicsPipelineCreationInfos create_infos;
    VkPipelineLayout pipe_layout;
    const char *vert_shader_file;
    const char *frag_shader_file;
    const char *geom_shader_file;
    VkRenderPass compatible_render_pass;
    uint32_t subpass_index;

    VkPipeline *p_pipeline;
} CreateGraphicsPipelineParam;

int create_graphics_pipeline(StackAllocator *stk_allocr,
                             size_t stk_offset,
                             VkAllocationCallbacks *alloc_callbacks,
                             CreateGraphicsPipelineParam param);

typedef struct {
    VkDevice device;

    VkPipeline *p_pipeline;
} ClearGraphicsPipelineParam;
void clear_graphics_pipeline(const VkAllocationCallbacks *alloc_callbacks,
                             ClearGraphicsPipelineParam param,
                             int err_code);

enum CreateSemaphoresCodes {
    CREATE_SEMAPHORES_FAILED = -0x7fff,
    CREATE_SEMAPHORES_ALLOC_FAIL,
    CREATE_SEMAPHORES_OK = 0,
};
typedef struct {
    uint32_t semaphores_count;
    VkDevice device;

    VkSemaphore **p_semaphores;
} CreateSemaphoresParam;
int create_semaphores(StackAllocator *stk_allocr, size_t stk_offset,
                      const VkAllocationCallbacks *alloc_callbacks,
                      CreateSemaphoresParam param);

typedef struct {
    VkDevice device;
    size_t semaphores_count;

    VkSemaphore **p_semaphores;
} ClearSemaphoresParam;

void clear_semaphores(const VkAllocationCallbacks *alloc_callbacks,
                      ClearSemaphoresParam param, int err_codes);


enum CreateFencesCodes {
    CREATE_FENCES_FAILED = -0x7fff,
    CREATE_FENCES_ALLOC_FAILED,
    CREATE_FENCES_OK = 0,
};

typedef struct {
    uint32_t fences_count;
    VkDevice device;

    VkFence **p_fences;
} CreateFencesParam;
int create_fences(StackAllocator *stk_allocr, size_t stk_offset,
                  const VkAllocationCallbacks *alloc_callbacks,
                  CreateFencesParam param);

typedef struct {
    VkDevice device;
    size_t fences_count;

    VkFence **p_fences;
} ClearFencesParam;

void clear_fences(const VkAllocationCallbacks *alloc_callbacks,
                  ClearFencesParam param, int err_codes);

typedef struct {
    VkDevice device;
    VkCommandPool cmd_pool;
    uint32_t cmd_buffer_count;

    VkCommandBuffer **p_cmd_buffers;
} CreatePrimaryCommandBuffersParam;
int create_primary_command_buffers(
  StackAllocator *stk_allocr, size_t stk_offset,
  VkAllocationCallbacks *alloc_callbacks,
  CreatePrimaryCommandBuffersParam param);

typedef struct {

    VkCommandBuffer **p_cmd_buffers;
} ClearPrimaryCommandBuffersParam;
void clear_primary_command_buffers(
  VkAllocationCallbacks *alloc_callbacks,
  ClearPrimaryCommandBuffersParam param, int err_codes);


enum BeginRenderingOperationsCodes {
    BEGIN_RENDERING_OPERATIONS_FAILED = -0x7fff,
    BEGIN_RENDERING_OPERATIONS_BEGIN_CMD_BUFFER_FAIL,
    BEGIN_RENDERING_OPERATIONS_WAIT_FOR_FENCE_FAIL,
    BEGIN_RENDERING_OPERATIONS_OK = 0,
    BEGIN_RENDERING_OPERATIONS_TRY_RECREATE_SWAPCHAIN,
    BEGIN_RENDERING_OPERATIONS_MINIMIZED,
};

typedef struct {
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkRenderPass render_pass;
    VkFramebuffer *framebuffers;
    VkExtent2D framebuffer_render_extent;
    VkCommandBuffer cmd_buffer;
    VkSemaphore present_done_semaphore;
    VkFence render_done_fence;
    VkClearValue clear_value;

    uint32_t *p_img_inx;

} BeginRenderingOperationsParam;
int begin_rendering_operations(BeginRenderingOperationsParam param);

enum EndRenderingOperationsCodes {
    END_RENDERING_OPERATIONS_FAILED = -0x7fff,
    END_RENDERING_OPERATIONS_GRAPHICS_QUEUE_SUBMIT_FAIL,
    END_RENDERING_OPERATIONS_END_CMD_BUFFER_FAIL,
    END_RENDERING_OPERATIONS_OK = 0,
    END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN,
};

typedef struct {
    VkDevice device;
    VkCommandBuffer cmd_buffer;

    VkSemaphore render_done_semaphore;
    VkSemaphore present_done_semaphore;
    VkFence render_done_fence;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkSwapchainKHR swapchain;
    uint32_t img_index;

} EndRenderingOperationsParam;
int end_rendering_operations(EndRenderingOperationsParam param);
