# vcpkg from VCPKG_ROOT, or else the checkout the bootstrap scripts make in .vcpkg/, so IDEs need no environment.
if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../.vcpkg/scripts/buildsystems/vcpkg.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/../.vcpkg/scripts/buildsystems/vcpkg.cmake")
else()
    message(FATAL_ERROR "vcpkg not found in VCPKG_ROOT or .vcpkg/; run the bootstrap script for this platform")
endif()
