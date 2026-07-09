# The main window and app dialogs. Static so the GUI tests can link it without the executable.
add_library(ofsshell STATIC
    BottomDock.h
    BottomDock.cpp
    MainWindow.h
    MainWindow.cpp
    MenuBuilder.h
    MenuBuilder.cpp
    ProjectView.h
    ProjectView.cpp
    StatusBar.h
    StatusBar.cpp
    WelcomePage.h
    WelcomePage.cpp
)
add_library(OFS::shell ALIAS ofsshell)
ofs_target_defaults(ofsshell)
target_link_libraries(ofsshell PUBLIC OFS::core Qt6::Widgets)
