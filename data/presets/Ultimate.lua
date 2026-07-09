-- Global preset "Ultimate": the welcome screen toolbar and its theme.

function apply()
    local start = FF.toolBar("start")
    start.iconSize = 32
    start:addButton("file_new")
    start:addButton("file_close")
    start:addButton("file_open")
    start:addButton("file_save")
    start:addSeparator()
    start:addButton("file_backup")
    start:addButton("tools_options")
    start:addSeparator()
    start:addButton("file_run")
end

iconset = "Ultimate"
stylesheet = "Ultimate"
colorset = "Ultimate"
