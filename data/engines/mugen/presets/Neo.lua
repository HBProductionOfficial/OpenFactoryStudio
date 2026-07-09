-- Interface preset "Neo".

-- Toolbars
local mainBar = FF.toolBar("main")
mainBar.iconSize = 32
mainBar:addButton("file_new")
mainBar:addButton("file_close")
mainBar:addButton("file_open")
mainBar:addButton("file_save")
mainBar:addSeparator()
mainBar:addButton("file_load_backup")
mainBar:addButton("file_backup")
mainBar:addSeparator()
mainBar:addButton("file_run")
mainBar:addButton("debug_start_engine")
mainBar:addSeparator()
mainBar:addButton("editor_def")
mainBar:addButton("editor_spr")
mainBar:addButton("editor_anim")
mainBar:addButton("editor_bg")
mainBar:addButton("editor_cmd")
mainBar:addButton("editor_st")
mainBar:addButton("editor_snd")
mainBar:addButton("editor_emul")
mainBar:addButton("ref_docs")
mainBar:addSeparator()
mainBar:addButton("tools_options")

local extraBar = FF.toolBar("extra")
extraBar.iconSize = 22
extraBar:addButton("editor_new")
extraBar:addButton("editor_open")
extraBar:addButton("editor_save")
extraBar:addButton("editor_saveas")
extraBar:addSeparator()
extraBar:addButton("edit_undo")
extraBar:addButton("edit_redo")
extraBar:addSeparator()
extraBar:addButton("edit_cut")
extraBar:addButton("edit_copy")
extraBar:addButton("edit_paste")
extraBar:addSeparator()
extraBar:addButton("edit_find_replace")

local codeBar = FF.toolBar("code")
codeBar:addButton("edit_find_replace_multi")
codeBar:addSeparator()
codeBar:addButton("code_goto")
codeBar:addButton("code_prev_error")
codeBar:addButton("code_next_error")
codeBar:addButton("code_comment")

local graphicsBar = FF.toolBar("graphics")
graphicsBar:addSeparator()
graphicsBar:addButton("view_organizer")
graphicsBar:addSeparator()
graphicsBar:addButton("view_zoomreset")
graphicsBar:addButton("view_zoomfit")
graphicsBar:addButton("view_zoomout")
graphicsBar:addButton("view_zoomin")
graphicsBar:addButton("view_center_origin")
graphicsBar:addButton("view_guide_lines")
graphicsBar:addButton("view_attack_box")
graphicsBar:addButton("view_collision_box")

local soundBar = FF.toolBar("sound")
soundBar:addSeparator()
soundBar:addButton("view_organizer")
soundBar:addSeparator()
soundBar:addButton("view_zoomreset")
soundBar:addButton("view_zoomfit")
soundBar:addButton("view_zoomout")
soundBar:addButton("view_zoomin")

