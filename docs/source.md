# Source Code Reference

This document describes every file under `src/` and the named functions declared
or implemented there. It follows the source tree. Anonymous Qt callbacks and
lambdas are described under the function that connects or owns them; they do not
have independent C++ names.

## Application entry point

### `src/host/main.cpp`

Starts the Qt application, configures application metadata and font, creates the
main window and Lua runtime, wires Lua output and console requests, runs the
startup script, opens any command-line paths, and enters the Qt event loop.

Functions:

- `hasJetBrainsMonoNerdFont()` checks Qt's registered font families without regard to case.
- `userFontDirectory()` chooses the current user's Windows Fonts directory, falling back to the application's local-data Fonts directory.
- `registerWindowsUserFont(fontPath)` writes the font registration under the current user's registry key and broadcasts `WM_FONTCHANGE`.
- `installJetBrainsMonoNerdFont()` extracts the bundled font and license, registers the font for the current user, and adds it to Qt's application font database.
- `main(argc, argv)` initializes QApplication and command-line parsing; initializes and runs Lua; routes console function requests through `lua/init.lua`; shows Mipsie; opens requested files/projects after startup; shuts down Lua and returns Qt's exit code.
- The Lua-console request callback in `main()` reloads `lua/init.lua`, verifies the requested global is a function, then executes the invocation. The zero-delay startup callback opens positional paths and restores focus to the editor.

### `src/iconBS.rc`

Windows resource-script entry that embeds `resources/logo.ico` as `IDI_ICON1` in
the executable.

## Lua bindings

### `src/host/bindings/bindingregistry.h` and `bindingregistry.cpp`

`BindingRegistry::registerAll(lua, mainWindow, networkManager, output)` creates
the global `mipsie` table, then installs logging, application, filesystem, UI,
HTTP, and keymap APIs. The supplied output callback is how bindings send log,
HTTP callback, and Lua keymap callback errors to the Lua console.

### `src/host/bindings/bindapplication.h` and `bindapplication.cpp`

`bindApplication(lua)` installs `mipsie.app`:

- `name()` returns Qt's application name.
- `version()` returns the application version.
- `architecture()` returns the host CPU architecture reported by Qt.
- `qt_version()` returns the linked Qt version.
- `quit()` requests application shutdown.

The four query closures convert Qt strings to UTF-8 `std::string`s for Lua.

### `src/host/bindings/bindlogging.h` and `bindlogging.cpp`

`bindLogging(lua, output)` installs `mipsie.log.info`, `.warn`, and `.error`.
Each accepts a message string and invokes the output callback with an `[INFO]`,
`[WARN]`, or `[ERROR]` prefix. The three Lua closures have no return value.

### `src/host/bindings/pathresolver.h`

Inline `PathResolver` helpers used by filesystem bindings:

- `baseDirectory()` returns the process working directory in a development build and the executable directory in a release build.
- `resolve(path)` resolves relative paths against `baseDirectory()`; absolute paths remain absolute.
- `applicationDirectory()` returns the executable directory.
- `workingDirectory()` returns the process's current working directory.
- `projectDirectory()` currently returns the development working directory or release executable directory; it does not track an independently opened project.

### `src/host/bindings/bindfilesystem.h` and `bindfilesystem.cpp`

`bindFilesystem(lua, mainWindow)` installs `mipsie.fs`:

- `toQString(value)` decodes a UTF-8 Lua string into a QString.
- `resolvePath(path)` applies the shared `PathResolver` policy.
- `toStdString(value)` encodes a QString as UTF-8.
- The `exists(path)` closure checks a resolved file or directory path.
- The `read_text(path)` closure reads a resolved file as UTF-8 and throws a descriptive exception if it cannot be opened.
- The `write_text(path, contents)` closure writes bytes to a resolved file, replacing its contents; its parent directory must already exist. Open/write failures are raised as Lua errors.
- The `list_dir(path)` closure returns a 1-based Lua array of names for entries in a directory, excluding `.` and `..`, sorted by name. A missing directory raises an error.
- `app_dir()`, `project_dir()`, `base_dir()`, and `current_dir()` closures expose the matching `PathResolver` directories.
- `rom_path()` returns MainWindow's configured project ROM, falling back to the standalone ROM, or an empty string when none is configured.
- `current_project_dir()` returns MainWindow's open project root or an empty string when there is no open project.
- `join(first, second)` joins the two components with Qt's `QDir::filePath` rules.

### `src/host/bindings/bindhttp.h` and `bindhttp.cpp`

`bindHttp(lua, networkManager, output)` installs asynchronous `mipsie.http`:

- `toQString(value)` and `toStdString(value)` convert URL, content, and error strings to/from UTF-8; a QByteArray overload preserves response bytes.
- `makeRequest(urlText)` validates an HTTP or HTTPS URL, sets a 30-second transfer timeout, and adds the `mipsie/<version>` User-Agent header. Invalid URLs raise an exception.
- `attachResponseCallback(lua, manager, reply, callback, output)` connects a reply's finished signal, builds the Lua response fields `status`, `body`, `error`, and `ok`, invokes the protected Lua callback, reports callback errors through `output`, and schedules reply cleanup.
- `get(url, callback)` starts a GET request.
- `post(url, body, callback)` starts a POST request with `application/json` content type.
- `post_as(url, body, content_type, callback)` starts a POST using the supplied content type.
- The three request closures require a live network manager and valid callback; requests are asynchronous and return no value.

### `src/host/bindings/bindui.h` and `bindui.cpp`

`bindUI(lua, mainWindow)` installs `mipsie.ui` and `mipsie.editor`.

Internal helpers:

