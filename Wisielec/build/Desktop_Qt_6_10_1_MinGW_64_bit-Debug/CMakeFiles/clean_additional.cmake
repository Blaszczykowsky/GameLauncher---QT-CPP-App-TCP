# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\MultiGameLauncher_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\MultiGameLauncher_autogen.dir\\ParseCache.txt"
  "MultiGameLauncher_autogen"
  )
endif()
