
include_directories($ENV{TDG_HOME}/ArtLib/)
add_definitions(-DUNICODE)
add_definitions(-DUNICODE -D_UNICODE)
add_library (TourDeGiroCommon ArtSimpleSpline.cpp FileExporters.h FileExporters.cpp CommStructs.cpp CommStructs.h Map.cpp Map.h Player.cpp Player.h PlayerConsts.cpp PlayerTests.cpp SprintClimbImpl.cpp SprintClimbImpl.h StatsStore.cpp StatsStore.h stdafx.cpp stdafx.h Tools.cpp Tools.h)
Set32Bit(TourDeGiroCommon)
ADD_MSVC_PRECOMPILED_HEADER(TourDeGiroCommon stdafx.h stdafx.cpp)
target_link_libraries(TourDeGiroCommon 
                      ArtLib)