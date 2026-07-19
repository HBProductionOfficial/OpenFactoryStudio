add_executable(ofs-sffdump sffdump/main.cpp)
ofs_target_defaults(ofs-sffdump)
target_link_libraries(ofs-sffdump PRIVATE OFS::formats)
