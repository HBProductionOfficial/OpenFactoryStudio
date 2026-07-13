#!/usr/bin/env python3
"""Builds the icon sets in data/iconsets/ and data/engines/mugen/iconsets/ from the tables below.

    Usage: build_iconsets.py [--check] [--sets Nuvola,Flat-Dark]. Needs Pillow and PySide6.
    """

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from PIL import Image  # noqa: E402

import art  # noqa: E402
import doc_files  # noqa: E402
import drawings as dw  # noqa: E402
from sources import SOURCES, fetch  # noqa: E402

ROOT = HERE.parents[1]
DATA = ROOT / "data"
REQUIRED = json.loads((HERE / "required_names.json").read_text(encoding="utf-8"))
PNG_SIZE = 48

# === Layout: icon name -> file without extension ================================================
# In LAYOUT_MUGEN a leading '*' puts the file in the global set.
LAYOUT_GLOBAL = {
    "anim_box_add_attack": "air/clsn1_add",
    "anim_box_add_collision": "air/clsn2_add",
    "anim_box_del": "air/clsn_delete",
    "anim_box_delall": "air/clsn_delete_frame",
    "anim_box_delall_anim": "air/clsn_delete_anim",
    "anim_box_prev": "air/clsn_previous",
    "anim_box_type": "air/clsn_switch",
    "anim_code": "air/edit_text",
    "anim_editcode": "air/edit_code",
    "anim_frame_align": "air/align",
    "anim_frameall_flip": "air/all_flip",
    "anim_frameall_offset": "air/all_offset",
    "anim_frameall_scale": "air/all_scale",
    "anim_frameall_time": "air/all_time",
    "anim_save_gif": "air/save_gif",
    "code_snippet_edit": "air/edit_text",
    "code_bool": "code/bool",
    "code_comment": "code/comment",
    "code_date": "code/date",
    "code_file": "code/file",
    "code_float": "code/float",
    "code_goto": "code/goto",
    "code_int": "code/int",
    "code_keyword": "code/keyword",
    "code_next_error": "code/next_error",
    "code_prev_error": "code/prev_error",
    "code_string": "code/string",
    "advpal_auto_sort": "editors/organizer",
    "advpal_optimize": "common/optimize",
    "advpal_redo": "common/redo",
    "advpal_undo": "common/undo",
    "advpal_zoom_in": "common/zoom_in",
    "advpal_zoom_out": "common/zoom_out",
    "anim_add": "common/add",
    "anim_delete": "common/delete",
    "anim_duplicate": "common/duplicate",
    "anim_frame_add": "common/add",
    "anim_frame_delete": "common/delete",
    "anim_new": "common/new",
    "anim_onion_find": "common/find",
    "anim_open": "common/open",
    "anim_play": "common/play",
    "anim_pos_down": "common/down",
    "anim_pos_left": "common/left",
    "anim_pos_right": "common/right",
    "anim_pos_up": "common/up",
    "anim_save": "common/save",
    "anim_saveas": "common/saveas",
    "anim_stop": "common/stop",
    "anim_thumb_add": "common/add",
    "anim_thumb_add_all": "common/add_all",
    "anim_thumb_new": "common/new",
    "anim_thumb_new_all": "common/new_all",
    "bottom_down": "common/down",
    "bottom_up": "common/up",
    "code_snippet_new": "common/add",
    "debug_interrupt": "common/pause",
    "debug_start": "common/play",
    "debug_stop": "common/stop",
    "edit_copy": "common/copy",
    "edit_cut": "common/cut",
    "edit_find_replace": "common/find",
    "edit_find_replace_multi": "common/find_multi",
    "edit_paste": "common/paste",
    "edit_redo": "common/redo",
    "edit_undo": "common/undo",
    "editor_new": "common/new",
    "editor_open": "common/open",
    "editor_save": "common/save",
    "editor_saveas": "common/saveas",
    "error": "common/error",
    "information": "common/information",
    "ok": "common/ok",
    "pal_add": "common/add",
    "pal_delete": "common/delete",
    "pal_open": "common/open",
    "pal_redo": "common/redo",
    "pal_save": "common/saveas",
    "pal_undo": "common/undo",
    "script_new": "common/new",
    "script_open": "common/open",
    "script_save": "common/save",
    "script_saveas": "common/saveas",
    "snd_discard_changes": "common/delete",
    "snd_new": "common/new",
    "snd_open": "common/open",
    "snd_pause": "common/pause",
    "snd_play": "common/play",
    "snd_save": "common/save",
    "snd_saveas": "common/saveas",
    "snd_stop": "common/stop",
    "spr_add": "common/add",
    "spr_delete": "common/delete",
    "spr_layer_add": "common/add",
    "spr_layer_delete": "common/delete",
    "spr_layer_play": "common/play",
    "spr_new": "common/new",
    "spr_open": "common/open",
    "spr_save": "common/save",
    "spr_saveas": "common/saveas",
    "spr_thumb_add": "common/new",
    "spr_thumb_add_aligned": "common/new_aligned",
    "spr_thumb_add_all": "common/new_all",
    "spr_thumb_add_all_aligned": "common/new_all_aligned",
    "spr_zoomin": "common/zoom_in",
    "spr_zoomout": "common/zoom_out",
    "view_calculator": "common/calculator",
    "view_zoomin": "common/zoom_in",
    "view_zoomout": "common/zoom_out",
    "warning": "common/warning",
    "breakpoint_active": "debug/breakpoint",
    "breakpoint_disabled": "debug/breakpoint_disabled",
    "breakpoint_ignored": "debug/breakpoint_ignored",
    "breakpoint_pending": "debug/breakpoint_pending",
    "debug_curr_line": "debug/current_line",
    "debug_restart": "debug/restart",
    "debug_start_emul": "debug/start_emulator",
    "debug_start_engine": "debug/start_engine",
    "debug_step_frame": "debug/step_frame",
    "debug_step_into": "debug/step_into",
    "debug_step_out": "debug/step_out",
    "debug_step_over": "debug/step_over",
    "editor_anim": "editors/animations",
    "editor_bg": "editors/backgrounds",
    "editor_emul": "editors/emulator",
    "editor_snd": "editors/sounds",
    "editor_spr": "editors/sprites",
    "ref_docs": "editors/docs",
    "view_organizer": "editors/organizer",
    "icon_animation": "files/animation",
    "icon_code": "files/code",
    "icon_command": "files/command",
    "icon_file": "files/file",
    "icon_folder": "files/folder",
    "icon_sound": "files/sound",
    "icon_sprite": "files/sprite",
    "snd_save_changes": "imgedit/commit",
    "snd_tool_draw": "imgedit/pencil",
    "snd_tool_move": "imgedit/move",
    "snd_tool_offset": "imgedit/hand",
    "spr_layer_duplicate": "imgedit/layers",
    "spr_layer_save": "imgedit/save_layers",
    "spr_saveimg": "imgedit/commit",
    "spr_tool_canvasmove": "imgedit/hand",
    "spr_tool_eraser": "imgedit/eraser",
    "spr_tool_freesel": "imgedit/free_select",
    "spr_tool_magicwand": "imgedit/magic_wand",
    "spr_tool_paintbucket": "imgedit/bucket",
    "spr_tool_pen": "imgedit/pencil",
    "spr_tool_picker": "pal/picker",
    "spr_tool_rectsel": "imgedit/rect_select",
    "spr_tool_remap_pen": "imgedit/remap_pencil",
    "spr_tool_selmove": "imgedit/move",
    "file_backup": "project/backup",
    "file_close": "project/close",
    "file_close_all": "project/close_all",
    "file_load_backup": "project/load_backup",
    "file_new": "common/new",
    "file_open": "project/open",
    "file_save": "common/save",
    "file_run": "misc/run",
    "script_run": "misc/run",
    "script_translate": "misc/translate",
    "tools_options": "misc/options",
    "code_snippet_insert": "organizer/down",
    "org_auto": "organizer/auto",
    "org_bottom": "organizer/bottom",
    "org_down": "organizer/down",
    "org_down_10": "organizer/down_10",
    "org_top": "organizer/top",
    "org_up": "organizer/up",
    "org_up_10": "organizer/up_10",
    "advpal_adapt_image": "pal/apply_sprite",
    "advpal_add_diff": "pal/add_missing",
    "advpal_colto_left": "pal/colors_to_left",
    "advpal_colto_right": "pal/colors_to_right",
    "advpal_commit_changes": "pal/commit_palette",
    "advpal_commit_spr_changes": "pal/commit_image",
    "advpal_del_duplicate": "pal/replace_duplicates",
    "advpal_effect_apply": "pal/apply",
    "advpal_gen_pal_imgs": "pal/palette_from_images",
    "advpal_gradient": "pal/gradient",
    "advpal_grayscale": "pal/grayscale",
    "advpal_huesat_apply": "pal/apply",
    "advpal_huesat_reset": "pal/reset",
    "advpal_insert_pal_imgs": "pal/palette_to_images",
    "advpal_invert_all": "pal/invert",
    "advpal_open": "common/open",
    "advpal_open_img": "sff/open_image",
    "advpal_pick_color": "pal/picker",
    "advpal_rgb_apply": "pal/apply",
    "advpal_rgb_reset": "pal/reset",
    "advpal_save": "common/save",
    "advpal_save_img": "sff/save_image",
    "advpal_save_new": "pal/save_new",
    "advpal_selto_left": "pal/selection_to_left",
    "advpal_selto_right": "pal/selection_to_right",
    "advpal_set_color": "pal/set_color",
    "advpal_switch_pos": "pal/switch_colors",
    "org_palette": "pal/editor",
    "pal_apply_sprite": "pal/apply_sprite",
    "pal_pick_color": "pal/picker",
    "pal_remap": "pal/remap",
    "pal_set_color": "pal/set_color",
    "pal_switch_color": "pal/switch_colors",
    "pal_switch_count": "pal/switch_count",
    "tools_pal_editor": "pal/editor",
    "quick_search_backward": "search/backward",
    "quick_search_case": "search/case",
    "quick_search_circular": "search/circular",
    "quick_search_close": "search/close",
    "quick_search_regexp": "search/regexp",
    "spr_crop": "sff/crop",
    "spr_duplicate": "common/duplicate",
    "spr_explorer": "sff/explorer",
    "spr_import_proj": "sff/import_project",
    "spr_openimg": "sff/open_image",
    "spr_saveimgas": "sff/save_image",
    "thumb_save": "sff/save_image",
    "snd_add": "common/add",
    "snd_delete": "common/delete",
    "snd_duplicate": "common/duplicate",
    "snd_open_sample": "snd/open_wav",
    "snd_play_loop": "snd/play_loop",
    "snd_save_sample": "snd/save_wav",
    "snd_tool_hresize": "snd/hresize",
    "snd_tool_select": "snd/select",
    "snd_tool_vresize": "snd/vresize",
    "snd_tool_zoom": "view/zoom_fit",
    "view_attack_box": "view/attack_box",
    "view_center_origin": "view/center_origin",
    "view_collision_box": "view/collision_box",
    "view_guide_lines": "view/guide_lines",
    "view_issues": "view/issues",
    "view_zoomfit": "view/zoom_fit",
    "view_zoomreset": "view/zoom_reset",
}