-- Layouts
local l1 = FF.verticalLayout()
l1.margin = 8
local l2 = FF.gridLayout()
l2.margin = 8
l2.spacing = 4
local l3 = FF.horizontalLayout()
local l4 = FF.verticalLayout()
l4.spacing = 4
local l5 = FF.gridLayout()
l5.margin = 8
l5.spacing = 6
local l6 = FF.gridLayout()
local l7 = FF.gridLayout()
l7.margin = 8
l7.spacing = 4
local l8 = FF.horizontalLayout()
local l9 = FF.gridLayout()
l9.margin = 0
l9.spacing = 4
local l10 = FF.gridLayout()
l10.margin = 0
l10.spacing = 4
local l11 = FF.horizontalLayout()
local l12 = FF.verticalLayout()
l12.margin = 8
l12.spacing = 6
local l13 = FF.gridLayout()
local l14 = FF.verticalLayout()
l14.margin = 8
local l15 = FF.gridLayout()
l15.margin = 8
l15.spacing = 4
local l16 = FF.gridLayout()
l16.margin = 8
l16.spacing = 4
local l17 = FF.horizontalLayout()
local l18 = FF.gridLayout()
l18.margin = 8
l18.spacing = 4
local l19 = FF.gridLayout()
l19.margin = 8
l19.spacing = 4
local l20 = FF.horizontalLayout()
local l21 = FF.horizontalLayout()
l21.margin = 0
l21.spacing = 4
local l22 = FF.gridLayout()
l22.margin = 8
l22.spacing = 4
local l23 = FF.verticalLayout()
l23.margin = 8
l23.spacing = 4
local l24 = FF.horizontalLayout()
l24.margin = 0
l24.spacing = 4
local l25 = FF.gridLayout()
l25.margin = 8
l25.spacing = 4
local l26 = FF.gridLayout()
l26.margin = 8
l26.spacing = 4
local l27 = FF.horizontalLayout()
l27.margin = 0
l27.spacing = 4
local l28 = FF.gridLayout()
l28.margin = 8
l28.spacing = 4
local l29 = FF.gridLayout()
l29.margin = 0
l29.spacing = 4
local l30 = FF.horizontalLayout()
l30.margin = 0
l30.spacing = 4
local l31 = FF.gridLayout()
l31.margin = 8
l31.spacing = 4
local l32 = FF.gridLayout()
l32.margin = 8
l32.spacing = 4
local l33 = FF.gridLayout()
l33.margin = 8
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
local l37 = FF.horizontalLayout()
l37.margin = 0
l37.spacing = 4
local l38 = FF.gridLayout()
l38.margin = 8
l38.spacing = 4
local l39 = FF.gridLayout()
l39.margin = 8
l39.spacing = 4
local l40 = FF.gridLayout()
l40.margin = 8
l40.spacing = 4
local l41 = FF.gridLayout()
l41.margin = 8
l41.spacing = 4
local l42 = FF.gridLayout()
l42.margin = 8
l42.spacing = 4
local l43 = FF.gridLayout()
l43.margin = 8
l43.spacing = 6
local l44 = FF.gridLayout()
l44.margin = 8
l44.spacing = 6
local l45 = FF.gridLayout()
l45.margin = 8
l45.spacing = 6
local l46 = FF.verticalLayout()
l46.margin = 8
l46.spacing = 6
local l47 = FF.gridLayout()
local l48 = FF.gridLayout()
l48.margin = 8
l48.spacing = 4
local l49 = FF.horizontalLayout()
local l50 = FF.gridLayout()
l50.margin = 8
l50.spacing = 4
local l51 = FF.horizontalLayout()
l51.margin = 0
l51.spacing = 4
local l52 = FF.gridLayout()
l52.margin = 8
l52.spacing = 4
local l53 = FF.gridLayout()
l53.margin = 8
l53.spacing = 4
local l54 = FF.verticalLayout()
l54.margin = 8
l54.spacing = 6
local l55 = FF.gridLayout()

l1:addControl("def_tree")

