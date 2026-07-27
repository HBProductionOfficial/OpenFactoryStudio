#!/usr/bin/env python3
"""Generates the state controller templates (data/engines/mugen/sctrls/*.sctrl) from syntax.xml.

Run it after adding a controller to syntax.xml. --check only reports out of date templates.
"""
from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DB = ROOT / 'data' / 'engines' / 'mugen' / 'syntax.xml'
OUT = ROOT / 'data' / 'engines' / 'mugen' / 'sctrls'
COMMENT_COLUMN = 32
VERSION_NAME = {'WinBeta': 'WinMUGEN', '1.0': '1.0', '1.1': '1.1', 'ext': 'extended engines'}

SUMMARY = {
    'AfterImage': 'Leaves a trail of delayed copies of the player',
    'AfterImageTime': 'Changes how long the current afterimage effect lasts',
    'AllPalFX': 'Palette effect on the whole screen: players, stage and lifebars',
    'AngleAdd': 'Adds to the angle used by AngleDraw',
    'AngleDraw': 'Draws the player rotated (and optionally scaled) for this tick',
    'AngleMul': 'Multiplies the angle used by AngleDraw',
    'AngleSet': 'Sets the angle used by AngleDraw',
    'AppendToClipboard': 'Adds a line of debug text to the player clipboard',
    'AssertSpecial': 'Turns on special engine flags for this tick',
    'AttackDist': 'Changes the guard distance of the active HitDef',
    'AttackMulSet': 'Sets the multiplier of the damage the player deals',
    'BGPalFX': 'Palette effect on the stage and the lifebars',
    'BindToParent': 'Keeps a helper at a position relative to its parent',
    'BindToRoot': 'Keeps a helper at a position relative to its root',
    'BindToTarget': 'Keeps the player at a position relative to a target',
    'ChangeAnim': 'Switches to another animation',
    'ChangeAnim2': "Switches to an animation of the state owner's AIR file",
    'ChangeState': 'Moves the player to another state',
    'ClearClipboard': 'Erases the debug clipboard',
    'CtrlSet': 'Gives or removes control',
    'DefenceMulSet': 'Sets the multiplier of the damage the player takes',
    'DestroySelf': 'Removes the helper that runs it',
    'DisplayToClipboard': 'Replaces the debug clipboard text with a formatted message',
    'EnvColor': 'Fills the screen with a solid colour',
    'EnvShake': 'Shakes the screen vertically',
    'Explod': 'Creates a visual effect such as a spark, dust or an overlay',
    'ExplodBindTime': 'Changes how long explods stay bound to their anchor',
    'FallEnvShake': 'Shakes the screen with the fall values of the received hit',
    'ForceFeedback': 'Rumbles a force feedback pad',
    'GameMakeAnim': 'Plays a fightfx animation (old controller, use Explod)',
    'Gravity': "Adds the player's yaccel constant to its y velocity",
    'Helper': 'Spawns a helper, a copy of the player running its own states',
    'HitAdd': 'Adds hits to the combo counter',
    'HitBy': 'Only the listed attack types can hit the player',
    'HitDef': 'Defines one hit of an attack',
    'HitFallDamage': 'Applies the fall damage of the received hit',
    'HitFallSet': 'Changes whether and how the player falls after a hit',
    'HitFallVel': 'Applies the fall velocities of the received hit',
    'HitOverride': 'Sends the player to a custom state when hit by matching attacks',
    'HitVelSet': 'Applies the get-hit velocity (old controller)',
    'LifeAdd': "Adds to the player's life",
    'LifeSet': "Sets the player's life",
    'MakeDust': 'Draws dust clouds (old controller, use Explod)',
    'ModifyExplod': 'Changes parameters of existing explods',
    'MoveHitReset': 'Clears the MoveHit, MoveContact and MoveGuarded flags',
    'NotHitBy': 'The listed attack types cannot hit the player',
    'Null': 'Does nothing; change a type to Null to disable a controller',
    'Offset': 'Shifts where the player is drawn',
    'PalFX': 'Temporary palette effect on the player',
    'ParentVarAdd': "Adds to a variable of the helper's parent",
    'ParentVarSet': "Sets a variable of the helper's parent",
    'Pause': 'Freezes the game for a while',
    'PlayerPush': 'Turns pushing between players on or off for this tick',
    'PlaySnd': 'Plays a sound',
    'PosAdd': 'Moves the player by an offset',
    'PosFreeze': 'Keeps the player in place for this tick',
    'PosSet': 'Puts the player at a position',
    'PowerAdd': 'Adds to the power bar',
    'PowerSet': 'Sets the power bar',
    'Projectile': 'Fires a projectile that carries its own HitDef',
    'RemapPal': 'Draws the sprites of one palette with another palette',
    'RemoveExplod': 'Removes explods',
    'ReversalDef': "Counters attacks that touch the player's attack boxes",
    'ScreenBound': 'Lets the player leave the screen and sets camera following',
    'SelfState': 'Returns a player in a custom state to its own states',
    'SndPan': 'Moves a playing sound in the stereo field',
    'SprPriority': 'Sets the drawing layer of the player',
    'StateTypeSet': 'Changes the state type, move type and physics',
    'StopSnd': 'Stops the sound playing on a channel',
    'SuperPause': 'Dramatic pause for super moves that darkens the screen',
    'TargetBind': 'Holds targets at a position relative to the player',
    'TargetDrop': 'Removes targets from the target list',
    'TargetFacing': 'Turns targets relative to the player',
    'TargetLifeAdd': 'Adds to the life of targets',
    'TargetPowerAdd': 'Adds to the power of targets',
    'TargetState': "Sends targets to one of the player's states",
    'TargetVelAdd': 'Adds to the velocity of targets',
    'TargetVelSet': 'Sets the velocity of targets',
    'Trans': "Overrides the player's transparency for this tick",
    'Turn': 'Turns the player around',
    'VarAdd': 'Adds to a variable',
    'VarRandom': 'Puts a random number in an int variable',
    'VarRangeSet': 'Sets a range of variables to one value',
    'VarSet': 'Sets a variable',
    'VelAdd': "Adds to the player's velocity",
    'VelMul': "Multiplies the player's velocity",
    'VelSet': "Sets the player's velocity",
    'VictoryQuote': 'Chooses the quote shown on the victory screen',
    'Width': "Changes the player's width for this tick",
    'Zoom': 'Zooms the camera (undocumented 1.1 controller)',
}

