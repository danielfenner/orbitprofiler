# OrbitDll
set(MSVC_INCREMENTAL_DEFAULT ON)
project(OrbitDll)

set(HEADERS OrbitDll.h)
set(SOURCES OrbitDll.cpp)

source_group("Header Files" FILES ${HEADERS})
source_group("Source Files" FILES ${SOURCES})

# Disable incremental linking as it hides the real address of functions.  
# This is annoying mainly for the OrbitProlog and OrbitEpilog related code.
if(WIN32)
    set( MSVC_INCREMENTAL_YES_FLAG "/INCREMENTAL:NO")
    set( ORBIT_DEBUG_LINKER_FLAGS "/debug /INCREMENTAL:NO" )
    
    # debug
    message("OrbitDll - Changing Debug linker flags from '${CMAKE_SHARED_LINKER_FLAGS_DEBUG}' to '${ORBIT_DEBUG_LINKER_FLAGS}'")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG ${ORBIT_DEBUG_LINKER_FLAGS} )

    # rewithdebinfo
    message("OrbitDll - Changing RelWithDebInfo linker flags from '${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}' to '${ORBIT_DEBUG_LINKER_FLAGS}'")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO ${ORBIT_DEBUG_LINKER_FLAGS} )
endif()

add_library(OrbitDll SHARED ${SOURCES} ${HEADERS})

set(MINHOOK_ROOT ${EXTERN_ROOT}/minhook)
target_include_directories(OrbitDll PRIVATE ${MINHOOK_ROOT}/include)

target_link_libraries(
    OrbitDll
    OrbitCore
	OrbitAsm
	${BREAKPAD_LIBRARIES}
	dbghelp.lib
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
set(ORBIT_DLL_NAME "Orbit64")
else()
set(ORBIT_DLL_NAME "Orbit32")
endif()

set_target_properties(OrbitDll PROPERTIES OUTPUT_NAME ${ORBIT_DLL_NAME})
