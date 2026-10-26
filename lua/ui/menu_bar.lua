local MenuBar = {}

function MenuBar.create(window)
    local menuBar = window:menu_bar()

    local fileMenu = menuBar:add_menu("File")
    local editMenu = menuBar:add_menu("Edit")
    local runMenu = menuBar:add_menu("Run")
    local helpMenu = menuBar:add_menu("Help")

    local openAction = host.ui.action({
        text = "Open...",
        shortcut = "Ctrl+O"
    })

    openAction:on_triggered(function()
        host.log.info("Open requested")
    end)

    fileMenu:add_action(openAction)

    local exitAction = host.ui.action({
        text = "Exit",
        shortcut = "Alt+F4"
    })

    exitAction:on_triggered(function()
        window:close()
    end)

    fileMenu:add_action(exitAction)

    return menuBar
end

return MenuBar
