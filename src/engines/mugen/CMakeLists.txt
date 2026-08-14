# The engine code is a static library so the tests can link it; the plugin wraps it.
add_library(ofs_mugen_lib STATIC
    AirFile.h
    AirFile.cpp
    DefFile.h
    DefFile.cpp
    MugenActions.h
    MugenActions.cpp
    MugenControls.h
    MugenControls.cpp
    MugenEngine.h
    MugenEngine.cpp
    MugenMenus.h
    MugenMenus.cpp
    MugenOutline.h
    MugenOutline.cpp
    MugenProject.h
    MugenProject.cpp
    MugenRun.h
    MugenRun.cpp
    MugenVariables.h
    MugenVariables.cpp
    ThrowCreator.h
    ThrowCreator.cpp
    ProjectGraphics.h
    ProjectGraphics.cpp
    dialogs/AnimationPreview.h
    dialogs/AnimationPreview.cpp
    dialogs/OffsetViewerDialog.h
    dialogs/OffsetViewerDialog.cpp
    dialogs/ThrowCreatorDialog.h
    dialogs/ThrowCreatorDialog.cpp
    MugenSyntax.h
    MugenSyntax.cpp
    expr/Expression.h
    expr/Expression.cpp
)
add_library(OFS::mugen ALIAS ofs_mugen_lib)
ofs_target_defaults(ofs_mugen_lib)
target_link_libraries(ofs_mugen_lib PUBLIC OFS::core)

ofs_add_engine_plugin(mugen
    SOURCES
        mugen.json
        MugenPlugin.h
        MugenPlugin.cpp
    LIBS
        OFS::mugen
)
