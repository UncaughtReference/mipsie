local MenuBar = require("ui.menu_bar")

local MainWindow = {}

function MainWindow.create()
    local window = host.ui.window({
        title = "Mipsie",
        width = 1100,
        height = 750
    })

    local layout = host.ui.vertical_layout()

    local EditorPanel = require("ui.panels.editor_panel")

    local editor = EditorPanel.create()

    layout:add(editor)

    window:set_layout(layout)

    MenuBar.create(window)

    return window
end

return MainWindow
