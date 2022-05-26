#----------------------------------------------------------------------------
#  Copyright (c) 2022 Arm Limited. All rights reserved.
#  SPDX-License-Identifier: Apache-2.0
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#----------------------------------------------------------------------------

function(set_platform_global_defaults)
    message(STATUS "Platform: Ensemble HP")

    if (NOT DEFINED CMAKE_SYSTEM_PROCESSOR)
        if (TARGET_SUBSYSTEM STREQUAL RTSS-HP)
            #set(CMAKE_SYSTEM_PROCESSOR cortex-m55 CACHE STRING "Cortex-M CPU to use")
            if(POLICY CMP0123)
                set(CMAKE_SYSTEM_ARCH armv8.1-m.main CACHE STRING "System arch to use")
            else()
                set(CMAKE_SYSTEM_PROCESSOR  cortex-m55)
            endif()
        elseif(TARGET_SUBSYSTEM STREQUAL RTSS-HE)
            # For CMake versions older than 3.21, the compiler and linker flags for
            # ArmClang are added by CMake automatically which makes it mandatory to
            # define the system processor. For CMake versions 3.21 or later (that
            # implement policy CMP0123) we use armv8.1-m as the arch until the
            # toolchain officially supports Cortex-M85. For older version of CMake
            # we revert to using Cortex-M55 as the processor (as this will work
            # for M85 too).
            if(POLICY CMP0123)
                set(CMAKE_SYSTEM_ARCH armv8.1-m.main CACHE STRING "System arch to use")
            else()
                set(CMAKE_SYSTEM_PROCESSOR  cortex-m55)
            endif()
        endif()
    endif()

    if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_DIR}/bare-metal-gcc.cmake
                CACHE FILEPATH "Toolchain file")
    endif()

    # if ((ETHOS_U_NPU_ID STREQUAL U55) AND (TARGET_SUBSYSTEM STREQUAL sse-310))
    #     message(FATAL_ERROR "Non compatible Ethos-U NPU processor ${ETHOS_U_NPU_ID} and target subsystem ${TARGET_SUBSYSTEM}")
    # endif()

    set(LINKER_SCRIPT_NAME "ensemble-${TARGET_SUBSYSTEM}" PARENT_SCOPE)
    set(PLATFORM_DRIVERS_DIR "${HAL_PLATFORM_DIR}/ensemble" PARENT_SCOPE)

endfunction()

function(platform_custom_post_build)
    set(oneValueArgs TARGET_NAME)
    cmake_parse_arguments(PARSED "" "${oneValueArgs}" "" ${ARGN} )

    set_target_properties(${PARSED_TARGET_NAME} PROPERTIES SUFFIX ".axf")

    message(STATUS "******************** PARSED_TARGET_NAME: ${PARSED_TARGET_NAME} ********************")
    message(STATUS "******************** LINKER_SCRIPT_NAME: ${LINKER_SCRIPT_NAME} ********************")
    message(STATUS "******************** CMAKE_SCRIPTS_DIR/platforms/ensemble/TARGET_SUBSYSTEM: ${CMAKE_SCRIPTS_DIR}/platforms/ensemble/${TARGET_SUBSYSTEM} ********************")

    # Add link options for the linker script to be used:
    add_linker_script(
            ${PARSED_TARGET_NAME}          # Target
            ${CMAKE_SCRIPTS_DIR}/platforms/ensemble/${TARGET_SUBSYSTEM}    # Directory path
            ${LINKER_SCRIPT_NAME})  # Name of the file without suffix

    add_target_map_file(
            ${PARSED_TARGET_NAME}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PARSED_TARGET_NAME}.map)

    set(SECTORS_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sectors)
    set(SECTORS_BIN_DIR ${SECTORS_DIR}/${use_case})

    file(REMOVE_RECURSE ${SECTORS_BIN_DIR})
    file(MAKE_DIRECTORY ${SECTORS_BIN_DIR})

    # if (TARGET_SUBSYSTEM STREQUAL sse-310)
    #     set(LINKER_SECTION_TAGS     "*.at_bram" "*.at_ddr")
    #     set(LINKER_OUTPUT_BIN_TAGS  "bram.bin"  "ddr.bin")
    # else()
    #     set(LINKER_SECTION_TAGS     "*.at_itcm" "*.at_ddr")
    #     set(LINKER_OUTPUT_BIN_TAGS  "itcm.bin"  "ddr.bin")
    # endif()

    add_bin_generation_command(
            TARGET_NAME ${PARSED_TARGET_NAME}
            OUTPUT_DIR  ${SECTORS_BIN_DIR}
            AXF_PATH    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PARSED_TARGET_NAME}.axf
            SECTION_PATTERNS    "${LINKER_SECTION_TAGS}"
            OUTPUT_BIN_NAMES    "${LINKER_OUTPUT_BIN_TAGS}")

    #set(MPS3_FPGA_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/scripts/mps3/${TARGET_SUBSYSTEM}/images.txt")

    # add_custom_command(TARGET ${PARSED_TARGET_NAME}
    #         POST_BUILD
    #         COMMAND ${CMAKE_COMMAND} -E copy ${MPS3_FPGA_CONFIG} ${SECTORS_DIR})

endfunction()
