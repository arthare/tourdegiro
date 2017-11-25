# Install script for directory: H:/gitroot/tdg-world

# Set the install prefix
IF(NOT DEFINED CMAKE_INSTALL_PREFIX)
  SET(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/TDG")
ENDIF(NOT DEFINED CMAKE_INSTALL_PREFIX)
STRING(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
IF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  IF(BUILD_TYPE)
    STRING(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  ELSE(BUILD_TYPE)
    SET(CMAKE_INSTALL_CONFIG_NAME "Release")
  ENDIF(BUILD_TYPE)
  MESSAGE(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
ENDIF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)

# Set the component getting installed.
IF(NOT CMAKE_INSTALL_COMPONENT)
  IF(COMPONENT)
    MESSAGE(STATUS "Install component: \"${COMPONENT}\"")
    SET(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  ELSE(COMPONENT)
    SET(CMAKE_INSTALL_COMPONENT)
  ENDIF(COMPONENT)
ENDIF(NOT CMAKE_INSTALL_COMPONENT)

IF(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  INCLUDE("H:/gitroot/tdg-world/ArtLib/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/GameAI/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/nedmalloc/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/TourDeGiroCommon/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/ServerLib/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/GameLib/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/Minixml/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/libcurl/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/TourDeGiroData/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/MinimalTdG/cmake_install.cmake")
  INCLUDE("H:/gitroot/tdg-world/zlib-1.2.8/cmake_install.cmake")

ENDIF(NOT CMAKE_INSTALL_LOCAL_ONLY)

IF(CMAKE_INSTALL_COMPONENT)
  SET(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
ELSE(CMAKE_INSTALL_COMPONENT)
  SET(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
ENDIF(CMAKE_INSTALL_COMPONENT)

FILE(WRITE "H:/gitroot/tdg-world/${CMAKE_INSTALL_MANIFEST}" "")
FOREACH(file ${CMAKE_INSTALL_MANIFEST_FILES})
  FILE(APPEND "H:/gitroot/tdg-world/${CMAKE_INSTALL_MANIFEST}" "${file}\n")
ENDFOREACH(file)
