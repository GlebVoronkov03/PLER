cmake_minimum_required(VERSION 3.20)
project(pler LANGUAGES CXX C VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Unzip-and-run: find libpler next to the binaries
if(NOT WIN32)
  set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
  if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path")
    set(CMAKE_BUILD_RPATH "@loader_path")
  else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_RPATH "$ORIGIN")
  endif()
endif()

option(PLER_CUDA "Build CUDA ray-cast backend" ON)
option(PLER_BUILD_GUI "Build Dear ImGui GUI" ON)
option(PLER_ASSIMP "Use Assimp for extra mesh formats" ON)

find_package(embree 4 QUIET)
if(NOT embree_FOUND)
  find_package(embree 3 QUIET)
endif()

set(PLER_HAS_ASSIMP OFF)
if(PLER_ASSIMP)
  find_package(assimp QUIET)
  if(assimp_FOUND)
    set(PLER_HAS_ASSIMP ON)
    message(STATUS "PLER: Assimp found")
  else()
    message(STATUS "PLER: Assimp not found; OBJ/STL only")
  endif()
endif()

add_subdirectory(pler_core)

set(PLER_HAS_CUDA OFF)
if(PLER_CUDA)
  find_program(_PLER_NVCC nvcc
    HINTS
      "$ENV{CUDA_PATH}/bin"
      "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/bin"
      "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8/bin"
  )
  if(_PLER_NVCC)
    add_subdirectory(pler_cuda)
    if(TARGET pler_cuda)
      target_link_libraries(pler_core PUBLIC pler_cuda)
      target_compile_definitions(pler_core PUBLIC PLER_HAS_CUDA=1)
      target_include_directories(pler_core PUBLIC ${CMAKE_SOURCE_DIR}/pler_cuda/include)
      set(PLER_HAS_CUDA ON)
    endif()
  else()
    message(STATUS "PLER: CUDA toolkit not found; CPU only")
  endif()
endif()

if(embree_FOUND)
  target_link_libraries(pler_core PUBLIC embree)
  target_compile_definitions(pler_core PUBLIC PLER_HAS_EMBREE=1)
  message(STATUS "PLER: Embree found")
else()
  message(STATUS "PLER: using built-in BVH raycaster")
endif()

# Version for all consumers of pler_core
target_compile_definitions(pler_core PUBLIC
  PLER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  PLER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  PLER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
  PLER_VERSION_STRING="${PROJECT_VERSION}"
)

add_subdirectory(pler_c)
add_subdirectory(pler_cli)

if(PLER_BUILD_GUI)
  add_subdirectory(pler_gui)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS pler RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET pler_gui)
  install(TARGETS pler_gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(TARGETS pler_shared
  EXPORT plerTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES pler_c/include/pler.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY pler_core/include/pler DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT plerTargets
  FILE plerTargets.cmake
  NAMESPACE pler::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pler
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/plerConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/plerConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pler
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/plerConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/plerConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/plerConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pler
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/../config/ DESTINATION ${CMAKE_INSTALL_DATADIR}/pler/config
        FILES_MATCHING PATTERN "*.yaml")
install(DIRECTORY ${CMAKE_SOURCE_DIR}/../docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR}
        FILES_MATCHING PATTERN "*.md")
install(DIRECTORY ${CMAKE_SOURCE_DIR}/../samples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/pler/samples)
install(FILES
  ${CMAKE_SOURCE_DIR}/../README.md
  ${CMAKE_SOURCE_DIR}/../LICENSE
  ${CMAKE_SOURCE_DIR}/../CITATION.cff
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
