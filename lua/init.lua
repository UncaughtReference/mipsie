function init()
    print("Hello from init.lua!! <3")
    print("\nThe current Lua implementation is very basic, but it will be updated based on people's needs!")
end

function foo()
    print("bar")
end

function weather(latitude, longitude)
    latitude = latitude or 52.2297
    longitude = longitude or 21.0122

    local url = string.format(
        "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,relative_humidity_2m",
        tostring(latitude),
        tostring(longitude)
    )

    mipsie.http.get(url, function(response)
        if not response.ok then
            mipsie.log.error(response.error)
            return
        end

        local current = response.body:match('"current"%s*:%s*{(.-)}')
        if not current then
            mipsie.log.error("Could not read current weather from the response.")
            return
        end

        local temperature = current:match('"temperature_2m"%s*:%s*([%-%d%.]+)')
        local humidity = current:match('"relative_humidity_2m"%s*:%s*([%-%d%.]+)')

        print(string.format("Temperature: %s°C, humidity: %s%%",
                            temperature or "unknown",
                            humidity or "unknown"))
    end)
end

function int_to_float_popup()
    local function int_to_float(inputRegister, outputRegister)
        local text = "mtc1 "..inputRegister..", "..outputRegister.."\ncvt.s.w "..outputRegister..", "..outputRegister.."\n"
        mipsie.editor.insert_text(text)  
    end
    
    local editorId = "";
    for _, widget in ipairs(mipsie.ui.widgets()) do
        if widget.class_name == "Editor" then
            editorId = widget.id
        end
    end

    local editor = mipsie.ui.find(editorId)
    if editor then
        editor.focus()
    end 

    local input = mipsie.ui.create_popup({"Input Register:", "Output Register:"})
    itf(input[1], input[2])
end

function rename_current_file()
    local function rename(current_file, desired_file_name)
        os.execute("cmd /c rename \""..current_file:gsub("/", "\\").."\" \""..desired_file_name.."\"")
        print("Renamed "..current_file.." to "..desired_file_name)
    end

    local input = mipsie.ui.create_popup({"New File Name:"})
    rename(mipsie.editor.current_file_path(), input[1])
end

mipsie.keymap.set("Ctrl+I", int_to_float_popup)
mipsie.keymap.set("Ctrl+Shift+R", rename_current_file)
