# mipsie Lua API

mipsie embeds Lua for startup scripts and small automation tasks. This guide
documents the Lua standard libraries and host functions currently exposed by
mipsie. I hope all this effort didn't go to waste :3

## Startup and the Lua Console

The startup script is `lua/init.lua`, relative to the application's working
directory. In a packaged installation, this is normally the mipsie install
directory. Edit that file to add global Lua functions:

```lua
function init()
    mipsie.log.info("mipsie Lua is ready")
end

function greet(name)
    print("Hello, " .. name)
end
```

mipsie executes `init.lua` during startup. If the file defines a global
`init()` function, mipsie calls it once after loading the file. Use the Lua
Console pane to invoke a global function by name, for example `greet("Claire")`.
Entering `greet` without parentheses calls it with no arguments.

Before each console invocation, mipsie executes `init.lua` again, then calls
the requested global function. Keep reusable functions at the file's top level;
top-level initialization code will also run again for every console command.
The console accepts a global function name and simple literal arguments:
numbers, quoted strings, `true`, `false`, and `nil`. It does not accept a Lua
expression such as `mipsie.fs.exists("file")` directly; put that expression in
a function in `init.lua` and call the function from the pane.

The standard Lua `print(...)` function is redirected to the Lua Console. Values
are converted with `tostring` and multiple arguments are joined with tabs. Lua
script errors are also written to the console.

The runtime opens Lua's base, coroutine, debug, io, math, os, package, string,
table, and utf8 libraries. Scripts can use their normal Lua functions in
addition to the mipsie APIs below.

## Logging: `mipsie.log`

These functions append a prefixed message to the Lua Console. Each accepts one
string argument and returns no value:

```lua
mipsie.log.info("Life loaded")    -- [INFO] Life loaded
mipsie.log.warn("No job gotten")  -- [WARN] No job gotten
mipsie.log.error("Life failed")   -- [ERROR] Life failed
```

## Application: `mipsie.app`

| Function | Result |
| --- | --- |
| `mipsie.app.name()` | Application name as a string. |
| `mipsie.app.version()` | Application version as a string. |
| `mipsie.app.architecture()` | CPU architecture reported by Qt, such as `x86_64`. |
| `mipsie.app.qt_version()` | Qt version string used by the running app. |
| `mipsie.app.quit()` | Requests application shutdown; returns no value. |

```lua
print(mipsie.app.name(), mipsie.app.version())
print("Architecture:", mipsie.app.architecture())
print("Qt:", mipsie.app.qt_version())
```

## File System: `mipsie.fs`

Paths passed to `mipsie.fs` are interpreted as UTF-8. Absolute paths are used
as given; relative paths are resolved against `mipsie.fs.base_dir()`. The base
directory is the process working directory in development builds and the
directory containing the mipsie executable in packaged builds.

| Function | Result and behavior |
| --- | --- |
| `mipsie.fs.exists(path)` | `true` if a file or directory exists at the resolved path; otherwise `false`. |
| `mipsie.fs.read_text(path)` | Reads the file as UTF-8 and returns a Lua string. Throws a Lua error if the file cannot be opened. |
| `mipsie.fs.write_text(path, contents)` | Writes the string to the resolved file, replacing existing contents. The parent directory must already exist. Throws a Lua error if opening or writing fails; returns no value on success. |
| `mipsie.fs.list_dir(path)` | Returns a 1-based Lua array of file and directory names, sorted by name. Omits `.` and `..`. Throws if the directory does not exist. |
| `mipsie.fs.app_dir()` | Directory containing the mipsie executable. |
| `mipsie.fs.project_dir()` | In development builds, the process working directory; in packaged builds, the executable directory. This currently does not track a separately opened project. |
| `mipsie.fs.base_dir()` | Directory used to resolve relative paths, as described above. |
| `mipsie.fs.current_dir()` | The process' current working directory. |
| `mipsie.fs.join(first, second)` | Joins two path components using Qt path rules and returns the resulting path. |

```lua
local test_path = mipsie.fs.join(mipsie.fs.app_dir(), "test.txt")
if mipsie.fs.exists(test_path) then
    local contents = mipsie.fs.read_text(test_path)
    print("Test contents:", #contents)
end

for index, name in ipairs(mipsie.fs.list_dir(mipsie.fs.base_dir())) do
    print(index, name)
end
```

## HTTP: `mipsie.http`

Requests are asynchronous. Each request function accepts a callback and returns
no value; the callback receives one response table when the request finishes.
Only valid `http://` and `https://` URLs are accepted. The transfer timeout is
30 seconds.

```lua
mipsie.http.get(url, callback)
mipsie.http.post(url, body, callback)
mipsie.http.post_as(url, body, content_type, callback)
```

`post` sends the body with the `application/json` content type. Use `post_as`
to specify a different content type. The callback receives:

| Field | Meaning |
| --- | --- |
| `status` | HTTP status code, or `0` if no HTTP status was received. |
| `body` | Response body as a Lua string. |
| `error` | Qt network error description, or an empty string when there was no network error. An HTTP error status such as 404 can still have an empty `error`. |
| `ok` | `true` only when there was no network error and the HTTP status is between 200 and 299, inclusive. |

An invalid URL, unavailable network service, or missing callback raises a Lua
error. Errors raised inside an HTTP callback are reported in the Lua Console.

```lua
mipsie.http.get("https://romhacking.com/Bowsers-Dank-Rave-12/data.json", function(response)
    if not response.ok then
        mipsie.log.error(":( Request failed: " .. response.status .. " " .. response.error)
        return
    end

    print("Response:", response.body)
end)
```

