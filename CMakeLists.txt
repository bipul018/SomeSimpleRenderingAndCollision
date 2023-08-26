cmake_minimum_required(VERSION 3.0)
project(ClionTest)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(UNICODE)
add_compile_definitions(_UNICODE)

add_executable(${PROJECT_NAME}
        src/common-stuff.c
        src/device-mem-stuff.c
        src/font_verts.c
        src/model_gen.c
        src/reflect-test.c
        src/render-stuff.c
        src/tryout.c
        src/window-stuff.c
        src/main.c
)


set(ext_lib Exts)
add_subdirectory(ext)
target_link_libraries(${PROJECT_NAME} PRIVATE ext_lib)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)


add_custom_target(shaders)

set(HLSL_SHADER_FILES shaders/demo0.vert.hlsl shaders/demo0.frag.hlsl)

set_source_files_properties(shaders/demo0.vert.hlsl PROPERTIES ShaderType "vs")
set_source_files_properties(shaders/demo0.frag.hlsl PROPERTIES ShaderType "ps")
set_source_files_properties(${HLSL_SHADER_FILES} PROPERTIES ShaderModel "6_0")

foreach (FILE ${HLSL_SHADER_FILES})
    get_filename_component(FILE_WE ${FILE} NAME_WE)
    get_source_file_property(shadertype ${FILE} ShaderType)
    get_source_file_property(shadermodel ${FILE} ShaderModel)
    add_custom_command(TARGET shaders
            COMMAND D:\\prgming\\VulkanSDK\\1.3.231.1\\Bin\\dxc.exe -spirv /nologo /Emain /T${shadertype}_${shadermodel} $<IF:$<CONFIG:DEBUG>,/Od,/O1> /Zi /Fo ${CMAKE_CURRENT_BINARY_DIR}/${FILE_WE}.${shadertype} ${CMAKE_BINARY_DIR}/${FILE_WE}.pdb ${FILE}
            MAIN_DEPENDENCY ${FILE}
            COMMENT "HLSL ${FILE}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            VERBATIM)
endforeach (FILE)

file(COPY Sanskr.ttf DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


add_dependencies(${PROJECT_NAME} shaders)