LAYOUT_MUGEN = {
    "anim_box_default": "air/clsn_default",
    "anim_frameall_trans": "air/all_trans",
    "spr_align": "*air/align",
    "st_sctrl_edit": "*air/edit_text",
    "bgs_controllers": "bg/controllers",
    "bgs_stage": "bg/stage",
    "code_cmd": "code/cmd",
    "code_coord": "code/coord",
    "code_exp": "code/exp",
    "code_flags": "code/flags",
    "code_ustring": "code/ustring",
    "code_version": "code/version",
    "bgctrls_add": "*common/add",
    "bgctrls_def_add": "*common/add",
    "bgctrls_def_delete": "*common/delete",
    "bgctrls_delete": "*common/delete",
    "bgs_add": "*common/add",
    "bgs_delete": "*common/delete",
    "bgs_duplicate": "*common/duplicate",
    "bgs_pause": "*common/pause",
    "bgs_play": "*common/play",
    "bgs_stop": "*common/stop",
    "cmd_new": "*common/new",
    "cmd_open": "*common/open",
    "cmd_save": "*common/save",
    "cmd_saveas": "*common/saveas",
    "def_new": "*common/new",
    "def_open": "*common/open",
    "def_save": "*common/save",
    "def_saveas": "*common/saveas",
    "snd_new": "*common/new",
    "snd_next_grp": "*common/right",
    "snd_open": "*common/open",
    "snd_prev_grp": "*common/left",
    "snd_save": "*common/save",
    "snd_saveas": "*common/saveas",
    "spr_next_grp": "*common/right",
    "spr_pos_down": "*common/down",
    "spr_pos_left": "*common/left",
    "spr_pos_right": "*common/right",
    "spr_pos_up": "*common/up",
    "spr_prev_grp": "*common/left",
    "st_new": "*common/new",
    "st_open": "*common/open",
    "st_save": "*common/save",
    "st_saveas": "*common/saveas",
    "st_sctrl_delete": "*common/delete",
    "st_sctrl_new": "*common/add",
    "editor_bg": "*editors/backgrounds",
    "editor_cmd": "editors/commands",
    "editor_def": "editors/definitions",
    "editor_st": "*editors/states",
    "spr_align_bottomcenter": "guides/bottom_center",
    "spr_align_bottomleft": "guides/bottom_left",
    "spr_align_bottomright": "guides/bottom_right",
    "spr_align_centercenter": "guides/center_center",
    "spr_align_centerleft": "guides/center_left",
    "spr_align_centerright": "guides/center_right",
    "spr_align_topcenter": "guides/top_center",
    "spr_align_topleft": "guides/top_left",
    "spr_align_topright": "guides/top_right",
    "spr_add": "*common/add",
    "spr_delete_grp": "sff/delete_group",
    "spr_edit_img": "sff/image_editor",
    "spr_export": "sff/export",
    "spr_import": "sff/import",
    "spr_openimg": "*sff/open_image",
    "spr_rename_grp": "sff/rename_group",
    "spr_save": "*common/save",
    "spr_saveimgas": "*sff/save_image",
    "snd_delete_grp": "snd/delete_group",
    "st_a": "st/a",
    "st_c": "st/c",
    "st_command": "st/command",
    "st_l": "st/l",
    "st_offset": "st/offset",
    "st_palfx": "st/palfx",
    "st_quote": "st/quote",
    "st_s": "st/s",
    "st_throw": "st/throw",
    "st_u": "st/u",
    "st_var_list": "st/var_list",
    "st_var_log": "st/var_log",
}


# === Recipes ====================================================================================
@dataclass(frozen=True)
class Badge:
    src: object                 # Nv or Draw
    scale: float = 0.56
    anchor: str = "br"          # br, bl, tr, tl or c


@dataclass(frozen=True)
class Nv:
    """A Nuvola picture (category/name of the upstream theme), optionally modified."""
    icon: str
    hue: float = 0.0            # hue rotation in degrees
    gray: float = 0.0           # 0..1 desaturation
    bright: float = 1.0
    flip: bool = False
    stack: int = 1              # draw n copies offset diagonally
    badges: tuple = ()


@dataclass(frozen=True)
class Draw:
    fn: Callable


@dataclass(frozen=True)
class Flat:
    """An outline icon ("name" from Tabler, "lucide:name" from Lucide) or a drawing (fn)."""
    icon: str | None = None
    badge: str | None = None
    accent: str | None = None
    inner: str | None = None
    fn: Callable | None = None


def B(src, scale=0.56, anchor="br"):
    return Badge(src, scale, anchor)


