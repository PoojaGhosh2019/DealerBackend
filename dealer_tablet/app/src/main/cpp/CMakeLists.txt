# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        dealer_backend

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
        dealer_backend.cpp dealer_tablet_backend.cpp)

#include_directories(${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/inc)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib
              # Specifies the name of the NDnative-libK library that
              # you want CMake to locate.
              log )

#add_library( gre SHARED IMPORTED )
#set_target_properties( gre PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libgre.so)

#add_library( greio SHARED IMPORTED )
#set_target_properties( greio PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libgreio.so)

#add_library( greio-tcp SHARED IMPORTED )
#set_target_properties( greio-tcp PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libgreio-tcp.so)

# Specifies libraries CMake should link to your target library. You
# can link multiple librariesgreio-tcp, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        dealer_backend
        ${log-lib}
)