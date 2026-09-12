#include "engines/mugen/MugenMenus.h"

#include <QCoreApplication>

namespace OFS::Mugen {

namespace {

using I = MenuItemSpec;

struct MenuTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Mugen::MenuTexts)
};

I a(const char* id)
{
    return I::action(QString::fromLatin1(id));
}

I sep()
{
    return I::separator();
}

} // namespace

QList<MenuSpec> mugenMenus()
{
    // The Definitions editor's file commands sit in a hidden menu, reached through the Extra toolbar.
    MenuSpec definitions{QStringLiteral("menu_definitions"),
                         QString(),
                         {a("def_new"), a("def_open"), a("def_save"), a("def_saveas_utf8"), a("def_saveas")},
                         true};

    MenuSpec palettes{QStringLiteral("menu_palettes"),
                      MenuTexts::tr("Palettes"),
                      {
                          a("pal_clear"),
                          a("pal_clear_unused"),
                          a("pal_add"),
                          a("pal_delete"),
                          a("pal_delete_duplicates"),
                          a("pal_open"),
                          a("pal_save"),
                          sep(),
                          a("pal_switch_count"),
                          a("pal_remap"),
                          sep(),
                          a("pal_undo"),
                          a("pal_redo"),
                          a("pal_switch_color"),
                          a("pal_set_color"),
                          a("pal_pick_color"),
                          a("pal_apply_sprite"),
                      },
                      false};

    MenuSpec backgrounds{QStringLiteral("menu_backgrounds"),
                         MenuTexts::tr("Backgrounds"),
                         {
                             a("bgs_add"),
                             a("bgs_delete"),
                             a("bgs_duplicate"),
                             sep(),
                             a("bgs_play"),
                             a("bgs_pause"),
                             a("bgs_stop"),
                             sep(),
                             I::submenu(QStringLiteral("menu_bg_controllers"), MenuTexts::tr("Controllers"),
                                        {
                                            a("bgctrls_def_add"),
                                            a("bgctrls_def_delete"),
                                            sep(),
                                            a("bgctrls_add"),
                                            a("bgctrls_delete"),
                                        }),
                             I::dynamic(QStringLiteral("bg_groups")),
                         },
                         false};

    MenuSpec sprites{QStringLiteral("menu_sprites"),
                     MenuTexts::tr("Sprites"),
                     {
                         a("spr_new"),
                         a("spr_open"),
                         a("spr_save"),
                         a("spr_saveas"),
                         sep(),
                         a("spr_add"),
                         a("spr_delete"),
                         a("spr_duplicate"),
                         a("spr_openimg"),
                         a("spr_saveimg"),
                         a("spr_saveimgas"),
                         a("spr_saveimg_aligned"),
                         a("spr_resize"),
                         a("spr_crop"),
                         a("spr_delete_grp"),
                         a("spr_rename_grp"),
                         a("spr_align"),
                         a("spr_edit_img"),
                         a("spr_split_channels"),
                         I::submenu(QStringLiteral("menu_spr_layers"), MenuTexts::tr("Layers"),
                                    {
                                        a("spr_layer_add"),
                                        a("spr_layer_delete"),
                                        a("spr_layer_duplicate"),
                                        sep(),
                                        a("spr_layer_mergevis"),
                                        a("spr_layer_mergeall"),
                                        sep(),
                                        a("spr_layer_save"),
                                        a("spr_layer_play"),
                                    }),
                         I::submenu(QStringLiteral("menu_spr_image"), MenuTexts::tr("Image"),
                                    {a("spr_view_original"), a("spr_image_properties")}),
                         I::submenu(QStringLiteral("menu_spr_select"), MenuTexts::tr("Select"),
                                    {a("spr_select_all"), a("spr_select_none"), a("spr_select_invert")}),
                         I::submenu(QStringLiteral("menu_spr_rotation"), MenuTexts::tr("Rotation"),
                                    {a("spr_image_rotate"), a("spr_image_fliph"), a("spr_image_flipv")}),
                         sep(),
                         a("spr_export"),
                         a("spr_import"),
                         a("spr_import_proj"),
                         sep(),
                         a("spr_frame_spr"),
                         sep(),
                         a("spr_explorer"),
                     },
                     false};

    MenuSpec animations{QStringLiteral("menu_animations"),
                        MenuTexts::tr("Animations"),
                        {
                            a("anim_new"),
                            a("anim_open"),
                            a("anim_save"),
                            a("anim_saveas_utf8"),
                            a("anim_saveas"),
                            sep(),
                            a("anim_editcode"),
                            a("anim_code"),
                            sep(),
                            a("anim_add"),
                            a("anim_delete"),
                            a("anim_duplicate"),
                            a("anim_save_gif"),
                            a("anim_play"),
                            a("anim_stop"),
                            sep(),
                            a("anim_frame_add"),
                            a("anim_frame_delete"),
                            a("anim_frameall_offset"),
                            a("anim_frameall_time"),
                            a("anim_frameall_trans"),
                            a("anim_frameall_flip"),
                            a("anim_frameall_scale"),
                            a("anim_frame_align"),
                            sep(),
                            a("anim_box_add_attack"),
                            a("anim_box_add_collision"),
                            a("anim_box_del"),
                            a("anim_box_delall"),
                            a("anim_box_delall_anim"),
                            a("anim_box_type"),
                            a("anim_box_default"),
                            a("anim_box_prev"),
                            a("anim_box_allinst"),
                            sep(),
                            a("anim_optimize"),
                            a("anim_import_proj"),
                        },
                        false};

    MenuSpec commands{QStringLiteral("menu_commands"),
                      MenuTexts::tr("Commands"),
                      {a("cmd_new"), a("cmd_open"), a("cmd_save"), a("cmd_saveas_utf8"), a("cmd_saveas")},
                      false};

    MenuSpec states{QStringLiteral("menu_states"),
                    MenuTexts::tr("States"),
                    {
                        a("st_new"),
                        a("st_open"),
                        a("st_save"),
                        a("st_saveas_utf8"),
                        a("st_saveas"),
                        sep(),
                        a("st_var_list"),
                        a("st_var_log"),
                        a("st_palfx"),
                        a("st_offset"),
                        a("st_throw"),
                    },
                    false};

    MenuSpec sounds{QStringLiteral("menu_sounds"),
                    MenuTexts::tr("Sounds"),
                    {
                        a("snd_new"),
                        a("snd_open"),
                        a("snd_save"),
                        a("snd_saveas"),
                        sep(),
                        a("snd_add"),
                        a("snd_delete"),
                        a("snd_duplicate"),
                        a("snd_open_sample"),
                        a("snd_save_sample"),
                        a("snd_resample"),
                        a("snd_play"),
                        a("snd_play_loop"),
                        a("snd_pause"),
                        a("snd_stop"),
                        a("snd_delete_grp"),
                        a("snd_rename_grp"),
                        sep(),
                        I::submenu(QStringLiteral("menu_snd_select"), MenuTexts::tr("Select"),
                                   {a("snd_select_all"), a("snd_select_none")}),
                        a("snd_remove_selected"),
                        a("snd_fill_silence"),
                        a("snd_fade_in"),
                        a("snd_fade_out"),
                        sep(),
                        a("snd_save_changes"),
                        a("snd_discard_changes"),
                        sep(),
                        a("snd_export"),
                        a("snd_import"),
                        a("snd_import_proj"),
                    },
                    false};

    return {definitions, palettes, backgrounds, sprites, animations, commands, states, sounds};
}

QList<MenuItemSpec> mugenToolsMenuItems()
{
    return {a("tools_export_html")};
}

} // namespace OFS::Mugen
