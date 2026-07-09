-- Global preset "Classic": the welcome screen toolbar and its theme.

function apply()
    local start = FF.toolBar("start")
    start.iconSize = 32
    start:addButton("file_new")
    start:addButton("file_close")
    start:addButton("file_open")
    start:addButton("file_save")
    start:addButton("file_backup")
    start:addSeparator()
    start:addButton("tools_pal_editor")
    start:addSeparator()
    start:addButton("tools_options")
    start:addButton("file_run")
end

iconset = "Classic"
stylesheet = "Classic"
colorset = "Classic"
