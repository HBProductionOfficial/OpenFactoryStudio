-- Global preset "Clean": the welcome screen toolbar and its theme.

function apply()
    local start = FF.toolBar("start")
    start.iconSize = 32
    start:addButton("file_new")
    start:addButton("file_close")
    start:addButton("file_open")
    start:addButton("file_save")
    start:addSeparator()
    start:addButton("file_run")
    start:addSeparator()
    start:addButton("tools_options")
end

iconset = "Nuvola"
stylesheet = "Dark"
colorset = "Dark"
