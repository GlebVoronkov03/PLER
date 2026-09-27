add_library(pler_shared SHARED src/pler_c.cpp)
set_target_properties(pler_shared PROPERTIES
  OUTPUT_NAME pler
  WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

target_compile_definitions(pler_shared PRIVATE PLER_BUILD_SHARED)
target_include_directories(pler_shared
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/pler_core/include
)

target_link_libraries(pler_shared PRIVATE pler_core)

if(MSVC)
  target_compile_options(pler_shared PRIVATE /W3 /EHsc)
  target_compile_definitions(pler_shared PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Version macros from parent project()
target_compile_definitions(pler_shared PUBLIC
  PLER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  PLER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  PLER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
  PLER_VERSION_STRING="${PROJECT_VERSION}"
)

add_executable(pler_c_example examples/pler_c_example.c)
target_link_libraries(pler_c_example PRIVATE pler_shared)
if(MSVC)
  target_compile_definitions(pler_c_example PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Alias for docs: consumers find "pler" package exporting pler::pler
add_library(pler::pler ALIAS pler_shared)
