# The main window and app dialogs. Static so the GUI tests can link it without the executable.
add_library(ofsshell STATIC
    AdvancedSearchPanel.h
    AdvancedSearchPanel.cpp
    BottomDock.h
    BottomDock.cpp
    CodePage.h
    CodePage.cpp
    ControllerTemplatesWidget.h
    ControllerTemplatesWidget.cpp
    FileDialogs.h
    FileDialogs.cpp
    DocumentationPopup.h
    DocumentationPopup.cpp
    MainWindow.h
    MainWindow.cpp
    MenuBuilder.h
    MenuBuilder.cpp
    ProjectView.h
    ProjectView.cpp
    SpritePage.h
    SpritePage.cpp
    StatusBar.h
    StatusBar.cpp
    UpdateChecker.h
    UpdateChecker.cpp
    QuickFindBar.h
    QuickFindBar.cpp
    WelcomePage.h
    WelcomePage.cpp
    dialogs/AboutDialog.h
    dialogs/AboutDialog.cpp
    dialogs/CalculatorDialog.h
    dialogs/CalculatorDialog.cpp
    dialogs/ClipboardContainer.h
    dialogs/ClipboardContainer.cpp
    dialogs/FirstRunDialog.h
    dialogs/FirstRunDialog.cpp
    dialogs/ColorsPage.h
    dialogs/ColorsPage.cpp
    dialogs/MugenOptionsPage.h
    dialogs/MugenOptionsPage.cpp
    dialogs/OptionsDialog.h
    dialogs/OptionsDialog.cpp
    dialogs/MultiLineFindDialog.h
    dialogs/MultiLineFindDialog.cpp
    dialogs/SnippetEditorDialog.h
    dialogs/SnippetEditorDialog.cpp
    dialogs/SnippetFieldsDialog.h
    dialogs/SnippetFieldsDialog.cpp
    dialogs/StateControllerDialog.h
    dialogs/StateControllerDialog.cpp
    dialogs/VariableLogDialog.h
    dialogs/VariableLogDialog.cpp
    dialogs/RunChooserDialog.h
    dialogs/RunChooserDialog.cpp
    dialogs/RunConfigDialog.h
    dialogs/RunConfigDialog.cpp
    dialogs/ShortcutEditor.h
    dialogs/ShortcutEditor.cpp
)
add_library(OFS::shell ALIAS ofsshell)
ofs_target_defaults(ofsshell)
target_link_libraries(ofsshell PUBLIC OFS::core Qt6::Widgets Qt6::Network Qt6::Concurrent)

# The About dialog reads the GPL text and the icon from embedded copies so they work with any install layout.
qt_add_resources(ofsshell "ofsshell_about"
    PREFIX "/ofs"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/data/icons/ofs.png")
