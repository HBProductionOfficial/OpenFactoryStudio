-- Interface preset "Clean".

-- Toolbars
local mainBar = FF.toolBar("main")
mainBar.iconSize = 32
mainBar:addButton("file_new")
mainBar:addButton("file_close")
mainBar:addButton("file_open")
mainBar:addButton("file_save")
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
mainBar:addButton("ref_docs")
mainBar:addSeparator()
mainBar:addButton("tools_options")

-- Layouts
local l1 = FF.verticalLayout()
l1.margin = 8
local l2 = FF.gridLayout()
l2.margin = 8
l2.spacing = 4
local l3 = FF.gridLayout()
l3.spacing = 4
local l4 = FF.gridLayout()
l4.margin = 8
l4.spacing = 6
local l5 = FF.gridLayout()
l5.margin = 8
l5.spacing = 4
local l6 = FF.gridLayout()
l6.spacing = 4
local l7 = FF.gridLayout()
l7.spacing = 4
local l8 = FF.gridLayout()
l8.spacing = 8
local l9 = FF.verticalLayout()
l9.margin = 8
l9.spacing = 6
local l10 = FF.verticalLayout()
l10.margin = 8
local l11 = FF.verticalLayout()
l11.margin = 8
l11.spacing = 4
local l12 = FF.gridLayout()
l12.margin = 8
l12.spacing = 4
local l13 = FF.gridLayout()
l13.margin = 8
l13.spacing = 4
local l14 = FF.gridLayout()
l14.margin = 8
l14.spacing = 4
local l15 = FF.horizontalLayout()
l15.margin = 0
l15.spacing = 4
local l16 = FF.gridLayout()
l16.margin = 8
l16.spacing = 4
local l17 = FF.verticalLayout()
l17.margin = 8
l17.spacing = 4
local l18 = FF.horizontalLayout()
l18.margin = 0
l18.spacing = 4
local l19 = FF.gridLayout()
l19.margin = 8
l19.spacing = 4
local l20 = FF.gridLayout()
l20.margin = 8
l20.spacing = 4
local l21 = FF.horizontalLayout()
l21.margin = 0
l21.spacing = 4
local l22 = FF.gridLayout()
l22.margin = 8
l22.spacing = 4
local l23 = FF.gridLayout()
l23.margin = 0
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
local l27 = FF.gridLayout()
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
l33.margin = 8
l33.spacing = 4
local l34 = FF.gridLayout()
l34.margin = 8
l34.spacing = 4
local l35 = FF.gridLayout()
l35.margin = 8
l35.spacing = 4
local l36 = FF.gridLayout()
l36.margin = 8
l36.spacing = 4
local l37 = FF.gridLayout()
l37.margin = 8
l37.spacing = 6
local l38 = FF.gridLayout()
l38.margin = 8
l38.spacing = 6
local l39 = FF.gridLayout()
l39.margin = 8
l39.spacing = 6
local l40 = FF.verticalLayout()
l40.margin = 8
l40.spacing = 6
local l41 = FF.gridLayout()
local l42 = FF.gridLayout()
l42.margin = 8
l42.spacing = 4
local l43 = FF.horizontalLayout()
local l44 = FF.gridLayout()
l44.margin = 8
l44.spacing = 4
local l45 = FF.gridLayout()
l45.margin = 8
l45.spacing = 4
local l46 = FF.gridLayout()
l46.margin = 8
l46.spacing = 4
local l47 = FF.verticalLayout()
l47.margin = 8
l47.spacing = 6

l1:addControl("def_tree")

