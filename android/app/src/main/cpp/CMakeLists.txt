cmake_minimum_required(VERSION 3.22.1)
project(analytic_continuation C)

set(NATIVE_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")

add_library(native_app_glue STATIC
    "${NATIVE_GLUE_DIR}/android_native_app_glue.c"
)
target_include_directories(native_app_glue PUBLIC "${NATIVE_GLUE_DIR}")

get_filename_component(APPLICATION_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(AICI_ICK_ROOT "${APPLICATION_ROOT}/_/ai-ci-ick" CACHE PATH "Pinned ai-ci checkout")
set(ICK_STAGE_ROOT "${APPLICATION_ROOT}/_/build/ick" CACHE PATH "Qualified ICK ABI stages")
include("${AICI_ICK_ROOT}/ick-android/OwnedC.cmake")
if(NOT ICK_COMPILER)
  ick_android_stage(ROOT "${ICK_STAGE_ROOT}/${ANDROID_ABI}" FORTIFY_SOURCE 2)
endif()
ick_android_objects(owned_objects NAME analytic_continuation SOURCES analytic_continuation_random.c holomorphic_walk.c
  INCLUDES "${NATIVE_GLUE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
  OPTIONS -Wall -Wextra -Wpedantic STANDARD 11)

add_library(analytic_continuation SHARED ${owned_objects})
set_target_properties(analytic_continuation PROPERTIES LINKER_LANGUAGE C)
target_compile_features(analytic_continuation PRIVATE c_std_11)
target_compile_options(analytic_continuation PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(analytic_continuation PRIVATE -u ANativeActivity_onCreate)
target_link_libraries(analytic_continuation
    native_app_glue
    android
    log
    EGL
    GLESv3
    m
)
