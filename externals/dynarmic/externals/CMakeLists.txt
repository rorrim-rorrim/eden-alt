include(CPM)

# Always build externals as static libraries, even when dynarmic is built as shared
if (BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL ON)
endif()

# Allow options shadowing with normal variables when subproject use old cmake policy
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Disable tests in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# For libraries that already come with a CMakeLists file,
# simply add the directory to that file as a subdirectory
# to have CMake automatically recognize them.

# biscuit

if (NOT TARGET biscuit::biscuit)
    if ("riscv" IN_LIST ARCHITECTURE)
        add_subdirectory(biscuit)
    endif()
endif()

# catch

# TODO(crueter): dedup
if (NOT TARGET Catch2::Catch2WithMain)
    if (DYNARMIC_TESTS)
        add_subdirectory(catch EXCLUDE_FROM_ALL)
    endif()
endif()

# fmt

if (NOT TARGET fmt::fmt)
    # fmtlib formatting library
    set(FMT_INSTALL ON)
    add_subdirectory(fmt)
endif()

# mcl

if (NOT TARGET merry::mcl)
    set(MCL_INSTALL ON)
    add_subdirectory(mcl)
endif()

# oaknut

if (NOT TARGET merry::oaknut)
    if ("arm64" IN_LIST ARCHITECTURE)
        add_subdirectory(oaknut)
    elseif (DYNARMIC_TESTS)
        add_subdirectory(oaknut EXCLUDE_FROM_ALL)
    endif()
endif()

# unordered_dense

CPMAddPackage(
    NAME unordered_dense
    URL "https://github.com/Lizzie841/unordered_dense/archive/e59d30b7b1.zip"
    URL_HASH SHA512=71eff7bd9ba4b9226967bacd56a8ff000946f8813167cb5664bb01e96fb79e4e220684d824fe9c59c4d1cc98c606f13aff05b7940a1ed8ab3c95d6974ee34fa0
    FIND_PACKAGE_ARGUMENTS "CONFIG"
    OPTIONS
        "UNORDERED_DENSE_INSTALL ON"
)

# xbyak
# uncomment if in an independent repo

# if (NOT TARGET xbyak::xbyak)
#     if ("x86_64" IN_LIST ARCHITECTURE)
#         add_subdirectory(xbyak)
#     endif()
# endif()

# zydis

if ("x86_64" IN_LIST ARCHITECTURE)
    CPMAddPackage(
        NAME Zycore
        URL "https://github.com/zyantific/zycore-c/archive/75a36c45ae.zip"
        URL_HASH SHA512=15aa399f39713e042c4345bc3175c82f14dca849fde2a21d4f591f62c43e227b70d868d8bb86beb5f4eb68b1d6bd3792cdd638acf89009e787e3d10ee7401924
    )

    CPMAddPackage(
        NAME Zydis
        VERSION 4
        URL "https://github.com/zyantific/zydis/archive/c2d2bab025.zip"
        URL_HASH SHA512=7b48f213ff7aab2926f8c9c65195959143bebbfb2b9a25051ffd8b8b0f1baf1670d9739781de674577d955925f91ac89376e16b476a03828c84e2fd765d45020
        OPTIONS
            "ZYDIS_BUILD_TOOLS OFF"
            "ZYDIS_BUILD_EXAMPLES OFF"
            "ZYDIS_BUILD_DOXYGEN OFF"
            "ZYAN_ZYCORE_PATH ${Zycore_SOURCE_DIR}"
            "CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON"
    )
endif()
