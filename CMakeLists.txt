# SPDX-License-Identifier: BSD-2-Clause
cmake_minimum_required(VERSION 3.2)

#
# Project name and base version.
#
# Note: the version here is the one currently *under development*. For released
# versions, checkout GIT tags like "release-0.1.1".
#
project(tilck VERSION 0.1.4 LANGUAGES C CXX ASM)

include(CheckCCompilerFlag)
include(other/cmake/utils.cmake)
include(other/cmake/errors.cmake)

set(CMAKE_VERBOSE_MAKEFILE off)
set(BUILD_SHARED_LIBRARIES off)           # disable shared libs globally
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # disable -rdynamic globally
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)  # disable -rdynamic globally

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.12.0")
   set(GLOB_CONF_DEP CONFIGURE_DEPENDS)
endif()

set(DEFAULT_TARGET_ARCH "i386")
set(BUILD_ARCH "x86_64")

if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")

   set(HOST_ARCH "x86_64")
   set(HOST_ARCH_BITS 64)

elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "amd64")

   set(HOST_ARCH "x86_64")
   set(HOST_ARCH_BITS 64)

elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")

   set(HOST_ARCH "i386")
   set(HOST_ARCH_BITS 32)

elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i386")

   set(HOST_ARCH "i386")
   set(HOST_ARCH_BITS 32)

endif()

# BEGIN [Environment-controlled "sticky" cache variables]

# Main variables
define_env_cache_str_var(TCROOT_PARENT ${CMAKE_SOURCE_DIR})
define_env_cache_str_var(TCROOT ${TCROOT_PARENT}/toolchain2)
define_env_cache_str_var(ARCH ${DEFAULT_TARGET_ARCH})
define_env_cache_bool_var(TEST_GCOV)
define_env_cache_bool_var(KERNEL_GCOV)

# USE_SYSCC-related variables
define_env_cache_bool_var(USE_SYSCC)
define_env_cache_str_var(GCOV "gcov")

# Debug cache variables

# We don't really need to cache the values of CC/CXX, but is useful to that
# in order to prevent user errors. Since CC/CXX affect the build only the first
# time CMake is run, it makes sense to check for the next runs whether the env
# vars CC/CXX changed (if set) by the user or an incorrect script. In case
# they are, we have to stop the build because their new value will be otherwise
# completely ignored. In order words, fail *early*.

define_env_cache_str_var(CC "<unset>")
define_env_cache_str_var(CXX "<unset>")

# END [Environment-controlled "sticky" cache variables]


# BEGIN [Setup of toolchain-related variables]

set(BUILD_SCRIPTS       ${CMAKE_SOURCE_DIR}/scripts/build_scripts)
set(BUILD_APPS          ${CMAKE_BINARY_DIR}/scripts/build_apps)
set(BTC_SCRIPT          ${CMAKE_SOURCE_DIR}/scripts/build_toolchain)
set(COVERAGE_HTML_DIR   ${CMAKE_BINARY_DIR}/coverage_html)
set(TCROOT_BA           ${TCROOT}/host_${BUILD_ARCH})
set(LCOV_VER            "1.15")
file(RELATIVE_PATH      BTC_SCRIPT_REL ${CMAKE_SOURCE_DIR} ${BTC_SCRIPT})
file(RELATIVE_PATH      TCROOT_REL     ${CMAKE_SOURCE_DIR}     ${TCROOT})

if (NOT EXISTS ${TCROOT})
   message(FATAL_ERROR "Toolchain not found: run ${BTC_SCRIPT_REL} first.")
endif()

#
# Use as default value for GCC_TC_VER, the contents of:
#      ${TCROOT}/.gcc_tc_ver_${ARCH}
# That file has been generated by the `build_toolchain` script which used
# the GCC_TC_VER environment variable or the default value if it was unset.
#

if (EXISTS ${TCROOT}/.gcc_tc_ver)

   if (NOT "$ENV{CI}")
      # Small hack to support older toolchains (and upgrade them)
      # TODO: drop this once we upgrade to toolchain3
      file(RENAME ${TCROOT}/.gcc_tc_ver ${TCROOT}/.gcc_tc_ver_i386)
   endif()

endif()

if (EXISTS ${TCROOT}/.gcc_tc_ver_${ARCH})

   file(READ ${TCROOT}/.gcc_tc_ver_${ARCH} SAVED_GCC_TC_VAR)