# --- Nuvola: the look closest to the default preset's icons -------------------------------------
CLOCK = Nv("apps/clock")
PLAY = Nv("actions/player_play")
FLOPPY = Nv("actions/filesave")
PICTURE = Nv("actions/thumbnail")
PALETTE = Nv("actions/colorize")
STOP_X = Nv("actions/stop")
MAG = Nv("actions/viewmag")
RED_BACK = Nv("actions/back", hue=145)
RED_FORWARD = Nv("actions/forward", hue=145)

NUVOLA = {
    "air/clsn1_add": Draw(dw.g_clsn1_add),
    "air/clsn2_add": Draw(dw.g_clsn2_add),
    "air/clsn_delete": Draw(dw.g_clsn_delete),
    "air/clsn_delete_frame": Draw(dw.g_clsn_delete_frame),
    "air/clsn_delete_anim": Draw(dw.g_clsn_delete_anim),
    "air/clsn_previous": Draw(dw.g_clsn_previous),
    "air/clsn_switch": Draw(dw.g_clsn_switch),
    "air/clsn_default": Draw(dw.g_clsn_default),
    "air/edit_text": Nv("apps/kwrite"),
    "air/edit_code": Nv("apps/package_editors"),
    "air/align": Nv("actions/rotate"),
    "air/all_flip": Draw(dw.g_all_flip),
    "air/all_offset": Draw(dw.g_all_offset),
    "air/all_scale": Draw(dw.g_all_scale),
    "air/all_time": Draw(dw.g_all_time),
    "air/all_trans": Draw(dw.g_all_trans),
    "air/save_gif": Nv("apps/mplayer", badges=(B(FLOPPY),)),
    "code/bool": Draw(dw.g_bool),
    "code/comment": Draw(dw.g_comment),
    "code/date": Draw(dw.g_date),
    "code/file": Nv("filesystems/folder_orange_open"),
    "code/float": Draw(dw.g_text("1.23", "#e6169a")),
    "code/goto": Nv("apps/kmenuedit", badges=(B(Nv("actions/forward"), 0.6),)),
    "code/int": Draw(dw.g_text("123", "#e61e1e")),
    "code/keyword": Draw(dw.g_icon_text("abc", "#f07a0a")),
    "code/next_error": RED_FORWARD,
    "code/prev_error": RED_BACK,
    "code/string": Draw(dw.g_icon_text("abc", "#1e8ce6", 34, quotes=True)),
    "code/cmd": Draw(dw.g_cmd_type),
    "code/coord": Draw(dw.g_text("XY", "#9a5a1a")),
    "code/exp": Draw(dw.g_icon_text("math-function", "#e0187a", 38, 6, 3.6)),
    "code/flags": Draw(dw.g_flags),
    "code/ustring": Draw(dw.g_icon_text("abc", "#3aa8f0")),
    "code/version": Draw(dw.g_text("V1.0", "#8a4a10")),
    "common/optimize": Nv("apps/ark"),
    "common/redo": Nv("actions/redo"),
    "common/undo": Nv("actions/undo"),
    "common/zoom_in": Nv("actions/viewmag+"),
    "common/zoom_out": Nv("actions/viewmag-"),
    "common/add": Draw(dw.g_add),
    "common/add_all": Draw(dw.g_add_all),
    "common/delete": STOP_X,
    "common/duplicate": Nv("mimetypes/kmultiple"),
    "common/new": Nv("actions/filenew"),
    "common/new_all": Nv("actions/filenew", stack=2),
    "common/new_aligned": Nv("actions/filenew", badges=(B(Draw(dw.g_crosshair), 0.6, "tl"),)),
    "common/new_all_aligned": Nv("actions/filenew", stack=2, badges=(B(Draw(dw.g_crosshair), 0.56, "tl"),)),
    "common/find": Nv("actions/find"),
    "common/find_multi": Nv("mimetypes/kmultiple", badges=(B(MAG, 0.62),)),
    "common/open": Nv("filesystems/folder_blue_open"),
    "common/play": PLAY,
    "common/pause": Nv("actions/player_pause"),
    "common/stop": Nv("actions/player_stop"),
    "common/down": Nv("actions/down"),
    "common/left": Nv("actions/back"),
    "common/right": Nv("actions/forward"),
    "common/up": Nv("actions/up"),
    "common/save": FLOPPY,
    "common/saveas": Nv("actions/save_all"),
    "common/copy": Nv("actions/editcopy"),
    "common/cut": Nv("actions/editcut"),
    "common/paste": Nv("actions/editpaste"),
    "common/error": Nv("actions/no"),
    "common/information": Nv("actions/messagebox_info"),
    "common/ok": Draw(dw.g_ok),
    "common/warning": Draw(dw.g_warning),
    "common/calculator": Nv("apps/kcalc"),
    "debug/breakpoint": Nv("actions/ledred"),
    "debug/breakpoint_disabled": Nv("actions/ledred", gray=0.85, bright=0.8),
    "debug/breakpoint_ignored": Nv("actions/ledred", badges=(B(Draw(dw.g_warning), 0.58),)),
    "debug/breakpoint_pending": Nv("actions/ledred", badges=(B(CLOCK, 0.58),)),
    "debug/current_line": Draw(dw.g_current_line),
    "debug/restart": Nv("actions/reload", hue=-110),
    "debug/start_emulator": Nv("actions/player_play", badges=(B(Nv("apps/package_games_arcade"), 0.62),)),
    "debug/start_engine": Nv("apps/bug", badges=(B(MAG, 0.62),)),
    "debug/step_frame": Nv("actions/player_end"),
    "debug/step_into": Draw(dw.g_step_into),
    "debug/step_out": Draw(dw.g_step_out),
    "debug/step_over": Draw(dw.g_step_over),
    "editors/animations": Nv("apps/kaboodle"),
    "editors/backgrounds": Nv("apps/background"),
    "editors/emulator": Nv("devices/tv", badges=(B(Nv("devices/joystick"), 0.62),)),
    "editors/sounds": Nv("apps/kmix"),
    "editors/sprites": PICTURE,
    "editors/docs": Nv("apps/help_index"),
    "editors/organizer": Nv("actions/kaboodleloop"),
    "editors/states": Nv("mimetypes/document"),
    "editors/definitions": Nv("apps/kdmconfig"),
    "editors/commands": Nv("apps/package_games_arcade"),
    "files/animation": Nv("apps/mplayer"),
    "files/code": Nv("mimetypes/source"),
    "files/command": Nv("apps/package_games"),
    "files/file": Nv("mimetypes/misc_doc"),
    "files/folder": Nv("filesystems/folder_yellow"),
    "files/sound": Nv("apps/arts"),
    "files/sprite": PICTURE,
    "imgedit/commit": Nv("actions/revert"),
    "imgedit/pencil": Nv("actions/pencil"),
    "imgedit/move": Draw(dw.g_outlined("arrows-move")),
    "imgedit/hand": Draw(dw.g_outlined("hand-stop")),
    "imgedit/layers": Nv("actions/thumbnail", stack=2),
    "imgedit/save_layers": Nv("actions/thumbnail", stack=2, badges=(B(Nv("actions/revert"), 0.6),)),
    "imgedit/eraser": Draw(dw.g_eraser),
    "imgedit/free_select": Draw(dw.g_outlined("lasso")),
    "imgedit/magic_wand": Nv("actions/wizard"),
    "imgedit/bucket": Nv("actions/color_fill"),
    "imgedit/rect_select": Draw(dw.g_rect_select),
    "imgedit/remap_pencil": Nv("actions/pencil", badges=(B(Draw(dw.g_switch_colors), 0.6, "tr"),)),
    "project/backup": Nv("actions/filesave", badges=(B(CLOCK, 0.6),)),
    "project/close": Nv("actions/editdelete"),
    "project/close_all": Nv("actions/editdelete", stack=2),
    "project/load_backup": Nv("filesystems/folder_blue", badges=(B(CLOCK, 0.6),)),
    "project/open": Nv("filesystems/folder_blue"),
    "misc/run": Nv("actions/misc", badges=(B(PLAY, 0.6),)),
    "misc/translate": Nv("actions/spellcheck"),
    "misc/options": Nv("apps/kcontrol"),
    "organizer/auto": Nv("actions/up", hue=150, badges=(B(Draw(dw.g_badge_text("AUTO", "#ffffff", "#2a5ad8", 11, 2.4)), 0.8, "bc"),)),
    "organizer/bottom": Nv("actions/bottom"),
    "organizer/down": Nv("actions/down"),
    "organizer/down_10": Nv("actions/down", hue=145, badges=(B(Draw(dw.g_badge_text("10", "#1a3ab0")), 0.6, "tr"),)),
    "organizer/top": Nv("actions/top"),
    "organizer/up": Nv("actions/up"),
    "organizer/up_10": Nv("actions/up", hue=145, badges=(B(Draw(dw.g_badge_text("10", "#1a3ab0")), 0.6, "br"),)),
    "pal/apply_sprite": Nv("actions/thumbnail", badges=(B(PALETTE, 0.62, "tr"),)),
    "pal/add_missing": Draw(dw.g_add_missing),
    "pal/colors_to_left": Draw(dw.g_colors_to("left")),
    "pal/colors_to_right": Draw(dw.g_colors_to("right")),
    "pal/commit_palette": Nv("actions/colorize", badges=(B(FLOPPY, 0.58),)),
    "pal/commit_image": Nv("actions/thumbnail", badges=(B(FLOPPY, 0.58),)),
    "pal/replace_duplicates": Draw(dw.g_replace_duplicates),
    "pal/apply": Nv("actions/apply"),
    "pal/palette_from_images": Nv("actions/thumbnail", stack=2, badges=(B(PALETTE, 0.6, "tr"),)),
    "pal/gradient": Draw(dw.g_gradient),
    "pal/grayscale": Draw(dw.g_grayscale),
    "pal/reset": Nv("actions/button_cancel"),
    "pal/palette_to_images": Nv("actions/colorize", badges=(B(PICTURE, 0.58),)),
    "pal/invert": Draw(dw.g_invert),
    "pal/picker": Nv("apps/kcolorchooser"),
    "pal/save_new": Nv("actions/filesave", badges=(B(Draw(dw.g_add), 0.56),)),
    "pal/selection_to_left": Draw(dw.g_selection_to("left")),
    "pal/selection_to_right": Draw(dw.g_selection_to("right")),
    "pal/set_color": Draw(dw.g_set_color),
    "pal/switch_colors": Draw(dw.g_switch_colors),
    "pal/editor": PALETTE,
    "pal/remap": Nv("apps/fsview"),
    "pal/switch_count": Draw(dw.g_switch_count),
    "search/backward": Nv("actions/undo"),
    "search/case": Draw(dw.g_icon_text("letter-case", "#e01e1e", 40, 0, 3.6)),
    "search/circular": Nv("actions/reload", hue=-110),
    "search/close": Nv("actions/button_cancel"),
    "search/regexp": Draw(dw.g_icon_text("regex", "#e01e1e", 38, 0, 3.6)),
    "sff/crop": Nv("apps/package_utilities"),
    "sff/explorer": Nv("actions/view_icon"),
    "sff/import_project": Nv("actions/thumbnail", badges=(B(RED_BACK, 0.6),)),
    "sff/open_image": Nv("filesystems/folder_image"),
    "sff/save_image": Nv("actions/thumbnail", badges=(B(FLOPPY, 0.58),)),
    "sff/delete_group": Nv("actions/thumbnail", badges=(B(STOP_X, 0.56),)),
    "sff/image_editor": Nv("actions/thumbnail", badges=(B(Nv("actions/pencil"), 0.62),)),
    "sff/export": Nv("actions/thumbnail", badges=(B(RED_FORWARD, 0.6),)),
    "sff/import": Nv("actions/thumbnail", stack=2, badges=(B(RED_BACK, 0.6),)),
    "sff/rename_group": Nv("actions/thumbnail", badges=(B(Draw(dw.g_badge_text("ABC", "#ffffff", "#1a3ab0", 11, 2.4)), 0.8, "bc"),)),
    "snd/open_wav": Nv("filesystems/folder_sound"),
    "snd/play_loop": Nv("actions/player_fwd"),
    "snd/save_wav": Nv("apps/arts", badges=(B(FLOPPY, 0.58),)),
    "snd/hresize": Draw(dw.g_hresize),
    "snd/select": Draw(dw.g_sound_select),
    "snd/vresize": Draw(dw.g_vresize),
    "snd/delete_group": Nv("apps/arts", badges=(B(STOP_X, 0.56),)),
    "view/attack_box": Draw(dw.g_view_box("attack")),
    "view/center_origin": Draw(dw.g_center_origin),
    "view/collision_box": Draw(dw.g_view_box("collision")),
    "view/guide_lines": Draw(dw.g_guide_lines),
    "view/issues": Nv("actions/messagebox_warning"),
    "view/zoom_fit": Nv("actions/viewmagfit"),
    "view/zoom_reset": Nv("actions/viewmag", badges=(B(Draw(dw.g_badge_text("1:1", "#1a3ab0", None, 16, 3.4)), 0.62, "bl"),)),
    "bg/controllers": Nv("actions/misc"),
    "bg/stage": Nv("apps/randr"),
    "guides/top_left": Draw(dw.g_align("tl")),
    "guides/top_center": Draw(dw.g_align("tc")),
    "guides/top_right": Draw(dw.g_align("tr")),
    "guides/center_left": Draw(dw.g_align("cl")),
    "guides/center_center": Draw(dw.g_align("cc")),
    "guides/center_right": Draw(dw.g_align("cr")),
    "guides/bottom_left": Draw(dw.g_align("bl")),
    "guides/bottom_center": Draw(dw.g_align("bc")),
    "guides/bottom_right": Draw(dw.g_align("br")),
    "st/a": Draw(dw.g_state("A", "blue")),
    "st/c": Draw(dw.g_state("C", "green")),
    "st/l": Draw(dw.g_state("L", "orange")),
    "st/s": Draw(dw.g_state("S", "yellow")),
    "st/u": Draw(dw.g_state("U", "grey")),
    "st/command": Nv("apps/package_games_arcade"),
    "st/offset": Nv("apps/kruler"),
    "st/palfx": Nv("apps/kcoloredit", badges=(B(Draw(dw.g_badge_text("FX", "#ffffff", "#d81818", 14, 3.0)), 0.62),)),
    "st/quote": Nv("apps/gaim"),
    "st/throw": Nv("apps/kuser"),
    "st/var_list": Draw(dw.g_var("#d81818", False)),
    "st/var_log": Draw(dw.g_var("#1a4ab0", True)),
}

