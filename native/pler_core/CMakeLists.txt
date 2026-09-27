set(PLER_CORE_SOURCES
  src/mesh_io.cpp
  src/mesh_io_assimp.cpp
  src/frame.cpp
  src/bvh.cpp
  src/raycast_cpu.cpp
  src/score.cpp
  src/align_volume.cpp
  src/align_icp.cpp
  src/topology.cpp
  src/pler.cpp
  src/job.cpp
  src/config.cpp
  src/log.cpp
  src/util.cpp
  src/cache.cpp
  src/calibrate.cpp
  src/report.cpp
  src/sysinfo.cpp
)

add_library(pler_core STATIC ${PLER_CORE_SOURCES})

target_include_directories(pler_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

if(MSVC)
  target_compile_options(pler_core PRIVATE /W3 /EHsc /bigobj)
  target_compile_definitions(pler_core PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(pler_core PRIVATE -Wall -Wextra)
endif()

if(PLER_HAS_ASSIMP AND TARGET assimp::assimp)
  target_link_libraries(pler_core PUBLIC assimp::assimp)
  target_compile_definitions(pler_core PUBLIC PLER_HAS_ASSIMP=1)
endif()
