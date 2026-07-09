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
    {"file_new",                "New",                         "Create a new project", "", false},
    {"file_open",               "Open...",                     "Open a project", "", false},
    {"file_open_archive",       "Open from archive...",        "Open from archive...", "", false},
    {"file_open_single",        "Open single file...",         "Open a single file", "", false},
    {"file_save",               "Save",                        "Save all files of the current project", "", false},
    {"file_saveas",             "Save as...",                  "Save all files of the current project as a new project", "", false},
    {"file_close",              "Close",                       "Close the current project", "", false},
    {"file_close_all",          "Close all",                   "Close all projects", "", false},
    {"file_close_others",       "Close others",                "Close all projects, except the current", "", false},
    {"file_backup",             "Create backup",               "Create a backup of the current project", "", false},
    {"file_load_backup",        "Load backup",                 "Load a backup of the current project", "", false},
    {"file_run",                "Run in engine...",            "Run the current project in engine...", "", false},
    {"file_welcome",            "Welcome screen",              "Open the Welcome screen", "", false},
    {"file_exit",               "Exit",                        "Exit the program", "", false},
    // Edit
    {"edit_undo",               "Undo",                        "Undo the last change", "", false},
    {"edit_redo",               "Redo",                        "Redo the last undo operation", "", false},
    {"edit_cut",                "Cut",                         "Cut the selection to the clipboard", "", false},
    {"edit_copy",               "Copy",                        "Copy the selection to the clipboard", "", false},
    {"edit_paste",              "Paste",                       "Paste the selection from the clipboard", "", false},
    {"edit_find_replace",       "Find/Replace",                "Find/Replace text in a single line", "", false},
    {"edit_find_replace_multi", "Find/Replace (multi-line)",   "Find/Replace text in multiple lines", "", false},
    // View
    {"view_left_panel",         "Left panel",                  "Show/Hide the left panel", "", true},
    {"view_right_panel",        "Right panel",                 "Show/Hide the right panel", "", true},
    {"view_organizer",          "Organizer",                   "Show/Hide the Organizer (right panel)", "", true},
    {"view_actions",            "Action list",                 "Show/Hide the Action list", "", true},
    {"view_calculator",         "Calculator",                  "Open the Calculator", "", false},
    {"view_clipboard",          "Clipboard container",         "Clipboard container", "", false},
    {"view_zoomreset",          "Actual size",                 "Actual size (100%)", "", false},
    {"view_zoomfit",            "Fit screen size",             "Fit to screen size", "", false},
    {"view_zoomout",            "Decrease zoom",               "Decrease the zoom level", "", false},
    {"view_zoomin",             "Increase zoom",               "Increase the zoom level", "", false},
    {"view_issues",             "Issues",                      "Issues", "", false},
    {"view_advanced_search",    "Advanced Search",             "Advanced Search", "", false},
    {"view_engine_output",      "Engine Output",               "Engine Output", "", false},
    {"view_debugger",           "Debugger",                    "Debugger", "", false},
    {"view_watch",              "Watch",                       "Watch", "", false},
    // Debug
    {"debug_start_engine",      "Start on Engine...",          "Start debugger on Engine", "", false},
    {"debug_start_emul",        "Start emulator",              "Start debugger on built-in emulator", "", false},
    {"debug_interrupt",         "Interrupt",                   "Interrupt", "", false},
    {"debug_continue",          "Continue",                    "Continue", "", false},
    {"debug_stop",              "Stop",                        "Stop", "", false},
    {"debug_restart",           "Restart",                     "Restart", "", false},
    {"debug_step_over",         "Step over",                   "Step to the next instruction without entering into functions", "", false},
    {"debug_step_into",         "Step into",                   "Step to the next instruction entering into functions", "", false},
    {"debug_step_out",          "Step out",                    "Step to the next instruction out of current function", "", false},
    {"debug_step_frame",        "Step frame",                  "Step to the next frame", "", false},
    {"debug_breakpoint",        "Toggle breakpoint",           "Toggle breakpoint", "", false},
    {"breakpoint_add",          "Add breakpoint...",           "Add breakpoint...", "", false},
    // Tools
    {"tools_options",           "Options...",                  "Show the Options window", "", false},
    {"tools_pal_editor",        "Advanced palette editor",     "Edit the palette in the 'Advanced palette editor'", "", false},
    {"tools_script_editor",     "Script editor",               "Script editor", "", false},
    // Help
    {"check_updates",           "Check updates...",            "Check for a new release of the program", "", false},
    {"ref_docs",                "Reference Documentation",     "Show the Reference Documentation", "", false},
    {"help_about",              "About...",                    "Show the About window", "", false},
    // Current editor file commands (Extra toolbar)
    {"editor_new",              "New",                         "New file (current editor)", "", false},
    {"editor_open",             "Open",                        "Open file (current editor)", "", false},
    {"editor_save",             "Save",                        "Save file (current editor)", "", false},
    {"editor_saveas",           "Save as",                     "Save as a new file (current editor)", "", false},
    // Debugger panel
    {"debug_start",             "Start/Resume",                "Start/Resume", "", false},
    {"breakpoint_edit",         "Edit selected breakpoint",    "Edit selected breakpoint", "", false},
    {"breakpoint_enable",       "Enable selected breakpoint",  "Enable selected breakpoint", "", false},
    {"breakpoint_disable",      "Disable selected breakpoint", "Disable selected breakpoint", "", false},
    {"breakpoint_remove",       "Remove selected breakpoint",  "Remove selected breakpoint", "", false},
    {"breakpoint_remove_all",   "Remove all breakpoints",      "Remove all breakpoints", "", false},
    // Watch panel
    {"watch_remove",            "Remove selected",             "Remove selected", "", false},
    {"watch_remove_all",        "Remove all",                  "Remove all", "", false},
    // Code editors
    {"view_code_bar",           "Code toolbar",                "Code toolbar", "", true},
    {"code_snippet_insert",     "Insert code snippet",         "Insert the current code snippet", "", false},
    {"code_snippet_new",        "New code snippet",            "Create a new code snippet", "", false},
    {"code_snippet_edit",       "Edit code snippet",           "Edit the current code snippet", "", false},
    {"code_goto",               "Go to line...",               "Go to the specified line", "", false},
    {"code_prev_error",         "Previous syntax error",       "Go to the previous syntax error", "", false},
    {"code_next_error",         "Next syntax error",           "Go to the next syntax error", "", false},
    {"code_prev_deprecation",   "Previous deprecation",        "Go to the previous deprecation", "", false},
    {"code_next_deprecation",   "Next deprecation",            "Go to the next deprecation", "", false},
    {"code_comment",            "Comment/Uncomment selection", "Comment/Uncomment all selected lines", "", false},
    {"code_fold_all",           "Fold all code blocks",        "Fold all code blocks", "", false},
    {"code_unfold_all",         "Unfold all code blocks",      "Unfold all code blocks", "", false},
    {"code_autoindent",         "Auto-indent selection",       "Auto-indent selected lines", "", false},
    {"code_autoformat",         "Auto-format selection",       "Auto-format selected lines", "", false},
    // Graphic workspaces
    {"view_center_origin",      "Centralize origin",           "Place the origin at the center of the workspace", "", false},
    {"view_main_cross",         "Main cross",                  "Show/Hide the main cross", "", true},
    {"view_secondary_cross",    "Secondary cross",             "Show/Hide the secondary cross", "", true},
    {"view_extra_cross",        "Extra cross",                 "Show/Hide the extra cross", "", true},
    {"view_rulers",             "Rulers",                      "Show/Hide the Rulers", "", true},
    {"view_guide_lines",        "Guide lines",                 "Show/Hide the Guide lines", "", true},
    {"view_attack_box",         "Attack Box",                  "Show/Hide the Attack contact boxes", "", true},
    {"view_collision_box",      "Collision Box",               "Show/Hide the Collision contact boxes", "", true},
    // Organizer panel
    {"org_up",                  "Move one position up",        "Move one position up", "", false},
    {"org_down",                "Move one position down",      "Move one position down", "", false},
    {"org_up_10",               "Move ten positions up",       "Move ten positions up", "", false},
    {"org_down_10",             "Move ten positions down",     "Move ten positions down", "", false},
    {"org_top",                 "Move to the top",             "Move to the top", "", false},
    {"org_bottom",              "Move to the bottom",          "Move to the bottom", "", false},
    {"org_auto",                "Organize automatically",      "Organize automatically", "", false},
    // Debugger data
    {"debug_data_watch1",       "Watch variable (1)",          "Watch variable (1)", "", false},
    {"debug_data_watch2",       "Watch variable (2)",          "Watch variable (2)", "", false},
    {"debug_data_breakpoint",   "Add data breakpoint...",      "Add data breakpoint...", "", false},
};
// clang-format on

} // namespace

void registerApplicationActions(ActionRegistry& registry)
{
    registerActions(registry, kApplicationActions);
}

} // namespace OFS