- `toStdString(value)` and `toQString(value)` convert between QString and UTF-8 strings.
- `directWidgets(parent)` lists only immediate QWidget children.
- `widgetId(widget, root)` builds a slash-separated path from the root class/name and child class, sibling index, and optional object name.
- `collectWidgets(parent, widgets)` recursively appends descendant widgets.
- `findWidget(root, id)` finds the first widget whose object name or generated path matches `id`.
- `widgetText(widget)` reads text from buttons, labels, line edits, text edits, combo boxes, and group-box titles; unsupported widgets return an empty string.
- `setWidgetText(widget, text)` sets text on buttons, labels, line edits, text edits, or a group-box title. It does not set a combo-box selection.
- `widgetDescription(lua, widget, root)` creates the table returned by `widgets()` with `id`, `object_name`, `class_name`, `text`, `visible`, and `enabled`.
- `actionDescription(lua, action)` creates an action table with `object_name`, `text`, `shortcut`, `enabled`, and `visible`.

Lua functions registered by `bindUI`:

- `mipsie.ui.widgets()` returns descriptions for the main window and recursive child widgets.
- `mipsie.ui.find(id)` returns `nil` or a widget handle. The handle's `text`, `set_text`, `visible`, `set_visible`, `enabled`, `set_enabled`, `focus`, `click`, `current_index`, and `set_current_index` closures re-find the target each time they run. `focus()` focuses an Editor's active QTextEdit buffer instead of its non-focusable container.
- `mipsie.ui.actions()` returns descriptions for the main window's QAction children.
- `mipsie.ui.find_action(key)` returns `nil` or the first action matching object name or exact displayed text. The handle exposes `trigger`, `enabled`, `set_enabled`, `visible`, `set_visible`, `shortcut`, and `set_shortcut` closures.
- `mipsie.ui.trigger_action(name)` finds and triggers the first matching enabled, visible action; it returns a boolean.
- `mipsie.editor.current_line()` returns the current line in the first Editor widget's selected buffer, or `nil` if none is available. It does not choose an Editor pane by keyboard focus.

### `src/host/bindings/bindkeymap.h` and `bindkeymap.cpp`

- `bindKeymap(lua, mainWindow, output)` installs the `mipsie.keymap` table.
- `mipsie.keymap.set(shortcut, callback)` validates a Qt portable-text shortcut, creates a main-window shortcut action, and invokes the Lua callback through a protected call when pressed. Reusing a shortcut replaces its old Lua action. Callback errors are sent to `output`.

### `src/host/bindings/bindingregistry.h`, `bindapplication.h`, `bindfilesystem.h`, `bindhttp.h`, `bindlogging.h`, `bindui.h`, and `bindkeymap.h`

These headers declare the registration entry points and their dependencies. The
binding APIs and behavior are described with their implementations above.

## Lua runtime

### `src/host/runtime/luaruntime.h` and `luaruntime.cpp`

`LuaRuntime` owns the Lua state, a parentless network manager used by HTTP
bindings, and runtime lifecycle state. It must be initialized and used on its
owner thread.

- `LuaRuntime(parent)` records the creating thread and constructs private runtime state.
- `~LuaRuntime()` calls `shutdown()`.
- `initialize(mainWindow)` opens the base/coroutine/debug/io/math/os/package/string/table/utf8 libraries, registers host bindings, redirects Lua `print(...)` to `scriptOutput`, creates the network manager, emits `initialized`, and returns whether setup succeeded. Initialization errors emit `scriptError`.
- `executeFile(path)` safely executes a Lua source file; it returns `false` if the runtime is unavailable or the script fails, emitting `scriptError` with the path and error text.
- `executeString(source, chunkName)` safely executes a Lua chunk with the same error reporting. The default chunk name is `=runtime`.
- `isInitialized()` reports whether setup completed and shutdown has not reset it.
- `shutdown()` emits `shuttingDown`, disconnects and aborts outstanding HTTP replies, explicitly deletes the network manager, deletes Lua keymap actions while Lua callbacks are still valid, disconnects Qt signal connections to and from the runtime, and marks it uninitialized. Repeated shutdown calls are ignored.
- Signals: `scriptError(source, message)`, `scriptOutput(line)`, `initialized()`, and `shuttingDown()` report script results and lifecycle events.

### `src/host/runtime/luaerror.h` and `luaerror.cpp`

`LuaError` is currently an empty placeholder class. `LuaError()` has an empty
implementation and carries no error data; runtime errors are presently reported
through `LuaRuntime` signals instead.

### `src/host/runtime/luaconfig.h`

Empty guarded header reserved for Lua configuration. It currently declares no
types, constants, or functions.

## Main window and menus

### `src/host/ui/mainwindow/mainwindow.h` and `mainwindow.cpp`

`MainWindow` owns the workspace splitter, settings, active editor, ROM/project
paths, Lua output history, and update-check state. Its constructor builds the
initial Editor, Assembler, Snippets, and reference panes, registers pane and
focus callbacks, loads settings/macros, sets up updates, and connects the file
tree. Lambdas installed by the constructor update active editor/edit target,
open files on activation, and refresh UI state after focus changes.

Anonymous-namespace helpers:

- `recentProjectsFilePath()` returns the app-data path for `recent_projects.json`.
- `generalSettingsFilePath()` returns the neighboring `general_settings.json` path.
- `macrosFilePath()` returns the neighboring `macros.json` path.
- `defaultMacros()` constructs the built-in F1/F2/F3 code macros.
- `loadMacros()` reads and validates the macros JSON array or falls back to the built-ins.
- `defaultGeneralSettings()` builds appearance, assembly-mode, armips-option, and keybind defaults; temporary ASM generation and `-root` default to `true`, and extra armips arguments default to an empty string.
- `loadGeneralSettings()` overlays saved settings and migrates legacy tab-shortcut spellings.
- `writeJsonObjectAtomically(path, object, errorMessage)` writes a JSON object with QSaveFile; it reports an error and returns `false` on failure.
- `writeJsonArrayAtomically(path, array, errorMessage)` does the same for arrays.
- `projectRomPathFromConfig(config)` prefers `romFilePathLinux` on Linux and falls back to `romFilePath` for older project files.
- `configFilePathForRom(romFilePath)` derives the `.config` path beside a ROM.
- `backupRomFileName(stamp, romSuffix)` forms a timestamped backup name and preserves the ROM suffix.
- `uniqueBackupStem(directoryPath, romSuffix)` finds a timestamped name not already used in the backup directory.
- `copyFileAtomically(sourcePath, destinationPath, errorMessage)` copies through QSaveFile and reports failures.

