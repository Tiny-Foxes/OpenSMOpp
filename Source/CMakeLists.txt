add_subdirectory(Libs)

set(Source
Main.cpp
Main.hpp
)

add_executable(${PROJECT_NAME} ${Source})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME} 
	socket
	SQLiteCpp
)

if (MINGW)
	target_link_libraries(${PROJECT_NAME} ws2_32)
	target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
elseif(UNIX AND NOT APPLE)
	target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
	Libs/mINI/src/
	Libs/socket-cpp/Socket/
	Libs/SQLiteCpp/include
)

if(WIN32)
	set(OS_NAME "Windows")
elseif(APPLE)
	set(OS_NAME "MacOS")
elseif(UNIX)
	set(OS_NAME "Linux")
else()
	set(OS_NAME "Unknown")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCH_NAME)
message("Build For System Arch: ${ARCH_NAME}")
if(ARCH_NAME MATCHES "^(x64|x86_64|amd64|win64)$")
	set(ARCH_NAME "X86_64")
elseif(ARCH_NAME MATCHES "^(x86|i686|i386|win32)$")
	set(ARCH_NAME "X86_32")
elseif(ARCH_NAME MATCHES "^(aarch64|arm64)$")
	set(ARCH_NAME "aarch64")
elseif(ARCH_NAME MATCHES "^arm")
	set(ARCH_NAME "armhf")
else()
	set(TARGET_ARCH "Unknown")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Releases"
		OUTPUT_NAME "${PROJECT_NAME}_${OS_NAME}_${ARCH_NAME}"
)