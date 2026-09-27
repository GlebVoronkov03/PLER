if(NOT PLER_BUILD_GUI)
  return()
endif()

include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
  GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.91.8
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})

add_executable(pler_gui
  main.cpp
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(pler_gui PRIVATE
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

target_link_libraries(pler_gui PRIVATE pler_core glfw)

target_compile_definitions(pler_gui PRIVATE
  PLER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  PLER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  PLER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
  PLER_VERSION_STRING="${PROJECT_VERSION}"
)

if(APPLE)
  set_target_properties(pler_gui PROPERTIES
    INSTALL_RPATH "@loader_path"
    BUILD_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
elseif(UNIX)
  set_target_properties(pler_gui PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()

if(WIN32)
  target_link_libraries(pler_gui PRIVATE opengl32 comdlg32 ole32 shell32)
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(pler_gui PRIVATE OpenGL::GL)
endif()

if(MSVC)
  target_compile_options(pler_gui PRIVATE /W3)
endif()
