
add_definitions(-DUNICODE)
add_definitions(-DUNICODE -D_UNICODE -DNO_NED_NAMESPACE)
include_directories(${CMAKE_SOURCE_DIR}/nedmalloc)

set (MINIXML_FILES mxml-attr.c mxml-entity.c mxml-file.c mxml-get.c mxml-index.c mxml-node.c mxml-private.c mxml-search.c mxml-set.c mxml-string.c)

add_library (minixml ${MINIXML_FILES})
Set32Bit(minixml)
target_link_libraries(minixml nedmalloc)