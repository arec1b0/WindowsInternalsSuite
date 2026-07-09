# Project-wide compiler options, applied through an INTERFACE target.
# MSVC-focused (VS 2022); tolerates clang-cl.

function(wis_apply_compiler_options target)
    target_compile_features(${target} INTERFACE cxx_std_20)

    # Unicode build: Win32 API macros resolve to the -W (wide) variants.
    # Lean Windows headers; drop min/max macros that break std::min/std::max.
    target_compile_definitions(${target} INTERFACE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        WINVER=0x0A00        # Windows 10 baseline
        _WIN32_WINNT=0x0A00  # Windows 10 baseline (also covers Windows 11)
    )

    if(MSVC)
        target_compile_options(${target} INTERFACE
            /permissive-      # strict standard conformance
            /utf-8            # source + execution character set = UTF-8
            /Zc:__cplusplus   # report the true __cplusplus value
            /Zc:preprocessor  # standards-conformant preprocessor
            /EHsc             # standard C++ exception model
            /MP               # parallel compilation across translation units
        )
    endif()
endfunction()