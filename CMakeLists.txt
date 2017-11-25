# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TDG) 

MACRO(ADD_MSVC_PRECOMPILED_HEADER ProjectName PrecompiledHeader PrecompiledSource)
  if (MSVC)
     set_target_properties(${ProjectName} PROPERTIES COMPILE_FLAGS \"/Yu\"${PrecompiledHeader})
     set_source_files_properties(${PrecompiledSource} PROPERTIES COMPILE_FLAGS \"/Yc\"${PrecompiledHeader})
  endif(MSVC)       
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)
MACRO(Set32Bit ProjectName)
  if(APPLE)
    get_target_property(TEMP ${ProjectName} LINK_FLAGS)
    if(TEMP STREQUAL "TEMP-NOTFOUND")
      SET(TEMP "") # set to empty string
    else()
      SET(TEMP "${TEMP} ") # a space to cleanly separate from existing content
    endif()
    # append our values
    SET(TEMP "${TEMP} -m32 -stdlib=libc++" )
    set_target_properties(${ProjectName} PROPERTIES LINK_FLAGS ${TEMP} )


    get_target_property(TEMP ${ProjectName} COMPILE_FLAGS)
    if(TEMP STREQUAL "TEMP-NOTFOUND")
      SET(TEMP "") # set to empty string
    else()
      SET(TEMP "${TEMP} ") # a space to cleanly separate from existing content
    endif()
    # append our values
    SET(TEMP "${TEMP} -m32 -stdlib=libc++" )
    set_target_properties(${ProjectName} PROPERTIES COMPILE_FLAGS ${TEMP} )
  endif()  
ENDMACRO(Set32Bit)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build-runtime-output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build-archive-output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build-library-output)

if(MSVC)
  add_definitions(-DBOOST_DISABLE_ASSERTS)
  if (CMAKE_BUILD_TYPE EQUAL "Debug")
    set(BOOST_LIBS libboost_thread-vc100-mt-gd-1_49.lib libboost_filesystem-vc100-mt-gd-1_49.lib libboost_system-vc100-mt-gd-1_49.lib)
  else()
    set(BOOST_LIBS libboost_thread-vc100-mt-1_49.lib libboost_filesystem-vc100-mt-1_49.lib libboost_system-vc100-mt-1_49.lib)
  endif()
elseif(APPLE)
  set(BOOST_LIBS c++ libboost_system.a libboost_filesystem.a libboost_thread.a)
  add_definitions(-DBOOST_DISABLE_ASSERTS)
  add_definitions(-DCMAKE_OSX_ARCHITECTURES=386)
  add_definitions(-m32)

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -v" CACHE STRING "c++ flags")
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -m32" CACHE STRING "c flags")
else()
  if (CMAKE_BUILD_TYPE EQUAL "Debug")
    set(BOOST_LIBS libboost_thread-mgw46-mt-d-1_49.a libboost_filesystem-mgw46-mt-d-1_49.a libboost_system-mgw46-mt-d-1_49.a)
  else()
    set(BOOST_LIBS libboost_thread-mgw46-mt-1_49.a libboost_filesystem-mgw46-mt-1_49.a libboost_system-mgw46-mt-1_49.a)
  endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/boost 
                    ${CMAKE_SOURCE_DIR}/ArtLib 
                    ${CMAKE_SOURCE_DIR}/TourDeGiroCommon 
                    ${CMAKE_SOURCE_DIR}/TourDeGiroData 
                    ${CMAKE_SOURCE_DIR}/ServerLib 
                    ${CMAKE_SOURCE_DIR}/GameLib)
link_directories(${CMAKE_SOURCE_DIR}/boost/lib)

if(MSVC)
  add_definitions( "/W0 /D_CRT_SECURE_NO_WARNINGS /wd4005 /wd4996 /nologo /MP /Zm180" )
else()
  #add_definitions(-DUNICODE)
  add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501)
  add_definitions("-DBOOST_THREAD_USE_LIB")
endif()

add_subdirectory (ArtLib)
add_subdirectory (GameAI)

add_subdirectory (nedmalloc)
add_subdirectory (TourDeGiroCommon) 
add_subdirectory (ServerLib) 
add_subdirectory (GameLib) 
add_subdirectory (minixml) 
add_subdirectory (libcurl) 
add_subdirectory (TourDeGiroData) 
add_subdirectory (MinimalTdG)
add_subdirectory (zlib-1.2.8)


