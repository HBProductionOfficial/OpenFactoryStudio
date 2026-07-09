-- Interface preset "Classic".

-- Toolbars
local mainBar = FF.toolBar("main")
mainBar.iconSize = 32
mainBar:addButton("file_new")
mainBar:addButton("file_close")
mainBar:addButton("file_open")
mainBar:addButton("file_save")
mainBar:addButton("file_backup")
mainBar:addSeparator()
mainBar:addButton("editor_def")
mainBar:addButton("editor_spr")
mainBar:addButton("editor_anim")
mainBar:addButton("editor_bg")
mainBar:addButton("editor_st")
mainBar:addButton("editor_cmd")
mainBar:addButton("editor_snd")
mainBar:addButton("view_organizer")
mainBar:addSeparator()
mainBar:addButton("tools_pal_editor")
mainBar:addSeparator()
mainBar:addButton("tools_options")
mainBar:addButton("file_run")
mainBar:addButton("debug_start_engine")

local extraBar = FF.toolBar("extra")
extraBar:addButton("view_calculator")
extraBar:addSeparator()
extraBar:addButton("view_zoomreset")
extraBar:addButton("view_zoomfit")
extraBar:addButton("view_zoomout")
extraBar:addButton("view_zoomin")
extraBar:addSeparator()
extraBar:addButton("spr_align_centerleft")
extraBar:addButton("spr_align_centercenter")
extraBar:addButton("spr_align_centerright")
extraBar:addButton("spr_align_topcenter")
extraBar:addButton("spr_align_bottomcenter")
extraBar:addButton("spr_align_topleft")
extraBar:addButton("spr_align_topright")
extraBar:addButton("spr_align_bottomleft")
extraBar:addButton("spr_align_bottomright")

-- Layouts
local l1 = FF.verticalLayout()
l1.margin = 8
local l2 = FF.gridLayout()
local l3 = FF.gridLayout()
l3.margin = 8
l3.spacing = 4
local l4 = FF.gridLayout()
local l5 = FF.gridLayout()
l5.spacing = 4
local l6 = FF.gridLayout()
l6.margin = 8
l6.spacing = 4
local l7 = FF.gridLayout()
local l8 = FF.gridLayout()
l8.margin = 8
l8.spacing = 4
local l9 = FF.horizontalLayout()
local l10 = FF.horizontalLayout()
local l11 = FF.gridLayout()
l11.margin = 0
l11.spacing = 4
local l12 = FF.verticalLayout()
l12.margin = 8
l12.spacing = 4
local l13 = FF.gridLayout()
local l14 = FF.verticalLayout()
l14.margin = 8
local l15 = FF.gridLayout()
local l16 = FF.verticalLayout()
l16.margin = 8
local l17 = FF.gridLayout()
local l18 = FF.verticalLayout()
l18.margin = 8
local l19 = FF.gridLayout()
local l20 = FF.gridLayout()
l20.spacing = 4
local l21 = FF.gridLayout()
l21.spacing = 4
local l22 = FF.gridLayout()
l22.margin = 8
l22.spacing = 4
local l23 = FF.gridLayout()
l23.margin = 8
l23.spacing = 4
local l24 = FF.horizontalLayout()
local l25 = FF.horizontalLayout()
l25.margin = 0
l25.spacing = 4
local l26 = FF.gridLayout()
l26.margin = 8
l26.spacing = 4
local l27 = FF.verticalLayout()
l27.margin = 8
l27.spacing = 4
local l28 = FF.horizontalLayout()
l28.margin = 0
l28.spacing = 4
local l29 = FF.gridLayout()
l29.margin = 8
l29.spacing = 4
local l30 = FF.gridLayout()
l30.margin = 8
l30.spacing = 4
local l31 = FF.horizontalLayout()
l31.margin = 0
l31.spacing = 4
local l32 = FF.gridLayout()
l32.margin = 8
l32.spacing = 4
local l33 = FF.gridLayout()
l33.margin = 0
l33.spacing = 4
local l34 = FF.horizontalLayout()
l34.margin = 0
l34.spacing = 4
local l35 = FF.gridLayout()
l35.margin = 8
l35.spacing = 4
local l36 = FF.gridLayout()
l36.margin = 8
l36.spacing = 4
local l37 = FF.gridLayout()
l37.margin = 8
l37.spacing = 4
local l38 = FF.horizontalLayout()
l38.margin = 0
l38.spacing = 4
local l39 = FF.gridLayout()
l39.margin = 8
l39.spacing = 4
local l40 = FF.gridLayout()
l40.margin = 8
l40.spacing = 4
local l41 = FF.horizontalLayout()
l41.margin = 0
l41.spacing = 4
local l42 = FF.gridLayout()
l42.margin = 8
l42.spacing = 4
local l43 = FF.gridLayout()
l43.margin = 8
l43.spacing = 4
local l44 = FF.gridLayout()
l44.margin = 8
l44.spacing = 4
local l45 = FF.gridLayout()
l45.margin = 8
l45.spacing = 4
local l46 = FF.gridLayout()
l46.margin = 8
l46.spacing = 4
local l47 = FF.gridLayout()
l47.margin = 8
l47.spacing = 6
local l48 = FF.gridLayout()
l48.margin = 8
l48.spacing = 6
local l49 = FF.gridLayout()
l49.margin = 8
l49.spacing = 6
local l50 = FF.verticalLayout()
l50.margin = 8
l50.spacing = 6
local l51 = FF.gridLayout()
local l52 = FF.gridLayout()
l52.margin = 8
l52.spacing = 4
local l53 = FF.horizontalLayout()
local l54 = FF.gridLayout()
l54.margin = 8
l54.spacing = 4
local l55 = FF.gridLayout()
l55.margin = 8
l55.spacing = 4
local l56 = FF.gridLayout()
l56.margin = 8
l56.spacing = 4
local l57 = FF.verticalLayout()
l57.margin = 8
l57.spacing = 6
local l58 = FF.gridLayout()