`MainWindow` functions:

- `MainWindow(parent)` initializes settings, UI, panes, keyboard shortcuts, Lua output routing, and update checks.
- `~MainWindow()` marks shutdown, stops an active update request, and releases window-owned resources.
- `hasEditTarget()` reports whether the last focused text or line editor is available for Edit menu commands.
- `eventFilter(watched, event)` handles Ctrl+1 through Ctrl+9 pane-focus shortcuts before forwarding other events to QMainWindow.
- `focusEditor()` focuses the active editor's current text buffer.
- `currentRomFilePath()` returns the configured project ROM path, falling back to the standalone ROM path.
- `currentProjectDirectory()` returns the open project's base directory, or an empty string when no project is open.
- `performEditAction(action)` dispatches Undo, Redo, Cut, Copy, Paste, or Select All to the last focused QTextEdit/QLineEdit.
- `toggleCommentOutSelection()` delegates comment toggling to the active Editor only when a text editor is the current edit target.
- `applyGeneralAppearance()` applies the app font, theme palette, accent color, and pane repaint.
- `applyEditorPreferences()` sends font size, tab width, theme, and font family to all Editor panes.
- `applyAssemblerPreferences()` sends the saved temporary-ASM setting to every Assembler pane.
- `applyKeyboardShortcuts()` applies saved key sequences to matching QAction object names and refreshes Edit-action state.
- `applyMacros()` refreshes macro shortcuts in all Editor panes.
- `updateEditActionStates()` enables Edit actions when an edit target exists.
- `findActiveEditor()` returns the active in-window Editor, with a first-Editor fallback.
- `configureAssemblerRom(assembler)` resolves and assigns the selected project/standalone ROM, prompting or reporting failure as needed.
- `registerEditor(editor)` connects Editor instruction changes and manages the active editor reference.
- `updateSnippetViews(instruction)` updates all Snippets panes for the displayed mnemonic.
- `createPaneContent(pane, kind)` constructs the requested pane widget, wires its signals, and installs it in the Pane.
- `appendLuaOutput(text)` adds Lua output to history and all open Lua Console panes.
- `appendLuaError(source, message)` formats a script error and forwards it to Lua output.
- `openFileDialog()` prompts for an assembly file and opens it in the active Editor.
- `openRomFile()` prompts for and selects a standalone ROM, updating Assembler panes.
- `openProjectFolder()` prompts for a project directory and opens it.
- `openCommandLinePath(path)` opens a command-line path as a project folder or file.
- `openProject()` presents the recent-project selection UI and opens the selected folder.
- `recentProjectPaths()` reads and validates the recent-project list.
- `rememberRecentProject(folderPath)` updates and persists the recent-project list.
- `openProjectFolderPath(selectedFolder)` validates a folder, loads or creates project ROM settings, updates panes and working directories, and remembers the project.
- `openSettings()` opens Settings, persists general/macros/project settings, then reapplies preferences and ROM configuration.
- `assembleAndUpdateChecksum()` validates assembler/ROM configuration and starts assembly/checksum work.
- `assembleUpdateChecksumAndOpenProject64()` starts the same operation with Project64 focus requested on success.
- `checkForUpdates()` starts a manual update check.
- `showAboutDialog()` displays the application version and credits.
- `openAppDataFolder()` creates and opens the application settings directory.
- `startUpdateCheck(manual)` rate-limits automatic requests, performs the remote version check, and reports updates/errors.
- `backupRom()` lets the user choose between configured standalone and project ROMs (or skips the chooser when only one is configured), then creates a timestamped backup of the selected ROM and its matching `.config` file.
- `restoreRomVersion()` presents available backups and restores the selected ROM/config with rollback handling.
- `saveCurrentFile()` saves the active editor document.
- `saveCurrentFileAs()` opens Save As for the active editor.
- `luaFunctionRequested(functionName, arguments)` signals that the Lua Console requested a global function call; `main.cpp` handles it by reloading `lua/init.lua` and invoking the function.
- `closeEvent(event)` prompts for modified files, saves or cancels close, and shuts down app-owned work safely.

The header also declares `showMainWindow(width, height, maximized)`, but no
definition for it is currently present in `mainwindow.cpp`.

### `src/host/ui/mainwindow/mainwindow.ui`

Qt Designer skeleton for `Ui::MainWindow`: a QMainWindow with a central widget,
menu bar, and status bar. Runtime menus and workspace panes are assembled in
C++ rather than defined in this form.

### `src/host/ui/mainwindow/menubar.h` and `menubar.cpp`

- `MenuBar(ui, window)` stores pointers to the generated UI and owning MainWindow.
- `openMarkdownWindow(parent, title, resourcePath)` reads an embedded Markdown document and opens it in its own delete-on-close `QDialog` containing a scrollable `QTextBrowser`; external links open in the system browser. If the document cannot be read, it shows a warning.
- `initialize()` creates File, Edit, Assemble, Settings, and Help actions, assigns object names and shortcuts, and connects them to MainWindow slots or edit actions. Its local `addEditAction` helper creates a QAction, registers it with the Edit menu, and routes it through `performEditAction`.
- The local `addDocumentationAction(text, resource, objectName)` helper in `initialize()` adds a Help menu item and connects it to `openMarkdownWindow`. It is used for LLM Use, Lua API, and Source Code.
- The `docs/docs.qrc` resource collection embeds `docs/llmUse.md`, `docs/lua.md`, and `docs/source.md` at `:/docs/llmUse.md`, `:/docs/lua.md`, and `:/docs/source.md` so the viewer works in packaged builds.

### `src/host/ui/mainwindow/settingsdialog.h` and `settingsdialog.cpp`

