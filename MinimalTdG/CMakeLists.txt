
include_directories(${CMAKE_SOURCE_DIR }/TourDeGiroData
                    ${CMAKE_SOURCE_DIR }/ArtLib
                    ${CMAKE_SOURCE_DIR }/GameLib)

link_directories(${CMAKE_SOURCE_DIR}/boost/lib)
                  
add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-DCURL_STATICLIB)
add_executable (MinimalTdG  main.cpp stdafx.cpp)

target_link_libraries(MinimalTdG 
                      ArtLib
                      GameLib
                      ServerLib
                      GameAI
                      TourDeGiroCommon
                      minixml
                      ${BOOST_LIBS})