# Short hints, keyed by parameter name in lower case or by "parameter@controller".
HINT = {
    'time': 'duration in ticks', 'pos': 'x, y', 'vel': 'x, y velocity', 'velocity': 'x, y velocity',
    'accel': 'x, y acceleration', 'scale': 'x, y scale', 'id': 'identifier', 'facing': '1 or -1',
    'vfacing': '1 or -1 (flip vertically)', 'x': 'x component', 'y': 'y component',
    'ctrl': '1 = give control', 'anim': 'animation number', 'elem': 'element to start from',
    'sprpriority': 'drawing layer', 'ontop': '1 = draw above everything', 'ownpal': '1 = own palette',
    'remappal': 'group, item of the palette to use', 'removetime': 'ticks, -1 forever, -2 at end of anim',
    'supermovetime': 'ticks unfrozen in a SuperPause', 'pausemovetime': 'ticks unfrozen in a Pause',
    'bindtime': 'ticks bound, -1 forever', 'trans': 'blending', 'alpha': 'source, destination (0-256)',
    'channel': 'sound channel, -1 any', 'pan': 'offset from the player', 'abspan': 'offset from the centre',
    'add': 'r, g, b added', 'mul': 'r, g, b multiplier (256 = 1)', 'sinadd': 'r, g, b amplitude, period',
    'invertall': '1 = negative colours', 'color': '0 = grey, 256 = normal', 'kill': '0 = cannot KO',
    'absolute': '1 = ignore attack/defence', 'v': 'int variable number', 'fv': 'float variable number',
    'var': 'var(n) = value', 'fvar': 'fvar(n) = value', 'sysvar': 'sysvar(n) = value',
    'sysfvar': 'sysfvar(n) = value', 'stateno': 'state number', 'postype': 'what pos is relative to',
    'space': 'coordinate space', 'random': 'x, y random spread', 'shadow': '1 = draw a shadow',
    'removeongethit': '1 = remove when hit', 'ignorehitpause': '1 = keep animating in hit pauses',
    'angle': 'degrees', 'xangle': 'degrees around x', 'yangle': 'degrees around y',
    'bindid': 'player to bind to, -1 any, -2 none', 'palcolor': '0 grey .. 256 normal',
    'palinvertall': '1 = negative', 'palbright': 'r, g, b added', 'palcontrast': 'r, g, b multiplier',
    'palpostbright': 'r, g, b added last', 'paladd': 'r, g, b added per frame',
    'palmul': 'r, g, b multiplier per frame', 'timegap': 'save every n-th frame',
    'framegap': 'draw every n-th saved frame', 'length': 'frames kept (max 60)',
    'attr': 'state type, attack class', 'hitflag': 'H L A F D + -', 'guardflag': 'H L A',
    'affectteam': 'E enemy, B both, F friend', 'animtype': 'get-hit animation',
    'air.animtype': 'when P2 is in the air', 'fall.animtype': 'when P2 is falling',
    'priority': 'priority 1-7, Hit/Miss/Dodge', 'damage': 'hit damage, guard damage',
    'pausetime': 'P1 pause, P2 shake', 'guard.pausetime': 'P1 pause, P2 shake when guarded',
    'sparkno': 'spark anim (S = own AIR)', 'guard.sparkno': 'guard spark (S = own AIR)',
    'sparkxy': 'spark x (from P2 front), y', 'hitsound': 'group, item (S = own SND)',
    'guardsound': 'group, item (S = own SND)', 'ground.type': 'High, Low, Trip, None',
    'air.type': 'type when P2 is in the air', 'ground.slidetime': 'ticks P2 slides',
    'guard.slidetime': 'ticks P2 slides when guarding', 'ground.hittime': 'ticks P2 stays in hit state',
    'guard.hittime': 'hit time when guarded', 'air.hittime': 'hit time in the air',
    'guard.ctrltime': 'ticks before P2 regains control', 'guard.dist': 'guard distance',
    'yaccel': 'P2 y acceleration', 'ground.velocity': 'P2 x, y velocity on the ground',
    'guard.velocity': 'P2 x velocity when guarding', 'air.velocity': 'P2 x, y velocity in the air',
    'airguard.velocity': 'x, y velocity when guarding in the air',
    'airguard.ctrltime': 'control time after an air guard', 'air.juggle': 'juggle points needed',
    'mindist': 'minimum x, y distance of P2', 'maxdist': 'maximum x, y distance of P2',
    'snap': 'x, y position P2 is moved to', 'p1sprpriority': 'P1 layer after contact',
    'p2sprpriority': 'P2 layer after contact', 'p1facing': '-1 = turn P1',
    'p1getp2facing': '1 same as P2, -1 opposite', 'p2facing': '1 face P1 direction, -1 away',
    'p1stateno': 'P1 state after the hit', 'p2stateno': 'P2 state after the hit (custom state)',
    'p2getp1state': '0 = P2 keeps its own states', 'forcestand': '1 = P2 stands up',
    'fall': '1 = knock down', 'fall.xvelocity': 'bounce x velocity', 'fall.yvelocity': 'bounce y velocity',
    'fall.recover': '0 = no recovery', 'fall.recovertime': 'ticks before recovery',
    'fall.damage': 'damage on landing', 'air.fall': 'fall when hit in the air',
    'forcenofall': '1 = stop P2 falling', 'down.velocity': 'x, y velocity on a downed P2',
    'down.hittime': 'slide time on a downed P2', 'down.bounce': '1 = downed P2 bounces',
    'chainid': 'only after hit with this id', 'nochainid': 'not after these ids',
    'hitonce': '1 = one opponent only', 'guard.kill': '0 = guard cannot KO', 'fall.kill': '0 = fall cannot KO',
    'numhits': 'hits added to the combo', 'getpower': 'P1 power on hit, on guard',
    'givepower': 'P2 power on hit, on guard', 'palfx.time': 'P2 palette effect ticks',
    'envshake.time': 'screen shake ticks', 'fall.envshake.time': 'shake ticks on landing',
    'sprpriority@hitdef': 'old name of p1sprpriority',
    'projid': 'projectile id', 'projanim': 'flight animation', 'projhitanim': 'animation on hit',
    'projremanim': 'animation on removal', 'projcancelanim': 'animation when cancelled',
    'projscale': 'x, y scale', 'projremove': '0 = keep after hitting', 'projremovetime': 'ticks, -1 forever',
    'remvelocity': 'x, y velocity while removed', 'velmul': 'x, y multiplier per tick',
    'projhits': 'hits before removal', 'projmisstime': 'ticks between hits', 'projpriority': 'clash priority',
    'projsprpriority': 'drawing layer', 'projedgebound': 'removal distance off screen',
    'projstagebound': 'removal distance off stage', 'projheightbound': 'lowest, highest y',
    'offset': 'x, y start offset', 'projshadow': '1 = draw a shadow', 'helpertype': 'player type is unsupported',
    'name': 'name in quotes', 'keyctrl': '1 = reads commands', 'flag': 'flag name', 'flag2': 'flag name',
    'flag3': 'flag name', 'text': 'message in quotes, %d %f for params', 'params': 'up to 6 expressions',
    'under': '1 = behind the players', 'freq': 'speed', 'ampl': 'amplitude', 'phase': 'phase offset',
    'waveform': 'motor pattern', 'self': '1 = own pad, 0 = opponent', 'recursive': '1 = also descendants',
    'removeexplods': '1 = also its explods', 'source': 'group, item of the palette to replace',
    'dest': 'group, item of the palette to use', 'statetype': 'S, C, A, L', 'movetype': 'I, A, H',
    'physics': 'S, C, A, N', 'excludeid': 'keep targets with this id', 'keepone': '1 = keep only one target',
    'range': 'lowest, highest', 'first': 'first variable', 'last': 'last variable', 'fvalue': 'float value',
    'edge': 'front, back edge width', 'player': 'front, back player width', 'movecamera': 'x, y follow',
    'darken': '0 = no darkening', 'p2defmul': 'defence multiplier of targets', 'poweradd': 'power added',
    'unhittable': '0 = can be hit', 'sound': 'group, item (S = own SND)', 'endcmdbuftime': 'command buffer at end',
    'movetime': 'ticks the player may still move', 'pausebg': '0 = stage keeps moving',
    'volumescale': 'percent', 'volume': 'replaced by volumescale in 1.0', 'lowpriority': '1 = cannot be cut',
    'freqmul': 'pitch multiplier', 'loop': '1 = loop', 'slot': '0-7', 'forceair': '1 = treat as airborne',
    'xvel': 'fall x velocity', 'yvel': 'fall y velocity', 'pos2': 'second cloud x, y', 'spacing': 'ticks between clouds',
    'lag': 'camera lag', 'camerabound': '1 = keep in camera bounds', 'stagebound': '1 = keep in stage bounds',
    'reversal.attr': 'attacks that can be reversed', 'size.xscale': 'helper x scale',
    'size.yscale': 'helper y scale', 'size.height': 'helper height', 'size.head.pos': 'x, y',
    'size.mid.pos': 'x, y', 'ground.cornerpush.veloff': 'P1 push-back in the corner',
    'air.cornerpush.veloff': 'corner push-back on an airborne P2',
    'down.cornerpush.veloff': 'corner push-back on a downed P2',
    'guard.cornerpush.veloff': 'corner push-back when guarded',
    'airguard.cornerpush.veloff': 'corner push-back on an air guard', 'palfx.mul': 'r, g, b multiplier',
    'palfx.add': 'r, g, b added', 'palfx.sinadd': 'r, g, b amplitude, period', 'palfx.invertall': '1 = negative',
    'palfx.color': '0 grey .. 256 normal', 'envshake.freq': 'shake speed', 'envshake.ampl': 'shake amplitude',
    'envshake.phase': 'shake phase', 'fall.envshake.freq': 'shake speed', 'fall.envshake.ampl': 'shake amplitude',
    'fall.envshake.phase': 'shake phase', 'scale@angledraw': 'x, y scale', 'size.ground.back': 'helper width',
    'size.ground.front': 'helper width', 'size.air.back': 'helper width', 'size.air.front': 'helper width',
    'size.proj.doscale': '1 = scale projectiles', 'size.shadowoffset': 'shadow offset',
    'supermove': 'old form of supermovetime',
}
VALUE_HINT = {
    'AfterImageTime': 'duration in ticks', 'AngleAdd': 'degrees', 'AngleDraw': 'degrees', 'AngleMul': 'factor',
    'AngleSet': 'degrees', 'AttackDist': 'guard distance', 'AttackMulSet': 'damage multiplier',
    'ChangeAnim': 'animation number', 'ChangeState': 'state number', 'CtrlSet': '1 = control',
    'DefenceMulSet': 'damage taken multiplier', 'EnvColor': 'r, g, b', 'ExplodBindTime': 'ticks',
    'GameMakeAnim': 'fightfx animation', 'HitAdd': 'hits', 'HitBy': 'state types, attack classes',
    'HitFallSet': '-1 keep, 0 no fall, 1 fall', 'LifeAdd': 'life added', 'LifeSet': 'new life',
    'PlayerPush': '0 = no pushing', 'PlaySnd': 'group, item (F = common sounds)', 'PosFreeze': '1 = freeze',
    'PowerAdd': 'power added', 'PowerSet': 'new power', 'ScreenBound': '0 = may leave the screen',
    'SprPriority': '-5 .. 5', 'TargetFacing': '1 same as player, -1 opposite', 'TargetLifeAdd': 'life added',
    'TargetPowerAdd': 'power added', 'TargetState': 'state number', 'VarAdd': 'value added',
    'VarRangeSet': 'int value', 'VarSet': 'new value', 'ParentVarAdd': 'value added',
    'ParentVarSet': 'new value', 'VictoryQuote': 'quote index, -1 random', 'Width': 'front, back width',
}
# Example values for the templates, keyed by parameter name or by "controller.parameter".
EXAMPLE = {
    'changestate.value': '0', 'changestate.ctrl': '1', 'changeanim.value': '0', 'ctrlset.value': '1',
    'hitdef.attr': 'S, NA', 'hitdef.damage': '20, 0', 'hitdef.pausetime': '8, 8', 'hitdef.sparkxy': '-10, -70',
    'hitdef.hitsound': '5, 0', 'hitdef.guardsound': '6, 0', 'hitdef.ground.velocity': '-4',
    'hitdef.air.velocity': '-2, -4', 'hitdef.priority': '4, Hit', 'hitdef.ground.slidetime': '8',
    'hitdef.ground.hittime': '10', 'hitdef.hitflag': 'MAF', 'hitdef.guardflag': 'MA',
    'hitdef.envshake.time': '10', 'hitdef.envshake.freq': '60', 'hitdef.envshake.ampl': '-4',
    'hitdef.palfx.mul': '256, 256, 256', 'hitdef.p1sprpriority': '1', 'hitdef.p2getp1state': '1',
    'hitdef.fall.recover': '1', 'hitdef.kill': '1', 'hitdef.guard.kill': '1', 'hitdef.fall.kill': '1',
    'hitdef.numhits': '1', 'hitdef.p1stateno': '-1', 'hitdef.p2stateno': '-1', 'hitdef.chainid': '-1',
    'hitdef.nochainid': '-1, -1', 'hitdef.id': '1', 'hitdef.yaccel': '.35',
    'projectile.projanim': '0', 'projectile.projremovetime': '-1', 'projectile.velocity': '4, 0',
    'projectile.velmul': '1, 1', 'projectile.projhits': '1', 'projectile.projpriority': '1',
    'projectile.projsprpriority': '3', 'projectile.projhitanim': '-1', 'projectile.projid': '1',
    'playsnd.value': '0, 0', 'playsnd.volumescale': '100', 'playsnd.channel': '-1', 'playsnd.freqmul': '1.0',
    'explod.anim': '0', 'explod.removetime': '-2', 'explod.bindtime': '1', 'explod.bindid': '-2',
    'explod.facing': '1', 'explod.vfacing': '1', 'explod.scale': '1, 1', 'explod.space': 'Stage',
    'explod.remappal': '-1, 0', 'explod.ignorehitpause': '1', 'explod.trans': 'Add', 'explod.alpha': '256, 256',
    'explod.id': '1', 'modifyexplod.id': '1', 'helper.name': '"Helper"', 'helper.id': '1', 'helper.stateno': '0',
    'helper.facing': '1', 'helper.remappal': '-1, 0', 'helper.size.xscale': '1', 'helper.size.yscale': '1',
    'superpause.time': '30', 'superpause.anim': '30', 'superpause.sound': '-1', 'superpause.darken': '1',
    'superpause.unhittable': '1', 'superpause.p2defmul': '0', 'pause.time': '30', 'pause.pausebg': '1',
    'palfx.time': '10', 'palfx.add': '0, 0, 0', 'palfx.mul': '256, 256, 256', 'palfx.sinadd': '0, 0, 0, 1',
    'palfx.color': '256', 'envshake.time': '10', 'envshake.freq': '60', 'envshake.ampl': '-4',
    'envcolor.value': '255, 255, 255', 'envcolor.time': '1', 'trans.trans': 'Add', 'trans.alpha': '256, 128',
    'assertspecial.flag': 'Invisible', 'afterimage.time': '1', 'afterimage.length': '20',
    'afterimage.palcolor': '256', 'afterimage.palbright': '30, 30, 30', 'afterimage.palcontrast': '120, 120, 220',
    'afterimage.palpostbright': '0, 0, 0', 'afterimage.paladd': '10, 10, 25', 'afterimage.palmul': '.65, .65, .75',
    'afterimage.timegap': '1', 'afterimage.framegap': '4', 'afterimage.trans': 'None',
    'forcefeedback.time': '60', 'forcefeedback.freq': '128, 0, 0, 0', 'forcefeedback.ampl': '128, 0, 0, 0',
    'forcefeedback.self': '1', 'remappal.source': '1, 1', 'remappal.dest': '1, 2', 'sprpriority.value': '2',
    'varrandom.range': '0, 1000', 'varrangeset.last': '59', 'hitoverride.attr': 'SCA, AA', 'hitoverride.time': '1',
    'hitoverride.slot': '0', 'hitby.value': 'SCA', 'nothitby.value': 'SCA', 'hitby.time': '1',
    'nothitby.time': '1', 'reversaldef.reversal.attr': 'SCA, NA, SA', 'reversaldef.pausetime': '8, 8',
    'displaytoclipboard.text': '"value: %d"', 'displaytoclipboard.params': 'var(0)',
    'appendtoclipboard.text': '"value: %d"', 'appendtoclipboard.params': 'var(0)',
    'screenbound.movecamera': '1, 1', 'screenbound.value': '1', 'posfreeze.value': '1', 'targetdrop.keepone': '1',
    'targetdrop.excludeid': '-1', 'width.edge': '0, 0', 'width.player': '0, 0', 'victoryquote.value': '-1',
    'zoom.scale': '1', 'lifeadd.kill': '1', 'targetlifeadd.kill': '1', 'stoppsnd.channel': '-1',
    'stopsnd.channel': '-1', 'sndpan.channel': '0', 'hitfallset.value': '-1', 'gravity': '',
    'statetypeset.statetype': 'S', 'statetypeset.movetype': 'I', 'statetypeset.physics': 'S',
    'bindtoparent.time': '1', 'bindtoroot.time': '1', 'bindtotarget.time': '1', 'bindtotarget.id': '-1',
    'bindtotarget.pos': '0, 0, Foot', 'targetbind.time': '1', 'targetbind.id': '-1', 'explodbindtime.id': '-1',
    'explodbindtime.time': '1', 'removeexplod.id': '-1', 'attackmulset.value': '1', 'defencemulset.value': '1',
    'anglemul.value': '1', 'playerpush.value': '0', 'pause.endcmdbuftime': '0', 'superpause.endcmdbuftime': '0',
    'projectile.attr': 'S, SP', 'projectile.projscale': '1, 1', 'projectile.projremove': '1',
    'explod.shadow': '0', 'projectile.projshadow': '0', 'assertspecial.flag2': 'NoAutoTurn',
    'assertspecial.flag3': 'NoShadow', 'projectile.offset': '20, -50', 'projectile.projremanim': '-1',
    'projectile.projcancelanim': '-1',
    # any controller
    'remappal': '-1, 0', 'scale': '1, 1', 'facing': '1', 'vfacing': '1', 'alpha': '256, 256', 'time': '1',
}
# Controllers that need one of several optional parameters to do anything. Those parameters are
# written uncommented, like required ones.
PRIMARY = {
    'VarSet': ['v', 'value'], 'VarAdd': ['v', 'value'], 'ParentVarSet': ['v', 'value'],
    'ParentVarAdd': ['v', 'value'], 'VarRangeSet': ['value'], 'HitBy': ['value', 'time'],
    'NotHitBy': ['value', 'time'], 'AfterImageTime': ['time'], 'SndPan': ['pan'],
    'VelSet': ['x', 'y'], 'VelAdd': ['x', 'y'], 'VelMul': ['x', 'y'], 'PosSet': ['x', 'y'],
    'PosAdd': ['x', 'y'], 'Offset': ['x', 'y'], 'TargetVelSet': ['x', 'y'], 'TargetVelAdd': ['x', 'y'],
    'StateTypeSet': ['statetype', 'movetype', 'physics'], 'Width': ['edge', 'player'],
    'RemapPal': ['source', 'dest'], 'HitVelSet': ['x', 'y'],
}
TYPE_EXAMPLE = {
    'int': '0', 'float': '0', 'int2': '0, 0', 'int3': '0, 0, 0', 'int4': '0, 0, 0, 0', 'float2': '0, 0',
    'float3': '0, 0, 0', 'ifloat3': '0, 0, 0, 0', 'intvec': '0', 'string': '""', 'ustring': '', 'file': '',
    'keyseq': 'x', 'exp': '0', 'expi': '0', 'expf': '0', 'expb': '0', 'exp2': '0, 0', 'exp3': '0, 0, 0',
    'exp4': '0, 0, 0, 0', 'exp2i': '0, 0', 'exp3i': '0, 0, 0', 'exp4i': '0, 0, 0, 0', 'exp2f': '0, 0',
    'exp3f': '0, 0, 0', 'exp4f': '0, 0, 0, 0', 'exp2b': '0, 0', 'exp3b': '0, 0, 0', 'expvec': '0',
    'hitattr': 'SCA', 'hitflags': 'MAF', 'guardflags': 'MA',
}