# --- Flat sets: one table, recoloured for dark and light interfaces -----------------------------
FLAT = {
    "air/clsn1_add": Flat(fn=dw.f_clsn_add("attack"), badge="plus"),
    "air/clsn2_add": Flat(fn=dw.f_clsn_add("collision"), badge="plus"),
    "air/clsn_delete": Flat(fn=dw.f_clsn_delete, badge="x"),
    "air/clsn_delete_frame": Flat(fn=dw.f_clsn_delete_frame, badge="x"),
    "air/clsn_delete_anim": Flat(fn=dw.f_clsn_delete_anim, badge="x"),
    "air/clsn_previous": Flat(fn=dw.f_clsn_previous),
    "air/clsn_switch": Flat(fn=dw.f_clsn_switch),
    "air/clsn_default": Flat(fn=dw.f_clsn_default),
    "air/edit_text": Flat("file-pencil"),
    "air/edit_code": Flat("file-code"),
    "air/align": Flat("layout-align-middle"),
    "air/all_flip": Flat(fn=dw.f_all("flip")),
    "air/all_offset": Flat(fn=dw.f_all("offset")),
    "air/all_scale": Flat(fn=dw.f_all("scale")),
    "air/all_time": Flat(fn=dw.f_all("time")),
    "air/all_trans": Flat(fn=dw.f_all("trans")),
    "air/save_gif": Flat("gif", badge="save"),
    "code/bool": Flat("toggle-left", accent="cyan"),
    "code/comment": Flat("message-2-code"),
    "code/date": Flat("clock", accent="purple"),
    "code/file": Flat("file", accent="orange"),
    "code/float": Flat(fn=dw.f_text("1.23", "#e040a0")),
    "code/goto": Flat("list-numbers", badge="right"),
    "code/int": Flat(fn=dw.f_text("123", "red")),
    "code/keyword": Flat("abc", accent="orange"),
    "code/next_error": Flat("arrow-big-right", accent="red"),
    "code/prev_error": Flat("arrow-big-left", accent="red"),
    "code/string": Flat("quote", accent="blue"),
    "code/cmd": Flat("keyboard"),
    "code/coord": Flat("math-xy"),
    "code/exp": Flat("math-function"),
    "code/flags": Flat("flag"),
    "code/ustring": Flat("abc"),
    "code/version": Flat("versions"),
    "common/optimize": Flat("adjustments-horizontal"),
    "common/redo": Flat("arrow-forward-up"),
    "common/undo": Flat("arrow-back-up"),
    "common/zoom_in": Flat("zoom-in"),
    "common/zoom_out": Flat("zoom-out"),
    "common/add": Flat("square-plus", accent="yellow"),
    "common/add_all": Flat("square-plus-2", accent="yellow"),
    "common/delete": Flat("trash", badge="x"),
    "common/duplicate": Flat("copy-plus"),
    "common/new": Flat("file", badge="plus"),
    "common/new_all": Flat("files", badge="plus"),
    "common/new_aligned": Flat("file", inner="focus-centered", badge="plus"),
    "common/new_all_aligned": Flat("files", inner="focus-centered", badge="plus"),
    "common/find": Flat("file-search"),
    "common/find_multi": Flat("files", badge="search"),
    "common/open": Flat("folder-open"),
    "common/play": Flat("player-play"),
    "common/pause": Flat("player-pause"),
    "common/stop": Flat("player-stop"),
    "common/down": Flat("chevron-down"),
    "common/left": Flat("chevron-left"),
    "common/right": Flat("chevron-right"),
    "common/up": Flat("chevron-up"),
    "common/save": Flat("device-floppy"),
    "common/saveas": Flat("device-floppy", badge="plus"),
    "common/copy": Flat("copy"),
    "common/cut": Flat("scissors"),
    "common/paste": Flat("clipboard"),
    "common/error": Flat(fn=dw.f_dot("red"), badge="xc"),
    "common/information": Flat(fn=dw.f_dot("blue"), badge="ic"),
    "common/ok": Flat(fn=dw.f_dot("green"), badge="checkc"),
    "common/warning": Flat("alert-triangle", accent="yellow"),
    "common/calculator": Flat("calculator"),
    "debug/breakpoint": Flat(fn=dw.f_dot("red")),
    "debug/breakpoint_disabled": Flat(fn=dw.f_dot("grey")),
    "debug/breakpoint_ignored": Flat(fn=dw.f_dot("red"), badge="warning"),
    "debug/breakpoint_pending": Flat(fn=dw.f_dot("red"), badge="clock"),
    "debug/current_line": Flat("arrow-right", accent="yellow"),
    "debug/restart": Flat("rotate-clockwise"),
    "debug/start_emulator": Flat("device-gamepad-2", badge="play"),
    "debug/start_engine": Flat("bug", badge="search"),
    "debug/step_frame": Flat("player-skip-forward"),
    "debug/step_into": Flat("arrow-down-to-arc"),
    "debug/step_out": Flat("arrow-up-from-arc"),
    "debug/step_over": Flat("arrow-forward"),
    "editors/animations": Flat("lucide:clapperboard"),
    "editors/backgrounds": Flat("library-photo"),
    "editors/emulator": Flat("device-tv-old", badge="gamepad"),
    "editors/sounds": Flat("volume"),
    "editors/sprites": Flat("photo"),
    "editors/docs": Flat("lifebuoy"),
    "editors/organizer": Flat("arrows-sort"),
    "editors/states": Flat("file-text"),
    "editors/definitions": Flat("users"),
    "editors/commands": Flat("lucide:joystick"),
    "files/animation": Flat("lucide:clapperboard"),
    "files/code": Flat("file-code"),
    "files/command": Flat("device-gamepad"),
    "files/file": Flat("file"),
    "files/folder": Flat("folder"),
    "files/sound": Flat("file-music"),
    "files/sprite": Flat("photo"),
    "imgedit/commit": Flat("device-floppy", badge="check"),
    "imgedit/pencil": Flat("pencil"),
    "imgedit/move": Flat("arrows-move"),
    "imgedit/hand": Flat("hand-move"),
    "imgedit/layers": Flat("stack-2"),
    "imgedit/save_layers": Flat("stack-2", badge="save"),
    "imgedit/eraser": Flat("eraser"),
    "imgedit/free_select": Flat("lasso"),
    "imgedit/magic_wand": Flat("wand"),
    "imgedit/bucket": Flat("bucket-droplet"),
    "imgedit/rect_select": Flat("marquee-2"),
    "imgedit/remap_pencil": Flat("pencil", badge="swap"),
    "project/backup": Flat("device-floppy", badge="clock"),
    "project/close": Flat("file", badge="x"),
    "project/close_all": Flat("files", badge="x"),
    "project/load_backup": Flat("folder-open", badge="clock"),
    "project/open": Flat("folder-open"),
    "misc/run": Flat("settings", badge="play"),
    "misc/translate": Flat("language"),
    "misc/options": Flat("tool"),
    "organizer/auto": Flat("sort-ascending-numbers"),
    "organizer/bottom": Flat("arrow-bar-to-down"),
    "organizer/down": Flat("chevron-down"),
    "organizer/down_10": Flat("chevrons-down"),
    "organizer/top": Flat("arrow-bar-to-up"),
    "organizer/up": Flat("chevron-up"),
    "organizer/up_10": Flat("chevrons-up"),
    "pal/apply_sprite": Flat("photo", badge="palette"),
    "pal/add_missing": Flat("color-swatch", badge="plus"),
    "pal/colors_to_left": Flat(fn=dw.f_colors_to("left")),
    "pal/colors_to_right": Flat(fn=dw.f_colors_to("right")),
    "pal/commit_palette": Flat("palette", badge="check"),
    "pal/commit_image": Flat("photo", badge="check"),
    "pal/replace_duplicates": Flat("copy-x"),
    "pal/apply": Flat("circle-check", accent="green"),
    "pal/palette_from_images": Flat("photo-scan", badge="palette"),
    "pal/gradient": Flat(fn=dw.f_grid(["yellow", "orange", "red"])),
    "pal/grayscale": Flat(fn=dw.f_grid(["#e0e0e0", "#a0a0a0", "#606060"])),
    "pal/reset": Flat("circle-x", accent="red"),
    "pal/palette_to_images": Flat("photo-down", badge="palette"),
    "pal/invert": Flat("contrast-2"),
    "pal/picker": Flat("color-picker"),
    "pal/save_new": Flat("device-floppy", badge="plus"),
    "pal/selection_to_left": Flat(fn=dw.f_selection_to("left")),
    "pal/selection_to_right": Flat(fn=dw.f_selection_to("right")),
    "pal/set_color": Flat("droplet"),
    "pal/switch_colors": Flat("arrows-exchange"),
    "pal/editor": Flat("palette"),
    "pal/remap": Flat("color-swatch"),
    "pal/switch_count": Flat("grid-4x4"),
    "search/backward": Flat("arrow-back"),
    "search/case": Flat("letter-case"),
    "search/circular": Flat("repeat"),
    "search/close": Flat("x"),
    "search/regexp": Flat("regex"),
    "sff/crop": Flat("crop"),
    "sff/explorer": Flat("layout-grid"),
    "sff/import_project": Flat("file-import"),
    "sff/open_image": Flat("photo-up"),
    "sff/save_image": Flat("photo-down"),
    "sff/delete_group": Flat("photo", badge="x"),
    "sff/image_editor": Flat("photo-edit"),
    "sff/export": Flat("file-export"),
    "sff/import": Flat("file-import", badge="plus"),
    "sff/rename_group": Flat("forms"),
    "snd/open_wav": Flat("music-up"),
    "snd/play_loop": Flat("repeat"),
    "snd/save_wav": Flat("music-down"),
    "snd/hresize": Flat("arrows-horizontal"),
    "snd/select": Flat("pointer"),
    "snd/vresize": Flat("arrows-vertical"),
    "snd/delete_group": Flat("volume", badge="x"),
    "view/attack_box": Flat(fn=dw.f_view_box("attack")),
    "view/center_origin": Flat("focus-centered"),
    "view/collision_box": Flat(fn=dw.f_view_box("collision")),
    "view/guide_lines": Flat(fn=dw.f_guide_lines),
    "view/issues": Flat("alert-triangle"),
    "view/zoom_fit": Flat("zoom-scan"),
    "view/zoom_reset": Flat("zoom-reset"),
    "bg/controllers": Flat("adjustments"),
    "bg/stage": Flat("building-arch"),
    "guides/top_left": Flat(fn=dw.f_align("tl")),
    "guides/top_center": Flat(fn=dw.f_align("tc")),
    "guides/top_right": Flat(fn=dw.f_align("tr")),
    "guides/center_left": Flat(fn=dw.f_align("cl")),
    "guides/center_center": Flat(fn=dw.f_align("cc")),
    "guides/center_right": Flat(fn=dw.f_align("cr")),
    "guides/bottom_left": Flat(fn=dw.f_align("bl")),
    "guides/bottom_center": Flat(fn=dw.f_align("bc")),
    "guides/bottom_right": Flat(fn=dw.f_align("br")),
    "st/a": Flat(fn=dw.f_state("A", "yellow")),
    "st/c": Flat(fn=dw.f_state("C", "green")),
    "st/l": Flat(fn=dw.f_state("L", "red")),
    "st/s": Flat(fn=dw.f_state("S", "cyan")),
    "st/u": Flat(fn=dw.f_state("U", "grey")),
    "st/command": Flat("lucide:joystick"),
    "st/offset": Flat("ruler"),
    "st/palfx": Flat("palette", badge="fx"),
    "st/quote": Flat("message"),
    "st/throw": Flat("hand-grab"),
    "st/var_list": Flat("variable"),
    "st/var_log": Flat("clipboard-list"),
}

