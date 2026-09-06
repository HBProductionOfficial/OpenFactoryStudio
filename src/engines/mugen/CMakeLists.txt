# The engine code is a static library so the tests can link it; the plugin wraps it.
add_library(ofs_mugen_lib STATIC
    AirFile.h
    AirFile.cpp
    AirEdits.h
    AirEdits.cpp
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
    dialogs/AddAnimationDialog.h
    dialogs/AddAnimationDialog.cpp
    dialogs/AnimationPreview.h
    dialogs/AnimationPreview.cpp
    dialogs/OffsetViewerDialog.h
    dialogs/OffsetViewerDialog.cpp
    dialogs/PalFXDialog.h
    dialogs/PalFXDialog.cpp
    dialogs/ThrowCreatorDialog.h
    dialogs/ThrowCreatorDialog.cpp
    dialogs/ExportHtmlDialog.h
    dialogs/ExportHtmlDialog.cpp
    MugenHtmlExport.h
    MugenHtmlExport.cpp
    MugenSyntax.h
    MugenSyntax.cpp
    expr/Expression.h
    expr/Expression.cpp
)
add_library(OFS::mugen ALIAS ofs_mugen_lib)
ofs_target_defaults(ofs_mugen_lib)
target_link_libraries(ofs_mugen_lib PUBLIC OFS::core)

# The emulator runtime has no widgets, so it runs headless and in the tests.
add_library(ofs_mugen_runtime STATIC
    runtime/Value.h
    runtime/Expr.h
    runtime/Expr.cpp
    runtime/IniText.h
    runtime/IniText.cpp
    runtime/CharacterData.h
    runtime/CharacterData.cpp
    runtime/AnimPlayer.h
    runtime/AnimPlayer.cpp
    runtime/Character.h
    runtime/Character.cpp
    runtime/CharacterControllers.cpp
    runtime/CharacterDetail.h
    runtime/CharacterEffects.cpp
    runtime/CharacterExplods.cpp
    runtime/CharacterHelpers.cpp
    runtime/CharacterHits.cpp
    runtime/CharacterTriggers.cpp
    runtime/Match.cpp
    runtime/Commands.h
    runtime/Commands.cpp
    runtime/Effects.h
    runtime/Explod.h
    runtime/Hits.h
)
add_library(OFS::mugen_runtime ALIAS ofs_mugen_runtime)
ofs_target_defaults(ofs_mugen_runtime)
target_link_libraries(ofs_mugen_runtime PUBLIC Qt6::Core OFS::core)

ofs_add_engine_plugin(mugen
    SOURCES
        mugen.json
        MugenPlugin.h
        MugenPlugin.cpp
    LIBS
        OFS::mugen
)