l1:addLayout(l2, 1)
l1:addControl("def_tree")

l2:addControl("def_new", 0, 0):icon(22):flat()
l2:addControl("def_open", 0, 1):icon(22):flat()
l2:addControl("def_save", 0, 2):icon(22):flat()
l2:addControl("def_saveas", 0, 3):icon(22):flat()
l2:addControl("edit_cut", 0, 4):icon(22):flat()
l2:addControl("edit_copy", 0, 5):icon(22):flat()
l2:addControl("edit_paste", 0, 6):icon(22):flat()
l2:addControl("edit_find_replace", 1, 0):icon(22):flat()
l2:addControl("edit_undo", 1, 1):icon(22):flat()
l2:addControl("edit_redo", 1, 2):icon(22):flat()

l3:addLayout(l4, 0, 0, 1, -1)
l3:addControl("spr_sel", 1, 0, 1, -1):horizontal()
l3:addControl("spr_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l3:addControl("spr_name_lb", 3, 0, 1, -1)
l3:addControl("spr_name", 4, 0, 1, -1)
l3:addLayout(l5, 5, 0, 1, -1)
l3:addControl("spr_dec_load", 6, 0, 1, -1)
l3:addControl("spr_trans_color", 7, 0, 1, -1)
l3:addControl("spr_switch_pal", 8, 0, 1, -1)
l3:addControl("spr_onion_skin", 9, 0, 1, -1)
l3:addControl("spr_onion_sel", 10, 0, 1, -1):horizontal()
l3:addControl("spr_onion_lb", 11, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l3:addControl("spr_info_lb", 12, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l3:addControl("spr_layers_menu", 3, 0):icon(22):flat()
l3:addControl("spr_layers_blendmode", 3, 1, 1, 3)
l3:addControl("spr_layers_opacity", 3, 4)
l3:addControl("spr_layers_list", 4, 0, 11, 5)
l3:addControl("spr_tool_canvasmove", 3, 5):icon(22):flat()
l3:addControl("spr_tool_selmove", 4, 5):icon(22):flat()
l3:addControl("spr_tool_rectsel", 5, 5):icon(22):flat()
l3:addControl("spr_tool_freesel", 6, 5):icon(22):flat()
l3:addControl("spr_tool_magicwand", 7, 5):icon(22):flat()
l3:addControl("spr_tool_picker", 8, 5):icon(22):flat()
l3:addControl("spr_tool_pen", 9, 5):icon(22):flat()
l3:addControl("spr_tool_eraser", 10, 5):icon(22):flat()
l3:addControl("spr_tool_remap_pen", 11, 5):icon(22):flat()
l3:addControl("spr_tool_paintbucket", 12, 5):icon(22):flat()
l3:addControl("spr_foreground", 13, 5):size(26, 26)
l3:addControl("spr_background", 14, 5):size(26, 26)
l3:addVerticalSpacer(15, 0)

l4:addControl("spr_add", 0, 0):icon(22):flat()
l4:addControl("spr_openimg", 0, 1):icon(22):flat()
l4:addControl("spr_saveimgas", 0, 2):icon(22):flat()
l4:addControl("spr_save", 0, 3):icon(22):flat()
l4:addControl("spr_duplicate", 0, 4):icon(22):flat()
l4:addControl("spr_delete", 0, 5):icon(22):flat()
l4:addControl("spr_delete_grp", 0, 6):icon(22):flat()
l4:addControl("edit_find_replace", 1, 0):icon(22):flat()
l4:addControl("spr_crop", 1, 1):icon(22):flat()
l4:addControl("spr_rename_grp", 1, 2):icon(22):flat()
l4:addControl("spr_saveimg", 0, 1):icon(22):flat()
l4:addControl("spr_layer_save", 0, 2):icon(22):flat()
l4:addControl("spr_layer_play", 0, 6):icon(22):flat()
l4:addControl("spr_explorer", 1, 5):icon(22):flat()
l4:addControl("spr_align", 1, 5):icon(22):flat()
l4:addControl("spr_edit_img", 1, 6):icon(22):flat()

l5:addControl("spr_grp_lb", 0, 0, 1, 2)
l5:addControl("spr_grp", 1, 0, 1, 2)
l5:addControl("spr_num_lb", 0, 2, 1, 2)
l5:addControl("spr_num", 1, 2, 1, 2)
l5:addControl("spr_xaxis_lb", 2, 0)
l5:addControl("spr_xaxis", 2, 1)
l5:addControl("spr_yaxis_lb", 2, 2)
l5:addControl("spr_yaxis", 2, 3)

l6:addLayout(l7, 0, 0, 1, -1)
l6:addControl("pal_sel", 1, 0, 1, -1):horizontal()
l6:addControl("pal_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l6:addControl("pal_grp_lb", 3, 0, 1, 2)
l6:addControl("pal_grp", 4, 0, 1, 2)
l6:addControl("pal_num_lb", 3, 2, 1, 2)
l6:addControl("pal_num", 4, 2, 1, 2)
l6:addControl("pal_view", 5, 0, 1, -1)
l6:addVerticalSpacer(6, 0)

l7:addControl("pal_add", 0, 0):icon(22):flat()
l7:addControl("pal_open", 0, 1):icon(22):flat()
l7:addControl("pal_save", 0, 2):icon(22):flat()
l7:addControl("pal_delete", 0, 3):icon(22):flat()
l7:addControl("pal_switch_count", 0, 4):icon(22):flat()
l7:addControl("pal_remap", 0, 5):icon(22):flat()
l7:addControl("pal_undo", 1, 0):icon(22):flat()
l7:addControl("pal_redo", 1, 1):icon(22):flat()
l7:addControl("pal_switch_color", 1, 2):icon(22):flat()
l7:addControl("pal_pick_color", 1, 3):icon(22):flat()
l7:addControl("pal_set_color", 1, 4):icon(22):flat()
l7:addControl("pal_apply_sprite", 1, 5):icon(22):flat()

l8:addLayout(l9, 0, 0, 1, -1)
l8:addControl("anim_delete", 1, 0):icon(22):flat()
l8:addControl("anim_sel", 1, 1, 1, 6):horizontal()
l8:addControl("anim_add", 1, 7):icon(22):flat()
l8:addControl("anim_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l8:addControl("anim_name_lb", 3, 0, 1, 2)
l8:addControl("anim_name", 3, 2, 1, 6)
l8:addControl("anim_number_lb", 4, 0, 1, 2)
l8:addControl("anim_number", 4, 2, 1, 5)
l8:addControl("edit_find_replace", 4, 7)
l8:addControl("anim_frame_lb", 5, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l8:addControl("anim_frame_delete", 6, 0):icon(22):flat()
l8:addControl("anim_frame_sel", 6, 1, 1, 6):horizontal()
l8:addControl("anim_frame_add", 6, 7):icon(22):flat()
l8:addControl("anim_frame_sel_lb", 7, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l8:addControl("anim_frame_grp_lb", 8, 0, 1, 4)
l8:addControl("anim_frame_grp", 9, 0, 1, 4)
l8:addControl("anim_frame_num_lb", 8, 4, 1, 4)
l8:addControl("anim_frame_num", 9, 4, 1, 4)
l8:addControl("anim_frame_xaxis_lb", 10, 0, 1, 2)
l8:addControl("anim_frame_xaxis", 10, 2, 1, 2)
l8:addControl("anim_frame_yaxis_lb", 10, 4, 1, 2)
l8:addControl("anim_frame_yaxis", 10, 6, 1, 2)
l8:addControl("anim_frame_time_lb", 11, 0, 1, 3)
l8:addControl("anim_frame_time", 12, 0, 1, 3)
l8:addControl("anim_frameall_time", 12, 3):icon(22):flat()
l8:addControl("anim_frame_flip_lb", 11, 4, 1, 3)
l8:addControl("anim_frame_flip", 12, 4, 1, 3)
l8:addControl("anim_frameall_flip", 12, 7):icon(22):flat()
l8:addControl("anim_frame_trans_lb", 13, 0, 1, 3)
l8:addControl("anim_frame_trans", 14, 0, 1, 3)
l8:addControl("anim_frame_transsrc_lb", 13, 3, 1, 2)
l8:addControl("anim_frame_transsrc", 14, 3, 1, 2)
l8:addControl("anim_frame_transdst_lb", 13, 5, 1, 2)
l8:addControl("anim_frame_transdst", 14, 5, 1, 2)
l8:addControl("anim_frameall_trans", 14, 7):icon(22):flat()
l8:addControl("anim_frame_loop", 15, 0, 1, -1)
l8:addLayout(l10, 16, 0, 1, -1)
l8:addLayout(l11, 17, 0, 1, -1)
l8:addControl("anim_frame_intp", 18, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l8:addControl("anim_frame_intp_blend", 19, 0, 1, 4)
l8:addControl("anim_frame_intp_offset", 19, 4, 1, 4)
l8:addControl("anim_frame_intp_scale", 20, 0, 1, 4)
l8:addControl("anim_frame_intp_angle", 20, 4, 1, 4)
l8:addControl("anim_onion_skin", 21, 0, 1, -1)
l8:addControl("anim_onion_sel", 22, 0, 1, 7):horizontal()
l8:addControl("anim_onion_find", 22, 7)
l8:addControl("anim_onion_framesel", 23, 0, 1, -1):horizontal()
l8:addControl("anim_onion_lb", 24, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l8:addVerticalSpacer(25, 0)

l9:addControl("anim_new"):icon(22):flat()
l9:addControl("anim_open"):icon(22):flat()
l9:addControl("anim_save"):icon(22):flat()
l9:addControl("anim_saveas"):icon(22):flat()
l9:addControl("anim_code"):icon(22):flat()
l9:addControl("anim_save_gif"):icon(22):flat()
l9:addControl("anim_play"):icon(22):flat()
l9:addControl("anim_stop"):icon(22):flat()

l10:addControl("anim_box_add_attack"):icon(22):flat()
l10:addControl("anim_box_add_collision"):icon(22):flat()
l10:addControl("anim_box_del"):icon(22):flat()
l10:addControl("anim_box_prev"):icon(22):flat()
l10:addControl("anim_box_type"):icon(22):flat()
l10:addControl("anim_box_default"):icon(22):flat()

l11:addControl("anim_frame_xscale_lb", 0, 0, 1, 3)
l11:addControl("anim_frame_xscale", 1, 0, 1, 3)
l11:addControl("anim_frame_yscale_lb", 0, 3, 1, 3)
l11:addControl("anim_frame_yscale", 1, 3, 1, 3)
l11:addControl("anim_frame_angle_lb", 0, 6, 1, 3)
l11:addControl("anim_frame_angle", 1, 6, 1, 3)

l12:addLayout(l13, 0)
l12:addControl("thumb_view")

l13:addControl("thumb_grp_list", 0, 0, 1, 3)
l13:addControl("thumb_action1", 0, 3):icon(22):flat()
l13:addControl("thumb_action2", 0, 4):icon(22):flat()
l13:addControl("thumb_action3", 0, 5):icon(22):flat()
l13:addControl("thumb_action4", 0, 6):icon(22):flat()
l13:addControl("thumb_save", 0, 7):icon(22):flat()

l14:addLayout(l15, 1)
l14:addControl("st_tree")

l15:addControl("st_new", 0, 0):icon(22):flat()
l15:addControl("st_open", 0, 1):icon(22):flat()
l15:addControl("st_save", 0, 2):icon(22):flat()
l15:addControl("st_saveas", 0, 3):icon(22):flat()
l15:addControl("edit_cut", 0, 4):icon(22):flat()
l15:addControl("edit_copy", 0, 5):icon(22):flat()
l15:addControl("edit_paste", 0, 6):icon(22):flat()
l15:addControl("edit_find_replace", 1, 0):icon(22):flat()
l15:addControl("edit_undo", 1, 1):icon(22):flat()
l15:addControl("edit_redo", 1, 2):icon(22):flat()
l15:addControl("st_palfx", 1, 3):icon(22):flat()
l15:addControl("st_offset", 1, 4):icon(22):flat()
l15:addControl("st_throw", 1, 5):icon(22):flat()
l15:addControl("st_var_log", 1, 6):icon(22):flat()

l16:addLayout(l17, 1)
l16:addControl("cmd_tree")

l17:addControl("cmd_new", 0, 0):icon(22):flat()
l17:addControl("cmd_open", 0, 1):icon(22):flat()
l17:addControl("cmd_save", 0, 2):icon(22):flat()
l17:addControl("cmd_saveas", 0, 3):icon(22):flat()
l17:addControl("edit_cut", 0, 4):icon(22):flat()
l17:addControl("edit_copy", 0, 5):icon(22):flat()
l17:addControl("edit_paste", 0, 6):icon(22):flat()
l17:addControl("edit_find_replace", 1, 0):icon(22):flat()
l17:addControl("edit_undo", 1, 1):icon(22):flat()
l17:addControl("edit_redo", 1, 2):icon(22):flat()

l18:addLayout(l19, 1)
l18:addControl("snd_sel"):horizontal()
l18:addControl("snd_sel_lb"):align(FF.Align.HCenter, FF.Align.VCenter)
l18:addLayout(l20, 1)
l18:addLayout(l21, 1)
local box_snd_selection = l18:addControl("snd_selection")
l18:addVerticalSpacer()

l19:addControl("snd_new", 0, 0):icon(22):flat()
l19:addControl("snd_open", 0, 1):icon(22):flat()
l19:addControl("snd_save", 0, 2):icon(22):flat()
l19:addControl("snd_saveas", 0, 3):icon(22):flat()
l19:addControl("snd_play", 0, 4):icon(22):flat()
l19:addControl("snd_play_loop", 0, 5):icon(22):flat()
l19:addControl("snd_pause", 0, 6):icon(22):flat()
l19:addControl("snd_add", 1, 0):icon(22):flat()
l19:addControl("snd_open_sample", 1, 1):icon(22):flat()
l19:addControl("snd_save_sample", 1, 2):icon(22):flat()
l19:addControl("snd_duplicate", 1, 3):icon(22):flat()
l19:addControl("snd_delete", 1, 4):icon(22):flat()
l19:addControl("snd_stop", 1, 6):icon(22):flat()

l20:addControl("snd_grp_lb", 0, 0, 1, 2)
l20:addControl("snd_grp", 0, 2, 1, 6)
l20:addControl("snd_num_lb", 1, 0, 1, 2)
l20:addControl("snd_num", 1, 2, 1, 6)
l20:addControl("snd_info_lb", 2, 0, 1, 7)
l20:addControl("snd_volume", 2, 7):size(25, 100):vertical()

l21:addControl("snd_tool_offset", 0, 0):icon(22):flat()
l21:addControl("snd_tool_zoom", 0, 1):icon(22):flat()
l21:addControl("snd_tool_select", 0, 2):icon(22):flat()
l21:addControl("snd_tool_move", 0, 3):icon(22):flat()
l21:addControl("snd_tool_hresize", 0, 4):icon(22):flat()
l21:addControl("snd_tool_vresize", 0, 5):icon(22):flat()
l21:addControl("snd_curr_sample_lb", 7, 0, 1, 3)
l21:addControl("snd_curr_sample", 7, 3, 1, 3)
l21:addControl("snd_save_changes", 7, 6):icon(22):flat()
l21:addControl("snd_discard_changes", 7, 7):icon(22):flat()

l22:addControl("snd_start_lb", 0, 0)
l22:addControl("snd_end_lb", 0, 1)
l22:addControl("snd_sel_start", 1, 0)
l22:addControl("snd_sel_end", 1, 1)

l23:addLayout(l24, 0, 0, 1, -1)
l23:addControl("bgs_delete", 1, 0):icon(22):flat()
l23:addControl("bgs_sel", 1, 1, 1, 6):horizontal()
l23:addControl("bgs_add", 1, 7):icon(22):flat()
l23:addControl("bgs_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l23:addControl("bgs_name_lb", 3, 0, 1, 5)
l23:addControl("bgs_name", 4, 0, 1, 5)
l23:addControl("bgs_type_lb", 3, 5, 1, 3)
l23:addControl("bgs_type", 4, 5, 1, 3)
l23:addLayout(l25, 5, 0, 1, -1)
l23:addLayout(l28, 6, 0, 1, -1)
l23:addLayout(l31, 7, 0, 1, -1)
l23:addLayout(l34, 8, 0, 1, -1)
l23:addControl("bgs_trans_lb", 9, 0, 1, 4)
l23:addControl("bgs_trans", 10, 0, 1, 4)
l23:addControl("bgs_transsrc_lb", 9, 4, 1, 2)
l23:addControl("bgs_transsrc", 10, 4, 1, 2)
l23:addControl("bgs_transdst_lb", 9, 6, 1, 2)
l23:addControl("bgs_transdst", 10, 6, 1, 2)
local box_bgs_parallax = l23:addControl("bgs_parallax", 11, 0, 1, -1)
l23:addLayout(l38, 12, 0, 1, -1)
l23:addLayout(l41, 13, 0, 1, -1)
local box_bgs_window = l23:addControl("bgs_window", 14, 0, 1, -1)
local box_bgs_sin_x = l23:addControl("bgs_sin_x", 15, 0, 1, -1)
local box_bgs_sin_y = l23:addControl("bgs_sin_y", 16, 0, 1, -1)
l23:addVerticalSpacer(17, 0)

l24:addControl("bgs_duplicate"):icon(22):flat()
l24:addControl("bgs_play"):icon(22):flat()
l24:addControl("bgs_pause"):icon(22):flat()
l24:addControl("bgs_stop"):icon(22):flat()

local box_bgs_spr = l25:addControl("bgs_spr")
l25:addLayout(l27, 0)

l26:addControl("bgs_grp_lb", 0, 0)
l26:addControl("bgs_spr_grp", 0, 1)
l26:addControl("bgs_num_lb", 1, 0)
l26:addControl("bgs_spr_num", 1, 1)

l27:addControl("bgs_anim_no_lb")
l27:addControl("bgs_anim_no")
l27:addControl("bgs_mask")

local box_bgs_start = l28:addControl("bgs_start")
local box_bgs_delta = l28:addControl("bgs_delta")

l29:addControl("bgs_x_lb", 0, 0)
l29:addControl("bgs_start_x", 0, 1)
l29:addControl("bgs_y_lb", 1, 0)
l29:addControl("bgs_start_y", 1, 1)

l30:addControl("bgs_x_lb", 0, 0)
l30:addControl("bgs_delta_x", 0, 1)
l30:addControl("bgs_y_lb", 1, 0)
l30:addControl("bgs_delta_y", 1, 1)

local box_bgs_velocity = l31:addControl("bgs_velocity")
l31:addLayout(l33, 0)

l32:addControl("bgs_x_lb", 0, 0)
l32:addControl("bgs_velocity_x", 0, 1)
l32:addControl("bgs_y_lb", 1, 0)
l32:addControl("bgs_velocity_y", 1, 1)

l33:addControl("bgs_id_lb", 0, 0)
l33:addControl("bgs_id", 0, 1)
l33:addControl("bgs_layer_lb", 1, 0)
l33:addControl("bgs_layer", 1, 1)
l33:addControl("bgs_pos_link", 2, 0, 1, -1)

local box_bgs_tile = l34:addControl("bgs_tile")
local box_bgs_tile_spacing = l34:addControl("bgs_tile_spacing")

l35:addControl("bgs_x_lb", 0, 0)
l35:addControl("bgs_tile_x", 0, 1)
l35:addControl("bgs_y_lb", 1, 0)
l35:addControl("bgs_tile_y", 1, 1)

l36:addControl("bgs_x_lb", 0, 0)
l36:addControl("bgs_tile_spacing_x", 0, 1)
l36:addControl("bgs_y_lb", 1, 0)
l36:addControl("bgs_tile_spacing_y", 1, 1)

l37:addControl("bgs_mode_lb", 0, 0)
l37:addControl("bgs_parallax_mode", 0, 1, 1, 3)
l37:addControl("bgs_top_lb", 1, 0, 1, 2)
l37:addControl("bgs_parallax_top", 2, 0, 1, 2)
l37:addControl("bgs_bottom_lb", 1, 2, 1, 2)
l37:addControl("bgs_parallax_bottom", 2, 2, 1, 2)

local box_bgs_scale_start = l38:addControl("bgs_scale_start")
local box_bgs_scale_delta = l38:addControl("bgs_scale_delta")

l39:addControl("bgs_x_lb", 0, 0)
l39:addControl("bgs_scale_start_x", 0, 1)
l39:addControl("bgs_y_lb", 1, 0)
l39:addControl("bgs_scale_start_y", 1, 1)

l40:addControl("bgs_x_lb", 0, 0)
l40:addControl("bgs_scale_delta_x", 0, 1)
l40:addControl("bgs_y_lb", 1, 0)
l40:addControl("bgs_scale_delta_y", 1, 1)

local box_bgs_y_scale = l41:addControl("bgs_y_scale")
local box_bgs_window_delta = l41:addControl("bgs_window_delta")

l42:addControl("bgs_start_lb", 0, 0)
l42:addControl("bgs_y_scale_start", 0, 1)
l42:addControl("bgs_delta_lb", 1, 0)
l42:addControl("bgs_y_scale_delta", 1, 1)

l43:addControl("bgs_x_lb", 0, 0)
l43:addControl("bgs_window_delta_x", 0, 1)
l43:addControl("bgs_y_lb", 1, 0)
l43:addControl("bgs_window_delta_y", 1, 1)

l44:addControl("bgs_mode_lb", 0, 0)
l44:addControl("bgs_window_mode", 0, 1, 1, 3)
l44:addControl("bgs_top_lb", 1, 0)
l44:addControl("bgs_window_top", 2, 0)
l44:addControl("bgs_left_lb", 1, 1)
l44:addControl("bgs_window_left", 2, 1)
l44:addControl("bgs_bottom_lb", 1, 2)
l44:addControl("bgs_window_bottom", 2, 2)
l44:addControl("bgs_right_lb", 1, 3)
l44:addControl("bgs_window_right", 2, 3)

l45:addControl("bgs_sin_x_ampl_lb", 0, 0)
l45:addControl("bgs_sin_x_ampl", 1, 0)
l45:addControl("bgs_sin_x_period_lb", 0, 1)
l45:addControl("bgs_sin_x_period", 1, 1)
l45:addControl("bgs_sin_x_offset_lb", 0, 2)
l45:addControl("bgs_sin_x_offset", 1, 2)

l46:addControl("bgs_sin_y_ampl_lb", 0, 0)
l46:addControl("bgs_sin_y_ampl", 1, 0)
l46:addControl("bgs_sin_y_period_lb", 0, 1)
l46:addControl("bgs_sin_y_period", 1, 1)
l46:addControl("bgs_sin_y_offset_lb", 0, 2)
l46:addControl("bgs_sin_y_offset", 1, 2)

l47:addControl("emul_start", 0, 0, 2, 2):icon(44):flat()
l47:addControl("emul_engine_lb", 0, 2, 1, 6)
l47:addControl("emul_engine", 1, 2, 1, 6)
local box_emul_match = l47:addControl("emul_match", 2, 0, 1, -1)
local box_emul_training = l47:addControl("emul_training", 3, 0, 1, -1)
l47:addControl("emul_screen_lb", 4, 0, 1, -1)
l47:addControl("emul_screen", 5, 0, 1, -1)
l47:addVerticalSpacer(6, 0)

l48:addControl("emul_match_mode_lb", 0, 0)
l48:addControl("emul_match_mode", 1, 0)
l48:addControl("emul_enemy_lb", 0, 1)
l48:addControl("emul_enemy", 1, 1)
l48:addControl("emul_rounds_lb", 2, 0)
l48:addControl("emul_rounds", 3, 0)
l48:addControl("emul_time_lb", 2, 1)
l48:addControl("emul_time", 3, 1)
l48:addControl("emul_explod_mode_lb", 4, 0, 1, -1)
l48:addControl("emul_explod_mode", 5, 0, 1, -1)
l48:addControl("emul_debug_info", 6, 0, 1, -1)

l49:addControl("emul_dummy_ctrl_lb", 0, 0)
l49:addControl("emul_dummy_ctrl", 1, 0)
l49:addControl("emul_dummy_mode_lb", 0, 1)
l49:addControl("emul_dummy_mode", 1, 1)
l49:addControl("emul_guard_mode_lb", 2, 0)
l49:addControl("emul_guard_mode", 3, 0)
l49:addControl("emul_distance_lb", 2, 1)
l49:addControl("emul_distance", 3, 1)
l49:addControl("emul_button_jam_lb", 4, 0)
l49:addControl("emul_button_jam", 5, 0)
l49:addControl("emul_ai_level_lb", 4, 1)
l49:addControl("emul_ai_level", 5, 1)

l50:addLayout(l51, 0)
l50:addControl("org_list")

l51:addControl("org_up", 0, 0):icon(22):flat()
l51:addControl("org_down", 0, 1):icon(22):flat()
l51:addControl("org_up_10", 0, 2):icon(22):flat()
l51:addControl("org_down_10", 0, 3):icon(22):flat()
l51:addControl("org_top", 0, 4):icon(22):flat()
l51:addControl("org_bottom", 0, 5):icon(22):flat()
l51:addControl("org_auto", 0, 6):icon(22):flat()
l51:addControl("org_palette", 0, 7):icon(22):flat()

l52:addLayout(l53, 0, 0, 1, -1)
l52:addControl("bgctrls_def_delete", 1, 0):icon(22):flat()
l52:addControl("bgctrls_def_sel", 1, 1, 1, 6):horizontal()
l52:addControl("bgctrls_def_add", 1, 7):icon(22):flat()
l52:addControl("bgctrls_def_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l52:addControl("bgctrls_def_name_lb", 3, 0, 1, 5)
l52:addControl("bgctrls_def_name", 4, 0, 1, 5)
l52:addControl("bgctrls_def_loop_lb", 3, 5, 1, 3)
l52:addControl("bgctrls_def_loop", 4, 5, 1, 3)
local box_bgctrls_def_ids = l52:addControl("bgctrls_def_ids", 5, 0, 1, -1)
l52:addControl("bgctrls_delete", 6, 0):icon(22):flat()
l52:addControl("bgctrls_sel", 6, 1, 1, 6):horizontal()
l52:addControl("bgctrls_add", 6, 7):icon(22):flat()
l52:addControl("bgctrls_sel_lb", 7, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l52:addControl("bgctrls_name_lb", 8, 0, 1, 4)
l52:addControl("bgctrls_name", 9, 0, 1, 4)
l52:addControl("bgctrls_type_lb", 8, 4, 1, 4)
l52:addControl("bgctrls_type", 9, 4, 1, 4)
local box_bgctrls_time = l52:addControl("bgctrls_time", 10, 0, 1, -1)
l52:addControl("bgctrls_bool", 11, 0, 1, -1)
l52:addControl("bgctrls_anim_no_lb", 12, 0, 1, 4)
l52:addControl("bgctrls_anim_no", 12, 4, 1, 4)
l52:addControl("bgctrls_x_lb", 13, 0)
l52:addControl("bgctrls_x", 13, 1, 1, 3)
l52:addControl("bgctrls_y_lb", 13, 4)
l52:addControl("bgctrls_y", 13, 5, 1, 3)
l52:addLayout(l56, 14, 0, 1, -1)
l52:addVerticalSpacer(15, 0)

l54:addControl("bgctrls_def_id_1", 0, 0)
l54:addControl("bgctrls_def_id_2", 0, 1)
l54:addControl("bgctrls_def_id_3", 0, 2)
l54:addControl("bgctrls_def_id_4", 0, 3)
l54:addControl("bgctrls_def_id_5", 0, 4)
l54:addControl("bgctrls_def_id_6", 1, 0)
l54:addControl("bgctrls_def_id_7", 1, 1)
l54:addControl("bgctrls_def_id_8", 1, 2)
l54:addControl("bgctrls_def_id_9", 1, 3)
l54:addControl("bgctrls_def_id_10", 1, 4)

l55:addControl("bgctrls_start_lb", 0, 0)
l55:addControl("bgctrls_start_time", 1, 0)
l55:addControl("bgctrls_end_lb", 0, 1)
l55:addControl("bgctrls_end_time", 1, 1)
l55:addControl("bgctrls_loop_lb", 0, 2)
l55:addControl("bgctrls_loop_time", 1, 2)

l56:addControl("bgctrls_ampl_lb", 0, 0)
l56:addControl("bgctrls_ampl", 1, 0)
l56:addControl("bgctrls_period_lb", 0, 1)
l56:addControl("bgctrls_period", 1, 1)
l56:addControl("bgctrls_offset_lb", 0, 2)
l56:addControl("bgctrls_offset", 1, 2)

l57:addLayout(l58, 0)
l57:addControl("debug_variables")

l58:addControl("debug_start", 0, 0):icon(22):flat()
l58:addControl("debug_interrupt", 0, 1):icon(22):flat()
l58:addControl("debug_stop", 0, 2):icon(22):flat()
l58:addControl("debug_restart", 0, 3):icon(22):flat()
l58:addControl("debug_step_over", 0, 4):icon(22):flat()
l58:addControl("debug_step_into", 0, 5):icon(22):flat()
l58:addControl("debug_step_out", 0, 6):icon(22):flat()
l58:addControl("debug_step_frame", 0, 7):icon(22):flat()

-- Panels
FF.panel("definitions").layout = l1
FF.panel("sprites").layout = l3
FF.extraPanel("palettes").layout = l6
FF.panel("animations").layout = l8
FF.extraPanel("thumbnails").layout = l12
FF.panel("states").layout = l14
FF.panel("commands").layout = l16
FF.panel("sounds").layout = l18
box_snd_selection:layout(l22)
FF.panel("backgrounds").layout = l23
box_bgs_spr:layout(l26)
box_bgs_start:layout(l29)
box_bgs_delta:layout(l30)
box_bgs_velocity:layout(l32)
box_bgs_tile:layout(l35)
box_bgs_tile_spacing:layout(l36)
box_bgs_parallax:layout(l37)
box_bgs_scale_start:layout(l39)
box_bgs_scale_delta:layout(l40)
box_bgs_y_scale:layout(l42)
box_bgs_window_delta:layout(l43)
box_bgs_window:layout(l44)
box_bgs_sin_x:layout(l45)
box_bgs_sin_y:layout(l46)
FF.panel("emulator").layout = l47
box_emul_match:layout(l48)
box_emul_training:layout(l49)
FF.extraPanel("organizer").layout = l50
FF.extraPanel("controllers").layout = l52
box_bgctrls_def_ids:layout(l54)
box_bgctrls_time:layout(l55)
FF.extraPanel("debug").layout = l57
