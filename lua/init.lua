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
