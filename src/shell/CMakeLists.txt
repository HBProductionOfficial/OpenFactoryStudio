# The main window and app dialogs. Static so the GUI tests can link it without the executable.
add_library(ofsshell STATIC
    MainWindow.h
    MainWindow.cpp
    dialogs/AboutDialog.h
    dialogs/AboutDialog.cpp
    dialogs/CalculatorDialog.h
    dialogs/CalculatorDialog.cpp
    dialogs/ClipboardContainer.h
    dialogs/ClipboardContainer.cpp
    dialogs/ColorsPage.h
    dialogs/ColorsPage.cpp
    dialogs/MugenOptionsPage.h
    dialogs/MugenOptionsPage.cpp
    dialogs/OptionsDialog.h
    dialogs/OptionsDialog.cpp
    dialogs/RunConfigDialog.h
    dialogs/RunConfigDialog.cpp
    dialogs/ShortcutEditor.h
    dialogs/ShortcutEditor.cpp
)
add_library(OFS::shell ALIAS ofsshell)
ofs_target_defaults(ofsshell)
target_link_libraries(ofsshell PUBLIC OFS::core Qt6::Widgets)

# The About dialog reads the GPL text and the icon from embedded copies so they work with any install layout.
qt_add_resources(ofsshell "ofsshell_about"
    PREFIX "/ofs"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/data/icons/ofs.png")