FLAT_THEMES = {
    "Flat-Dark": {"glyph": "#cccccc", "badge_dark": "#1a1a1a"},
    "Flat-Light": {"glyph": "#3c3c3c", "badge_dark": "#1a1a1a"},
}

PLUS = Nv("actions/edit_add")
MINUS = Nv("actions/edit_remove")
CLASSIC = {
    "common/new": Nv("mimetypes/misc_doc", badges=(B(PLUS, 0.6, "c"),)),
    "project/close": Nv("filesystems/file_locked"),
    "common/save": Nv("devices/zip_unmount"),
    "project/backup": Nv("mimetypes/file_locked"),
    "misc/run": Nv("devices/samba_unmount", badges=(B(Nv("actions/forward"), 0.6, "bl"),)),
    "common/add": PLUS,
    "common/remove": MINUS,
    "editors/animations": Nv("apps/aktion"),
    "editors/sounds": Nv("apps/juk"),
    "editors/sprites": Nv("actions/kgpg_photo"),
    "editors/organizer": Nv("actions/view_top_bottom", badges=(B(Nv("actions/kaboodleloop"), 0.6),)),
    "air/clsn1_add": Nv("actions/edit_add", hue=-120),
    "air/clsn2_add": Nv("actions/edit_add", hue=100),
    "air/clsn_delete": Nv("actions/edit_remove", hue=-120),
    "air/save_gif": Nv("apps/khangman"),
    "imgedit/commit": Nv("actions/revert"),
    "pal/switch_count": Nv("actions/reload"),
}
ULTIMATE = {
    **CLASSIC,
    "air/clsn_switch": Nv("actions/2uparrow", badges=(B(Nv("actions/2downarrow", hue=145), 0.7, "br"),)),
    "air/clsn_previous": Nv("actions/undo"),
}
# Names that use a different file in a set.
NAME_OVERRIDES = {
    "Classic": {"anim_delete": "common/remove", "anim_frame_delete": "common/remove",
                "snd_discard_changes": "common/remove", "spr_layer_delete": "common/remove"},
    "Ultimate": {"anim_delete": "common/remove", "anim_frame_delete": "common/remove",
                 "snd_discard_changes": "common/remove", "spr_layer_delete": "common/remove",
                 "spr_delete": "common/remove", "snd_delete": "common/remove"},
}


