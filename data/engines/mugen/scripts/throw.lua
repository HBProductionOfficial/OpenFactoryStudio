-- States > Throw creator: the states of a throw, made from the settings of the dialog.
--
-- throw(t) receives:
--   t.stateno     StateDef of the throw attempt. The throw itself uses stateno + 10, the
--                 opponent stateno + 20 while held and stateno + 21 once thrown.
--   t.startAnim   animation of the attempt
--   t.throwAnim   animation of the throw
--   t.thrownAnim  animation of the opponent while held
--   t.xVel        velocity of the opponent when thrown
--   t.yVel
--   t.binds       one entry per element of the throw, up to the element the throw stops on:
--                 x, y (where the opponent is held) and priority (sprite priority, 0 for none)
-- and returns the text inserted at the cursor.

local function decimal(value)
    if value == 0 then
        value = 0.0 -- no "-0.0"
    end
    return string.format("%.1f", value)
end

function throw(t)
    local n = t.stateno
    local lines = {}
    local function add(...)
        for _, line in ipairs({ ... }) do
            lines[#lines + 1] = line
        end
    end

    -- The attempt: a throw HitDef that sends both players to their states when it connects.
    add("[Statedef " .. n .. "]",
        "type = S",
        "movetype= A",
        "physics = S",
        "juggle = 0",
        "velset = 0, 0",
        "anim = " .. t.startAnim,
        "",
        "[State " .. n .. ", 1]",
        "type = HitDef",
        "trigger1 = Time = 0",
        "attr = S, NT",
        "hitflag = M-",
        "priority = 1, Miss",
        "sparkno = -1",
        "p1sprpriority = 1",
        "p1facing = ifelse(command = \"holdfwd\", 1, -1)",
        "p2facing = 1",
        "p1stateno = " .. (n + 10),
        "p2stateno = " .. (n + 20),
        "guard.dist = 0",
        "fall = 1",
        "",
        "[State " .. n .. ", 2]",
        "type = ChangeState",
        "trigger1 = AnimTime = 0",
        "value = 0",
        "ctrl = 1",
        "")

    -- The throw: the opponent is held at each element's position, then hurt and thrown.
    local throwState = n + 10
    add("[Statedef " .. throwState .. "]",
        "type = S",
        "movetype= A",
        "physics = N",
        "anim = " .. t.throwAnim,
        "poweradd = 60",
        "")
    for element, bind in ipairs(t.binds) do
        if bind.priority ~= 0 then
            add("[State " .. throwState .. ", SprPriority]",
                "type = SprPriority",
                "trigger1 = AnimElem = " .. element,
                "value = " .. bind.priority,
                "")
        end
        add("[State " .. throwState .. ", Bind " .. element .. "]",
            "type = TargetBind",
            "trigger1 = AnimElem = " .. element,
            "pos = " .. decimal(bind.x) .. ", " .. decimal(bind.y),
            "")
    end
    local last = #t.binds
    add("[State " .. throwState .. ", Hurt]",
        "type = TargetLifeAdd",
        "trigger1 = AnimElem = " .. last,
        "value = -78",
        "",
        "[State " .. throwState .. ", Throw]",
        "type = TargetState",
        "trigger1 = AnimElem = " .. last,
        "value = " .. (n + 21),
        "",
        "[State " .. throwState .. ", State End]",
        "type = ChangeState",
        "trigger1 = AnimTime = 0",
        "value = 0",
        "ctrl = 1",
        "")

    -- The opponent, held.
    local held = n + 20
    add("[Statedef " .. held .. "]",
        "type = A",
        "movetype = H",
        "physics = N",
        "velset = 0,0",
        "",
        "[State " .. held .. ", 1]",
        "type = ChangeAnim2",
        "trigger1 = Time = 0",
        "value = " .. t.thrownAnim,
        "")

    -- The opponent, thrown: falls until it recovers or hits the ground.
    local thrown = n + 21
    add("[Statedef " .. thrown .. "]",
        "type = A",
        "movetype = H",
        "physics = N",
        "velset = " .. t.xVel .. "," .. t.yVel,
        "poweradd = 40",
        "",
        "[State " .. thrown .. ", 1] ;Gravity",
        "type = VelAdd",
        "trigger1 = 1",
        "y = .4",
        "",
        "[State " .. thrown .. ", 2] ; Recover near ground (use ChangeState)",
        "type = ChangeState",
        "triggerall = Vel Y > 0",
        "triggerall = Pos Y >= -20",
        "triggerall = alive",
        "triggerall = CanRecover",
        "trigger1 = Command = \"recovery\"",
        "value = 5200 ;HITFALL_RECOVER",
        "",
        "[State " .. thrown .. ", 3] ; Recover in mid air (use SelfState)",
        "type = SelfState",
        "triggerall = Vel Y > 0",
        "triggerall = alive",
        "triggerall = CanRecover",
        "trigger1 = Command = \"recovery\"",
        "value = 5210 ;HITFALL_AIRRECOVER",
        "",
        "[State " .. thrown .. ", 4] ;Hit ground",
        "type = SelfState",
        "trigger1 = Vel Y > 0",
        "trigger1 = Pos Y >= 0",
        "value = 5100 ;Hit ground")

    return table.concat(lines, "\n") .. "\n"
end