`SettingsDialog(projectFolder, romFilePath, projectSettings, generalSettings,
macros, parent)` builds General, Macros, and Current Project tabs. Its key-sequence
editors correspond to the keybind JSON names; macro rows contain a shortcut and
multiline text; project settings edit `romFilePath` on Windows and
`romFilePathLinux` on Linux while preserving the other platform's value. The General tab includes
checkboxes for temporary ASM generation and the armips `-root` option, plus a
line edit for additional armips arguments.

- `generalSettings()` overlays selected fonts, sizes, tab width, theme, accent, temporary-ASM and `-root` choices, additional armips arguments, and edited keybinds on the original object.
- `projectSettings()` returns original settings with a normalized absolute ROM path when the project page is active.
- `macros()` serializes current macro rows to a JSON array.
- `addMacroEditor(shortcut, text)` adds one editable macro row and its Remove callback.
- `chooseAccentColor()` opens QColorDialog and updates the selected accent color.
- `browseRomFile()` opens the ROM file picker and fills the ROM path field.
- `updateAccentColorButton()` refreshes the button's color preview and label.
- `acceptSettings()` validates duplicate macro shortcuts and required ROM path, then accepts or shows a warning.
- Constructor callbacks add/remove macros, scroll the newest macro into view, and connect Save/Cancel buttons.

### `src/host/ui/mainwindow/splitter.h` and `splitter.cpp`

`Splitter` is a QSplitter specialized for nested Pane management.

- `Splitter(orientation, parent)` creates a nested-capable splitter.
- `addPane(pane)` inserts and registers a Pane.
- `setInitialSizes(sizes)` stores sizes and applies immediately if visible.
- `showEvent(event)` applies initial sizes once after first show.
- `registerPane(pane)` registers shortcuts and routes split/content/delete requests. Its delete callback removes a pane and collapses one-child nested splitters.
- `rootSplitter()` finds the outermost Splitter ancestor.
- `addPaneShortcut(pane)` adds a pane to the root shortcut order.
- `removePaneShortcut(pane)` removes it and refreshes assignments.
- `refreshPaneShortcuts()` assigns Ctrl+1..Ctrl+9 to the first nine registered panes.
- `splitPane(pane, orientation)` replaces the pane with a nested splitter containing the original pane and a new empty pane.
- `contentRequested(pane, kind)` forwards requests for MainWindow to create/change pane content.

## Shared pane framework

### `src/host/ui/mainwindow/panes/pane.h` and `pane.cpp`

`PaneKind` enumerates Empty, Editor, FileTree, FunctionView, RamAddressView,
StructView, MarioActionsView, EepromView, Assembler, Snippets, LuaConsole, and
CmdConsole.

- `Pane(parent)` builds the Split/Pane menus, placeholder widget, focus border tracking, and pane-focus action. Menu callbacks emit split, content-change, and delete signals; its focus callback directs focus to a pane-specific input, tree, editor, text editor, or first focusable child.
- `setPaneShortcut(shortcut)` updates the displayed shortcut and installs the action on the containing window.
- `setContent(widget, kind)` deletes old content, reparents/inserts the new widget, updates pane kind, and refreshes checkmarks.
- `updatePaneChoiceChecks()` marks the current content type in the Pane menu.
- `kind()` returns the current PaneKind.
- `paintEvent(event)` paints the pane border using the accent color when focused.
- Signals: `splitRequested`, `contentRequested`, and `deleteRequested` are requests handled by Splitter/MainWindow.

### `src/host/ui/mainwindow/panes/searchablelisttable.h` and `searchablelisttable.cpp`

`SearchableTableColumn` stores a JSON key, column title, and width stretch.
`SearchableListTable(resourcePath, columns, copyFormatter, parent)` loads a JSON
`entries` array, builds a filterable custom multi-column list, and copies a row's
formatted value on double-click.

Internal functions/classes:

- `SearchFilterModel::setSearchText(text)` trims the query and invalidates the proxy filter.
- `SearchFilterModel::filterAcceptsRow(row, parent)` shows a row when any configured column contains the case-insensitive query.
- `MultiColumnDelegate(columnWidths, columnCount, parent)` associates a list row with resizable column headers.
- `MultiColumnDelegate::paint(...)` paints selection and elided cells using current header widths.
- `MultiColumnDelegate::sizeHint(...)` enforces a minimum 24-pixel row height.
- The search-text callback updates the filter; the double-click callback collects row fields and invokes `copyFormatter` before copying nonempty text to the clipboard.

## Pane implementations

### `src/host/ui/mainwindow/panes/assembler/assembler.h` and `assembler.cpp`

`Assembler` runs the external armips assembler using QProcess, updates the ROM
checksum in-process, displays output, maps errors to source lines, and optionally
focuses Project64 after success.

- `Assembler(parent)` builds the output view, ROM label, process, and signal connections.
- `setRomFilePath(filePath)` updates the configured ROM path and label.
- `setCreateTemporaryAsmFile(enabled)` selects temporary wrapped-source assembly or direct assembly from the saved source file.
- `setUseArmipsRootOption(enabled)` controls whether the source directory is passed using armips's `-root` option.
- `setAdditionalArmipsArguments(arguments)` stores extra command-line arguments for armips; `startArmips()` tokenizes them with `QProcess::splitCommand` and appends them to the process arguments.
- `isBusy()` reports whether an external process stage is active.
- `assemble(openProject64OnSuccess)` captures the current source buffer/path, validates state, saves modified source before direct assembly, and starts the assembly stage.
- `startArmips(assemblyText, rootDirectory)` either writes the wrapped temporary source or uses the original ASM path, then launches armips with the source, optional `-root` path, and configured extra arguments. Linux builds find `tools/armips_linux/armips` by searching upward from the app and current directories; Windows builds use `tools/armips.exe` from the current directory.
- `updateChecksum()` calculates the N64 checksum over bytes `0x1000` through `0x100FFF` and writes the two checksum words at offset `0x10`, on any platform.
- `appendProcessOutput(standardError)` reads available process bytes into the appropriate output buffer.
- `flushProcessOutput()` converts complete buffered lines into output rows.
- `appendOutputLine(line, isError)` adds a formatted line and source-navigation metadata to the model.
- `sourceLineForOutput(line)` parses assembler diagnostics and maps temporary/absolute source locations to editor lines.
- `navigateToOutputLine(index)` opens/focuses the source location associated with a clicked output row.
- `finishProcess(exitCode, crashed)` flushes armips output, updates the ROM checksum after successful assembly, reports completion/failure, and performs optional Project64 focus.
- `handleFailedToStart()` reports process-launch failure and resets state.
- `focusProject64()` locates and activates the Project64 window on Windows.
- `removeTemporaryAsm()` deletes the generated temporary source file.

