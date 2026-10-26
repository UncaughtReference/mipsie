local EditorPanel = {}

function EditorPanel.create()
    local editor = host.ui.text_editor({
        placeholder = "Write MIPS assembly here..."
    })

    return editor
end

return EditorPanel