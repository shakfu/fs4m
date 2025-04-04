include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)

if(BUILD_STATIC)
	set(local_prefix ${CMAKE_BINARY_DIR}/thirdparty/install)
endif()

message(STATUS "local_prefix: ${local_prefix}")

include_directories( 
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
)

file(GLOB PROJECT_SRC
     "*.h"
	"*.c"
     "*.cpp"
)

add_library(
	${PROJECT_NAME} 
	MODULE
	${PROJECT_SRC}
)

target_include_directories(
	${PROJECT_NAME} 
	PUBLIC
	${local_prefix}/include
	$<$<BOOL:${BUILD_STATIC}>:${homebrew_prefix}/include>
)

target_link_directories(
	${PROJECT_NAME} 
	PUBLIC
	${local_prefix}/lib
)

target_link_libraries(
	${PROJECT_NAME} 
	PUBLIC
	-lfluidsynth
   	"$<$<BOOL:${BUILD_STATIC}>:-framework Foundation>"
   	"$<$<BOOL:${BUILD_STATIC}>:-framework CoreFoundation>"
   	"$<$<BOOL:${BUILD_STATIC}>:-framework CoreAudio>"
   	"$<$<BOOL:${BUILD_STATIC}>:-framework CoreMIDI>"
   	"$<$<BOOL:${BUILD_STATIC}>:-framework AudioUnit>"
   	$<$<BOOL:${BUILD_STATIC}>:${homebrew_prefix}/lib/libglib-2.0.a>
   	$<$<BOOL:${BUILD_STATIC}>:${homebrew_prefix}/lib/libgthread-2.0.a>
   	$<$<BOOL:${BUILD_STATIC}>:${homebrew_prefix}/lib/libintl.a>
   	$<$<BOOL:${BUILD_STATIC}>:-liconv>
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