### `src/host/ui/mainwindow/panes/cmdconsole/cmdconsole.h` and `cmdconsole.cpp`

`CmdConsole` embeds a Windows `cmd.exe` subprocess, merged output view, command
input, Clear action, and Restart action.

- `CmdConsole(workingDirectory, parent)` builds controls, connects process/input callbacks, and starts the shell.
- `startShell()` resolves `cmd.exe`, applies the working directory, and launches `/Q /D`.
- `setWorkingDirectory(directory)` normalizes the directory and sends `cd /d` to a running shell.
- `restartShell()` kills any current shell and starts a fresh one.
- `runCommand()` echoes and writes the nonempty command to the shell stdin.
- `appendOutput(text)` appends a new output block and scrolls to it.
- `readOutput()` reads local-code-page output from QProcess and appends it.
- Process callbacks enable/disable input and report start, error, and exit status.

### `src/host/ui/mainwindow/panes/editor/editor.h` and `editor.cpp`

`Editor` manages editor tabs, line numbers, label navigation, MIPS highlighting
and completion, find/replace, macros, save state, and editing shortcuts.

Anonymous helper classes/functions:

- `CodeTextEdit::insertFromMimeData(source)` normalizes CRLF/CR to LF before pasting plain text, preserving line breaks.
- `LineNumberArea(editor, parent)` installs viewport/document/scroll/cursor callbacks and sizes its gutter.
- `LineNumberArea::setTheme(theme)` chooses light or dark gutter colors.
- `LineNumberArea::setEditorFont(font)` applies font and recomputes width.
- `LineNumberArea::updateWidth()` sizes the gutter for the current line-number digit count.
- `LineNumberArea::eventFilter(watched, event)` repaints after viewport resize/paint.
- `LineNumberArea::paintEvent(event)` paints visible line numbers and emphasizes the current line.
- `textEditorForTab(tab)` returns the tab's QTextEdit directly or finds its child editor.

`Editor` functions:

- `Editor(parent)` creates the tab widget, label/search widgets, splitter, and find/replace controls.
- `initialize()` creates default tab/actions, builds the Labels panel and find/replace UI, connects shortcuts, and emits initial instruction state.
- `setEditorPreferences(fontSize, tabWidth, theme, fontFamily)` stores preferences, rebuilds highlighters, and applies editor/gutter styling to every buffer.
- `setMacros(macros)` removes old QShortcuts and creates shortcuts that insert each nonempty macro into the focused editor.
- `openFile(filePath)` selects an already-open file tab or reads the file into a new tab; large files require confirmation.
- `newTab(name)` creates a blank named buffer.
- `closeTab(index)` prompts to save modified text, cancels on user choice, and removes the tab when safe.
- `saveCurrentFile()` saves the active path or delegates to Save As.
- `saveCurrentFileAs()` chooses a destination and writes the active buffer.
- `hasUnsavedFiles()` checks all tab documents for modified state.
- `saveAllModifiedFiles()` saves each modified buffer and stops on failure/cancel.
- `currentEditor()` returns the active QTextEdit.
- `currentFilePath()` returns the active tab path.
- `currentFileTitle()` returns the active tab's displayed title.
- `saveCurrentFileToPath(filePath)` writes UTF-8 through QSaveFile, updates tab path/title, and clears modified state.
- `goToLine(line, buffer)` selects a source buffer if provided, positions the cursor, focuses it, and scrolls the line toward viewport center.
- `toggleCommentOut()` toggles a leading semicolon on each selected line as one undo edit and preserves a line-based selection.
- `currentInstruction()` strips comments and label prefixes from the cursor line, then returns a recognized MIPS mnemonic or empty string.
- `emitCurrentInstruction()` emits `currentInstructionChanged` only when the instruction changes.
- `showFindReplace(showReplaceField)` opens the popup and seeds Find from selected text.
- `positionFindReplacePopup()` places the popup at the editor's upper-right corner.
- `findNext(backwards)` searches forward/backward and wraps at document boundaries.
- `replaceCurrentMatch()` replaces the selected/current matching text and advances to the next match.
- `replaceAllMatches()` replaces all matches in one undo group.
- `refreshLabels()` rebuilds the Labels panel from colon-terminated label lines in the active buffer.
- `goToLabel(index)` navigates to the selected label and centers its line vertically.
- `resizeEvent(event)` repositions the visible find/replace popup.
- `eventFilter(watched, event)` routes Down from label search into the result list.
- `createEditorTab(name, filePath, initialContents)` creates buffer/tab, line-number gutter, highlighter, completer, modified-state dot, and update callbacks.
- `applyTextEditorPreferences(textEditor)` applies font, tab stops, text colors, background, and gutter preferences.
- `currentInstructionChanged(instruction)` signals a change in the active mnemonic to MainWindow/Snippets.

### `src/host/ui/mainwindow/panes/editor/mipshighlighter.h` and `mipshighlighter.cpp`

`MipsCompletionKind` classifies completion entries as directive, instruction,
register, label, or constant. `MipsCompletionItem` holds display text and kind.

