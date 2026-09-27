@echo off
set LOG=C:\_Metric_PLER+\native\deps\cuda_install.log
echo Installing CUDA 12.2 toolkit components... > "%LOG%"
"C:\_Metric_PLER+\native\deps\cuda_12.2.2_windows_network.exe" -s nvcc_12.2 cudart_12.2 cupti_12.2 thrust_12.2 cublas_12.2 nvrtc_12.2 visual_studio_integration_12.2 -logFile "%LOG%"
echo EXIT=%ERRORLEVEL% >> "%LOG%"
