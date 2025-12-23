# cmake/vgcpu_sanitizers.cmake
# -----------------------------------------------------------------------------
# Sanitizer Configuration (Blueprint [REQ-97], [REQ-98], [DEC-BUILD-04])
# Sanitizers apply only to VGCPU targets, not third-party code.
# Sanitizers are NOT combined per [DEC-BUILD-04].
# -----------------------------------------------------------------------------

function(vgcpu_apply_sanitizers target)
    if(VGCPU_ENABLE_ASAN)
        message(STATUS "[REQ-97] Enabling AddressSanitizer for ${target}")
        target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address)
    endif()

    if(VGCPU_ENABLE_UBSAN)
        message(STATUS "[REQ-97] Enabling UBSan for ${target}")
        target_compile_options(${target} PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=undefined)
    endif()

    if(VGCPU_ENABLE_TSAN)
        message(STATUS "[REQ-97] Enabling ThreadSanitizer for ${target}")
        target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=thread)
    endif()
endfunction()

# Verify sanitizers are not combined (would cause issues)
if(VGCPU_ENABLE_ASAN AND VGCPU_ENABLE_TSAN)
    message(FATAL_ERROR "[DEC-BUILD-04] Cannot combine ASan and TSan. Use separate presets.")
endif()

if(VGCPU_ENABLE_ASAN AND VGCPU_ENABLE_UBSAN)
    message(WARNING "[DEC-BUILD-04] Combining ASan+UBSan may work on some platforms but is not officially supported.")
endif()
