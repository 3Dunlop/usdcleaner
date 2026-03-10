# Stub OpenSubdivConfig.cmake for NVIDIA pre-built USD

set(OpenSubdiv_FOUND TRUE)
set(OpenSubdiv_VERSION "3.6.0")

# USD's pxrTargets.cmake links against osdCPU and osdGPU directly,
# so we just need to make find_package succeed.
