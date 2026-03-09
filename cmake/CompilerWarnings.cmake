# Shared compiler warning configuration

function(usdcleaner_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /wd4244  # conversion from 'type1' to 'type2', possible loss of data
            /wd4267  # conversion from 'size_t' to 'type', possible loss of data
            /wd4305  # truncation from 'double' to 'float'
            /wd4100  # unreferenced formal parameter (common in USD callbacks)
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
        )
    endif()
endfunction()