@dataclass
class SetSpec:
    name: str
    kind: str                     # nuvola, flat or variant
    recipes: dict
    theme: dict = field(default_factory=dict)
    base: str | None = None       # for variants: the set whose files are referenced
    description: str = ""


SETS = [
    SetSpec("Nuvola", "nuvola", NUVOLA,
            description="Shiny color icons based on the Nuvola theme (the default preset)."),
    SetSpec("Flat-Dark", "flat", FLAT, FLAT_THEMES["Flat-Dark"],
            description="Flat outline icons in one color, with light shapes for dark interface styles."),
    SetSpec("Flat-Light", "flat", FLAT, FLAT_THEMES["Flat-Light"],
            description="Flat outline icons in one color, with dark shapes for light interface styles."),
    SetSpec("Classic", "variant", CLASSIC, base="Nuvola",
            description="The Nuvola icons with the changes of the Classic preset."),
    SetSpec("Ultimate", "variant", ULTIMATE, base="Nuvola",
            description="The Nuvola icons with the changes of the Ultimate preset."),
]


# === Rendering ==================================================================================
class Context:
    def __init__(self, cache: Path):
        nuv = fetch(SOURCES["nuvola"], cache)
        tab = fetch(SOURCES["tabler"], cache)
        luc = fetch(SOURCES["lucide"], cache)
        self.roots = {"nuvola": nuv, "tabler": tab, "lucide": luc}
        self.nuvola_root = nuv / SOURCES["nuvola"].root
        self._tabler = art.OutlineLibrary([tab / "package/icons/outline"])
        self._lucide = art.OutlineLibrary([luc / "package/icons"])
        self.used: set[str] = set()
        self._img_cache: dict[str, tuple[int, Image.Image]] = {}

    # Libraries record which icons a drawing used, for the Sources file.
    @property
    def tabler(self) -> art.OutlineLibrary:
        return _Recorder(self._tabler, "tabler", self.used)

    @property
    def lucide(self) -> art.OutlineLibrary:
        return _Recorder(self._lucide, "lucide", self.used)

    def lib_for(self, icon: str) -> art.OutlineLibrary:
        return self.lucide if icon.startswith("lucide:") else self.tabler

    def nuvola(self, icon: str) -> Image.Image:
        """The picture at 48 px, scaled down from a larger size when there is no 48 px one."""
        if icon in self._img_cache:
            size, img = self._img_cache[icon]
            self.used.add(f"nuvola:{size}x{size}/{icon}")
            return img
        cat, name = icon.split("/")
        found = None
        for size in (48, 64, 128, 32):
            p = self.nuvola_root / f"{size}x{size}" / cat / f"{name}.png"
            if p.is_file():
                found = (size, p)
                break
        if not found:
            raise KeyError(f"Nuvola icon not found: {icon}")
        size, p = found
        if size == 32:
            print(f"  note: {icon} only exists up to 32 px", file=sys.stderr)
        img = art.fit(Image.open(p), PNG_SIZE)
        self.used.add(f"nuvola:{size}x{size}/{icon}")
        self._img_cache[icon] = (size, img)
        return img


