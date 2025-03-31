include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

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
)


include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
