#include <stdio.h>

#include <spirv_cross/spirv_cross_c.h>

#include "render-stuff.h"
#include "spirv_reflect.h"
void from_spv(StackAllocator* stk_allocr, size_t offset) {

    size_t stk_offset = 0;
    size_t word_count = 0;
    uint32_t *shader_code = read_spirv_in_stk_allocr(
      stk_allocr, &stk_offset, "shaders/out/demo0.frag.hlsl.spv", &word_count);

    SpvReflectShaderModule module;
    SpvReflectResult result;
    result = spvReflectCreateShaderModule(word_count, shader_code, &module);

    

    // Get the descriptor bindings for push constants

    for (size_t i = 0; i < module.descriptor_set_count; ++i) {
        printf("Set: %u , Binding Count: %u\n", module.descriptor_sets[i].set,
               module.descriptor_sets[i].binding_count);
        for (size_t j = 0; j < module.descriptor_sets[i].binding_count; ++j) {
            printf("Binding No : %u Absoulte Offset : %u \n", module.descriptor_sets[i].bindings[j]->binding,
                   module.descriptor_sets[i].bindings[j]->block.absolute_offset);
        }
    }

    for (size_t i = 0; i < module.push_constant_block_count; ++i) {
        printf("Name: %s, Member Count: %u, Block Size: %u, Padded Size: %u, Offset: %u\n",
               module.push_constant_blocks[i].name, module.push_constant_blocks[i].member_count,
               module.push_constant_blocks[i].size, module.push_constant_blocks[i].padded_size,
               module.push_constant_blocks[i].offset);
        for (size_t j = 0; j < module.push_constant_blocks[i].member_count; ++j) {
            printf("Member Name: %s, Member Offset: %u, Member Size: %u\n",
                   module.push_constant_blocks[i].members[j].name,
                   module.push_constant_blocks[i].members[j].offset,
                   module.push_constant_blocks[i].members[j].size);
        }
    }

    

    // Clean up
    spvReflectDestroyShaderModule(&module);
}

int main2(void) {



    printf("Let's begin !!\n");

    StackAllocator stk_allocr = alloc_stack_allocator(SIZE_GB(2), 100);
    from_spv(&stk_allocr, 0);
    //return 0;
    size_t stk_offset = 0;
    size_t word_count = 0;
    uint32_t *shader_code =
      read_spirv_in_stk_allocr(&stk_allocr, &stk_offset, "shaders/out/demo0.frag.hlsl.spv", &word_count);

    const SpvId *spirv = shader_code;
    word_count /= sizeof(*spirv);
    
    spvc_context context = NULL;
    spvc_parsed_ir parsed_ir = NULL;
    spvc_compiler compiler = NULL;
    spvc_compiler_options options = NULL;
    spvc_resources resources = NULL;
    const spvc_reflected_resource *list = NULL;
    const char *result = NULL;
    size_t count;
    size_t i;

    // Create context.
    spvc_context_create(&context);

    // Set debug callback.
    spvc_context_set_error_callback(context, (spvc_error_callback)printf, "%s");

    // Parse the SPIR-V.
    spvc_context_parse_spirv(context, spirv, word_count, &parsed_ir);

    

    // Hand it off to a compiler instance and give it ownership of the IR.
    spvc_context_create_compiler(context, SPVC_BACKEND_HLSL, parsed_ir,
                                 SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);

    // Do some basic reflection.
    spvc_compiler_create_shader_resources(compiler, &resources);
    spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &list,
                                              &count);

    for (i = 0; i < count; i++) {
        spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationOffset, 16);
        printf("ID: %u, BaseTypeID: %u, TypeID: %u, Name: %s\n", list[i].id, list[i].base_type_id,
               list[i].type_id, list[i].name);
        printf("  Set: %u, Binding: %u, Offset : %u\n",
               spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationDescriptorSet),
               spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding),
               spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationOffset));
                
    }

    spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list,
                                              &count);
    unsigned set =
      spvc_compiler_get_decoration(compiler, list[0].id, SpvDecorationBinding);
    set++;
    spvc_compiler_set_decoration(compiler, list[0].id, SpvDecorationBinding, set);

    // Modify options.
    spvc_compiler_create_compiler_options(compiler, &options);
    //spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 330);
    //spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_FALSE);

    //spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_TRUE);
    spvc_compiler_install_compiler_options(compiler, options);

    
    spvc_compiler_compile(compiler, &result);
    printf("Cross-compiled source: %s\n", result);

    // Frees all memory we allocated so far.
    spvc_context_destroy(context);


    dealloc_stack_allocator(&stk_allocr);
    return 0;
}