cmake_minimum_required(VERSION 3.0)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 17)

get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

set(LIB_NAME ${CURRENT_DIR_NAME}_lib)

add_library(${LIB_NAME} OBJECT
        imgui/cimgui.cpp
        imgui/imgui/imgui.cpp
        imgui/imgui/imgui_demo.cpp
        imgui/imgui/imgui_draw.cpp
        imgui/imgui/imgui_impl_vulkan.cpp
        imgui/imgui/imgui_impl_win32.cpp
        imgui/imgui/imgui_tables.cpp
        imgui/imgui/imgui_widgets.cpp
        spirv_reflect.c)

target_include_directories(${LIB_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui)


target_include_directories(${LIB_NAME} PUBLIC
        "D:\\prgming\\stb"
        "D:\\prgming\\VulkanSDK\\1.3.236.0\\Include")


target_link_directories(${LIB_NAME} PUBLIC "D:\\prgming\\VulkanSDK\\1.3.236.0\\Lib")

target_link_libraries(${LIB_NAME} PUBLIC vulkan-1.lib spirv-cross-c-shared.lib dwmapi.lib)