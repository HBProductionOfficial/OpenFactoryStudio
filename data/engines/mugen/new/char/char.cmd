; Commands: the button sequences the character recognises, and the states they lead to.
;
; Buttons: a, b, c, x, y, z and s (start).
; Directions: B, DB, D, DF, F, UF, U, UB (back, down-back, down, down-forward, forward...), relative
; to the way the character faces.
;
; A command is a list of buttons and directions separated by commas:
;   command = ~D, DF, F, x
; Modifiers:
;   /    held:      /D       down is held (with the next element: /F, a)
;   ~    released:  ~a       a is released; ~30a: released after being held for 30 ticks
;   $    with its neighbours: $D also accepts DB and DF
;   +    together:  a+b      a and b pressed at the same time
;   >    at once:   a, >b    nothing else may be pressed between a and b
; time        ticks allowed to enter the whole command (command.time when left out)
; buffer.time ticks the command stays active once it is complete (command.buffer.time when
;             left out; 1 to 30)
;
; Example:
; [Command]
; name = "QCF_x"
; command = ~D, DF, F, x
; time = 15

;-| Button remapping |-----------------------------------------------------------
; Which button each button acts as (x = y makes x act as y).
[Remap]
x = x
y = y
z = z
a = a
b = b
c = c
s = s

;-| Defaults |-------------------------------------------------------------------
[Defaults]
command.time = 15        ; Time of the commands that do not give one
command.buffer.time = 1  ; Buffer time of the commands that do not give one

;-| Commands |-------------------------------------------------------------------
; Write the [Command] blocks here, before state -1.

;-| State -1 |-------------------------------------------------------------------
; State -1 runs every tick. Its controllers usually change state when a command is entered,
; for example:
;
; [State -1, Strong punch]
; type = ChangeState
; value = 210
; triggerall = command = "y"
; trigger1 = statetype = S
; trigger1 = ctrl
;
; Controllers that come first are checked first: put the special moves before the plain
; buttons.

[Statedef -1]
