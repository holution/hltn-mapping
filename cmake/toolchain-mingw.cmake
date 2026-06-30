# MinGW-w64 toolchain file for use with system CMake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)

set(CMAKE_C_COMPILER C:/msys64/mingw64/bin/gcc.exe)
set(CMAKE_CXX_COMPILER C:/msys64/mingw64/bin/g++.exe)
set(CMAKE_RC_COMPILER C:/msys64/mingw64/bin/windres.exe)
set(CMAKE_MAKE_PROGRAM C:/msys64/mingw64/bin/mingw32-make.exe)

set(CMAKE_FIND_ROOT_PATH C:/msys64/mingw64)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ALWAYS)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ALWAYS)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ALWAYS)

set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
  C:/msys64/mingw64/include
  C:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/16.1.0/include)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
  C:/msys64/mingw64/include
  C:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/16.1.0/include/c++
  C:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/16.1.0/include/c++/x86_64-w64-mingw32
  C:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/16.1.0/include)