else()

   if (NOT "$ENV{CI}")

      message(FATAL_ERROR
              "${TCROOT}/.gcc_tc_ver_${ARCH} not found: run ${BTC_SCRIPT_REL}")

   else()

      if (EXISTS ${TCROOT}/.gcc_tc_ver)
         # Support older pre-built toolchains in docker images used by CI
         # TODO: drop this once we upgrade to toolchain3
         file(READ ${TCROOT}/.gcc_tc_ver SAVED_GCC_TC_VAR)
      endif()

      # Small hack to make older container builds to work
      set(SAVED_GCC_TC_VAR "6.4.0")

   endif()

endif()

# Define our special "sticky" env. variable GCC_TC_VER
define_env_cache_str_var(GCC_TC_VER "${SAVED_GCC_TC_VAR}")

# END [Setup of toolchain-related variables]

if (USE_SYSCC)
   set(SYS_CC "${CMAKE_C_COMPILER}")
   set(SYS_CXX "${CMAKE_CXX_COMPILER}")
endif()

if (NOT "${HOST_ARCH}" STREQUAL "x86_64")
   if (NOT "${HOST_ARCH}" STREQUAL "${ARCH}")
      show_wrong_arch_error()
   else()
      if (USE_SYSCC)
         show_same_arch_build_warning()
      else()
         show_missing_use_syscc_error()
      endif()
   endif()
endif()

# Reset the default flags
set(CMAKE_C_FLAGS "")
set(CMAKE_C_FLAGS_INT "")
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_MINSIZEREL "")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "")
set(CMAKE_C_FLAGS_RELEASE "")

