# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\GraKosci_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\GraKosci_autogen.dir\\ParseCache.txt"
  "GraKosci_autogen"
  )
endif()