class _Recorder:
    def __init__(self, lib, key, used):
        self._lib, self._key, self._used = lib, key, used

    def elements(self, name):
        self._used.add(f"{self._key}:{name}")
        return self._lib.elements(name)

    def find(self, name):
        return self._lib.find(name)


def render_raster(recipe, ctx: Context, size: int = PNG_SIZE) -> Image.Image:
    if isinstance(recipe, Draw):
        svg = art.Svg(48)
        recipe.fn(svg, ctx)
        return art.render_svg(svg.render(), size)
    img = ctx.nuvola(recipe.icon)
    if recipe.hue:
        img = art.hue_shift(img, recipe.hue)
    if recipe.gray or recipe.bright != 1.0:
        img = art.desaturate(img, recipe.gray, recipe.bright)
    if recipe.flip:
        img = art.mirror(img)
    if recipe.stack > 1:
        img = art.stack(img, PNG_SIZE, recipe.stack, 8)
    for b in recipe.badges:
        img = art.overlay(img, render_raster(b.src, ctx, PNG_SIZE), b.scale, b.anchor)
    if size != PNG_SIZE:
        img = img.resize((size, size), Image.LANCZOS)
    return img


BADGES = {
    # name: (fill accent, glyph drawing in a 24 unit box centred on (18, 18))
    "plus": ("yellow", lambda c: dw.f_stroke("M18 15v6M15 18h6", c, 1.8)),
    "x": ("red", lambda c: dw.f_stroke("M16 16l4 4M20 16l-4 4", "#ffffff", 1.8)),
    "check": ("green", lambda c: dw.f_stroke("M15.6 18.2l1.7 1.7l3.2 -3.6", "#ffffff", 1.8)),
    "play": ("green", lambda c: art.path("M16.6 15.4v5.2l4.3 -2.6z", fill="#ffffff", stroke="#ffffff",
                                          stroke_width=0.8, stroke_linejoin="round")),
    "clock": ("blue", lambda c: dw.f_stroke("M18 15.2v3l2 1.3", "#ffffff", 1.6)),
    "search": ("blue", lambda c: dw.f_stroke("M17.2 17.2m-1.9 0a1.9 1.9 0 1 0 3.8 0a1.9 1.9 0 1 0 -3.8 0M18.6 18.6l2 2",
                                             "#ffffff", 1.4)),
    "save": ("blue", lambda c: dw.f_stroke("M15.6 15.4h4.2l1.2 1.2v4h-5.4zM17 15.4v1.6h2.4", "#ffffff", 1.2)),
    "right": ("blue", lambda c: dw.f_stroke("M15.2 18h5.6M18.4 15.6l2.4 2.4l-2.4 2.4", "#ffffff", 1.6)),
    "warning": ("yellow", lambda c: dw.f_stroke("M18 15.4v3M18 20.6v0.01", c, 1.8)),
    "swap": ("orange", lambda c: dw.f_stroke("M15.4 16.8h5l-1.4 -1.4M20.6 19.2h-5l1.4 1.4", c, 1.4)),
    "palette": ("orange", lambda c: art.circle(16.5, 17, 1.0, fill=c) + art.circle(19.5, 17, 1.0, fill=c)
                + art.circle(18, 19.8, 1.0, fill=c)),
    "gamepad": ("blue", lambda c: dw.f_stroke("M15.2 18h3M16.7 16.5v3M19.6 17.4v0.01M20.8 18.8v0.01", "#ffffff", 1.4)),
    "fx": ("red", None),
    # centred status glyphs (no badge circle)
    "xc": (None, lambda c: dw.f_stroke("M9 9l6 6M15 9l-6 6", "#ffffff", 2.4)),
    "ic": (None, lambda c: dw.f_stroke("M12 11v5M12 8v0.01", "#ffffff", 2.6)),
    "checkc": (None, lambda c: dw.f_stroke("M8.5 12.2l2.5 2.5l4.6 -5", "#ffffff", 2.4)),
}


def render_flat(recipe: Flat, ctx: Context, theme: dict) -> str:
    glyph = theme["glyph"]
    dark = theme["badge_dark"]
    svg = art.Svg(24)
    main = dw.FLAT_ACCENTS.get(recipe.accent, recipe.accent) if recipe.accent else glyph
    if recipe.fn:
        recipe.fn(svg, ctx, main)
    if recipe.icon:
        lib = ctx.lib_for(recipe.icon)
        svg.add(art.outline_group(lib, recipe.icon.split(":")[-1], main))
    if recipe.inner:
        svg.add(art.outline_group(ctx.tabler, recipe.inner, main, 2.4, "translate(7.5,9.5) scale(0.42)"))
    if recipe.badge:
        accent, glyph_fn = BADGES[recipe.badge]
        if accent:
            svg.add(art.circle(18, 18, 5.6, fill=dw.FLAT_ACCENTS[accent]))
        if recipe.badge == "fx":
            els, _ = art.text_elements(ctx.tabler, "FX", 18, 20.3, 4.6, "#ffffff", 1.2, gap=1.0, anchor="middle")
            svg.body.extend(els)
        elif glyph_fn:
            svg.add(glyph_fn(dark))
    return svg.render()


# === Writing sets ===============================================================================
@dataclass
class Output:
    rel: str                      # path relative to the set folder, with extension
    data: bytes
    sources: list[str]
    names: list[str]


def ext_for(recipe) -> str:
    return ".png" if isinstance(recipe, Nv) else ".svg"


def build_file(recipe, ctx: Context, spec: SetSpec) -> tuple[bytes, list[str]]:
    ctx.used = set()
    if isinstance(recipe, Flat):
        data = render_flat(recipe, ctx, spec.theme).encode("utf-8")
    elif isinstance(recipe, Draw):
        svg = art.Svg(48)
        recipe.fn(svg, ctx)
        data = svg.render().encode("utf-8")
    else:
        data = art.png_bytes(render_raster(recipe, ctx))
    return data, sorted(ctx.used)


def layout_for(spec: SetSpec) -> tuple[dict, dict]:
    glob = dict(LAYOUT_GLOBAL)
    glob.update({k: v for k, v in NAME_OVERRIDES.get(spec.name, {}).items() if k in glob})
    eng = dict(LAYOUT_MUGEN)
    return glob, eng


def xml_file(entries: list[tuple[str, str]], comment: str) -> bytes:
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', f"<!-- {comment} -->", "<iconset>"]
    for name, file in sorted(entries):
        lines.append(f'    <icon name="{name}" file="{file}"/>')
    lines.append("</iconset>")
    return ("\n".join(lines) + "\n").encode("utf-8")


def recipe_for(spec: SetSpec, key: str):
    if key in spec.recipes:
        return spec.recipes[key]
    if spec.kind == "variant":
        return None  # taken from the base set
    raise KeyError(f"{spec.name}: no recipe for {key}")