set(CMAKE_CXX_FLAGS "")
set(CMAKE_CXX_FLAGS_INT "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_MINSIZEREL "")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
set(CMAKE_CXX_FLAGS_RELEASE "")

# Compile options
set(ARCH_GTESTS         OFF CACHE BOOL   "Build unit tests for the target arch")

# Non-boolean kernel options
set(TIMER_HZ            250 CACHE STRING "System timer HZ")
set(USER_STACK_PAGES     16 CACHE STRING "User apps stack size in pages")
set(TTY_COUNT             2 CACHE STRING "Number of TTYs (default)")
set(MAX_HANDLES          16 CACHE STRING "Max handles/process (keep small)")

set(FBCON_BIGFONT_THR   160 CACHE STRING
    "Max term cols with 8x16 font. After that, a 16x32 font will be used")

# Other non-boolean options

set(FATPART_CLUSTER_SIZE  8 CACHE STRING
    "Cluster size (in sectors) of the FAT ramdisk")

set(TERM_SCROLL_LINES 5 CACHE STRING
    "Number of lines to scroll on Shift+PgUp/PgDown")

set(

   USERAPPS_CFLAGS
   "-ggdb -Os -Wall -Wno-unused-parameter"

   CACHE STRING "Build flags for C/C++ user programs"
)

# Kernel options (enabled by default)

set(KRN_TRACK_NESTED_INTERR ON CACHE BOOL
    "Track the nested interrupts")

set(PANIC_SHOW_STACKTRACE ON CACHE BOOL
    "Show the stacktrace in case of a kernel panic")

set(DEBUG_CHECKS ON CACHE BOOL
    "Keep checking ASSERTs and other debug checks even in release builds")

set(KERNEL_SELFTESTS ON CACHE BOOL
    "Compile-in self-tests in the kernel")

set(KERNEL_STACK_ISOLATION ON CACHE BOOL
    "Put the kernel stack in hi the vmem in isolated pages")

set(FB_CONSOLE_BANNER ON CACHE BOOL
    "Show a top banner when using fb_console")

set(FB_CONSOLE_CURSOR_BLINK ON CACHE BOOL
    "Support cursor blinking in the fb_console")

if ($ENV{TILCK_NO_LOGO})
   set(KERNEL_SHOW_LOGO OFF CACHE BOOL
      "Show Tilck's logo after boot")
else()
   set(KERNEL_SHOW_LOGO ON CACHE BOOL
      "Show Tilck's logo after boot")
endif()

set(KERNEL_SYMBOLS ON CACHE BOOL
    "Keep symbol tables loaded in the kernel for backtraces and self tests")

set(BOOTLOADER_LEGACY ON CACHE BOOL
    "Build the legacy bootloader")

set(BOOTLOADER_EFI ON CACHE BOOL
    "Build the EFI bootloader")

set(SERIAL_CON_IN_VIDEO_MODE ON CACHE BOOL
    "Always open a console on /dev/ttyS0 even in video mode (w/o -sercon)")

set(KRN_PRINTK_ON_CURR_TTY ON CACHE BOOL
    "Make printk() always flush on the current TTY")

set(TYPICAL_DEVEL_USERAPPS ON CACHE BOOL
    "Include in Tilck's ramdisk some apps for testing the kernel")

set(BOOT_INTERACTIVE ON CACHE BOOL
    "Have user-interactive commands in the bootloaders")

set(KERNEL_64BIT_OFFT ON CACHE BOOL
    "Make offt be 64-bit even on 32-bit systems")

set(KRN_CLOCK_DRIFT_COMP ON CACHE BOOL
    "Compensate periodically for the clock drift in the system time")

# Kernel options (disabled by default)

set(KRN_PAGE_FAULT_PRINTK OFF CACHE BOOL
    "Use printk() to display info when a process is killed due to page fault")

set(KRN_NO_SYS_WARN OFF CACHE BOOL
    "Show a warning when a not-implemented syscall is called")

set(KERNEL_UBSAN OFF CACHE BOOL "Turn on the UBSAN for the kernel")

set(KERNEL_BIG_IO_BUF OFF CACHE BOOL "Use a much-bigger buffer for I/O")

set(TERM_BIG_SCROLL_BUF OFF CACHE BOOL
    "Use a 4x bigger scrollback buffer for the terminal")

set(KERNEL_SYSCC OFF CACHE BOOL
    "Use system's compiler for the kernel instead of toolchain's one")

set(KERNEL_FORCE_TC_ISYSTEM OFF CACHE BOOL "\
DANGEROUS. Do NOT use for other than static analysis builds. \
BRUTALLY forces the build system to use tc's sysroot \
for system headers even when a system compiler is used.\
")

set(FORK_NO_COW OFF CACHE BOOL
    "Make fork() to perform a full-copy instead of using copy-on-write")

set(MMAP_NO_COW OFF CACHE BOOL
    "Make mmap() to allocate real memory instead mapping the zero-page + COW")

set(PANIC_SHOW_REGS OFF CACHE BOOL
    "Show the content of the main registers in case of kernel panic")

set(KMALLOC_HEAVY_STATS OFF CACHE BOOL
    "Count the number of allocations for each distinct size")

set(KMALLOC_FREE_MEM_POISONING OFF CACHE BOOL
    "Make kfree() to poison the memory")

set(KMALLOC_SUPPORT_DEBUG_LOG OFF CACHE BOOL
    "Compile-in kmalloc debug messages")

set(KMALLOC_SUPPORT_LEAK_DETECTOR OFF CACHE BOOL
    "Compile-in kmalloc's leak detector")

set(BOOTLOADER_POISON_MEMORY OFF CACHE BOOL
    "Make the bootloader to poison all the available memory")

set(WCONV OFF CACHE BOOL
    "Compile with -Wconversion when clang is used")

set(FAT_TEST_DIR OFF CACHE BOOL
    "Create a test dir with many files in the FAT32 init ramdisk")

set(PS2_DO_SELFTEST OFF CACHE BOOL
    "Do PS/2 controller selftests in init_kb()")

set(PS2_VERBOSE_DEBUG_LOG OFF CACHE BOOL
    "Enable developer-only verbose logging")

set(FB_CONSOLE_USE_ALT_FONTS OFF CACHE BOOL
    "Use the fonts in other/alt_fonts instead of the default ones")

set(KRN_RESCHED_ENABLE_PREEMPT OFF CACHE BOOL
    "Check for need_resched and yield in enable_preemption()")

set(TINY_KERNEL OFF CACHE BOOL "\
Advanced option, use carefully. Forces the Tilck kernel \
to be as small as possible. Incompatibile with many modules \
like console, fb, tracing and several kernel options like \
KERNEL_SELFTESTS")

set(INIT_REPORT_PROC_EXIT OFF CACHE BOOL
    "Make init to report on TTY when children (often acquired) exit")

set(PREFERRED_GFX_MODE_W "800" CACHE STRING
    "Preferred video mode (width)")

set(PREFERRED_GFX_MODE_H "600" CACHE STRING
    "Preferred video mode (height)")

set(KMALLOC_FIRST_HEAP_SIZE_KB "auto" CACHE STRING
    "Size in KB of kmalloc's first heap. Must be multiple of 64.")

set(PCI_VENDORS_LIST OFF CACHE BOOL
    "Compile-in the list of all known PCI vendors")

set(EFI_BOOTLOADER_DEBUG OFF CACHE BOOL
    "Enable an early DEBUG dialog (see docs/debugging.md)")

set(FB_CONSOLE_FAILSAFE_OPT OFF CACHE BOOL
    "Optimize fb_console's failsafe mode for older machines")

list(
   APPEND no_arch_modules_whitelist

   console
   tracing
   sysfs
)

list(
   APPEND tiny_kernel_modules_blacklist

   fb
   console
   tracing
   kb8042
   acpi
)

list(
   APPEND disabled_modules_list

   # No modules in this list, at the moment
)

file(GLOB modules ${GLOB_CONF_DEP} "${CMAKE_SOURCE_DIR}/modules/*")

foreach(modpath ${modules})

   get_filename_component(modname ${modpath} NAME_WE)

   if (NOT IS_DIRECTORY ${modpath})
      continue()
   endif()

   if (TINY_KERNEL)

      list(FIND tiny_kernel_modules_blacklist ${modname} _index)

      if (${_index} GREATER -1)
         if (MOD_${modname})
            message(WARNING "TINY_KERNEL=1, force-setting MOD_${modname}=0")
            set(MOD_${modname} OFF)
         endif()
      endif()
      unset(_index)

   endif(TINY_KERNEL)

   list(APPEND modules_list ${modname})

   list(FIND disabled_modules_list ${modname} _index)

   if (${_index} GREATER -1)
      set(MOD_${modname} OFF CACHE BOOL "Compile-in the ${modname} module")
   else()
      set(MOD_${modname} ON CACHE BOOL "Compile-in the ${modname} module")
   endif()

endforeach()

if (TINY_KERNEL)

   if (NOT ${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
      message(FATAL_ERROR "TINY_KERNEL=1 requires CMAKE_BUILD_TYPE=MinSizeRel")
   endif()

   if (KERNEL_GCOV)
      message(FATAL_ERROR "TINY_KERNEL=1 requires KERNEL_GCOV=0")
   endif()

   if (KERNEL_SELFTESTS)
      message(WARNING "TINY_KERNEL=1, force-setting KERNEL_SELFTESTS=0")
      set(KERNEL_SELFTESTS OFF)
   endif()

   if (KERNEL_SYMBOLS)
      message(WARNING "TINY_KERNEL=1, force-setting KERNEL_SYMBOLS=0")
      set(KERNEL_SYMBOLS OFF)
   endif()

   if (KERNEL_SHOW_LOGO)
      message(WARNING "TINY_KERNEL=1, force-setting KERNEL_SHOW_LOGO=0")
      set(KERNEL_SHOW_LOGO OFF)
   endif()

   if (PCI_VENDORS_LIST)
      message(WARNING "TINY_KERNEL=1, force-setting PCI_VENDORS_LIST=0")
      set(PCI_VENDORS_LIST OFF)
   endif()

endif()

if (KMALLOC_FIRST_HEAP_SIZE_KB STREQUAL "auto")

   if (TINY_KERNEL)
      set(KMALLOC_FIRST_HEAP_SIZE_KB_VAL 64)
   else()
      set(KMALLOC_FIRST_HEAP_SIZE_KB_VAL 128)
   endif()

else()
   set(KMALLOC_FIRST_HEAP_SIZE_KB_VAL ${KMALLOC_FIRST_HEAP_SIZE_KB})
endif()


# Create a list with all the kernel opts, in order to dump it later
# -------------------------------------------------------------------

list(
   APPEND kernel_opts_list

   # Various options
   ARCH
   USE_SYSCC
   ARCH_GTESTS

   # Non-boolean options
   TIMER_HZ
   USER_STACK_PAGES
   FATPART_CLUSTER_SIZE
   PREFERRED_GFX_MODE_W
   PREFERRED_GFX_MODE_H
   KMALLOC_FIRST_HEAP_SIZE_KB
   KMALLOC_FIRST_HEAP_SIZE_KB_VAL

   # Boolean options ENABLED by default
   KRN_TRACK_NESTED_INTERR
   PANIC_SHOW_STACKTRACE
   DEBUG_CHECKS
   KERNEL_SELFTESTS
   KERNEL_STACK_ISOLATION
   FB_CONSOLE_BANNER
   KRN_PRINTK_ON_CURR_TTY
   KERNEL_SHOW_LOGO
   KERNEL_SYMBOLS
   BOOTLOADER_LEGACY
   BOOTLOADER_EFI
   BOOT_INTERACTIVE
   KRN_NO_SYS_WARN
   KERNEL_64BIT_OFFT
   KRN_CLOCK_DRIFT_COMP

   # Boolean options DISABLED by default
   KERNEL_UBSAN
   KERNEL_BIG_IO_BUF
   KRN_RESCHED_ENABLE_PREEMPT
   TERM_BIG_SCROLL_BUF
   TEST_GCOV
   KERNEL_GCOV
   KERNEL_SYSCC
   KERNEL_FORCE_TC_ISYSTEM
   FORK_NO_COW
   MMAP_NO_COW
   PANIC_SHOW_REGS
   KMALLOC_HEAVY_STATS
   KMALLOC_FREE_MEM_POISONING
   KMALLOC_SUPPORT_DEBUG_LOG
   KMALLOC_SUPPORT_LEAK_DETECTOR
   BOOTLOADER_POISON_MEMORY
   WCONV
   FAT_TEST_DIR
   PS2_DO_SELFTEST
   PS2_VERBOSE_DEBUG_LOG
   FB_CONSOLE_USE_ALT_FONTS
   TINY_KERNEL
   INIT_REPORT_PROC_EXIT
   PCI_VENDORS_LIST
   EFI_BOOTLOADER_DEBUG
   FB_CONSOLE_FAILSAFE_OPT
)

###########################################################


if (${ARCH} STREQUAL "i386")

   set(ARCH_SHORT       "x86")
   set(ARCH_FAMILY      "generic_x86")
   set(ARCH_ELF_NAME    "elf32-i386")
   set(ARCH_LD_OUTPUT   "elf_i386")
   set(ARCH_BFD         "i386")
   set(ARCH_GCC_TC      "i686")
   set(ARCH_GCC_FLAGS   "-m32 -march=${ARCH_GCC_TC}")
   set(ARCH_BITS        32)

   # Fundamental kernel MM constants
   set(KERNEL_BASE_VA     0xC0000000)  # Better not touch!
   set(KERNEL_PADDR       0x00100000)  # Better not touch!
   set(LINEAR_MAPPING_MB         896)  # Better not touch!

elseif(${ARCH} STREQUAL "x86_64")

   set(ARCH_SHORT       "x86_64")
   set(ARCH_FAMILY      "generic_x86")
   set(ARCH_ELF_NAME    "elf64-x86-64")
   set(ARCH_LD_OUTPUT   "elf_x86_64")
   set(ARCH_BFD         "i386:x86-64")
   set(ARCH_GCC_TC      "x86_64")
   set(ARCH_GCC_FLAGS   "")
   set(ARCH_BITS        64)

   # Fundamental kernel MM constants
   set(KERNEL_BASE_VA      0x100000000000)  # +16 TB
   set(KERNEL_PADDR            0x00100000)  # Better not touch!
   set(LINEAR_MAPPING_MB             4096)  # Might be updated.

else()

   message(FATAL_ERROR "Architecture '${ARCH}' not supported.")
endif()

message(STATUS "TCROOT: ${TCROOT}")
message(STATUS "GCC_TC_VER: ${GCC_TC_VER}")

set(BUSYBOX_CONFIG            ${TCROOT}/${ARCH}/busybox/.config)
set(NCURSES_INST              ${TCROOT}/${ARCH}/ncurses/install)

string(REPLACE                "." "_" GCC_TC_VER_ "${GCC_TC_VER}")
set(GCC_NAME_PFX              gcc_${GCC_TC_VER_}_${ARCH})

# BEGIN [ARCH-dependent paths]

   # Main toolchain (musl)
   set(GCC_TC_ROOT            ${TCROOT_BA}/${GCC_NAME_PFX}_musl)
   set(GCC_TC_LIBC_DIRNAME    ${ARCH_GCC_TC}-buildroot-linux-musl)
   set(GCC_TC_SYSROOT         ${GCC_TC_ROOT}/${GCC_TC_LIBC_DIRNAME}/sysroot)
   set(GCC_TC_ISYSTEM         ${GCC_TC_SYSROOT}/usr/include)
   set(GCC_TOOLCHAIN          ${GCC_TC_ROOT}/bin)

   # Glibc toolchain
   set(GCC_TC_GLIBC_ROOT      ${TCROOT_BA}/${GCC_NAME_PFX}_glibc)
   set(GCC_TOOLCHAIN_GLIBC    ${GCC_TC_GLIBC_ROOT}/bin)

# END [ARCH-dependent paths]

if (USE_SYSCC)

   if (NOT EXISTS ${TCROOT}/${ARCH}/musl/install/bin/musl-gcc)
      show_no_musl_syscc_error()
   endif()

endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "SYS CC compiler name: ${CMAKE_C_COMPILER_ID}")
message(STATUS "SYS CXX compiler name: ${CMAKE_CXX_COMPILER_ID}")

# Include the main lists of compiler flags used in the project
include(other/cmake/compiler_flags.cmake)

# Include the funcs for building the kernel modules, used both by the kernel
# and the gtests targets.

include(other/cmake/wrapped_syms.cmake)
include(other/cmake/build_modules.cmake)

foreach (opt ${kernel_opts_list})
   message(STATUS "${opt} = ${${opt}}")
endforeach()

message(STATUS "****************** KERNEL MODULES *******************")

foreach (mod ${modules_list})
   message(STATUS "Module ${mod} = ${MOD_${mod}}")
endforeach()

message(STATUS "**************** END KERNEL MODULES *****************")

# Config validation checks
# -----------------------------------

if (NOT ${MOD_console} AND NOT ${MOD_serial})
   message(FATAL_ERROR "Cannot disable both the `console` and `serial` mods")
endif()

foreach (mod ${modules_list})

   set(deps_file "${CMAKE_SOURCE_DIR}/modules/${mod}/module_deps")

   if (NOT ${MOD_${mod}})
      continue() # Module not compiled-in: skip it.
   endif()

   list(APPEND enabled_modules_list ${mod})

   if (NOT EXISTS ${deps_file})
      continue() # No dependencies. Great!
   endif()

   # Read the dependencies in the `deps` list
   file(STRINGS ${deps_file} deps)

   foreach (d ${deps})
      if (NOT ${MOD_${d}})
         message(FATAL_ERROR "The module '${mod}' requires the module '${d}'")
      endif()
   endforeach()

   unset(deps)
   unset(deps_file)

endforeach()

JOIN("${enabled_modules_list}" ${SPACE} ENABLED_MODULES_LIST)
unset(enabled_modules_list)

################################################################################

set(BOOTLOADER_BIN_FILE    ${CMAKE_BINARY_DIR}/boot/legacy/bootloader.bin)

if ("${HOST_ARCH}" STREQUAL "x86_64")
   set(EFI_x86_64_FILE     ${CMAKE_BINARY_DIR}/boot/efi/BOOTX64.EFI)
endif()

set(EFI_ia32_FILE          ${CMAKE_BINARY_DIR}/boot/efi/BOOTIA32.EFI)
set(KERNEL_UNSTRIPPED      ${CMAKE_BINARY_DIR}/tilck_unstripped)
set(KERNEL_FILE            ${CMAKE_BINARY_DIR}/tilck)
set(IMG_FILE               ${CMAKE_BINARY_DIR}/tilck.img)
set(TILCK_DEVSHELL_PATH    "/initrd/usr/bin/devshell")

if (${BOOTLOADER_LEGACY} OR ${BOOTLOADER_EFI})
   set(KERNEL_BOOTPART_PATH    "tilck")
endif()

################################################################################
# Bootloaders, build-scripts and link-scripts config

set(SECTOR_SIZE              512)  # CHS Logic sector size.  Do *not* touch!
set(CHS_HPC                   16)  # CHS heads per cylinder. Do *not* touch!
set(CHS_SPT                   63)  # CHS sectors per track.  Do *not* touch!
set(BL_ST2_DATA_SEG       0x2000)  # Better not touch!
set(EARLY_BOOT_SZ           4096)  # Better not touch!
set(STAGE3_ENTRY_OFF        4096)  # Better not touch!
set(DISK_UUID         0x4906062b)  # UUID of Tilck's disk image ${IMG_FILE}
set(MAX_BOOTLOADER_BIN_SZ_KB  64)  # Max size of bootloader.bin (keep updated!)
set(IMG_SZ_MB                 36)  # Total size of tilck.img

if (KERNEL_UBSAN)
   set(INITRD_SZ_MB              30)  # Size of the initrd RAMDISK in MB
   set(BOOTPART_SZ_SEC        10240)  # Size of the boot partition in sectors
else()
   set(INITRD_SZ_MB              32)  # Size of the initrd RAMDISK in MB
   set(BOOTPART_SZ_SEC         6144)  # Size of the boot partition in sectors
endif()


math(EXPR IMG_SZ_SEC        "${IMG_SZ_MB} * 1024 * 1024 / ${SECTOR_SIZE}")
math(EXPR BOOT_SECTORS      "${MAX_BOOTLOADER_BIN_SZ_KB}*1024/${SECTOR_SIZE}")
math(EXPR BOOTPART_SEC      "${BOOT_SECTORS}")
math(EXPR BOOTPART_LAST_SEC "${BOOTPART_SEC} + ${BOOTPART_SZ_SEC} - 1")
math(EXPR INITRD_SECTOR     "${BOOTPART_LAST_SEC} + 1")
math(EXPR INITRD_SZ_SEC     "${INITRD_SZ_MB} * 1048576 / ${SECTOR_SIZE}")
math(EXPR INITRD_LAST_SEC   "${INITRD_SECTOR} + ${INITRD_SZ_SEC} - 1")

math(EXPR SEC_PER_CYL   "${CHS_HPC} * ${CHS_SPT}")
math(EXPR CHS_CYLS      "(${IMG_SZ_SEC} + ${SEC_PER_CYL} - 1) / ${SEC_PER_CYL}")
unset(SEC_PER_CYL)

# For qemu run scripts
set(QEMU_CHS_GEOM         "cyls=${CHS_CYLS},heads=${CHS_HPC},secs=${CHS_SPT}")
set(QEMU_RAM_OPT          "")  # use qemu's default: 128 MB
set(QEMU_COMMON_OPTS      "-rtc base=localtime")

execute_process(COMMAND uname -r OUTPUT_VARIABLE UNAME_R)
string(TOLOWER ${UNAME_R} UNAME_R)
string(FIND ${UNAME_R} "microsoft" _index)

if (${_index} EQUAL -1)
   set(QEMU_COMMON_OPTS "${QEMU_COMMON_OPTS} -device sb16")
endif()

# Set kernel's stack size in number of pages.
set(KERNEL_STACK_PAGES          2)

set(FATHACK ${BUILD_APPS}/fathack)

if (${ARCH_BITS} EQUAL 32)
   set(ELFHACK ${BUILD_APPS}/elfhack32)
else()
   set(ELFHACK ${BUILD_APPS}/elfhack64)
endif()

# Options for extra apps
include(other/cmake/extra_apps.cmake)

# Generate config files & build scripts (before including sub-dirs)
include(other/cmake/gen_config_pre.cmake)

# Include kernel's noarch sources
include(kernel/sources.cmake)

################################################################################
# Include sub-directories

if (BOOTLOADER_LEGACY)
   add_subdirectory(boot/legacy)
endif()

if (BOOTLOADER_EFI)
   add_subdirectory(boot/efi)
endif()

add_subdirectory(userapps)
add_subdirectory(kernel)
add_subdirectory(scripts/build_apps)

################################################################################

# Generate config files & build scripts (after including sub-dirs)
include(other/cmake/gen_config_post.cmake)

set(BUILD_SCRIPTS_FILES_LIST "")
file(GLOB files ${GLOB_CONF_DEP} "${BUILD_SCRIPTS}/*")
foreach (f ${files})
   list(APPEND BUILD_SCRIPTS_FILES_LIST "${file}")
endforeach()
unset(files)

set(dd_opts "status=none" "conv=notrunc")

set(
   mbr_img_deps

   # We MUST depend on BOTH each wrapper-target AND its OUTPUT in order to
   # our image file to be re-built when the *.bin files are re-built.

   kernel
   ${KERNEL_FILE}

   userapps
   ${USERAPPS_FILES_LIST}
   ${EXTRA_APPS_LIST}

   fathack
   mbrhack
   ${CMAKE_BINARY_DIR}/scripts/build_apps/fathack
   ${CMAKE_BINARY_DIR}/scripts/build_apps/mbrhack
   ${CMAKE_SOURCE_DIR}/sysroot/etc/start
   ${CMAKE_BINARY_DIR}/config_fatpart
   ${BUILD_SCRIPTS_FILES_LIST}
)

if (BOOTLOADER_LEGACY)
   list(
      APPEND mbr_img_deps

      bootloader
      ${BOOTLOADER_BIN_FILE}
   )
endif()

if (BOOTLOADER_EFI)

   list(
      APPEND mbr_img_deps

      efi_bootloader
      ${EFI_x86_64_FILE}
   )

   if (${ARCH} STREQUAL "i386")
      list(APPEND mbr_img_deps ${EFI_ia32_FILE})
   endif()

endif()

# [begin] Set some convenience variables
   set(MBRHACK ${BUILD_APPS}/mbrhack -q ${IMG_FILE})
   set(PARTED parted ${IMG_FILE} -s -a minimal)
   set(CREATE_EMPTY_IMG ${BUILD_SCRIPTS}/create_empty_img_if_necessary)

   # [begin] Setting one long variable, MBRHACK_BPB
      set(MBRHACK_BPB ${SECTOR_SIZE} ${CHS_HPC})
      set(MBRHACK_BPB ${MBRHACK_BPB} ${CHS_SPT} ${IMG_SZ_SEC} ${BOOT_SECTORS})
      set(MBRHACK_BPB ${MBRHACK_BPB} ${BOOT_SECTORS} ${DISK_UUID})
   # [end]
# [end]

if (BOOTLOADER_LEGACY)
   add_custom_command(
      OUTPUT
         ${IMG_FILE}
      COMMAND
         ${CREATE_EMPTY_IMG} ${IMG_FILE} ${IMG_SZ_MB}
      COMMAND
         dd ${dd_opts} if=${BOOTLOADER_BIN_FILE} of=${IMG_FILE}
      COMMAND
         ${MBRHACK} bpb ${MBRHACK_BPB}
      COMMAND
         ${MBRHACK} add 0x0e ${BOOTPART_SEC} +${BOOTPART_SZ_SEC}
      COMMAND
         ${MBRHACK} add 0x0e ${INITRD_SECTOR} +${INITRD_SZ_MB}M
      COMMAND
         ${BUILD_BOOTPART}
      COMMAND
         ${BUILD_FATPART}
      COMMAND
         ${FATHACK} --truncate fatpart
      COMMAND
         ${FATHACK} --align_first_data_sector fatpart
      COMMAND
         dd ${dd_opts} if=bootpart of=${IMG_FILE} seek=${BOOTPART_SEC}
      COMMAND
         dd ${dd_opts} if=fatpart of=${IMG_FILE} seek=${INITRD_SECTOR}
      DEPENDS
         ${mbr_img_deps}
      COMMENT
         "Creating a dual-bootable (legacy+efi) image file (MBR)"
      VERBATIM
   )
else()
   add_custom_command(
      OUTPUT
         ${IMG_FILE}
      COMMAND
         ${CREATE_EMPTY_IMG} ${IMG_FILE} ${IMG_SZ_MB}
      COMMAND
         ${PARTED} mklabel msdos
      COMMAND
         ${PARTED} mkpart primary FAT16 ${BOOTPART_SEC}s ${BOOTPART_LAST_SEC}s
      COMMAND
         ${PARTED} mkpart primary FAT16 ${INITRD_SECTOR}s ${INITRD_LAST_SEC}s
      COMMAND
         ${BUILD_BOOTPART}
      COMMAND
         ${BUILD_FATPART}
      COMMAND
         ${FATHACK} --truncate fatpart
      COMMAND
         ${FATHACK} --align_first_data_sector fatpart
      COMMAND
         dd ${dd_opts} if=bootpart of=${IMG_FILE} seek=${BOOTPART_SEC}
      COMMAND
         dd ${dd_opts} if=fatpart of=${IMG_FILE} seek=${INITRD_SECTOR}
      DEPENDS
         ${mbr_img_deps}
      COMMENT
         "Creating a EFI-only bootable image file (MBR)"
      VERBATIM
   )
endif()

# [begin] Unset the convenience variables
   unset(MBRHACK_BPB)
   unset(CREATE_EMPTY_IMG)
   unset(PARTED)
   unset(MBRHACK)
# [end]

add_custom_target(

    tilck_image ALL

    DEPENDS
        ${IMG_FILE}
)

set(GTEST_VER "1.12.1")
set(GTEST_SOURCE_DIR ${TCROOT}/noarch/gtest-${GTEST_VER}/googletest)
set(GMOCK_SOURCE_DIR ${TCROOT}/noarch/gtest-${GTEST_VER}/googlemock)

if (ARCH_GTESTS)
   set(GTEST_TC_BUILD_DIR ${TCROOT}/host_${ARCH}/gtest-${GTEST_VER})
   set(GMOCK_TC_BUILD_DIR ${TCROOT}/host_${ARCH}/gmock-${GTEST_VER})
   set(GTEST_BTC_COMMAND "build_gtest_arch build_gmock_arch")
else()
   set(GTEST_TC_BUILD_DIR ${TCROOT}/host_${HOST_ARCH}/gtest-${GTEST_VER})
   set(GMOCK_TC_BUILD_DIR ${TCROOT}/host_${HOST_ARCH}/gmock-${GTEST_VER})
   set(GTEST_BTC_COMMAND "build_gtest build_gmock")
endif()

add_custom_command(

   OUTPUT
      test_fatpart

   COMMAND
      ${BUILD_TEST_FATPART}

   COMMAND
      ${BUILD_APPS}/fathack --truncate test_fatpart

   DEPENDS
      fathack
      ${CMAKE_BINARY_DIR}/scripts/build_apps/fathack
)

add_custom_target(

   test_fatpart_target

   DEPENDS
      test_fatpart
)

if (EXISTS ${GTEST_TC_BUILD_DIR} AND EXISTS ${GMOCK_TC_BUILD_DIR})
   add_subdirectory(tests/unit)
   add_dependencies(gtests test_fatpart_target)
else()
   no_googletest_lib_fake_error_target()
endif()

