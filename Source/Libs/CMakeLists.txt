set(SOCKET_CPP_BUILD_WITHOUT_SECURE_CLASSES 1)
add_subdirectory(socket-cpp)

add_subdirectory(SQLiteCpp)

set_property(TARGET socket PROPERTY FOLDER Libraries)
set_property(TARGET SQLiteCpp PROPERTY FOLDER Libraries)
set_property(TARGET sqlite3 PROPERTY FOLDER Libraries)
set_property(TARGET SQLiteCpp_cpplint PROPERTY FOLDER Libraries)
