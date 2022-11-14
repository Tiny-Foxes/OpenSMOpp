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

target_include_directories(${PROJECT_NAME} PUBLIC
	Libs/mINI/src/
	Libs/socket-cpp/Socket/
	Libs/SQLiteCpp/include
)
