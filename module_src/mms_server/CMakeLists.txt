cmake_minimum_required(VERSION 2.8.12)

# automagically detect if we should cross-compile
if(DEFINED ENV{TOOLCHAIN})
    set(CMAKE_C_COMPILER	$ENV{TOOLCHAIN}gcc)
    set(CMAKE_CXX_COMPILER	$ENV{TOOLCHAIN}g++)
    set(CMAKE_AR	"$ENV{TOOLCHAIN}ar" CACHE FILEPATH "CW archiver" FORCE)
endif()

project(module_template)

ENABLE_TESTING()

set(PROJECT_VERSION_MAJOR "0")
set(PROJECT_VERSION_MINOR "9")
set(PROJECT_VERSION_PATCH "0")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

  
# feature checks
include(CheckLibraryExists)
check_library_exists(rt clock_gettime "time.h" CONFIG_SYSTEM_HAS_CLOCK_GETTIME)

# check if we are on a little or a big endian
include (TestBigEndian)
test_big_endian(PLATFORM_IS_BIGENDIAN)

#include(GNUInstallDirs)

include_directories(
)


#SET(CMAKE_EXE_LINKER_FLAGS "-static")   

set(module_template_SRCS
   cfg_parse.c
   mem_file.c
   main.c
)



add_library(module_template SHARED
  ${module_template_SRCS}
)

set_target_properties(module_template PROPERTIES VERSION ${PROJECT_VERSION})
#set_target_properties(module_template PROPERTIES PUBLIC_HEADER ../../module_interface.h)


#add_executable( module_template
#  ${module_template_SRCS}
#)

target_link_libraries (module_template
)

install(TARGETS module_template
    LIBRARY DESTINATION lib ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include ${CMAKE_INSTALL_INCLUDEDIR})