- `makeFormat(color, bold, italic)` creates a QTextCharFormat.
- `wordPattern(words)` creates exact-token matching for mnemonics, including dotted instruction names.
- `directiveWordPattern(words)` matches directives with optional leading dots such as `.orga`.
- `directivePattern()`, `instructionPattern()`, and `registerPattern()` each return the static regex alternation for their respective token categories.
- `MipsHighlighter(document)` attaches to a QTextDocument and initializes theme formatting.
- `setTheme(theme)` rebuilds format rules for directives, instructions, registers, numbers, strings, labels, symbols, and comments.
- `completionItems()` converts directive/instruction regex entries and register aliases into categorized completions.
- `completionKeywords()` returns just the completion strings.
- `highlightBlock(text)` applies syntax rules and carries multi-line block-comment state.

### `src/host/ui/mainwindow/panes/editor/mipscompleter.h` and `mipscompleter.cpp`

`MipsCompleter` drives QCompleter for each editor and discovers labels/constants
from current source.

- `categoryLetter(kind)` returns the one-letter popup category marker.
- `categoryColor(kind)` returns the category color.
- `CompletionDelegate(view)` customizes popup rendering.
- `CompletionDelegate::paint(...)` paints the category marker, selection, and elided completion text.
- `CompletionDelegate::sizeHint(...)` adds space for the category marker.
- `MipsCompleter(editor)` configures the popup/model, installs event filters, and connects document, cursor, focus, and activation callbacks.
- `eventFilter(watched, event)` controls popup visibility/selection and forwards completion keyboard events to the editor.
- `currentPrefix(startPosition)` extracts the token prefix around the cursor and optionally reports its document start.
- `dynamicCompletionCandidates()` scans source for `equ` constants and colon labels not already in static completion items.
- `rebuildModel()` repopulates the model from static highlighter completions.
- `refreshDynamicCandidates()` updates dynamic rows after relevant source changes.
- `updatePopup(force)` refreshes and displays matching candidates when appropriate.
- `insertCompletion(completion)` replaces the current prefix with the chosen completion.

### `src/host/ui/mainwindow/panes/filetree/filetree.h` and `filetree.cpp`

`FileTreeProxyModel` keeps the configured project root and its ancestors visible
while recursive text filtering runs.

- `FileTreeProxyModel(fileSystemModel, parent)` enables recursive, case-insensitive filtering.
- `setSearchRootPath(path)` normalizes the root and invalidates the filter.
- `filterAcceptsRow(sourceRow, sourceParent)` retains root/ancestor rows and otherwise applies Qt's recursive filter.
- `isSearchRootOrAncestor(index)` tests the candidate path against the configured root.
- `FileTree(parent)` creates the search field, tree, filesystem model, and proxy model.
- `initialize(rootPath)` sets up layout, filtering, activation/click signals, and the root.
- `focusSearch()` focuses and selects the search text when the field is visible.
- `setRootPath(rootPath)` shows/configures the tree for a project or clears/hides it for an empty path.
- `onItemClicked(index)` emits `fileSelected` for a file.
- `onItemActivated(index)` emits `fileDoubleClicked` for a file.
- `eventFilter(watched, event)` toggles selected folders on Enter; Down in search moves selection to the first result.
- Signals: `fileSelected(path)` and `fileDoubleClicked(path)` carry absolute file paths.

### `src/host/ui/mainwindow/panes/functionview/functionview.h`, `functionview.cpp`, and `functionview.qrc`

`FunctionView` reads `:/functionview/functions.json`, displays RAM address,
return type, function name, and arguments, filters across all those fields, and
copies `jal 0x<address>` on double-click.

- `FunctionFilterModel::setSearchText(text)` updates the trimmed search query.
- `FunctionFilterModel::filterAcceptsRow(sourceRow, sourceParent)` searches address, display name, return type, arguments, and prototype.
- `FunctionDelegate(columnWidths, parent)` stores the header splitter.
- `FunctionDelegate::paint(...)` paints four proportional, elided columns with normal/selected text colors.
- `FunctionDelegate::sizeHint(...)` enforces a 24-pixel minimum height.
- `FunctionView(parent)` loads JSON entries, constructs models/header/delegate, and connects search, resize, and double-click callbacks.
- The `functionview.qrc` resource file exposes `functions.json` at `:/functionview/functions.json`.

### `src/host/ui/mainwindow/panes/ramaddressview/ramaddressview.h`, `ramaddressview.cpp`, `ramaddressview.qrc`, and `ram_addresses.json`

`RamAddressView` loads the RAM map and displays address, length, type,
description, and notes. Search checks every field; double-click copies the
address.

- `AddressFilterModel::setSearchText(text)` updates the trimmed query.
- `AddressFilterModel::filterAcceptsRow(sourceRow, sourceParent)` searches address, length, type, description, and notes.
- `AddressDelegate(columnWidths, parent)` stores the header splitter.
- `AddressDelegate::paint(...)` paints the five proportional, elided columns.
- `AddressDelegate::sizeHint(...)` enforces a 24-pixel minimum row height.
- `RamAddressView(parent)` loads resource entries and connects search, column resize, and address-copy callbacks.
- `ramaddressview.qrc` exposes the JSON at `:/ramaddressview/ram_addresses.json`.
- `ram_addresses.json` is the data source with source/archive metadata and an `entries` array containing address, length, type, description, and notes.

### `src/host/ui/mainwindow/panes/searchablelisttable.h` and `searchablelisttable.cpp`

This shared implementation is used by Struct, Mario Actions, and EEPROM panes.
See the shared-pane section for the constructor and internal filter/delegate
functions.

### `src/host/ui/mainwindow/panes/structview/structview.h`, `structview.cpp`, `structs.json`, and `structview.qrc`

- `StructView(parent)` configures SearchableListTable with Name, Offset, Type, and Description columns and copies the selected row's offset on double-click.
- `structs.json` contains source/archive metadata and an `entries` array of structure names, offsets, field types, and descriptions.
- `structview.qrc` exposes the data as `:/structview/structs.json`.

### `src/host/ui/mainwindow/panes/marioactionsview/marioactionsview.h`, `marioactionsview.cpp`, `actions.json`, and `marioactionsview.qrc`

