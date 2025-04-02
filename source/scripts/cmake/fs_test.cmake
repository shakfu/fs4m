
string(REGEX REPLACE "(.*)/" "" THIS_FOLDER_NAME "${CMAKE_CURRENT_SOURCE_DIR}")
string(REPLACE "~" "_tilde" THIS_FOLDER_NAME "${THIS_FOLDER_NAME}")
project(${THIS_FOLDER_NAME})

message(STATUS "local_prefix: ${local_prefix}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})

file(GLOB PROJECT_SRC
    "*.h"
	"*.c"
    "*.cpp"
)

add_executable(
	${PROJECT_NAME}
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
