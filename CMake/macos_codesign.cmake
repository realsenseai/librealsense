# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# macOS code-signing helpers for the USB device-capture entitlement
# (com.apple.vm.device-access). With this entitlement libusb's Darwin backend can
# take EXCLUSIVE ownership of the RealSense UVC interfaces from the userspace
# UVCAssistant camera extension instead of racing it - which is what makes
# multi-camera bring-up (rs-multicam) reliable on macOS. See
# src/libusb/handle-libusb.h and CMake/macos-device-access.entitlements for the why.
#
# Everything here is a no-op off macOS. Ad-hoc signing ("--sign -") is used so no
# developer identity is required; codesign always succeeds, but AMFI only honors the
# restricted entitlement at launch when SIP is disabled:
#   csrutil disable   (from macOS Recovery, then reboot)
#
# WARNING: do NOT set amfi_get_out_of_my_way=1. On Apple Silicon (M1+) the USB
# controller is driven by DriverKit system extensions that require AMFI validation
# to load. Setting that boot-arg prevents those extensions from loading, which
# breaks USB enumeration entirely — no USB devices will appear on the system.

# Resolve the entitlements path relative to THIS module at include time. Example/tool
# subdirectories each call project(), which resets PROJECT_SOURCE_DIR, so we cannot
# rely on it. Cache it so it stays visible across all scopes.
set(LRS_MACOS_DEVICE_ACCESS_ENTITLEMENTS "${CMAKE_CURRENT_LIST_DIR}/macos-device-access.entitlements"
    CACHE INTERNAL "Path to the macOS com.apple.vm.device-access entitlements plist")

# Sign a single target post-build. Must be called from the directory that defines the
# target (CMake restricts add_custom_command(TARGET) to the defining directory).
function(lrs_codesign_device_access target)
    if(NOT APPLE)
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND codesign --force --sign - --entitlements "${LRS_MACOS_DEVICE_ACCESS_ENTITLEMENTS}" --timestamp=none "$<TARGET_FILE:${target}>"
        COMMENT "Codesigning ${target} with com.apple.vm.device-access entitlement"
        VERBATIM)
endfunction()

# Recursively collect all (non-imported) executable target names under a directory
# tree into out_var. Reading target properties across directories is allowed.
function(_lrs_collect_executables dir out_var)
    set(_acc "")
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_tgt IN LISTS _targets)
        get_target_property(_type ${_tgt} TYPE)
        get_target_property(_imported ${_tgt} IMPORTED)
        if(_type STREQUAL "EXECUTABLE" AND NOT _imported)
            list(APPEND _acc ${_tgt})
        endif()
    endforeach()

    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_subdir IN LISTS _subdirs)
        _lrs_collect_executables("${_subdir}" _sub)
        list(APPEND _acc ${_sub})
    endforeach()

    set(${out_var} "${_acc}" PARENT_SCOPE)
endfunction()

# Create one aggregate target that signs every executable in the tree. Built as part
# of ALL and made to depend on every executable, so a full build signs everything
# after it links. Call once from the top-level CMakeLists after all add_subdirectory()
# calls. (We cannot attach POST_BUILD commands to targets in other directories, hence
# the single aggregate target rather than per-target hooks.) A partial build of one
# target won't trigger this; run `make lrs-codesign-all` or do a full build to sign.
function(lrs_codesign_all_executables dir)
    if(NOT APPLE)
        return()
    endif()

    _lrs_collect_executables("${dir}" _all_exes)
    if(NOT _all_exes)
        return()
    endif()

    set(_cmds "")
    foreach(_exe IN LISTS _all_exes)
        list(APPEND _cmds
            COMMAND codesign --force --sign - --entitlements "${LRS_MACOS_DEVICE_ACCESS_ENTITLEMENTS}" --timestamp=none "$<TARGET_FILE:${_exe}>")
    endforeach()

    add_custom_target(lrs-codesign-all ALL ${_cmds}
        COMMENT "macOS: codesigning all executables with com.apple.vm.device-access entitlement"
        VERBATIM)
    add_dependencies(lrs-codesign-all ${_all_exes})
endfunction()
