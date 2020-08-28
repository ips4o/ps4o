if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()

# # Set a default build type if none was specified
# set(default_build_type "Release")
 
# if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
#   message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
#   # set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
#   #     STRING "Choose the type of build." FORCE)
#   # set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
#   #   "Debug" "Release")
# endif()
