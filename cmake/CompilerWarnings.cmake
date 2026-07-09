# Warning configuration, applied through an INTERFACE target.

function(wis_apply_compiler_warnings target)
    if(MSVC)
        set(_warnings
            /W4        # high warning level
            /w14242    # conversion: possible loss of data
            /w14254    # larger bit-field conversion: possible loss of data
            /w14263    # member function does not override any base method
            /w14265    # class has virtual functions but no virtual destructor
            /w14555    # expression has no effect
            /w14640    # thread-unsafe static member initialization
            /w14826    # sign-extension conversion may be unexpected
        )
        if(WIS_WARNINGS_AS_ERRORS)
            list(APPEND _warnings /WX)
        endif()
        target_compile_options(${target} INTERFACE ${_warnings})
    endif()
endfunction()