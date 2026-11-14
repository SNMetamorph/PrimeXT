# Overlay of the vcpkg builtin triplet of the same name.
#
# The only change is VCPKG_CMAKE_SYSTEM_VERSION: the builtin triplet uses API 28, but the app
# is linked against API 21 (minSdkVersion in android/app/build.gradle, raised to the NDK minimum).
# Dependencies built against API 28 headers reference libc symbols (stderr, stdout, __fwrite_chk, ...)
# that don't exist in the API 21 libc and fail to link into libclient.so.
# See tests/vcpkg_ports/check_android_triplets.cmake, keep this in sync with build.gradle.
set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 21)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=armv7a-linux-androideabi")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=armeabi-v7a -DANDROID_ARM_NEON=ON)
