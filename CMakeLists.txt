cmake_minimum_required(VERSION 3.16)
project(hltn-mapping VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Paths ---
set(OBS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../obs-studio")
set(OBS_BIN_DIR "C:/Program Files/obs-studio/bin/64bit")

# --- Headers ---
set(LIBOBS_INCLUDE_DIR "${OBS_ROOT}/libobs")
set(OBS_GRAPHICS_INCLUDE_DIR "${OBS_ROOT}/libobs/graphics")

# --- Create import library from DLL for MinGW ---
set(OBS_DLL "${OBS_BIN_DIR}/obs.dll")
set(OBS_IMPLIB "${CMAKE_BINARY_DIR}/libobs.dll.a")

add_custom_command(OUTPUT ${OBS_IMPLIB}
  COMMAND ${CMAKE_COMMAND} -E echo "Generating import lib for obs.dll..."
  COMMAND C:/msys64/mingw64/bin/gendef - "${OBS_DLL}" > "${CMAKE_BINARY_DIR}/obs.def" 2>NUL
  COMMAND C:/msys64/mingw64/bin/dlltool -d "${CMAKE_BINARY_DIR}/obs.def" -l "${OBS_IMPLIB}" -D "${OBS_DLL}"
  DEPENDS ${OBS_DLL}
  COMMENT "Creating import library for obs.dll"
  BYPRODUCTS "${CMAKE_BINARY_DIR}/obs.def")

add_custom_target(obs_implib DEPENDS ${OBS_IMPLIB})

# --- Imported target ---
add_library(obs SHARED IMPORTED)
set_target_properties(obs PROPERTIES
  IMPORTED_LOCATION "${OBS_DLL}"
  IMPORTED_IMPLIB "${OBS_IMPLIB}"
  INTERFACE_INCLUDE_DIRECTORIES
    "${LIBOBS_INCLUDE_DIR};${OBS_GRAPHICS_INCLUDE_DIR}")

# --- Sources ---
set(SOURCES
  src/plugin.cpp
  src/mesh-filter.cpp
  src/mesh.cpp
  src/editor.cpp)

set(HEADERS
  src/mesh-filter.h
  src/mesh.h
  src/editor.h)

# --- Plugin target ---
add_library(hltn-mapping MODULE ${SOURCES} ${HEADERS})
target_include_directories(hltn-mapping PRIVATE src)
target_link_libraries(hltn-mapping PRIVATE obs)

add_dependencies(hltn-mapping obs_implib)

if(WIN32)
  set_target_properties(hltn-mapping PROPERTIES PREFIX "")
endif()

# --- Deploy ---
set(OBS_PLUGIN_DIR "$ENV{ProgramData}/obs-studio/plugins/hltn-mapping/bin/64bit")

install(TARGETS hltn-mapping
  RUNTIME DESTINATION ${OBS_PLUGIN_DIR}
  LIBRARY DESTINATION ${OBS_PLUGIN_DIR})

install(FILES data/locale/en-US.ini
  DESTINATION "$ENV{APPDATA}/obs-studio/plugins/hltn-mapping/data/locale")

set(MINGW_BIN "C:/msys64/mingw64/bin")
set(MINGW_RUNTIME_DLLS
  ${MINGW_BIN}/libgcc_s_seh-1.dll
  ${MINGW_BIN}/libstdc++-6.dll
  ${MINGW_BIN}/libwinpthread-1.dll)

add_custom_target(deploy
  COMMAND ${CMAKE_COMMAND} -E make_directory "${OBS_PLUGIN_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:hltn-mapping>" "${OBS_PLUGIN_DIR}/hltn-mapping.dll"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_RUNTIME_DLLS} "${OBS_PLUGIN_DIR}/"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/data/locale/en-US.ini"
    "$ENV{ProgramData}/obs-studio/plugins/hltn-mapping/data/locale/en-US.ini"
  COMMENT "Deploy to ${OBS_PLUGIN_DIR}")