class Param:
    def __init__(self, el):
        self.name = el.get('name')
        self.type = el.get('type')
        self.required = el.get('required') == 'true'
        self.list = el.get('list')
        self.char = el.get('char')
        self.indexed = el.get('format') == 'indexed'
        self.minver = el.get('minver')
        self.deprecated = el.get('deprecated')


def load(db_path: Path):
    root = ET.parse(db_path).getroot()
    state = next(f for f in root.findall('file') if f.get('type') == 'state')
    blocks = {}
    for b in state.findall('block'):
        full = b.get('type').replace('::', ':')
        name, _, base = full.partition(':')
        blocks[name] = (base or None, [Param(p) for p in b.findall('param')])
    return blocks


def params_of(blocks, name):
    """Own parameters first, then inherited ones that are not overridden, as (param, origin)."""
    own_base, own = blocks[name]
    origin = name.split('>', 1)[1]
    out = [(p, origin) for p in own]
    seen = {p.name.lower() for p in own}
    base = own_base
    while base:
        key = base if base in blocks else 'State>' + base
        if key not in blocks:
            break
        origin = key.split('>', 1)[1]
        base, params = blocks[key]
        for p in params:
            if p.name.lower() not in seen:
                seen.add(p.name.lower())
                out.append((p, origin))
    return out


