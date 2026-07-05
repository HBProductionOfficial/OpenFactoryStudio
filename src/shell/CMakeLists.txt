# The main window and app dialogs. Static so the GUI tests can link it without the executable.
add_library(ofsshell STATIC
    MainWindow.h
    MainWindow.cpp
)
add_library(OFS::shell ALIAS ofsshell)
ofs_target_defaults(ofsshell)
target_link_libraries(ofsshell PUBLIC OFS::core Qt6::Widgets)