## Keyboard Shortcuts: `mipsie.keymap`

`mipsie.keymap.set(shortcut, callback)` registers a Lua callback on a Qt
portable-text shortcut such as `Ctrl+P`. The callback runs when the shortcut is
pressed while the Mipsie main window is active. Calling `set` again with the
same shortcut replaces its previous Lua callback. The function returns no
value; an empty or invalid shortcut or a non-function callback raises a Lua
error. Errors raised by a callback are reported in the Lua Console.

```lua
mipsie.keymap.set("Ctrl+P", function()
    print("Lua shortcut pressed")
end)
```

Shortcuts are active for the current app session and are not saved to Settings.
If a shortcut is already assigned to another enabled action, Qt may treat it as
ambiguous; choose a shortcut that is not already in use.

## User Interface: `mipsie.ui`

The UI API lists and locates widgets and actions in mipsie's main window. Widget
and action handles are Lua tables whose functions are called with dot syntax,
such as `widget.text()`.

### Widget listing and lookup

`mipsie.ui.widgets()` returns a 1-based array containing a description for the
main window and each descendant widget. Each description has:

| Field | Meaning |
| --- | --- |
| `id` | Generated lookup path accepted by `mipsie.ui.find()`. |
| `object_name` | Qt object name, or an empty string if none is set. |
| `class_name` | Qt widget class name. |
| `text` | Current text/title for supported widget types; otherwise an empty string. |
| `visible` | Current visibility state. |
| `enabled` | Current enabled state. |

Text is reported for buttons, labels, line edits, text edits, combo boxes, and
group boxes. Generated IDs are paths containing widget classes and each child
widget's index among its parent's direct child widgets. A child path segment may
also include its object name, for example `QTabWidget[2]{editorTabs}`. Prefer
copying the `id` from `widgets()` instead of building a path yourself.

`mipsie.ui.find(id)` accepts a widget's object name or generated ID. If an
object name is shared by multiple widgets, it returns the first match. It
returns `nil` when there is no match, or a widget handle with `id`,
`object_name`, and `class_name` fields and these functions:

| Function | Behavior |
| --- | --- |
| `text()` | Gets text/title for buttons, labels, line edits, text edits, combo boxes, or group boxes. Unsupported widget types return an empty string. |
| `set_text(text)` | Sets text on buttons, labels, line edits, and text edits, or a title on a group box. Returns a boolean indicating whether the widget exists and supports setting text. It does not change a combo box's selected item. |
| `visible()` / `enabled()` | Gets the current state. Returns `false` if the widget has gone away. |
| `set_visible(value)` / `set_enabled(value)` | Sets the state and returns `true` if the widget still exists, otherwise `false`. |
| `focus()` | Gives the widget focus and returns whether it still exists. |
| `click()` | Calls `click()` for a button and returns `true`; returns `false` if the widget is missing or is not a button. A disabled button may not activate. |
| `current_index()` | Gets the selected index of a tab widget or combo box; returns `-1` for other widget types. Indices are zero-based. |
| `set_current_index(index)` | Selects a zero-based index on a tab widget or combo box. Returns `false` for other widget types or an invalid index. |

```lua
for _, widget in ipairs(mipsie.ui.widgets()) do
    print(widget.id, widget.class_name, widget.object_name, widget.text)
end

local label_list = mipsie.ui.find("LabelListView")
if label_list then
    label_list.focus()
end
```

### Actions

`mipsie.ui.actions()` returns a 1-based array describing the main window's Qt
actions. Each description contains `object_name`, `text`, `shortcut`, `enabled`,
and `visible` fields.

`mipsie.ui.find_action(key)` finds the first action whose object name or
displayed text exactly equals `key`. If more than one action matches, it returns
the first. It returns `nil` if nothing matches. The
returned action handle has `object_name` and `text` fields and these functions:

| Function | Behavior |
| --- | --- |
| `trigger()` | Triggers the action and returns `true` if it exists, is enabled, and is visible; otherwise returns `false`. |
| `enabled()` / `visible()` | Gets the current state; returns `false` if the action has gone away. |
| `set_enabled(value)` / `set_visible(value)` | Changes the state and returns `true` if the action still exists, otherwise `false`. |
| `shortcut()` | Gets the shortcut using Qt portable text, for example `Ctrl+S`. Returns an empty string if the action has gone away. |
| `set_shortcut(shortcut)` | Sets a shortcut from Qt portable text and returns `true` if the action still exists. |

`mipsie.ui.trigger_action(name)` searches by object name or displayed text,
then triggers the first matching action if it is enabled and visible. It returns
`true` when triggered and `false` when there is no match or the action cannot
be triggered. `set_shortcut()` changes the live Qt action; it does not save a
new shortcut in mipsie's Settings file.

```lua
for _, action in ipairs(mipsie.ui.actions()) do
    print(action.object_name, action.text, action.shortcut)
end

local save = mipsie.ui.find_action("saveFileAction")
if save then
    print("Save shortcut:", save.shortcut())
    save.trigger()
end

-- Equivalent to finding and triggering the action by name:
mipsie.ui.trigger_action("saveFileAction")
```

## Editor: `mipsie.editor`

`mipsie.editor.current_line()` returns the text, without its newline, on the
current line in the first Editor pane's selected buffer. It returns `nil` if
there is no editor pane or buffer. In a layout with multiple Editor panes, this
binding does not select the pane with keyboard focus.

```lua
local line = mipsie.editor.current_line()
if line then
    print("Current line:", line)
end
```
