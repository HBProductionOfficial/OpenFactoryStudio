#include "core/actions/MenuSpec.h"

#include <QCoreApplication>

namespace OFS {

namespace {

using I = MenuItemSpec;

QString tr(const char* text)
{
    return QCoreApplication::translate("Menus", text);
}

} // namespace

QList<MenuSpec> applicationMenusBeforeEngine()
{
    MenuSpec project{QStringLiteral("menu_project"),
                     tr("Project"),
                     {
                         I::action(QStringLiteral("file_new")),
                         I::action(QStringLiteral("file_open")),
                         I::action(QStringLiteral("file_open_archive")),
                         I::action(QStringLiteral("file_open_single")),
                         I::submenu(QStringLiteral("menu_open_recent"), tr("Open recent"),
                                    {I::dynamic(QStringLiteral("recent_projects"))}),
                         I::action(QStringLiteral("file_save")),
                         I::action(QStringLiteral("file_saveas")),
                         I::separator(),
                         I::action(QStringLiteral("file_close")),
                         I::action(QStringLiteral("file_close_all")),
                         I::action(QStringLiteral("file_close_others")),
                         I::separator(),
                         I::action(QStringLiteral("file_backup")),
                         I::action(QStringLiteral("file_load_backup")),
                         I::separator(),
                         I::action(QStringLiteral("file_run")),
                         I::separator(),
                         I::action(QStringLiteral("file_welcome")),
                         I::separator(),
                         I::action(QStringLiteral("file_exit")),
                     },
                     false};

    MenuSpec edit{
        QStringLiteral("menu_edit"),
        tr("Edit"),
        {
            I::action(QStringLiteral("edit_undo")),
            I::action(QStringLiteral("edit_redo")),
            I::separator(),
            I::action(QStringLiteral("edit_cut")),
            I::action(QStringLiteral("edit_copy")),
            I::action(QStringLiteral("edit_paste")),
            I::separator(),
            I::action(QStringLiteral("edit_find_replace")),
            I::action(QStringLiteral("edit_find_replace_multi")),
            I::separator(),
            I::submenu(QStringLiteral("menu_code"), tr("Code"),
                       {
                           I::action(QStringLiteral("code_goto")),
                           I::action(QStringLiteral("code_prev_error")),
                           I::action(QStringLiteral("code_next_error")),
                           I::action(QStringLiteral("code_prev_deprecation")),
                           I::action(QStringLiteral("code_next_deprecation")),
                           I::separator(),
                           I::action(QStringLiteral("code_comment")),
                           I::action(QStringLiteral("code_fold_all")),
                           I::action(QStringLiteral("code_unfold_all")),
                           I::separator(),
                           I::action(QStringLiteral("code_autoindent")),
                           I::action(QStringLiteral("code_autoformat")),
                       }),
            I::submenu(QStringLiteral("menu_encoding"), tr("Encoding"), {I::dynamic(QStringLiteral("encodings"))}),
        },
        false};

    MenuSpec view{QStringLiteral("menu_view"),
                  tr("View"),
                  {
                      I::action(QStringLiteral("view_left_panel")),
                      I::action(QStringLiteral("view_right_panel")),
                      I::action(QStringLiteral("view_organizer")),
                      I::action(QStringLiteral("view_actions")),
                      I::action(QStringLiteral("view_calculator")),
                      I::action(QStringLiteral("view_clipboard")),
                      I::submenu(QStringLiteral("menu_zoom"), tr("Zoom"),
                                 {
                                     I::action(QStringLiteral("view_zoomreset")),
                                     I::action(QStringLiteral("view_zoomfit")),
                                     I::action(QStringLiteral("view_zoomout")),
                                     I::action(QStringLiteral("view_zoomin")),
                                 }),
                      I::separator(),
                      I::action(QStringLiteral("view_issues")),
                      I::action(QStringLiteral("view_advanced_search")),
                      I::action(QStringLiteral("view_engine_output")),
                      I::action(QStringLiteral("view_debugger")),
                      I::action(QStringLiteral("view_watch")),
                      I::separator(),
                      I::action(QStringLiteral("view_code_bar")),
                      I::separator(),
                      I::action(QStringLiteral("view_center_origin")),
                      I::action(QStringLiteral("view_main_cross")),
                      I::action(QStringLiteral("view_secondary_cross")),
                      I::action(QStringLiteral("view_extra_cross")),
                      I::action(QStringLiteral("view_rulers")),
                      I::action(QStringLiteral("view_guide_lines")),
                      I::action(QStringLiteral("view_attack_box")),
                      I::action(QStringLiteral("view_collision_box")),
                  },
                  false};

    MenuSpec debug{QStringLiteral("menu_debug"),
                   tr("Debug"),
                   {
                       I::action(QStringLiteral("debug_start_engine")),
                       I::action(QStringLiteral("debug_start_emul")),
                       I::separator(),
                       I::action(QStringLiteral("debug_interrupt")),
                       I::action(QStringLiteral("debug_continue")),
                       I::action(QStringLiteral("debug_stop")),
                       I::action(QStringLiteral("debug_restart")),
                       I::separator(),
                       I::action(QStringLiteral("debug_step_over")),
                       I::action(QStringLiteral("debug_step_into")),
                       I::action(QStringLiteral("debug_step_out")),
                       I::action(QStringLiteral("debug_step_frame")),
                       I::separator(),
                       I::action(QStringLiteral("debug_breakpoint")),
                       I::action(QStringLiteral("breakpoint_add")),
                   },
                   false};

    return {project, edit, view, debug};
}

QList<MenuSpec> applicationMenusAfterEngine()
{
    MenuSpec tools{QStringLiteral("menu_tools"),
                   tr("Tools"),
                   {
                       I::action(QStringLiteral("tools_options")),
                       I::separator(),
                       I::action(QStringLiteral("tools_pal_editor")),
                       I::action(QStringLiteral("tools_script_editor")),
                       I::separator(),
                       I::dynamic(QStringLiteral("engine_tools")),
                       I::dynamic(QStringLiteral("tool_scripts")),
                   },
                   false};

    MenuSpec help{QStringLiteral("menu_help"),
                  tr("Help"),
                  {
                      I::action(QStringLiteral("check_updates")),
                      I::separator(),
                      I::action(QStringLiteral("ref_docs")),
                      I::action(QStringLiteral("help_about")),
                  },
                  false};

    return {tools, help};
}

} // namespace OFS