- `MarioActionsView(parent)` configures SearchableListTable with Section, Action, Group, Handler Function, and Description columns; double-click copies the action value.
- `actions.json` contains source/archive metadata and action section/name/group/handler/description entries.
- `marioactionsview.qrc` exposes the data as `:/marioactionsview/actions.json`.

### `src/host/ui/mainwindow/panes/eepromview/eepromview.h`, `eepromview.cpp`, `eeprom.json`, and `eepromview.qrc`

- `EepromView(parent)` configures SearchableListTable with Table, Range, and Description columns; double-click copies the range.
- `eeprom.json` contains source/archive metadata and EEPROM section/range/description entries.
- `eepromview.qrc` exposes the data as `:/eepromview/eeprom.json`.

### `src/host/ui/mainwindow/panes/snippets/snippets.h`, `snippets.cpp`, `instructions.json`, and `snippets.qrc`

- `Snippets(parent)` loads the instruction JSON into a mnemonic-indexed map and prepares its QTextBrowser.
- `showInstruction(instruction)` displays the selected instruction's name, summary, forms, operands, and source link; it avoids redundant redraws and handles an empty/missing mnemonic.
- `instructions.json` stores source/highlighter metadata and an `entries` array. Each instruction entry has name, long name, summary, example forms, operand descriptions, and documentation URL.
- `snippets.qrc` exposes the JSON as `:/snippets/instructions.json`.

### `src/host/ui/mainwindow/panes/luaconsole/luaconsole.h` and `luaconsole.cpp`

- `parseFunctionCall(input, functionName, arguments)` accepts a global identifier with optional parentheses and simple quoted-string, number, boolean, or nil arguments; it rejects expressions and malformed separators.
- `LuaConsole(parent)` creates a read-only output view and function input. Return validates input, echoes the call, clears the field, and emits `functionRequested`.
- `appendOutput(text)` appends a line and scrolls it into view.
- Signal `functionRequested(functionName, arguments)` is handled by `main.cpp`, which reloads `lua/init.lua` and invokes the global.

## Remaining source resources

### `src/host/ui/mainwindow/panes/eepromview/eepromview.qrc`, `functionview.qrc`, `marioactionsview.qrc`, `ramaddressview.qrc`, `snippets.qrc`, and `structview.qrc`

These Qt resource collection files embed their adjacent JSON datasets into the
application under the resource paths documented above. They contain no C++
functions.

### JSON data schemas

- `functionview/functions.json`: top-level `source` metadata and `entries`; each function record provides `name`, `address`, `returnType`, `arguments`, and `prototype`.
- `marioactionsview/actions.json`: `source`, `archive`, and `entries` with `section`, `action`, `group`, `handler`, and `description`.
- `ramaddressview/ram_addresses.json`: `source`, `archive`, and `entries` with `address`, `length`, `type`, `description`, and `notes`.
- `structview/structs.json`: `source`, `archive`, and `entries` with `name`, `offset`, `type`, and `description`.
- `eepromview/eeprom.json`: `source`, `archive`, and `entries` with `section`, `range`, and `description`.
- `snippets/instructions.json`: documentation source metadata, highlighter source path, and instruction `entries` with `name`, `longName`, `summary`, `forms`, `operands`, and `source`.

These data files are consumed by the corresponding panes; their individual
records are reference data rather than executable source functions.

## Complete file index

The entries below name every file under `src/`. For files already grouped
above, the referenced section explains their classes and functions in detail.

### Application and Lua host

- `src/iconBS.rc` — Windows resource script assigning the application icon.
- `src/host/main.cpp` — application entry point, font setup, startup routing, Lua startup, and Qt event loop; see Application entry point.
- `src/host/bindings/bindapplication.h` — declaration of the application Lua binding registrar.
- `src/host/bindings/bindapplication.cpp` — implementation of `mipsie.app` functions.
- `src/host/bindings/bindfilesystem.h` — declaration of the filesystem Lua binding registrar.
- `src/host/bindings/bindfilesystem.cpp` — path conversion helpers and `mipsie.fs` functions.
- `src/host/bindings/bindhttp.h` — declaration of the HTTP Lua binding registrar.
- `src/host/bindings/bindhttp.cpp` — HTTP request validation and asynchronous `mipsie.http` functions.
- `src/host/bindings/bindingregistry.h` — declaration of the Lua binding registry entry point.
- `src/host/bindings/bindingregistry.cpp` — creates `mipsie` and registers each binding module.
- `src/host/bindings/bindlogging.h` — declaration of the logging Lua binding registrar.
- `src/host/bindings/bindlogging.cpp` — registers the Lua info, warning, and error logging functions.
- `src/host/bindings/bindui.h` — declaration of the UI Lua binding registrar.
- `src/host/bindings/bindui.cpp` — widget/action lookup helpers and `mipsie.ui`/`mipsie.editor` functions.
- `src/host/bindings/bindkeymap.h` — declaration of the keymap Lua binding registrar.
- `src/host/bindings/bindkeymap.cpp` — registers `mipsie.keymap.set` and connects Lua callbacks to main-window shortcuts.
- `src/host/bindings/pathresolver.h` — inline helpers for application, project, base, and working paths.
- `src/host/runtime/luaconfig.h` — currently empty guarded header reserved for Lua configuration.
- `src/host/runtime/luaerror.h` — declaration of the currently empty `LuaError` placeholder.
- `src/host/runtime/luaerror.cpp` — empty `LuaError` constructor implementation.
- `src/host/runtime/luaruntime.h` — `LuaRuntime` class, lifecycle API, and runtime signals.
- `src/host/runtime/luaruntime.cpp` — Lua state setup, script execution, error reporting, and shutdown implementation.

### Main window and pane framework

