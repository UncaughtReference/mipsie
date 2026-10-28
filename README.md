# mipsie
Refer to [docs/llmUse.md](docs/llmUse.md) for LLM use info <br>
Refer to [docs/lua.md](docs/lua.md) for Lua API info <br>
Refer to [docs/source.md](docs/source.md) for source code breakdown

## Goals
This program aims to make writing assembly for SM64 romhacks easier and more efficient.
The Lua API aims to make the IDE more customizable.

## UI Overview
### Menu Bar
* File
  - Open File <br>
  Allows you to open an ASM file.
  - Open ROM <br>
  Allows you to open a standalone ROM for assembling.
  - Open Folder <br>
  Allows you to open a folder and create a project.
  - Open Project <br>
  Allows you to open a recently opened project.
  - Save File
  - Save File As <br>
  Saves the currently opened file.
  - Search Files <br>
  Focuses on the file search.
  - New Tab
  - Close Tab
  - Previous Tab
  - Next Tab <br>
  Controls for the *Editor* tabs.
* Edit
  - Undo
  - Redo
  - Cut
  - Copy
  - Paste <br>
  Standard *Editor* actions.
  - Comment Out <br>
  Toggles commenting for the current *Editor* selection.
  - Find
  - Find and Replace <br>
  *Editor* search actions.
  - Search Labels <br>
  Focuses on the *Search Labels* menu in the Editor.
* Assemble
  - Assemble and Update Checksum <br>
  Creates a temporary ASM file (if needed) with the following contents:
  ```
  MIPSIE_ROM_FILENAME equ [configured ROM path]
  MIPSIE_FILE_POS equ 0x00000000

  .open MIPSIE_ROM_FILENAME, MIPSIE_FILE_POS

  .n64
  [ASM file contents]
  .close
  ```
  and runs armips, then updates the checksum.
  - Assemble, Update Checksum, Focus PJ64 <br>
  Same as above, except focuses on an existing PJ64 process if it exists.
* Backup
  - Backup ROM <br>
  Backs up the configured ROM and .config file to `[rom base directory]\backups`.
  - Restore Version <br>
  Brings up a window asking to restore a previous backup.
  Backs up the current ROM and .config file to `[backups directory]\brokenROMs`.
* Settings
  - Settings <br>
  Opens the settings.
* Help
  - LLM Use <br>
  Opens information on LLM use.
  - Lua API <br>
  Opens information on the Lua API.
  - Source Code <br>
  Opens information on the source code.
  - Check For Updates <br>
  Checks if there are any new releases on GitHub.
  - Open App Data <br>
  Opens %LOCALAPPDATA%\mipsie\mipsie. This is where most of the config is stored.

---
## File Tree
  Generic file tree, the root directory being the project directory. You can navigate 
  it using your keyboard or mouse. Use the search function to quickly find files.
## Editor
  The *Editor* contains autocomplete and label searching. Otherwise, it's your average text editor.
## Snippets
  Instruction snippets that update based on your *Edtior* cursor position. 
  Taken from [here](https://asm-editor.specy.app).
## Function View, RAM Address View, Struct View, Mario Actions View, EEPROM View
  Searchable tables for the aforementioned fields. You can double click on an entry to copy its main value.
## Assembler
  Contains information about the assembling process. For errors on specific lines, you can double click on them to bring yourself to the mentioned file and line.
## Lua Console
  Contains output from init.lua and allows you to run functions from the file.
## Command Prompt
   Generic Command Prompt, meant to be used mainly for version control.
   
---
## Macros
  Macros allow you to assign a keyboard shortcut to insert some text. By default includes the function wrapper, 0x8034 loading and 0x80361160 loading.

# Have Fun!
