#include "core/actions/ActionCatalog.h"

#include "core/actions/ActionRegistry.h"

#include <QCoreApplication>

namespace OFS {

void registerActions(ActionRegistry& registry, const ActionDef* defs, int count)
{
    for (int i = 0; i < count; ++i) {
        const ActionDef& d = defs[i];
        ActionRegistry::Spec spec;
        spec.id = QString::fromLatin1(d.id);
        spec.text = QCoreApplication::translate("Actions", d.text);
        spec.description =
            d.description && *d.description ? QCoreApplication::translate("Actions", d.description) : spec.text;
        spec.scope = QString::fromLatin1(d.scope);
        spec.checkable = d.checkable;
        registry.add(spec);
    }
}

namespace {

// clang-format off
const ActionDef kApplicationActions[] = {
    // Project
    {"file_new",                QT_TRANSLATE_NOOP("Actions", "New"),                         QT_TRANSLATE_NOOP("Actions", "Create a new project"), "", false},
    {"file_open",               QT_TRANSLATE_NOOP("Actions", "Open..."),                     QT_TRANSLATE_NOOP("Actions", "Open a project"), "", false},
    {"file_open_archive",       QT_TRANSLATE_NOOP("Actions", "Open from archive..."),        QT_TRANSLATE_NOOP("Actions", "Open from archive..."), "", false},
    {"file_open_single",        QT_TRANSLATE_NOOP("Actions", "Open single file..."),         QT_TRANSLATE_NOOP("Actions", "Open a single file"), "", false},
    {"file_save",               QT_TRANSLATE_NOOP("Actions", "Save"),                        QT_TRANSLATE_NOOP("Actions", "Save all files of the current project"), "", false},
    {"file_saveas",             QT_TRANSLATE_NOOP("Actions", "Save as..."),                  QT_TRANSLATE_NOOP("Actions", "Save all files of the current project as a new project"), "", false},
    {"file_close",              QT_TRANSLATE_NOOP("Actions", "Close"),                       QT_TRANSLATE_NOOP("Actions", "Close the current project"), "", false},
    {"file_close_all",          QT_TRANSLATE_NOOP("Actions", "Close all"),                   QT_TRANSLATE_NOOP("Actions", "Close all projects"), "", false},
    {"file_close_others",       QT_TRANSLATE_NOOP("Actions", "Close others"),                QT_TRANSLATE_NOOP("Actions", "Close all projects, except the current"), "", false},
    {"file_backup",             QT_TRANSLATE_NOOP("Actions", "Create backup"),               QT_TRANSLATE_NOOP("Actions", "Create a backup of the current project"), "", false},
    {"file_load_backup",        QT_TRANSLATE_NOOP("Actions", "Load backup"),                 QT_TRANSLATE_NOOP("Actions", "Load a backup of the current project"), "", false},
    {"file_run",                QT_TRANSLATE_NOOP("Actions", "Run in engine..."),            QT_TRANSLATE_NOOP("Actions", "Run the current project in engine..."), "", false},
    {"file_welcome",            QT_TRANSLATE_NOOP("Actions", "Welcome screen"),              QT_TRANSLATE_NOOP("Actions", "Open the Welcome screen"), "", false},
    {"file_exit",               QT_TRANSLATE_NOOP("Actions", "Exit"),                        QT_TRANSLATE_NOOP("Actions", "Exit the program"), "", false},
    // Edit
    {"edit_undo",               QT_TRANSLATE_NOOP("Actions", "Undo"),                        QT_TRANSLATE_NOOP("Actions", "Undo the last change"), "", false},
    {"edit_redo",               QT_TRANSLATE_NOOP("Actions", "Redo"),                        QT_TRANSLATE_NOOP("Actions", "Redo the last undo operation"), "", false},
    {"edit_cut",                QT_TRANSLATE_NOOP("Actions", "Cut"),                         QT_TRANSLATE_NOOP("Actions", "Cut the selection to the clipboard"), "", false},
    {"edit_copy",               QT_TRANSLATE_NOOP("Actions", "Copy"),                        QT_TRANSLATE_NOOP("Actions", "Copy the selection to the clipboard"), "", false},
    {"edit_paste",              QT_TRANSLATE_NOOP("Actions", "Paste"),                       QT_TRANSLATE_NOOP("Actions", "Paste the selection from the clipboard"), "", false},
    {"edit_find_replace",       QT_TRANSLATE_NOOP("Actions", "Find/Replace"),                QT_TRANSLATE_NOOP("Actions", "Find/Replace text in a single line"), "", false},
    {"edit_find_replace_multi", QT_TRANSLATE_NOOP("Actions", "Find/Replace (multi-line)"),   QT_TRANSLATE_NOOP("Actions", "Find/Replace text in multiple lines"), "", false},
    // View
    {"view_left_panel",         QT_TRANSLATE_NOOP("Actions", "Left panel"),                  QT_TRANSLATE_NOOP("Actions", "Show/Hide the left panel"), "", true},
    {"view_right_panel",        QT_TRANSLATE_NOOP("Actions", "Right panel"),                 QT_TRANSLATE_NOOP("Actions", "Show/Hide the right panel"), "", true},
    {"view_organizer",          QT_TRANSLATE_NOOP("Actions", "Organizer"),                   QT_TRANSLATE_NOOP("Actions", "Show/Hide the Organizer (right panel)"), "", true},
    {"view_actions",            QT_TRANSLATE_NOOP("Actions", "Action list"),                 QT_TRANSLATE_NOOP("Actions", "Show/Hide the Action list"), "", true},
    {"view_calculator",         QT_TRANSLATE_NOOP("Actions", "Calculator"),                  QT_TRANSLATE_NOOP("Actions", "Open the Calculator"), "", false},
    {"view_clipboard",          QT_TRANSLATE_NOOP("Actions", "Clipboard container"),         QT_TRANSLATE_NOOP("Actions", "Clipboard container"), "", false},
    {"view_zoomreset",          QT_TRANSLATE_NOOP("Actions", "Actual size"),                 QT_TRANSLATE_NOOP("Actions", "Actual size (100%)"), "", false},
    {"view_zoomfit",            QT_TRANSLATE_NOOP("Actions", "Fit screen size"),             QT_TRANSLATE_NOOP("Actions", "Fit to screen size"), "", false},
    {"view_zoomout",            QT_TRANSLATE_NOOP("Actions", "Decrease zoom"),               QT_TRANSLATE_NOOP("Actions", "Decrease the zoom level"), "", false},
    {"view_zoomin",             QT_TRANSLATE_NOOP("Actions", "Increase zoom"),               QT_TRANSLATE_NOOP("Actions", "Increase the zoom level"), "", false},
    {"view_issues",             QT_TRANSLATE_NOOP("Actions", "Issues"),                      QT_TRANSLATE_NOOP("Actions", "Issues"), "", false},
    {"view_advanced_search",    QT_TRANSLATE_NOOP("Actions", "Advanced Search"),             QT_TRANSLATE_NOOP("Actions", "Advanced Search"), "", false},
    {"view_engine_output",      QT_TRANSLATE_NOOP("Actions", "Engine Output"),               QT_TRANSLATE_NOOP("Actions", "Engine Output"), "", false},
    {"view_debugger",           QT_TRANSLATE_NOOP("Actions", "Debugger"),                    QT_TRANSLATE_NOOP("Actions", "Debugger"), "", false},
    {"view_watch",              QT_TRANSLATE_NOOP("Actions", "Watch"),                       QT_TRANSLATE_NOOP("Actions", "Watch"), "", false},
    // Debug
    {"debug_start_engine",      QT_TRANSLATE_NOOP("Actions", "Start on Engine..."),          QT_TRANSLATE_NOOP("Actions", "Start debugger on Engine"), "", false},
    {"debug_start_emul",        QT_TRANSLATE_NOOP("Actions", "Start emulator"),              QT_TRANSLATE_NOOP("Actions", "Start debugger on built-in emulator"), "", false},
    {"debug_interrupt",         QT_TRANSLATE_NOOP("Actions", "Interrupt"),                   QT_TRANSLATE_NOOP("Actions", "Interrupt"), "", false},
    {"debug_continue",          QT_TRANSLATE_NOOP("Actions", "Continue"),                    QT_TRANSLATE_NOOP("Actions", "Continue"), "", false},
    {"debug_stop",              QT_TRANSLATE_NOOP("Actions", "Stop"),                        QT_TRANSLATE_NOOP("Actions", "Stop"), "", false},
    {"debug_restart",           QT_TRANSLATE_NOOP("Actions", "Restart"),                     QT_TRANSLATE_NOOP("Actions", "Restart"), "", false},
    {"debug_step_over",         QT_TRANSLATE_NOOP("Actions", "Step over"),                   QT_TRANSLATE_NOOP("Actions", "Step to the next instruction without entering into functions"), "", false},
    {"debug_step_into",         QT_TRANSLATE_NOOP("Actions", "Step into"),                   QT_TRANSLATE_NOOP("Actions", "Step to the next instruction entering into functions"), "", false},
    {"debug_step_out",          QT_TRANSLATE_NOOP("Actions", "Step out"),                    QT_TRANSLATE_NOOP("Actions", "Step to the next instruction out of current function"), "", false},
    {"debug_step_frame",        QT_TRANSLATE_NOOP("Actions", "Step frame"),                  QT_TRANSLATE_NOOP("Actions", "Step to the next frame"), "", false},
    {"debug_breakpoint",        QT_TRANSLATE_NOOP("Actions", "Toggle breakpoint"),           QT_TRANSLATE_NOOP("Actions", "Toggle breakpoint"), "", false},
    {"breakpoint_add",          QT_TRANSLATE_NOOP("Actions", "Add breakpoint..."),           QT_TRANSLATE_NOOP("Actions", "Add breakpoint..."), "", false},
    // Tools
    {"tools_options",           QT_TRANSLATE_NOOP("Actions", "Options..."),                  QT_TRANSLATE_NOOP("Actions", "Show the Options window"), "", false},
    {"tools_pal_editor",        QT_TRANSLATE_NOOP("Actions", "Advanced palette editor"),     QT_TRANSLATE_NOOP("Actions", "Edit the palette in the 'Advanced palette editor'"), "", false},
    {"tools_script_editor",     QT_TRANSLATE_NOOP("Actions", "Script editor"),               QT_TRANSLATE_NOOP("Actions", "Script editor"), "", false},
    // Help
    {"check_updates",           QT_TRANSLATE_NOOP("Actions", "Check updates..."),            QT_TRANSLATE_NOOP("Actions", "Check for a new release of the program"), "", false},
    {"ref_docs",                QT_TRANSLATE_NOOP("Actions", "Reference Documentation"),     QT_TRANSLATE_NOOP("Actions", "Show the Reference Documentation"), "", false},
    {"help_about",              QT_TRANSLATE_NOOP("Actions", "About..."),                    QT_TRANSLATE_NOOP("Actions", "Show the About window"), "", false},
    // Current editor file commands (Extra toolbar)
    {"editor_new",              QT_TRANSLATE_NOOP("Actions", "New"),                         QT_TRANSLATE_NOOP("Actions", "New file (current editor)"), "", false},
    {"editor_open",             QT_TRANSLATE_NOOP("Actions", "Open"),                        QT_TRANSLATE_NOOP("Actions", "Open file (current editor)"), "", false},
    {"editor_save",             QT_TRANSLATE_NOOP("Actions", "Save"),                        QT_TRANSLATE_NOOP("Actions", "Save file (current editor)"), "", false},
    {"editor_saveas",           QT_TRANSLATE_NOOP("Actions", "Save as"),                     QT_TRANSLATE_NOOP("Actions", "Save as a new file (current editor)"), "", false},
    // Debugger panel
    {"debug_start",             QT_TRANSLATE_NOOP("Actions", "Start/Resume"),                QT_TRANSLATE_NOOP("Actions", "Start/Resume"), "", false},
    {"breakpoint_edit",         QT_TRANSLATE_NOOP("Actions", "Edit selected breakpoint"),    QT_TRANSLATE_NOOP("Actions", "Edit selected breakpoint"), "", false},
    {"breakpoint_enable",       QT_TRANSLATE_NOOP("Actions", "Enable selected breakpoint"),  QT_TRANSLATE_NOOP("Actions", "Enable selected breakpoint"), "", false},
    {"breakpoint_disable",      QT_TRANSLATE_NOOP("Actions", "Disable selected breakpoint"), QT_TRANSLATE_NOOP("Actions", "Disable selected breakpoint"), "", false},
    {"breakpoint_remove",       QT_TRANSLATE_NOOP("Actions", "Remove selected breakpoint"),  QT_TRANSLATE_NOOP("Actions", "Remove selected breakpoint"), "", false},
    {"breakpoint_remove_all",   QT_TRANSLATE_NOOP("Actions", "Remove all breakpoints"),      QT_TRANSLATE_NOOP("Actions", "Remove all breakpoints"), "", false},
    // Watch panel
    {"watch_remove",            QT_TRANSLATE_NOOP("Actions", "Remove selected"),             QT_TRANSLATE_NOOP("Actions", "Remove selected"), "", false},
    {"watch_remove_all",        QT_TRANSLATE_NOOP("Actions", "Remove all"),                  QT_TRANSLATE_NOOP("Actions", "Remove all"), "", false},
    // Code editors
    {"view_code_bar",           QT_TRANSLATE_NOOP("Actions", "Code toolbar"),                QT_TRANSLATE_NOOP("Actions", "Code toolbar"), "", true},
    {"code_snippet_insert",     QT_TRANSLATE_NOOP("Actions", "Insert code snippet"),         QT_TRANSLATE_NOOP("Actions", "Insert the current code snippet"), "", false},
    {"code_snippet_new",        QT_TRANSLATE_NOOP("Actions", "New code snippet"),            QT_TRANSLATE_NOOP("Actions", "Create a new code snippet"), "", false},
    {"code_snippet_edit",       QT_TRANSLATE_NOOP("Actions", "Edit code snippet"),           QT_TRANSLATE_NOOP("Actions", "Edit the current code snippet"), "", false},
    {"code_goto",               QT_TRANSLATE_NOOP("Actions", "Go to line..."),               QT_TRANSLATE_NOOP("Actions", "Go to the specified line"), "", false},
    {"code_prev_error",         QT_TRANSLATE_NOOP("Actions", "Previous syntax error"),       QT_TRANSLATE_NOOP("Actions", "Go to the previous syntax error"), "", false},
    {"code_next_error",         QT_TRANSLATE_NOOP("Actions", "Next syntax error"),           QT_TRANSLATE_NOOP("Actions", "Go to the next syntax error"), "", false},
    {"code_prev_deprecation",   QT_TRANSLATE_NOOP("Actions", "Previous deprecation"),        QT_TRANSLATE_NOOP("Actions", "Go to the previous deprecation"), "", false},
    {"code_next_deprecation",   QT_TRANSLATE_NOOP("Actions", "Next deprecation"),            QT_TRANSLATE_NOOP("Actions", "Go to the next deprecation"), "", false},
    {"code_comment",            QT_TRANSLATE_NOOP("Actions", "Comment/Uncomment selection"), QT_TRANSLATE_NOOP("Actions", "Comment/Uncomment all selected lines"), "", false},
    {"code_fold_all",           QT_TRANSLATE_NOOP("Actions", "Fold all code blocks"),        QT_TRANSLATE_NOOP("Actions", "Fold all code blocks"), "", false},
    {"code_unfold_all",         QT_TRANSLATE_NOOP("Actions", "Unfold all code blocks"),      QT_TRANSLATE_NOOP("Actions", "Unfold all code blocks"), "", false},
    {"code_autoindent",         QT_TRANSLATE_NOOP("Actions", "Auto-indent selection"),       QT_TRANSLATE_NOOP("Actions", "Auto-indent selected lines"), "", false},
    {"code_autoformat",         QT_TRANSLATE_NOOP("Actions", "Auto-format selection"),       QT_TRANSLATE_NOOP("Actions", "Auto-format selected lines"), "", false},
    // Graphic workspaces
    {"view_center_origin",      QT_TRANSLATE_NOOP("Actions", "Centralize origin"),           QT_TRANSLATE_NOOP("Actions", "Place the origin at the center of the workspace"), "", false},
    {"view_main_cross",         QT_TRANSLATE_NOOP("Actions", "Main cross"),                  QT_TRANSLATE_NOOP("Actions", "Show/Hide the main cross"), "", true},
    {"view_secondary_cross",    QT_TRANSLATE_NOOP("Actions", "Secondary cross"),             QT_TRANSLATE_NOOP("Actions", "Show/Hide the secondary cross"), "", true},
    {"view_extra_cross",        QT_TRANSLATE_NOOP("Actions", "Extra cross"),                 QT_TRANSLATE_NOOP("Actions", "Show/Hide the extra cross"), "", true},
    {"view_rulers",             QT_TRANSLATE_NOOP("Actions", "Rulers"),                      QT_TRANSLATE_NOOP("Actions", "Show/Hide the Rulers"), "", true},
    {"view_guide_lines",        QT_TRANSLATE_NOOP("Actions", "Guide lines"),                 QT_TRANSLATE_NOOP("Actions", "Show/Hide the Guide lines"), "", true},
    {"view_attack_box",         QT_TRANSLATE_NOOP("Actions", "Attack Box"),                  QT_TRANSLATE_NOOP("Actions", "Show/Hide the Attack contact boxes"), "", true},
    {"view_collision_box",      QT_TRANSLATE_NOOP("Actions", "Collision Box"),               QT_TRANSLATE_NOOP("Actions", "Show/Hide the Collision contact boxes"), "", true},
    // Organizer panel
    {"org_up",                  QT_TRANSLATE_NOOP("Actions", "Move one position up"),        QT_TRANSLATE_NOOP("Actions", "Move one position up"), "", false},
    {"org_down",                QT_TRANSLATE_NOOP("Actions", "Move one position down"),      QT_TRANSLATE_NOOP("Actions", "Move one position down"), "", false},
    {"org_up_10",               QT_TRANSLATE_NOOP("Actions", "Move ten positions up"),       QT_TRANSLATE_NOOP("Actions", "Move ten positions up"), "", false},
    {"org_down_10",             QT_TRANSLATE_NOOP("Actions", "Move ten positions down"),     QT_TRANSLATE_NOOP("Actions", "Move ten positions down"), "", false},
    {"org_top",                 QT_TRANSLATE_NOOP("Actions", "Move to the top"),             QT_TRANSLATE_NOOP("Actions", "Move to the top"), "", false},
    {"org_bottom",              QT_TRANSLATE_NOOP("Actions", "Move to the bottom"),          QT_TRANSLATE_NOOP("Actions", "Move to the bottom"), "", false},
    {"org_auto",                QT_TRANSLATE_NOOP("Actions", "Organize automatically"),      QT_TRANSLATE_NOOP("Actions", "Organize automatically"), "", false},
    // Debugger data
    {"debug_data_watch1",       QT_TRANSLATE_NOOP("Actions", "Watch variable (1)"),          QT_TRANSLATE_NOOP("Actions", "Watch variable (1)"), "", false},
    {"debug_data_watch2",       QT_TRANSLATE_NOOP("Actions", "Watch variable (2)"),          QT_TRANSLATE_NOOP("Actions", "Watch variable (2)"), "", false},
    {"debug_data_breakpoint",   QT_TRANSLATE_NOOP("Actions", "Add data breakpoint..."),      QT_TRANSLATE_NOOP("Actions", "Add data breakpoint..."), "", false},
};
// clang-format on

} // namespace

void registerApplicationActions(ActionRegistry& registry)
{
    registerActions(registry, kApplicationActions);
}

} // namespace OFS