l2:addLayout(l3, 0, 0, 1, -1)
l2:addControl("spr_sel", 1, 0, 1, -1):horizontal()
l2:addControl("spr_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l2:addControl("spr_name_lb", 3, 0, 1, -1)
l2:addControl("spr_name", 4, 0, 1, -1)
l2:addControl("spr_prev_grp", 6, 0):icon(22):flat()
l2:addControl("spr_grp_lb", 5, 1, 1, 3)
l2:addControl("spr_grp", 6, 1, 1, 3)
l2:addControl("spr_next_grp", 6, 4):icon(22):flat()
l2:addControl("spr_num_lb", 5, 5, 1, 3)
l2:addControl("spr_num", 6, 5, 1, 3)
l2:addControl("spr_xaxis_lb", 7, 0, 1, 2)
l2:addControl("spr_xaxis", 7, 2, 1, 3)
l2:addControl("spr_yaxis_lb", 8, 0, 1, 2)
l2:addControl("spr_yaxis", 8, 2, 1, 3)
l2:addControl("spr_pos_left", 7, 5, 2, 1):icon(22):flat()
l2:addControl("spr_pos_up", 7, 6):icon(22):flat()
l2:addControl("spr_pos_right", 7, 7, 2, 1):icon(22):flat()
l2:addControl("spr_pos_down", 8, 6):icon(22):flat()
l2:addLayout(l4, 9, 0, 1, -1)
l2:addControl("spr_layers_menu", 3, 0):icon(22):flat()
l2:addControl("spr_layers_blendmode", 3, 1, 1, 4)
l2:addControl("spr_layers_opacity", 3, 5, 1, 2)
l2:addControl("spr_layers_list", 4, 0, 11, 7)
l2:addControl("spr_tool_canvasmove", 3, 7):icon(22):flat()
l2:addControl("spr_tool_selmove", 4, 7):icon(22):flat()
l2:addControl("spr_tool_rectsel", 5, 7):icon(22):flat()
l2:addControl("spr_tool_freesel", 6, 7):icon(22):flat()
l2:addControl("spr_tool_magicwand", 7, 7):icon(22):flat()
l2:addControl("spr_tool_picker", 8, 7):icon(22):flat()
l2:addControl("spr_tool_pen", 9, 7):icon(22):flat()
l2:addControl("spr_tool_eraser", 10, 7):icon(22):flat()
l2:addControl("spr_tool_remap_pen", 11, 7):icon(22):flat()
l2:addControl("spr_tool_paintbucket", 12, 7):icon(22):flat()
l2:addControl("spr_foreground", 13, 7):size(26, 26)
l2:addControl("spr_background", 14, 7):size(26, 26)
l2:addVerticalSpacer(15, 0)

l3:addControl("spr_add"):icon(22):flat()
l3:addControl("spr_delete"):icon(22):flat()
l3:addControl("spr_duplicate"):icon(22):flat()
l3:addControl("spr_delete_grp"):icon(22):flat()
l3:addControl("spr_rename_grp"):icon(22):flat()
l3:addControl("spr_saveimg"):icon(22):flat()
l3:addControl("spr_openimg"):icon(22):flat()
l3:addControl("spr_saveimgas"):icon(22):flat()
l3:addControl("spr_layer_save"):icon(22):flat()
l3:addControl("spr_layer_play"):icon(22):flat()
l3:addControl("spr_explorer"):icon(22):flat()
l3:addControl("spr_edit_img"):icon(22):flat()

l4:addControl("spr_dec_load")
l4:addControl("spr_trans_color")
l4:addControl("spr_info_lb"):align(FF.Align.HCenter, FF.Align.VCenter)
l4:addControl("spr_onion_skin")
l4:addControl("spr_onion_sel"):horizontal()
l4:addControl("spr_onion_lb"):align(FF.Align.HCenter, FF.Align.VCenter)

l5:addLayout(l6, 0, 0, 1, -1)
l5:addControl("pal_sel", 1, 0, 1, -1):horizontal()
l5:addControl("pal_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l5:addControl("spr_switch_pal", 3, 0, 1, -1)
l5:addControl("pal_grp_lb", 4, 0)
l5:addControl("pal_grp", 5, 0)
l5:addControl("pal_num_lb", 4, 1)
l5:addControl("pal_num", 5, 1)
l5:addControl("pal_view", 6, 0, 1, -1)
l5:addVerticalSpacer(7, 0)

l6:addControl("pal_add", 0, 0):icon(22):flat()
l6:addControl("pal_delete", 0, 1):icon(22):flat()
l6:addControl("pal_open", 0, 2):icon(22):flat()
l6:addControl("pal_save", 0, 3):icon(22):flat()
l6:addControl("pal_undo", 0, 4):icon(22):flat()
l6:addControl("pal_redo", 0, 5):icon(22):flat()
l6:addControl("pal_switch_count", 1, 0):icon(22):flat()
l6:addControl("pal_switch_color", 1, 1):icon(22):flat()
l6:addControl("pal_pick_color", 1, 2):icon(22):flat()
l6:addControl("pal_set_color", 1, 3):icon(22):flat()
l6:addControl("pal_remap", 1, 4):icon(22):flat()
l6:addControl("pal_apply_sprite", 1, 5):icon(22):flat()
l6:addControl("tools_pal_editor", 0, 6, 2, 2):size(42, 56):icon(32):flat()

l7:addLayout(l8, 0, 0, 1, -1)
l7:addControl("anim_sel", 1, 0, 1, -1):horizontal()
l7:addControl("anim_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l7:addControl("anim_number_lb", 3, 0, 1, 3)
l7:addControl("anim_number", 4, 0, 1, 3)
l7:addControl("anim_name_lb", 3, 3, 1, 5)
l7:addControl("anim_name", 4, 3, 1, 5)
l7:addControl("anim_frame_lb", 5, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l7:addControl("anim_frame_sel", 6, 0, 1, -1):horizontal()
l7:addControl("anim_frame_sel_lb", 7, 0, 1, 6):align(FF.Align.HCenter, FF.Align.VCenter)
l7:addControl("anim_frame_add", 7, 6):icon(22):flat()
l7:addControl("anim_frame_delete", 7, 7):icon(22):flat()
l7:addLayout(l9, 8, 0, 3, 5)
l7:addControl("anim_pos_left", 8, 5, 2, 1):icon(22):flat()
l7:addControl("anim_pos_up", 8, 6):icon(22):flat()
l7:addControl("anim_pos_right", 8, 7, 2, 1):icon(22):flat()
l7:addControl("anim_pos_down", 9, 6):icon(22):flat()
l7:addControl("anim_frameall_offset", 10, 5):icon(22):flat()
l7:addControl("anim_frame_align", 10, 7):icon(22):flat()
l7:addControl("anim_frame_time_lb", 11, 0, 1, 3)
l7:addControl("anim_frame_time", 12, 0, 1, 3)
l7:addControl("anim_frameall_time", 12, 3):icon(22):flat()
l7:addControl("anim_frame_flip_lb", 11, 4, 1, 3)
l7:addControl("anim_frame_flip", 12, 4, 1, 3)
l7:addControl("anim_frameall_flip", 12, 7):icon(22):flat()
l7:addControl("anim_frame_trans_lb", 13, 0, 1, 3)
l7:addControl("anim_frame_trans", 14, 0, 1, 3)
l7:addControl("anim_frame_transsrc_lb", 13, 3, 1, 2)
l7:addControl("anim_frame_transsrc", 14, 3, 1, 2)
l7:addControl("anim_frame_transdst_lb", 13, 5, 1, 2)
l7:addControl("anim_frame_transdst", 14, 5, 1, 2)
l7:addControl("anim_frameall_trans", 14, 7):icon(22):flat()
l7:addControl("anim_frame_loop", 15, 0, 1, -1)
l7:addLayout(l10, 16, 0, 1, -1)
l7:addControl("anim_frame_intp", 17, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l7:addControl("anim_frame_intp_blend", 18, 0, 1, 4)
l7:addControl("anim_frame_intp_offset", 18, 4, 1, 4)
l7:addControl("anim_frame_intp_scale", 19, 0, 1, 4)
l7:addControl("anim_frame_intp_angle", 19, 4, 1, 4)
l7:addLayout(l11, 20, 0, 1, -1)
l7:addControl("anim_onion_skin", 21, 0, 1, -1)
l7:addControl("anim_onion_sel", 22, 0, 1, 7):horizontal()
l7:addControl("anim_onion_find", 22, 7)
l7:addControl("anim_onion_framesel", 23, 0, 1, -1):horizontal()
l7:addControl("anim_onion_lb", 24, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l7:addVerticalSpacer(25, 0)

l8:addControl("anim_add"):icon(22):flat()
l8:addControl("anim_delete"):icon(22):flat()
l8:addControl("anim_duplicate"):icon(22):flat()
l8:addControl("anim_code"):icon(22):flat()
l8:addControl("anim_save_gif"):icon(22):flat()
l8:addControl("anim_play"):icon(22):flat()
l8:addControl("anim_stop"):icon(22):flat()
l8:addControl("anim_editcode"):icon(22):flat()

l9:addControl("anim_frame_grp_lb", 0, 0):size(70, 16)
l9:addControl("anim_frame_grp", 1, 0):size(70, 16)
l9:addControl("anim_frame_num_lb", 0, 1):size(70, 16)
l9:addControl("anim_frame_num", 1, 1):size(70, 16)
l9:addControl("anim_frame_xaxis_lb", 2, 0):size(70, 16)
l9:addControl("anim_frame_xaxis", 3, 0):size(70, 16)
l9:addControl("anim_frame_yaxis_lb", 2, 1):size(70, 16)
l9:addControl("anim_frame_yaxis", 3, 1):size(70, 16)

l10:addControl("anim_frame_xscale_lb", 0, 0, 1, 2)
l10:addControl("anim_frame_xscale", 1, 0, 1, 2)
l10:addControl("anim_frame_yscale_lb", 0, 2, 1, 2)
l10:addControl("anim_frame_yscale", 1, 2, 1, 2)
l10:addControl("anim_frameall_scale", 1, 4):icon(22):flat()
l10:addControl("anim_frame_angle_lb", 0, 5, 1, 2)
l10:addControl("anim_frame_angle", 1, 5, 1, 2)

l11:addControl("anim_box_add_attack"):icon(22):flat()
l11:addControl("anim_box_add_collision"):icon(22):flat()
l11:addControl("anim_box_del"):icon(22):flat()
l11:addControl("anim_box_delall"):icon(22):flat()
l11:addControl("anim_box_delall_anim"):icon(22):flat()
l11:addControl("anim_box_prev"):icon(22):flat()
l11:addControl("anim_box_type"):icon(22):flat()
l11:addControl("anim_box_default"):icon(22):flat()

l12:addLayout(l13, 0)
l12:addControl("thumb_view")

l13:addControl("thumb_grp_list", 0, 0, 1, 3)
l13:addControl("thumb_action1", 0, 3):icon(22):flat()
l13:addControl("thumb_action2", 0, 4):icon(22):flat()
l13:addControl("thumb_action3", 0, 5):icon(22):flat()
l13:addControl("thumb_action4", 0, 6):icon(22):flat()
l13:addControl("thumb_save", 0, 7):icon(22):flat()

l14:addControl("cmd_tree")

l15:addControl("st_palfx", 0, 0):icon(22):flat()
l15:addControl("st_offset", 0, 1):icon(22):flat()
l15:addControl("st_throw", 0, 2):icon(22):flat()
l15:addControl("st_var_log", 0, 3):icon(22):flat()
l15:addControl("st_var_list", 0, 4):icon(22):flat()
l15:addControl("st_tree", 1, 0, 1, 8)

l16:addLayout(l17, 0, 0, 1, -1)
l16:addControl("snd_sel", 1, 0, 1, -1):horizontal()
l16:addControl("snd_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l16:addControl("snd_prev_grp", 4, 0):icon(22):flat()
l16:addControl("snd_grp_lb", 3, 1, 1, 3)
l16:addControl("snd_grp", 4, 1, 1, 3)
l16:addControl("snd_next_grp", 4, 4):icon(22):flat()
l16:addControl("snd_num_lb", 3, 5, 1, 3)
l16:addControl("snd_num", 4, 5, 1, 3)
l16:addControl("snd_info_lb", 5, 0, 1, 7)
l16:addControl("snd_volume", 5, 7):vertical()
l16:addControl("snd_tool_offset", 6, 0):icon(22):flat()
l16:addControl("snd_tool_zoom", 6, 1):icon(22):flat()
l16:addControl("snd_tool_select", 6, 2):icon(22):flat()
l16:addControl("snd_tool_move", 6, 3):icon(22):flat()
l16:addControl("snd_tool_hresize", 6, 4):icon(22):flat()
l16:addControl("snd_tool_vresize", 6, 5):icon(22):flat()
l16:addControl("snd_curr_sample_lb", 7, 0, 1, 3)
l16:addControl("snd_curr_sample", 7, 3, 1, 3)
l16:addControl("snd_save_changes", 7, 6):icon(22):flat()
l16:addControl("snd_discard_changes", 7, 7):icon(22):flat()
local box_snd_selection = l16:addControl("snd_selection", 8, 0, 1, -1)
l16:addVerticalSpacer(9, 0)

l17:addControl("snd_add"):icon(22):flat()
l17:addControl("snd_delete"):icon(22):flat()
l17:addControl("snd_open_sample"):icon(22):flat()
l17:addControl("snd_save_sample"):icon(22):flat()
l17:addControl("snd_play"):icon(22):flat()
l17:addControl("snd_play_loop"):icon(22):flat()
l17:addControl("snd_pause"):icon(22):flat()
l17:addControl("snd_stop"):icon(22):flat()

l18:addControl("snd_start_lb", 0, 0)
l18:addControl("snd_end_lb", 0, 1)
l18:addControl("snd_sel_start", 1, 0)
l18:addControl("snd_sel_end", 1, 1)

l19:addLayout(l20, 0, 0, 1, -1)
l19:addControl("bgs_sel", 1, 0, 1, -1):horizontal()
l19:addControl("bgs_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l19:addControl("bgs_name_lb", 3, 0, 1, 5)
l19:addControl("bgs_name", 4, 0, 1, 5)
l19:addControl("bgs_type_lb", 3, 5, 1, 3)
l19:addControl("bgs_type", 4, 5, 1, 3)
l19:addLayout(l21, 5, 0, 1, -1)
l19:addLayout(l24, 6, 0, 1, -1)
l19:addLayout(l27, 7, 0, 1, -1)
l19:addLayout(l30, 8, 0, 1, -1)
l19:addControl("bgs_trans_lb", 9, 0, 1, 4)
l19:addControl("bgs_trans", 10, 0, 1, 4)
l19:addControl("bgs_transsrc_lb", 9, 4, 1, 2)
l19:addControl("bgs_transsrc", 10, 4, 1, 2)
l19:addControl("bgs_transdst_lb", 9, 6, 1, 2)
l19:addControl("bgs_transdst", 10, 6, 1, 2)
local box_bgs_parallax = l19:addControl("bgs_parallax", 11, 0, 1, -1)
l19:addLayout(l34, 12, 0, 1, -1)
l19:addLayout(l37, 13, 0, 1, -1)
local box_bgs_window = l19:addControl("bgs_window", 14, 0, 1, -1)
local box_bgs_sin_x = l19:addControl("bgs_sin_x", 15, 0, 1, -1)
local box_bgs_sin_y = l19:addControl("bgs_sin_y", 16, 0, 1, -1)
l19:addVerticalSpacer(17, 0)

l20:addControl("bgs_add"):icon(22):flat()
l20:addControl("bgs_delete"):icon(22):flat()
l20:addControl("bgs_duplicate"):icon(22):flat()
l20:addControl("bgs_play"):icon(22):flat()
l20:addControl("bgs_pause"):icon(22):flat()
l20:addControl("bgs_stop"):icon(22):flat()

local box_bgs_spr = l21:addControl("bgs_spr")
l21:addLayout(l23, 0)

l22:addControl("bgs_grp_lb", 0, 0)
l22:addControl("bgs_spr_grp", 0, 1)
l22:addControl("bgs_num_lb", 1, 0)
l22:addControl("bgs_spr_num", 1, 1)

l23:addControl("bgs_anim_no_lb")
l23:addControl("bgs_anim_no")
l23:addControl("bgs_mask")

local box_bgs_start = l24:addControl("bgs_start")
local box_bgs_delta = l24:addControl("bgs_delta")

l25:addControl("bgs_x_lb", 0, 0)
l25:addControl("bgs_start_x", 0, 1)
l25:addControl("bgs_y_lb", 1, 0)
l25:addControl("bgs_start_y", 1, 1)

l26:addControl("bgs_x_lb", 0, 0)
l26:addControl("bgs_delta_x", 0, 1)
l26:addControl("bgs_y_lb", 1, 0)
l26:addControl("bgs_delta_y", 1, 1)

local box_bgs_velocity = l27:addControl("bgs_velocity")
l27:addLayout(l29, 0)

l28:addControl("bgs_x_lb", 0, 0)
l28:addControl("bgs_velocity_x", 0, 1)
l28:addControl("bgs_y_lb", 1, 0)
l28:addControl("bgs_velocity_y", 1, 1)

l29:addControl("bgs_id_lb", 0, 0)
l29:addControl("bgs_id", 0, 1)
l29:addControl("bgs_layer_lb", 1, 0)
l29:addControl("bgs_layer", 1, 1)
l29:addControl("bgs_pos_link", 2, 0, 1, -1)

local box_bgs_tile = l30:addControl("bgs_tile")
local box_bgs_tile_spacing = l30:addControl("bgs_tile_spacing")

l31:addControl("bgs_x_lb", 0, 0)
l31:addControl("bgs_tile_x", 0, 1)
l31:addControl("bgs_y_lb", 1, 0)
l31:addControl("bgs_tile_y", 1, 1)

l32:addControl("bgs_x_lb", 0, 0)
l32:addControl("bgs_tile_spacing_x", 0, 1)
l32:addControl("bgs_y_lb", 1, 0)
l32:addControl("bgs_tile_spacing_y", 1, 1)

l33:addControl("bgs_mode_lb", 0, 0)
l33:addControl("bgs_parallax_mode", 0, 1, 1, 3)
l33:addControl("bgs_top_lb", 1, 0, 1, 2)
l33:addControl("bgs_parallax_top", 2, 0, 1, 2)
l33:addControl("bgs_bottom_lb", 1, 2, 1, 2)
l33:addControl("bgs_parallax_bottom", 2, 2, 1, 2)

local box_bgs_scale_start = l34:addControl("bgs_scale_start")
local box_bgs_scale_delta = l34:addControl("bgs_scale_delta")

l35:addControl("bgs_x_lb", 0, 0)
l35:addControl("bgs_scale_start_x", 0, 1)
l35:addControl("bgs_y_lb", 1, 0)
l35:addControl("bgs_scale_start_y", 1, 1)

l36:addControl("bgs_x_lb", 0, 0)
l36:addControl("bgs_scale_delta_x", 0, 1)
l36:addControl("bgs_y_lb", 1, 0)
l36:addControl("bgs_scale_delta_y", 1, 1)

local box_bgs_y_scale = l37:addControl("bgs_y_scale")
local box_bgs_window_delta = l37:addControl("bgs_window_delta")

l38:addControl("bgs_start_lb", 0, 0)
l38:addControl("bgs_y_scale_start", 0, 1)
l38:addControl("bgs_delta_lb", 1, 0)
l38:addControl("bgs_y_scale_delta", 1, 1)

l39:addControl("bgs_x_lb", 0, 0)
l39:addControl("bgs_window_delta_x", 0, 1)
l39:addControl("bgs_y_lb", 1, 0)
l39:addControl("bgs_window_delta_y", 1, 1)

l40:addControl("bgs_mode_lb", 0, 0)
l40:addControl("bgs_window_mode", 0, 1, 1, 3)
l40:addControl("bgs_top_lb", 1, 0)
l40:addControl("bgs_window_top", 2, 0)
l40:addControl("bgs_left_lb", 1, 1)
l40:addControl("bgs_window_left", 2, 1)
l40:addControl("bgs_bottom_lb", 1, 2)
l40:addControl("bgs_window_bottom", 2, 2)
l40:addControl("bgs_right_lb", 1, 3)
l40:addControl("bgs_window_right", 2, 3)

l41:addControl("bgs_sin_x_ampl_lb", 0, 0)
l41:addControl("bgs_sin_x_ampl", 1, 0)
l41:addControl("bgs_sin_x_period_lb", 0, 1)
l41:addControl("bgs_sin_x_period", 1, 1)
l41:addControl("bgs_sin_x_offset_lb", 0, 2)
l41:addControl("bgs_sin_x_offset", 1, 2)

l42:addControl("bgs_sin_y_ampl_lb", 0, 0)
l42:addControl("bgs_sin_y_ampl", 1, 0)
l42:addControl("bgs_sin_y_period_lb", 0, 1)
l42:addControl("bgs_sin_y_period", 1, 1)
l42:addControl("bgs_sin_y_offset_lb", 0, 2)
l42:addControl("bgs_sin_y_offset", 1, 2)

l43:addControl("emul_start", 0, 0, 2, 2):icon(44):flat()
l43:addControl("emul_engine_lb", 0, 2, 1, 6)
l43:addControl("emul_engine", 1, 2, 1, 6)
local box_emul_match = l43:addControl("emul_match", 2, 0, 1, -1)
local box_emul_training = l43:addControl("emul_training", 3, 0, 1, -1)
l43:addControl("emul_screen_lb", 4, 0, 1, -1)
l43:addControl("emul_screen", 5, 0, 1, -1)
l43:addVerticalSpacer(6, 0)

l44:addControl("emul_match_mode_lb", 0, 0)
l44:addControl("emul_match_mode", 1, 0)
l44:addControl("emul_enemy_lb", 0, 1)
l44:addControl("emul_enemy", 1, 1)
l44:addControl("emul_rounds_lb", 2, 0)
l44:addControl("emul_rounds", 3, 0)
l44:addControl("emul_time_lb", 2, 1)
l44:addControl("emul_time", 3, 1)
l44:addControl("emul_explod_mode_lb", 4, 0, 1, -1)
l44:addControl("emul_explod_mode", 5, 0, 1, -1)
l44:addControl("emul_debug_info", 6, 0, 1, -1)

l45:addControl("emul_dummy_ctrl_lb", 0, 0)
l45:addControl("emul_dummy_ctrl", 1, 0)
l45:addControl("emul_dummy_mode_lb", 0, 1)
l45:addControl("emul_dummy_mode", 1, 1)
l45:addControl("emul_guard_mode_lb", 2, 0)
l45:addControl("emul_guard_mode", 3, 0)
l45:addControl("emul_distance_lb", 2, 1)
l45:addControl("emul_distance", 3, 1)
l45:addControl("emul_button_jam_lb", 4, 0)
l45:addControl("emul_button_jam", 5, 0)
l45:addControl("emul_ai_level_lb", 4, 1)
l45:addControl("emul_ai_level", 5, 1)

l46:addLayout(l47, 0)
l46:addControl("org_list")

l47:addControl("org_up", 0, 0):icon(22):flat()
l47:addControl("org_down", 0, 1):icon(22):flat()
l47:addControl("org_up_10", 0, 2):icon(22):flat()
l47:addControl("org_down_10", 0, 3):icon(22):flat()
l47:addControl("org_top", 0, 4):icon(22):flat()
l47:addControl("org_bottom", 0, 5):icon(22):flat()
l47:addControl("org_auto", 0, 6):icon(22):flat()
l47:addControl("org_palette", 0, 7):icon(22):flat()

l48:addLayout(l49, 0, 0, 1, -1)
l48:addControl("bgctrls_def_sel", 1, 0, 1, -1):horizontal()
l48:addControl("bgctrls_def_sel_lb", 2, 0, 1, 6):align(FF.Align.HCenter, FF.Align.VCenter)
l48:addControl("bgctrls_def_add", 2, 6):icon(22):flat()
l48:addControl("bgctrls_def_delete", 2, 7):icon(22):flat()
l48:addControl("bgctrls_def_name_lb", 3, 0, 1, 5)
l48:addControl("bgctrls_def_name", 4, 0, 1, 5)
l48:addControl("bgctrls_def_loop_lb", 3, 5, 1, 3)
l48:addControl("bgctrls_def_loop", 4, 5, 1, 3)
local box_bgctrls_def_ids = l48:addControl("bgctrls_def_ids", 5, 0, 1, -1)
l48:addControl("bgctrls_sel", 6, 0, 1, -1):horizontal()
l48:addControl("bgctrls_sel_lb", 7, 0, 1, 6):align(FF.Align.HCenter, FF.Align.VCenter)
l48:addControl("bgctrls_add", 7, 6):icon(22):flat()
l48:addControl("bgctrls_delete", 7, 7):icon(22):flat()
l48:addControl("bgctrls_name_lb", 8, 0, 1, 4)
l48:addControl("bgctrls_name", 9, 0, 1, 4)
l48:addControl("bgctrls_type_lb", 8, 4, 1, 4)
l48:addControl("bgctrls_type", 9, 4, 1, 4)
l48:addControl("bgctrls_bool", 11, 4, 1, 4)
l48:addControl("bgctrls_anim_no_lb", 12, 0, 1, 4)
l48:addControl("bgctrls_anim_no", 12, 4, 1, 4)
l48:addLayout(l51, 13, 0, 1, -1)
l48:addLayout(l52, 14, 0, 1, -1)
local box_bgctrls_time = l48:addControl("bgctrls_time", 15, 0, 1, -1)
l48:addVerticalSpacer(17, 0)

l50:addControl("bgctrls_def_id_1", 0, 0)
l50:addControl("bgctrls_def_id_2", 0, 1)
l50:addControl("bgctrls_def_id_3", 0, 2)
l50:addControl("bgctrls_def_id_4", 0, 3)
l50:addControl("bgctrls_def_id_5", 0, 4)
l50:addControl("bgctrls_def_id_6", 1, 0)
l50:addControl("bgctrls_def_id_7", 1, 1)
l50:addControl("bgctrls_def_id_8", 1, 2)
l50:addControl("bgctrls_def_id_9", 1, 3)
l50:addControl("bgctrls_def_id_10", 1, 4)

l51:addControl("bgctrls_x_lb")
l51:addControl("bgctrls_x")
l51:addControl("bgctrls_y_lb")
l51:addControl("bgctrls_y")

l52:addControl("bgctrls_ampl_lb", 0, 0)
l52:addControl("bgctrls_ampl", 1, 0)
l52:addControl("bgctrls_period_lb", 0, 1)
l52:addControl("bgctrls_period", 1, 1)
l52:addControl("bgctrls_offset_lb", 0, 2)
l52:addControl("bgctrls_offset", 1, 2)

l53:addControl("bgctrls_start_lb", 0, 0)
l53:addControl("bgctrls_start_time", 1, 0)
l53:addControl("bgctrls_end_lb", 0, 1)
l53:addControl("bgctrls_end_time", 1, 1)
l53:addControl("bgctrls_loop_lb", 0, 2)
l53:addControl("bgctrls_loop_time", 1, 2)

l54:addLayout(l55, 0)
l54:addControl("debug_variables")

l55:addControl("debug_start", 0, 0):icon(22):flat()
l55:addControl("debug_interrupt", 0, 1):icon(22):flat()
l55:addControl("debug_stop", 0, 2):icon(22):flat()
l55:addControl("debug_restart", 0, 3):icon(22):flat()
l55:addControl("debug_step_over", 0, 4):icon(22):flat()
l55:addControl("debug_step_into", 0, 5):icon(22):flat()
l55:addControl("debug_step_out", 0, 6):icon(22):flat()
l55:addControl("debug_step_frame", 0, 7):icon(22):flat()

-- Panels
FF.panel("definitions").layout = l1
FF.panel("sprites").layout = l2
FF.extraPanel("palettes").layout = l5
FF.panel("animations").layout = l7
FF.extraPanel("thumbnails").layout = l12
FF.panel("commands").layout = l14
FF.panel("states").layout = l15
FF.panel("sounds").layout = l16
box_snd_selection:layout(l18)
FF.panel("backgrounds").layout = l19
box_bgs_spr:layout(l22)
box_bgs_start:layout(l25)
box_bgs_delta:layout(l26)
box_bgs_velocity:layout(l28)
box_bgs_tile:layout(l31)
box_bgs_tile_spacing:layout(l32)
box_bgs_parallax:layout(l33)
box_bgs_scale_start:layout(l35)
box_bgs_scale_delta:layout(l36)
box_bgs_y_scale:layout(l38)
box_bgs_window_delta:layout(l39)
box_bgs_window:layout(l40)
box_bgs_sin_x:layout(l41)
box_bgs_sin_y:layout(l42)
FF.panel("emulator").layout = l43
box_emul_match:layout(l44)
box_emul_training:layout(l45)
FF.extraPanel("organizer").layout = l46
FF.extraPanel("controllers").layout = l48
box_bgctrls_def_ids:layout(l50)
box_bgctrls_time:layout(l53)
FF.extraPanel("debug").layout = l54