def build_set(spec: SetSpec, ctx: Context) -> dict:
    glob_layout, eng_layout = layout_for(spec)
    files: dict[tuple[str, str], Output] = {}   # (where, key) -> output
    entries_g, entries_e = [], []
    base_rel_g = f"../{spec.base}/" if spec.base else ""
    base_rel_e = f"../{spec.base}/" if spec.base else ""

    def produce(where: str, key: str, name: str) -> tuple[str | None, str]:
        recipe = recipe_for(spec, key)
        if recipe is None:
            base = next(s for s in SETS if s.name == spec.base)
            ext = ext_for(base.recipes[key])
            return None, key + ext
        ext = ext_for(recipe)
        rel = key + ext
        if (where, key) not in files:
            data, used = build_file(recipe, ctx, spec)
            files[(where, key)] = Output(rel, data, used, [])
        files[(where, key)].names.append(name)
        return where, rel

    for name, key in glob_layout.items():
        where, rel = produce("global", key, name)
        entries_g.append((name, rel if where else base_rel_g + rel))
    for name, key in eng_layout.items():
        if key.startswith("*"):
            where, rel = produce("global", key[1:], name)
            entries_e.append((name, "*" + (rel if where else base_rel_g + rel)))
        else:
            where, rel = produce("engine", key, name)
            entries_e.append((name, rel if where else base_rel_e + rel))

    comment = "Built by tools/iconsets/build_iconsets.py from its tables; edit those instead of this file."
    written = {"global": {"iconset.xml": xml_file(entries_g, comment)},
               "engine": {"iconset.xml": xml_file(entries_e, comment)}}
    for (where, key), o in files.items():
        written[where][o.rel] = o.data
    for where in ("global", "engine"):
        outs = [o for (w, _), o in files.items() if w == where]
        text = sources_text(spec, where, outs)
        written[where]["Sources.pdf"] = doc_files.pdf(text, text.splitlines()[0][2:])
        # Variant sets also carry the licenses of the base set files they refer to.
        extra = {"nuvola", "tabler"} if spec.kind == "variant" else set()
        for fname, data in license_files(ctx, outs, extra).items():
            written[where][fname] = data
    return written


def license_files(ctx: Context, outs: list[Output], extra: set[str] = frozenset()) -> dict[str, bytes]:
    used = {s.split(":")[0] for o in outs for s in o.sources} | set(extra)
    out = {}
    for key in sorted(used):
        for lf, name in zip(SOURCES[key].license_files, license_names(key)):
            out[name] = (ctx.roots[key] / lf).read_bytes().replace(b"\r\n", b"\n")
    return out


def sources_text(spec: SetSpec, where: str, outs: list[Output]) -> str:
    used = {s.split(":")[0] for o in outs for s in o.sources}
    if spec.kind == "variant":
        used |= {"nuvola", "tabler"}
    used = sorted(used)
    title = f"{spec.name} icon set" + (" (M.U.G.E.N extras)" if where == "engine" else "")
    lines = [f"# {title}", "", spec.description, ""]
    lines.append("Each file below is listed with the icon names that use it and the pictures it's made from.")
    lines.append("")
    lines.append("This folder is made by tools/iconsets/build_iconsets.py from its tables. Run it again to update it.")
    lines.append("")
    lines.append("## Licenses")
    lines.append("")
    if used:
        for key in used:
            s = SOURCES[key]
            lines.append(f"- **{s.title} {s.version}** ({s.copyright}), {s.license}, from {s.homepage} "
                         f"(archive: {s.url}). License: " + ", ".join(license_names(key)) + ".")
    lines.append("- Pictures marked *drawn* were made for Open Factory Studio and are under the GPL-3.0 like the "
                 "program (see LICENSE in the source code). Where they have letters or numbers, the outlines "
                 "come from Tabler Icons (MIT).")
    if spec.kind == "variant":
        lines.append(f"- Files listed as ../{spec.base}/... belong to the {spec.base} set. See its Sources file.")
    if where == "engine":
        lines.append("- Files that start with * use the file with the same name in the global set. See "
                     "data/iconsets/" + spec.name + "/Sources.pdf.")
    lines.append("")
    lines.append(f"Sources: {', '.join(used) if used else 'none'}")
    lines.append("")
    lines.append("## Files")
    lines.append("")
    lines.append("| File | Used by | Made from |")
    lines.append("| --- | --- | --- |")
    for o in sorted(outs, key=lambda o: o.rel):
        lines.append(f"| {o.rel} | {', '.join(sorted(o.names))} | {describe(o.sources)} |")
    lines.append("")
    return "\n".join(lines)


def license_names(key: str) -> list[str]:
    """Names of the copied license files, e.g. LICENSE.Nuvola.txt."""
    src = SOURCES[key]
    stems = {"license.txt": "LICENSE", "LICENSE": "LICENSE", "readme.txt": "README", "author": "AUTHORS"}
    return [f"{stems[Path(lf).name]}.{src.title.split()[0]}.txt" for lf in src.license_files]


def describe(used: list[str]) -> str:
    parts = []
    nuv = [u.split(":", 1)[1] for u in used if u.startswith("nuvola:")]
    tab = [u.split(":", 1)[1] for u in used if u.startswith("tabler:")]
    luc = [u.split(":", 1)[1] for u in used if u.startswith("lucide:")]
    glyphs = [t for t in tab if t.startswith(("letter-", "number-"))]
    icons = [t for t in tab if t not in glyphs]
    if nuv:
        parts.append("Nuvola " + ", ".join(f"`{n}`" for n in nuv))
    if icons:
        parts.append("Tabler " + ", ".join(f"`{n}`" for n in icons))
    if luc:
        parts.append("Lucide " + ", ".join(f"`{n}`" for n in luc))
    drawn = not nuv and not icons and not luc
    if glyphs:
        parts.append("Tabler letters " + ", ".join(glyphs))
    if drawn or glyphs:
        parts.insert(0, "*drawn*")
    return "; ".join(parts) if parts else "*drawn*"


def write_tree(files: dict[str, bytes], target: Path) -> None:
    if target.exists():
        shutil.rmtree(target)
    for rel, data in files.items():
        p = target / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)


def compare_tree(files: dict[str, bytes], target: Path) -> list[str]:
    problems = []
    on_disk = {str(p.relative_to(target)).replace("\\", "/") for p in target.rglob("*") if p.is_file()} \
        if target.exists() else set()
    for rel, data in files.items():
        p = target / rel
        if not p.is_file():
            problems.append(f"missing: {p}")
        elif not same_file(rel, p.read_bytes(), data):
            problems.append(f"differs: {p}")
    for rel in sorted(on_disk - set(files)):
        problems.append(f"unexpected: {target / rel}")
    return problems


def same_file(rel: str, old: bytes, new: bytes) -> bool:
    # The PDF depends on the fonts of the machine, so it only has to be there.
    return rel.endswith(".pdf") or old == new


def check_complete(spec: SetSpec, written: dict) -> None:
    g = written["global"]["iconset.xml"].decode()
    e = written["engine"]["iconset.xml"].decode()
    gn = set(re.findall(r'name="([^"]+)"', g))
    en = set(re.findall(r'name="([^"]+)"', e))
    missing = sorted(set(REQUIRED["global"]) - gn) + sorted(set(REQUIRED["mugen"]) - en)
    if missing:
        raise SystemExit(f"{spec.name}: names without an icon: {', '.join(missing)}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build the icon sets.")
    parser.add_argument("--check", action="store_true", help="compare with data/ instead of writing")
    parser.add_argument("--sets", help="comma separated set names (default: all)")
    parser.add_argument("--cache", type=Path, default=HERE / ".cache", help="download cache folder")
    parser.add_argument("--deny-hashes", type=Path,
                        help="file with SHA-256 digests that no output file may match (one per line)")
    args = parser.parse_args()
    wanted = set(args.sets.split(",")) if args.sets else {s.name for s in SETS}
    ctx = Context(args.cache)
    deny = set(args.deny_hashes.read_text().split()) if args.deny_hashes else set()
    problems = []
    for spec in SETS:
        if spec.name not in wanted:
            continue
        print(f"building {spec.name} ...")
        written = build_set(spec, ctx)
        check_complete(spec, written)
        for where, files in written.items():
            for rel, data in files.items():
                if hashlib.sha256(data).hexdigest() in deny:
                    problems.append(f"{spec.name}/{where}/{rel}: matches a denied digest")
        targets = {"global": DATA / "iconsets" / spec.name, "engine": DATA / "engines/mugen/iconsets" / spec.name}
        for where, files in written.items():
            if args.check:
                problems.extend(compare_tree(files, targets[where]))
            else:
                write_tree(files, targets[where])
    for p in problems:
        print(p)
    if problems:
        return 1
    print("check passed" if args.check else "done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