def example(ctrl: str, p: Param, origin: str | None = None) -> str:
    low = p.name.lower()
    keys = [f'{ctrl.lower()}.{low}', f'{(origin or ctrl).lower()}.{low}']
    if low.startswith('afterimage.'):
        keys.append('afterimage.' + low[len('afterimage.'):])
    keys.append(low)
    for key in keys:
        if key in EXAMPLE:
            return EXAMPLE[key]
    if p.type in ('keyword', 'expkeyword', 'exp2keyword') and p.list:
        first = p.list.split(',')[0]
        return {'keyword': first, 'expkeyword': '4, ' + first, 'exp2keyword': '0, 0, ' + first}[p.type]
    if p.name.lower() in ('mul',) and p.type.startswith('exp3'):
        return '256, 256, 256'
    return TYPE_EXAMPLE.get(p.type, '0')


def hint(ctrl: str, p: Param) -> str:
    low = p.name.lower()
    if low.startswith('afterimage.') and low[len('afterimage.'):] in HINT:
        low = low[len('afterimage.'):]
    if low == 'value' and ctrl in VALUE_HINT:
        text = VALUE_HINT[ctrl]
    else:
        text = HINT.get(f'{low}@{ctrl.lower()}') or HINT.get(low)
        if text is None and p.type in ('keyword', 'expkeyword', 'exp2keyword') and p.list:
            text = p.list.replace(',', ', ')
        if text is None:
            text = ''
    words = set(text.replace(',', ' ').replace(':', ' ').lower().split())
    listed = p.list and all(k.lower() in words for k in p.list.split(','))
    if p.type == 'keyword' and p.list and len(p.list) <= 40 and not listed:
        text = (text + ': ' if text else '') + p.list.replace(',', ', ')
    notes = []
    if p.minver:
        notes.append(VERSION_NAME.get(p.minver, p.minver) + '+')
    if p.deprecated:
        notes.append('deprecated since ' + VERSION_NAME.get(p.deprecated, p.deprecated))
    if notes:
        text = (text + ' ' if text else '') + '(' + ', '.join(notes) + ')'
    return text


