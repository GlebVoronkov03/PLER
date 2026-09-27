add_executable(pler main.cpp)
target_link_libraries(pler PRIVATE pler_core)
target_compile_definitions(pler PRIVATE
  PLER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  PLER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  PLER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
  PLER_VERSION_STRING="${PROJECT_VERSION}"
)
if(APPLE)
  set_target_properties(pler PROPERTIES
    INSTALL_RPATH "@loader_path"
    BUILD_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
elseif(UNIX)
  set_target_properties(pler PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()
if(MSVC)
  target_compile_options(pler PRIVATE /W3)
  target_compile_definitions(pler PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
