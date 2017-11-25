
add_definitions(-DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -DNO_NED_NAMESPACE)

include_directories()
                    
LINK_DIRECTORIES()
            
                  
add_library (nedmalloc nedmalloc.c)
Set32Bit(nedmalloc)

SET_SOURCE_FILES_PROPERTIES( nedmalloc.c PROPERTIES LANGUAGE CXX )