def line(text: str, comment: str) -> str:
    if not comment:
        return text.rstrip()
    pad = max(COMMENT_COLUMN - len(text), 1)
    return text + ' ' * pad + '; ' + comment


def render(ctrl: str, params) -> str:
    out = [f'; {ctrl}: {SUMMARY.get(ctrl, "")}'.rstrip(), f'[State 0, {ctrl}]', f'type = {ctrl}', 'trigger1 =']
    primary = [n.lower() for n in PRIMARY.get(ctrl, [])]
    shown = []
    for p, origin in params:
        if '*' in p.name:
            continue
        on = p.required or p.name.lower() in primary
        shown.append((0 if on else 1, p, origin, on))
    shown.sort(key=lambda t: t[0])
    for _, p, origin, on in shown:
        name = f'{p.name}(0)' if p.indexed else p.name
        text = f'{name} = {example(ctrl, p, origin)}'.rstrip()
        out.append(line(text if on else ';' + text, hint(ctrl, p)))
    if 'ignorehitpause' not in {p.name.lower() for p, _ in params}:
        out.append(line(';ignorehitpause = 0', '1 = also run during hit pauses'))
    out.append(line(';persistent = 1', '0 = once per state, n = every n-th time'))
    return '\r\n'.join(out) + '\r\n'


