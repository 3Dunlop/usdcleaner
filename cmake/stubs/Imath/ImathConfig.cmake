# Stub ImathConfig.cmake for NVIDIA pre-built USD

if(NOT TARGET Imath::Imath)
    add_library(Imath::Imath SHARED IMPORTED)
    set_target_properties(Imath::Imath PROPERTIES
        IMPORTED_IMPLIB "$ENV{USD_ROOT}/lib/Imath-3_1.lib"
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{USD_ROOT}/include;$ENV{USD_ROOT}/include/Imath"
    )
endif()

set(Imath_FOUND TRUE)
set(Imath_VERSION "3.1.0")