l2:addControl("spr_sel", 0, 0, 1, -1):horizontal()
l2:addControl("spr_sel_lb", 1, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l2:addLayout(l3, 2, 0, 1, -1)
l2:addControl("spr_dec_load", 3, 0, 1, -1)
l2:addControl("spr_trans_color", 5, 0, 1, -1)
l2:addControl("spr_info_lb", 6, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l2:addControl("spr_onion_skin", 7, 0, 1, -1)
l2:addControl("spr_onion_sel", 8, 0, 1, -1):horizontal()
l2:addControl("spr_onion_lb", 9, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l2:addControl("spr_layers_menu", 2, 0):icon(22):flat()
l2:addControl("spr_layers_blendmode", 2, 1, 1, 2)
l2:addControl("spr_layers_opacity", 2, 3)
l2:addControl("spr_layers_list", 3, 0, 11, 4)
l2:addControl("spr_tool_canvasmove", 2, 4):icon(22):flat()
l2:addControl("spr_tool_selmove", 3, 4):icon(22):flat()
l2:addControl("spr_tool_rectsel", 4, 4):icon(22):flat()
l2:addControl("spr_tool_freesel", 5, 4):icon(22):flat()
l2:addControl("spr_tool_magicwand", 6, 4):icon(22):flat()
l2:addControl("spr_tool_picker", 7, 4):icon(22):flat()
l2:addControl("spr_tool_pen", 8, 4):icon(22):flat()
l2:addControl("spr_tool_eraser", 9, 4):icon(22):flat()
l2:addControl("spr_tool_remap_pen", 10, 4):icon(22):flat()
l2:addControl("spr_tool_paintbucket", 11, 4):icon(22):flat()
l2:addControl("spr_foreground", 12, 4):size(26, 26)
l2:addControl("spr_background", 13, 4):size(26, 26)
l2:addVerticalSpacer(14, 0)

l3:addControl("spr_name_lb", 0, 0, 1, -1)
l3:addControl("spr_name", 1, 0, 1, -1)
l3:addControl("spr_grp_lb", 2, 0, 1, 2)
l3:addControl("spr_grp", 3, 0, 1, 2)
l3:addControl("spr_num_lb", 2, 2, 1, 2)
l3:addControl("spr_num", 3, 2, 1, 2)
l3:addControl("spr_xaxis_lb", 4, 0, 1, 2)
l3:addControl("spr_xaxis", 5, 0, 1, 2)
l3:addControl("spr_yaxis_lb", 4, 2, 1, 2)
l3:addControl("spr_yaxis", 5, 2, 1, 2)

l4:addControl("pal_sel", 0, 0, 1, -1):horizontal()
l4:addControl("pal_sel_lb", 1, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l4:addControl("spr_switch_pal", 2, 0, 1, -1)
l4:addControl("pal_grp_lb", 3, 0)
l4:addControl("pal_grp", 4, 0)
l4:addControl("pal_num_lb", 3, 1)
l4:addControl("pal_num", 4, 1)
l4:addControl("pal_view", 5, 0, 1, -1)
l4:addVerticalSpacer(6, 0)

l5:addControl("anim_sel", 0, 0, 1, -1):horizontal()
l5:addControl("anim_sel_lb", 1, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l5:addLayout(l6, 2, 0, 2, -1)
l5:addControl("anim_frame_lb", 4, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l5:addControl("anim_frame_sel", 5, 0, 1, -1):horizontal()
l5:addControl("anim_frame_sel_lb", 6, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l5:addControl("anim_frame_grp_lb", 7, 0, 1, 2)
l5:addControl("anim_frame_grp", 8, 0, 1, 2)
l5:addControl("anim_frame_num_lb", 7, 2, 1, 2)
l5:addControl("anim_frame_num", 8, 2, 1, 2)
l5:addControl("anim_frame_xaxis_lb", 9, 0, 1, 2)
l5:addControl("anim_frame_xaxis", 10, 0, 1, 2)
l5:addControl("anim_frame_yaxis_lb", 9, 2, 1, 2)
l5:addControl("anim_frame_yaxis", 10, 2, 1, 2)
l5:addControl("anim_frame_time_lb", 11, 0, 1, 2)
l5:addControl("anim_frame_time", 12, 0, 1, 2)
l5:addControl("anim_frame_flip_lb", 11, 2, 1, 2)
l5:addControl("anim_frame_flip", 12, 2, 1, 2)
l5:addControl("anim_frame_trans_lb", 13, 0, 1, 2)
l5:addControl("anim_frame_trans", 14, 0, 1, 2)
l5:addControl("anim_frame_transsrc_lb", 13, 2)
l5:addControl("anim_frame_transsrc", 14, 2)
l5:addControl("anim_frame_transdst_lb", 13, 3)
l5:addControl("anim_frame_transdst", 14, 3)
l5:addLayout(l7, 15, 0, 1, -1)
l5:addControl("anim_frame_loop", 16, 0, 1, -1)
l5:addControl("anim_frame_intp", 17, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l5:addControl("anim_frame_intp_blend", 18, 0, 1, 2)
l5:addControl("anim_frame_intp_offset", 18, 2, 1, 2)
l5:addControl("anim_frame_intp_scale", 19, 0, 1, 2)
l5:addControl("anim_frame_intp_angle", 19, 2, 1, 2)
l5:addLayout(l8, 20, 0, 1, -1)
l5:addVerticalSpacer(21, 0)

l6:addControl("anim_number_lb", 0, 0)
l6:addControl("anim_number", 1, 0)
l6:addControl("anim_name_lb", 0, 1, 1, 3)
l6:addControl("anim_name", 1, 1, 1, 3)

l7:addControl("anim_frame_xscale_lb", 0, 0)
l7:addControl("anim_frame_xscale", 1, 0)
l7:addControl("anim_frame_yscale_lb", 0, 1)
l7:addControl("anim_frame_yscale", 1, 1)
l7:addControl("anim_frame_angle_lb", 0, 2)
l7:addControl("anim_frame_angle", 1, 2)

l8:addControl("anim_onion_skin", 0, 0, 1, -1)
l8:addControl("anim_onion_sel", 1, 0, 1, 7):horizontal()
l8:addControl("anim_onion_find", 1, 7)
l8:addControl("anim_onion_framesel", 2, 0, 1, -1):horizontal()
l8:addControl("anim_onion_lb", 3, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)

l9:addControl("thumb_grp_list")
l9:addControl("thumb_view")

l10:addControl("cmd_tree")

l11:addControl("st_tree")

l12:addControl("snd_sel", 1, 0, 1, -1):horizontal()
l12:addControl("snd_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l12:addControl("snd_grp_lb", 3, 0, 1, 4)
l12:addControl("snd_grp", 4, 0, 1, 4)
l12:addControl("snd_num_lb", 3, 4, 1, 4)
l12:addControl("snd_num", 4, 4, 1, 4)
l12:addControl("snd_info_lb", 5, 0, 1, 7)
l12:addControl("snd_volume", 5, 7):vertical()
l12:addControl("snd_curr_sample_lb", 6, 0, 1, 3)
l12:addControl("snd_curr_sample", 6, 3, 1, 3)
l12:addControl("snd_save_changes", 6, 6):icon(22):flat()
l12:addControl("snd_discard_changes", 6, 7):icon(22):flat()
local box_snd_selection = l12:addControl("snd_selection", 7, 0, 1, -1)
l12:addVerticalSpacer(8, 0)

l13:addControl("snd_start_lb", 0, 0)
l13:addControl("snd_end_lb", 0, 1)
l13:addControl("snd_sel_start", 1, 0)
l13:addControl("snd_sel_end", 1, 1)

l14:addControl("bgs_sel", 1, 0, 1, -1):horizontal()
l14:addControl("bgs_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l14:addControl("bgs_name_lb", 3, 0, 1, 5)
l14:addControl("bgs_name", 4, 0, 1, 5)
l14:addControl("bgs_type_lb", 3, 5, 1, 3)
l14:addControl("bgs_type", 4, 5, 1, 3)
l14:addLayout(l15, 5, 0, 1, -1)
l14:addLayout(l18, 6, 0, 1, -1)
l14:addLayout(l21, 7, 0, 1, -1)
l14:addLayout(l24, 8, 0, 1, -1)
l14:addControl("bgs_trans_lb", 9, 0, 1, 4)
l14:addControl("bgs_trans", 10, 0, 1, 4)
l14:addControl("bgs_transsrc_lb", 9, 4, 1, 2)
l14:addControl("bgs_transsrc", 10, 4, 1, 2)
l14:addControl("bgs_transdst_lb", 9, 6, 1, 2)
l14:addControl("bgs_transdst", 10, 6, 1, 2)
local box_bgs_parallax = l14:addControl("bgs_parallax", 11, 0, 1, -1)
l14:addLayout(l28, 12, 0, 1, -1)
l14:addLayout(l31, 13, 0, 1, -1)
local box_bgs_window = l14:addControl("bgs_window", 14, 0, 1, -1)
local box_bgs_sin_x = l14:addControl("bgs_sin_x", 15, 0, 1, -1)
local box_bgs_sin_y = l14:addControl("bgs_sin_y", 16, 0, 1, -1)
l14:addVerticalSpacer(17, 0)

local box_bgs_spr = l15:addControl("bgs_spr")
l15:addLayout(l17, 0)

l16:addControl("bgs_grp_lb", 0, 0)
l16:addControl("bgs_spr_grp", 0, 1)
l16:addControl("bgs_num_lb", 1, 0)
l16:addControl("bgs_spr_num", 1, 1)

l17:addControl("bgs_anim_no_lb")
l17:addControl("bgs_anim_no")
l17:addControl("bgs_mask")

local box_bgs_start = l18:addControl("bgs_start")
local box_bgs_delta = l18:addControl("bgs_delta")

l19:addControl("bgs_x_lb", 0, 0)
l19:addControl("bgs_start_x", 0, 1)
l19:addControl("bgs_y_lb", 1, 0)
l19:addControl("bgs_start_y", 1, 1)

l20:addControl("bgs_x_lb", 0, 0)
l20:addControl("bgs_delta_x", 0, 1)
l20:addControl("bgs_y_lb", 1, 0)
l20:addControl("bgs_delta_y", 1, 1)

local box_bgs_velocity = l21:addControl("bgs_velocity")
l21:addLayout(l23, 0)

l22:addControl("bgs_x_lb", 0, 0)
l22:addControl("bgs_velocity_x", 0, 1)
l22:addControl("bgs_y_lb", 1, 0)
l22:addControl("bgs_velocity_y", 1, 1)

l23:addControl("bgs_id_lb", 0, 0)
l23:addControl("bgs_id", 0, 1)
l23:addControl("bgs_layer_lb", 1, 0)
l23:addControl("bgs_layer", 1, 1)
l23:addControl("bgs_pos_link", 2, 0, 1, -1)

local box_bgs_tile = l24:addControl("bgs_tile")
local box_bgs_tile_spacing = l24:addControl("bgs_tile_spacing")

l25:addControl("bgs_x_lb", 0, 0)
l25:addControl("bgs_tile_x", 0, 1)
l25:addControl("bgs_y_lb", 1, 0)
l25:addControl("bgs_tile_y", 1, 1)

l26:addControl("bgs_x_lb", 0, 0)
l26:addControl("bgs_tile_spacing_x", 0, 1)
l26:addControl("bgs_y_lb", 1, 0)
l26:addControl("bgs_tile_spacing_y", 1, 1)

l27:addControl("bgs_mode_lb", 0, 0)
l27:addControl("bgs_parallax_mode", 0, 1, 1, 3)
l27:addControl("bgs_top_lb", 1, 0, 1, 2)
l27:addControl("bgs_parallax_top", 2, 0, 1, 2)
l27:addControl("bgs_bottom_lb", 1, 2, 1, 2)
l27:addControl("bgs_parallax_bottom", 2, 2, 1, 2)

local box_bgs_scale_start = l28:addControl("bgs_scale_start")
local box_bgs_scale_delta = l28:addControl("bgs_scale_delta")

l29:addControl("bgs_x_lb", 0, 0)
l29:addControl("bgs_scale_start_x", 0, 1)
l29:addControl("bgs_y_lb", 1, 0)
l29:addControl("bgs_scale_start_y", 1, 1)

l30:addControl("bgs_x_lb", 0, 0)
l30:addControl("bgs_scale_delta_x", 0, 1)
l30:addControl("bgs_y_lb", 1, 0)
l30:addControl("bgs_scale_delta_y", 1, 1)

local box_bgs_y_scale = l31:addControl("bgs_y_scale")
local box_bgs_window_delta = l31:addControl("bgs_window_delta")

l32:addControl("bgs_start_lb", 0, 0)
l32:addControl("bgs_y_scale_start", 0, 1)
l32:addControl("bgs_delta_lb", 1, 0)
l32:addControl("bgs_y_scale_delta", 1, 1)

l33:addControl("bgs_x_lb", 0, 0)
l33:addControl("bgs_window_delta_x", 0, 1)
l33:addControl("bgs_y_lb", 1, 0)
l33:addControl("bgs_window_delta_y", 1, 1)

l34:addControl("bgs_mode_lb", 0, 0)
l34:addControl("bgs_window_mode", 0, 1, 1, 3)
l34:addControl("bgs_top_lb", 1, 0)
l34:addControl("bgs_window_top", 2, 0)
l34:addControl("bgs_left_lb", 1, 1)
l34:addControl("bgs_window_left", 2, 1)
l34:addControl("bgs_bottom_lb", 1, 2)
l34:addControl("bgs_window_bottom", 2, 2)
l34:addControl("bgs_right_lb", 1, 3)
l34:addControl("bgs_window_right", 2, 3)

l35:addControl("bgs_sin_x_ampl_lb", 0, 0)
l35:addControl("bgs_sin_x_ampl", 1, 0)
l35:addControl("bgs_sin_x_period_lb", 0, 1)
l35:addControl("bgs_sin_x_period", 1, 1)
l35:addControl("bgs_sin_x_offset_lb", 0, 2)
l35:addControl("bgs_sin_x_offset", 1, 2)

l36:addControl("bgs_sin_y_ampl_lb", 0, 0)
l36:addControl("bgs_sin_y_ampl", 1, 0)
l36:addControl("bgs_sin_y_period_lb", 0, 1)
l36:addControl("bgs_sin_y_period", 1, 1)
l36:addControl("bgs_sin_y_offset_lb", 0, 2)
l36:addControl("bgs_sin_y_offset", 1, 2)

l37:addControl("emul_engine_lb", 0, 0, 1, -1)
l37:addControl("emul_engine", 1, 0, 1, -1)
local box_emul_match = l37:addControl("emul_match", 2, 0, 1, -1)
local box_emul_training = l37:addControl("emul_training", 3, 0, 1, -1)
l37:addControl("emul_screen_lb", 4, 0, 1, -1)
l37:addControl("emul_screen", 5, 0, 1, -1)
l37:addVerticalSpacer(6, 0)

l38:addControl("emul_match_mode_lb", 0, 0)
l38:addControl("emul_match_mode", 1, 0)
l38:addControl("emul_enemy_lb", 0, 1)
l38:addControl("emul_enemy", 1, 1)
l38:addControl("emul_rounds_lb", 2, 0)
l38:addControl("emul_rounds", 3, 0)
l38:addControl("emul_time_lb", 2, 1)
l38:addControl("emul_time", 3, 1)
l38:addControl("emul_explod_mode_lb", 4, 0, 1, -1)
l38:addControl("emul_explod_mode", 5, 0, 1, -1)
l38:addControl("emul_debug_info", 6, 0, 1, -1)

l39:addControl("emul_dummy_ctrl_lb", 0, 0)
l39:addControl("emul_dummy_ctrl", 1, 0)
l39:addControl("emul_dummy_mode_lb", 0, 1)
l39:addControl("emul_dummy_mode", 1, 1)
l39:addControl("emul_guard_mode_lb", 2, 0)
l39:addControl("emul_guard_mode", 3, 0)
l39:addControl("emul_distance_lb", 2, 1)
l39:addControl("emul_distance", 3, 1)
l39:addControl("emul_button_jam_lb", 4, 0)
l39:addControl("emul_button_jam", 5, 0)
l39:addControl("emul_ai_level_lb", 4, 1)
l39:addControl("emul_ai_level", 5, 1)

l40:addLayout(l41, 0)
l40:addControl("org_list")

l41:addControl("org_up", 0, 0):icon(22):flat()
l41:addControl("org_down", 0, 1):icon(22):flat()
l41:addControl("org_up_10", 0, 2):icon(22):flat()
l41:addControl("org_down_10", 0, 3):icon(22):flat()
l41:addControl("org_top", 0, 4):icon(22):flat()
l41:addControl("org_bottom", 0, 5):icon(22):flat()
l41:addControl("org_auto", 0, 6):icon(22):flat()
l41:addControl("org_palette", 0, 7):icon(22):flat()

l42:addLayout(l43, 0, 0, 1, -1)
l42:addControl("bgctrls_def_sel", 1, 0, 1, -1):horizontal()
l42:addControl("bgctrls_def_sel_lb", 2, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l42:addControl("bgctrls_def_name_lb", 3, 0, 1, 5)
l42:addControl("bgctrls_def_name", 4, 0, 1, 5)
l42:addControl("bgctrls_def_loop_lb", 3, 5, 1, 3)
l42:addControl("bgctrls_def_loop", 4, 5, 1, 3)
local box_bgctrls_def_ids = l42:addControl("bgctrls_def_ids", 5, 0, 1, -1)
l42:addControl("bgctrls_sel", 6, 0, 1, -1):horizontal()
l42:addControl("bgctrls_sel_lb", 7, 0, 1, -1):align(FF.Align.HCenter, FF.Align.VCenter)
l42:addControl("bgctrls_name_lb", 8, 0, 1, 4)
l42:addControl("bgctrls_name", 9, 0, 1, 4)
l42:addControl("bgctrls_type_lb", 8, 4, 1, 4)
l42:addControl("bgctrls_type", 9, 4, 1, 4)
local box_bgctrls_time = l42:addControl("bgctrls_time", 10, 0, 1, -1)
l42:addControl("bgctrls_bool", 11, 0, 1, -1)
l42:addControl("bgctrls_anim_no_lb", 12, 0, 1, 4)
l42:addControl("bgctrls_anim_no", 12, 4, 1, 4)
l42:addControl("bgctrls_x_lb", 13, 0)
l42:addControl("bgctrls_x", 13, 1, 1, 3)
l42:addControl("bgctrls_y_lb", 13, 4)
l42:addControl("bgctrls_y", 13, 5, 1, 3)
l42:addLayout(l46, 14, 0, 1, -1)
l42:addVerticalSpacer(15, 0)

l44:addControl("bgctrls_def_id_1", 0, 0)
l44:addControl("bgctrls_def_id_2", 0, 1)
l44:addControl("bgctrls_def_id_3", 0, 2)
l44:addControl("bgctrls_def_id_4", 0, 3)
l44:addControl("bgctrls_def_id_5", 0, 4)
l44:addControl("bgctrls_def_id_6", 1, 0)
l44:addControl("bgctrls_def_id_7", 1, 1)
l44:addControl("bgctrls_def_id_8", 1, 2)
l44:addControl("bgctrls_def_id_9", 1, 3)
l44:addControl("bgctrls_def_id_10", 1, 4)

l45:addControl("bgctrls_start_lb", 0, 0)
l45:addControl("bgctrls_start_time", 1, 0)
l45:addControl("bgctrls_end_lb", 0, 1)
l45:addControl("bgctrls_end_time", 1, 1)
l45:addControl("bgctrls_loop_lb", 0, 2)
l45:addControl("bgctrls_loop_time", 1, 2)

l46:addControl("bgctrls_ampl_lb", 0, 0)
l46:addControl("bgctrls_ampl", 1, 0)
l46:addControl("bgctrls_period_lb", 0, 1)
l46:addControl("bgctrls_period", 1, 1)
l46:addControl("bgctrls_offset_lb", 0, 2)
l46:addControl("bgctrls_offset", 1, 2)

l47:addControl("debug_variables")

-- Panels
FF.panel("definitions").layout = l1
FF.panel("sprites").layout = l2
FF.extraPanel("palettes").layout = l4
FF.panel("animations").layout = l5
FF.extraPanel("thumbnails").layout = l9
FF.panel("commands").layout = l10
FF.panel("states").layout = l11
FF.panel("sounds").layout = l12
box_snd_selection:layout(l13)
FF.panel("backgrounds").layout = l14
box_bgs_spr:layout(l16)
box_bgs_start:layout(l19)
box_bgs_delta:layout(l20)
box_bgs_velocity:layout(l22)
box_bgs_tile:layout(l25)
box_bgs_tile_spacing:layout(l26)
box_bgs_parallax:layout(l27)
box_bgs_scale_start:layout(l29)
box_bgs_scale_delta:layout(l30)
box_bgs_y_scale:layout(l32)
box_bgs_window_delta:layout(l33)
box_bgs_window:layout(l34)
box_bgs_sin_x:layout(l35)
box_bgs_sin_y:layout(l36)
FF.panel("emulator").layout = l37
box_emul_match:layout(l38)
box_emul_training:layout(l39)
FF.extraPanel("organizer").layout = l40
FF.extraPanel("controllers").layout = l42
box_bgctrls_def_ids:layout(l44)
box_bgctrls_time:layout(l45)
FF.extraPanel("debug").layout = l47
