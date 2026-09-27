find_program(PLER_NVCC nvcc
  HINTS
    "$ENV{CUDA_PATH}/bin"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6/bin"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4/bin"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/bin"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.1/bin"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.0/bin"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8/bin"
)

if(NOT PLER_NVCC)
  message(STATUS "PLER: nvcc not found; skipping pler_cuda")
  return()
endif()

set(CU_SRC ${CMAKE_CURRENT_SOURCE_DIR}/raycast.cu)
set(CU_OBJ ${CMAKE_CURRENT_BINARY_DIR}/raycast.cu.obj)
set(PLER_CUDA_ARCH "75")

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pler_cuda_stub.cpp
     "namespace { int pler_cuda_stub = 0; }\n")

add_custom_command(
  OUTPUT ${CU_OBJ}
  COMMAND "${PLER_NVCC}" -c "${CU_SRC}" -o "${CU_OBJ}"
          -std=c++17 -O3 -Xcompiler "/MD"
          -gencode=arch=compute_${PLER_CUDA_ARCH},code=sm_${PLER_CUDA_ARCH}
          -I"${CMAKE_CURRENT_SOURCE_DIR}/include"
  DEPENDS ${CU_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/include/pler_cuda.h
  COMMENT "nvcc compiling raycast.cu (sm_${PLER_CUDA_ARCH})"
  VERBATIM
)

add_library(pler_cuda STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/pler_cuda_stub.cpp
  ${CU_OBJ}
)
set_source_files_properties(${CU_OBJ} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
target_include_directories(pler_cuda PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

get_filename_component(NVCC_BIN "${PLER_NVCC}" DIRECTORY)
get_filename_component(CUDA_ROOT "${NVCC_BIN}" DIRECTORY)
target_link_directories(pler_cuda PUBLIC "${CUDA_ROOT}/lib/x64")
target_link_libraries(pler_cuda PUBLIC cudart)

message(STATUS "PLER: CUDA via nvcc (${PLER_NVCC})")