- `src/host/ui/mainwindow/mainwindow.h` — MainWindow class API, slots, signals, and owned state.
- `src/host/ui/mainwindow/mainwindow.cpp` — MainWindow implementation, settings helpers, project/ROM workflows, menus, and pane coordination.
- `src/host/ui/mainwindow/mainwindow.ui` — Qt Designer skeleton for the main window.
- `src/host/ui/mainwindow/menubar.h` — MenuBar class declaration.
- `src/host/ui/mainwindow/menubar.cpp` — menu and action construction and signal wiring.
- `src/host/ui/mainwindow/settingsdialog.h` — SettingsDialog API and control members.
- `src/host/ui/mainwindow/settingsdialog.cpp` — settings tabs, data serialization, validation, and control callbacks.
- `src/host/ui/mainwindow/splitter.h` — nested pane splitter API.
- `src/host/ui/mainwindow/splitter.cpp` — pane insertion/removal, layout, splitting, and pane shortcuts.
- `src/host/ui/mainwindow/panes/pane.h` — Pane type enum, widget API, signals, and menu state.
- `src/host/ui/mainwindow/panes/pane.cpp` — Pane menus, focus routing, content replacement, and border painting.
- `src/host/ui/mainwindow/panes/searchablelisttable.h` — shared searchable JSON table API and column description type.
- `src/host/ui/mainwindow/panes/searchablelisttable.cpp` — JSON loading, filtering, custom row painting, and clipboard handling.

### Pane implementations and reference data

- `src/host/ui/mainwindow/panes/assembler/assembler.h` — Assembler class API and process state.
- `src/host/ui/mainwindow/panes/assembler/assembler.cpp` — armips process management, in-process ROM checksum updates, and output navigation.
- `src/host/ui/mainwindow/panes/cmdconsole/cmdconsole.h` — embedded command console API and process members.
- `src/host/ui/mainwindow/panes/cmdconsole/cmdconsole.cpp` — Windows command-shell startup, input, output, and restart handling.
- `src/host/ui/mainwindow/panes/editor/editor.h` — Editor tab, editing, navigation, save, and signal API.
- `src/host/ui/mainwindow/panes/editor/editor.cpp` — editor tab implementation, line numbers, save state, editing, search, and labels.
- `src/host/ui/mainwindow/panes/editor/mipscompleter.h` — MIPS completion model/delegate and completer declarations.
- `src/host/ui/mainwindow/panes/editor/mipscompleter.cpp` — completion categories, dynamic symbols, popup behavior, and insertion.
- `src/host/ui/mainwindow/panes/editor/mipshighlighter.h` — completion data types and syntax highlighter API.
- `src/host/ui/mainwindow/panes/editor/mipshighlighter.cpp` — MIPS token patterns, theme formats, completion data, and highlighting.
- `src/host/ui/mainwindow/panes/eepromview/eepromview.h` — EEPROM reference pane declaration.
- `src/host/ui/mainwindow/panes/eepromview/eepromview.cpp` — EEPROM table configuration and range-copy action.
- `src/host/ui/mainwindow/panes/eepromview/eeprom.json` — EEPROM section, range, and description records.
- `src/host/ui/mainwindow/panes/eepromview/eepromview.qrc` — embeds `eeprom.json` as `:/eepromview/eeprom.json`.
- `src/host/ui/mainwindow/panes/filetree/filetree.h` — file tree, proxy model, path API, and signals.
- `src/host/ui/mainwindow/panes/filetree/filetree.cpp` — recursive project filtering, selection, activation, and keyboard handling.
- `src/host/ui/mainwindow/panes/functionview/functionview.h` — function reference pane declaration.
- `src/host/ui/mainwindow/panes/functionview/functionview.cpp` — function dataset loading, filtering, display, and call-copy behavior.
- `src/host/ui/mainwindow/panes/functionview/functions.json` — function names, addresses, return types, arguments, and prototypes.
- `src/host/ui/mainwindow/panes/functionview/functionview.qrc` — embeds `functions.json` as `:/functionview/functions.json`.
- `src/host/ui/mainwindow/panes/luaconsole/luaconsole.h` — Lua console API and function-request signal.
- `src/host/ui/mainwindow/panes/luaconsole/luaconsole.cpp` — console output and parsing of supported global function calls.
- `src/host/ui/mainwindow/panes/marioactionsview/marioactionsview.h` — Mario action reference pane declaration.
- `src/host/ui/mainwindow/panes/marioactionsview/marioactionsview.cpp` — action table configuration and action-copy behavior.
- `src/host/ui/mainwindow/panes/marioactionsview/actions.json` — action section, name, group, handler, and description records.
- `src/host/ui/mainwindow/panes/marioactionsview/marioactionsview.qrc` — embeds `actions.json` as `:/marioactionsview/actions.json`.
- `src/host/ui/mainwindow/panes/ramaddressview/ramaddressview.h` — RAM address reference pane declaration.
- `src/host/ui/mainwindow/panes/ramaddressview/ramaddressview.cpp` — address dataset loading, filtering, display, and copy behavior.
- `src/host/ui/mainwindow/panes/ramaddressview/ram_addresses.json` — address, length, type, description, and notes records.
- `src/host/ui/mainwindow/panes/ramaddressview/ramaddressview.qrc` — embeds `ram_addresses.json` as `:/ramaddressview/ram_addresses.json`.
- `src/host/ui/mainwindow/panes/snippets/snippets.h` — instruction help pane API.
- `src/host/ui/mainwindow/panes/snippets/snippets.cpp` — instruction documentation loading and display.
- `src/host/ui/mainwindow/panes/snippets/instructions.json` — instruction names, forms, summaries, operands, and source URLs.
- `src/host/ui/mainwindow/panes/snippets/snippets.qrc` — embeds `instructions.json` as `:/snippets/instructions.json`.
- `src/host/ui/mainwindow/panes/structview/structview.h` — structure reference pane declaration.
- `src/host/ui/mainwindow/panes/structview/structview.cpp` — structure table configuration and offset-copy behavior.
- `src/host/ui/mainwindow/panes/structview/structs.json` — structure names, offsets, field types, and descriptions.
- `src/host/ui/mainwindow/panes/structview/structview.qrc` — embeds `structs.json` as `:/structview/structs.json`.
