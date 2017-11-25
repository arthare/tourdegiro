if(MSVC)
  set(PIPE_FILES PipeNet.cpp)
  set(PSAPI_LIB psapi.lib)
elseif(APPLE)
  set(PIPE_FILES PipeNet_OSX.cpp)
endif()



include_directories($ENV{TDG_HOME}/boost/ ${CMAKE_BINARY_DIR}/)


add_definitions(-DUNICODE)
add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-m32)

add_library (ArtLib ArtNet.h ArtNet.cpp ArtTools.h ArtTools.cpp AutoCS.h AutoCS.cpp BoostNet.cpp ${PIPE_FILES} ArtVector.h ArtVector.cpp md5.h md5.cpp stdafx.h stdafx.cpp)
Set32Bit(ArtLib)
ADD_MSVC_PRECOMPILED_HEADER(ArtLib stdafx.h stdafx.cpp)
target_link_libraries(ArtLib libcurl ${PSAPI_LIB})