def render_statedef(params) -> str:
    out = ['; StateDef: starts a state and sets its initial properties', '[Statedef 0]']
    values = {'type': 'S', 'movetype': 'I', 'physics': 'S', 'anim': '0', 'velset': '0, 0', 'ctrl': '1',
              'sprpriority': '0'}
    hints = {'type': 'S stand, C crouch, A air, L lie down, U unchanged',
             'movetype': 'I idle, A attack, H being hit, U unchanged',
             'physics': 'S stand, C crouch, A air, N none, U unchanged', 'anim': 'animation number',
             'velset': 'x, y velocity on entry', 'ctrl': '1 = give control', 'poweradd': 'power added on entry',
             'juggle': 'juggle points this attack needs', 'facep2': '1 = turn to face the opponent',
             'hitdefpersist': '1 = keep the active HitDef', 'movehitpersist': '1 = keep the MoveHit flags',
             'hitcountpersist': '1 = keep the hit counter', 'sprpriority': 'drawing layer'}
    common = ['type', 'movetype', 'physics', 'anim', 'velset', 'ctrl', 'sprpriority']
    for p in sorted(params, key=lambda p: p.name.lower() not in common):
        low = p.name.lower()
        on = low in common
        text = f'{p.name} = {values.get(low, "0")}'
        out.append(line(text if on else ';' + text, hints.get(low, '')))
    return '\r\n'.join(out) + '\r\n'


def generate(db_path: Path) -> dict[str, str]:
    blocks = load(db_path)
    files = {'StateDef.sctrl': render_statedef(blocks['StateDef'][1])}
    for name in blocks:
        if name.startswith('State>'):
            ctrl = name.split('>', 1)[1]
            files[f'{ctrl}.sctrl'] = render(ctrl, params_of(blocks, name))
    return files


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--syntax', type=Path, default=DB)
    ap.add_argument('--out', type=Path, default=OUT)
    ap.add_argument('--check', action='store_true', help='only report templates that are out of date')
    args = ap.parse_args(argv)
    files = generate(args.syntax)
    stale = []
    if not args.check:
        args.out.mkdir(parents=True, exist_ok=True)
    for fname, text in sorted(files.items()):
        path = args.out / fname
        data = text.encode('ascii')
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                stale.append(fname)
        elif not path.is_file() or path.read_bytes() != data:
            path.write_bytes(data)
    if args.check:
        extra = sorted(p.name for p in args.out.glob('*.sctrl') if p.name not in files)
        for f in stale:
            print(f'out of date: {f}')
        for f in extra:
            print(f'not generated from syntax.xml: {f}')
        return 1 if stale or extra else 0
    print(f'{len(files)} templates in {args.out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
