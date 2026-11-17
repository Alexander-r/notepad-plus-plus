# Plugins that come with Notepad++

The plugins in the Windows package of Notepad++ 8.9.9.1, built for the Qt port. They are kept apart from the editor's
code so that they can move to a repository of their own.

| Plugin | Version | Upstream |
|---|---|---|
| `mimeTools/` MIME Tools | 3.2 | [npp-plugins/mimetools](https://github.com/npp-plugins/mimetools) at `7a54297`, by Don Ho, GPL |
| `NppConverter/` Converter | 4.7 | [npp-plugins/converter](https://github.com/npp-plugins/converter) at `d3837f3`, by Don Ho, GPL |
| `NppExport/` NppExport | 0.4.0 | [chcg/NPP_ExportPlugin](https://github.com/chcg/NPP_ExportPlugin) at `ff15bcd` |
| `NppExec/` NppExec | 0.8.12.1 | [d0vgan/nppexec](https://github.com/d0vgan/nppexec) at `7a5d279`, by Vitaliy Dovgan, GPL 3 |
| `DSpellCheck/` DSpellCheck | 1.5.0 | [Predelnik/DSpellCheck](https://github.com/Predelnik/DSpellCheck) at `e214b57` (`v1.5.0`), by Sergey Semushin, GPL |

Only their sources are here, without the copies of the Notepad++ headers that they carry and their Visual Studio
projects. NppExport's repository is an unofficial copy of the SourceForge project and has no license file.

The first three are those of the Windows package. NppExec and DSpellCheck are not in it, but are installed by many
through Plugins Admin, which the port lacks. The Windows package also has `Config/nppPluginList.dll`, which is the list of Plugins
Admin, removed from the port.

DSpellCheck builds against the system's Hunspell (pkg-config `hunspell`) and iconv, in place of the copies it carries,
so its `deps/` keeps only the header-only libraries, Aspell's header and its unit test framework.

## Building

Inside the Notepad++ tree, the main build builds them into `build/bin/plugins/Name/Name.so`, beside the program,
which is where Notepad++ looks for the plugins it comes with. A user's own go in `~/.config/notepad++/plugins`.

On their own they need the Notepad++ source for the headers listed in `CMakeLists.txt`:

```
cmake -S plugins -B build-plugins -DNPP_SOURCE_DIR=/path/to/notepad-plus-plus
cmake --build build-plugins -j
```

A plugin takes nothing from Notepad++ but `SendMessage`, which the program exports, and exports only the six
functions of the plugin interface, as a DLL does.

## Tests

`tests/pluginsTest.cpp` runs the plugins in Notepad++ itself, as a user does: from `bin/plugins`, through the
Plugins menu, with their dialogs and panels. It links Notepad++'s `nppapp` library, so it is only built inside the
Notepad++ tree, by `ctest` as `plugins` and `pluginsConverterSettings`.

`tests/data` has what the original NppExport writes on Windows for the document of `exportToFiles`, made with the
comparison kit through Proton, with one difference: the original's RTF names the default font by a number it never
set, which the port makes 0. The outputs of Converter in the tests are also those of the original.

DSpellCheck's own unit tests, from `DSpellCheck/test`, run as `dspellcheckUnit`. The in-app tests give it the small
English dictionary of `tests/data/hunspell`, and stand in for GitHub with a local server for the downloads, which
`NPP_TEST_DSPELLCHECK_SERVER` points the plugin to. Against the original DSpellCheck 1.5.0 under Proton, with the
same dictionary, its menu, the controls of its dialogs and the words it marks in text and code are the same.

NppExec's unit tests, from `NppExec/UnitTests`, run as `nppexecUnit`. The in-app tests type in its console as a user does,
and wait for NppExec to be ready through `NPPM_MSGTOPLUGIN`, as another plugin would. Against the original NppExec 0.8.12.1
under Proton, its menu with the commands' IDs, the controls of its dialogs and the output of a script of its own commands
are the same.

## How NppExec is ported

Its engine is as it was. What NppExec took from Windows is in `NppExec/src/qtport`:

- **Threads and waits:** NppExec's scripts run on threads that wait on events, threads and processes through handles
  (`WaitForMultipleObjects`). `NppExecSync` gives these the meaning they have on Windows with the standard library.
- **Child processes** (`NppExecProcess`) start with fork and exec, with pipes, or with a pseudo terminal (`forkpty`) for
  the pseudo console option. The default run policy starts the program itself, found through `PATH`; the one of
  `COMSPEC` runs the line with `/bin/sh -c`. Ctrl+C, Ctrl+Break and `WM_CLOSE` of the kill methods are `SIGINT`,
  `SIGQUIT` and `SIGTERM` to the child's process group, and `TerminateProcess` is `SIGKILL`.
- **Windows and dialogs:** the window classes of the original (`CAnyWindow`, `CAnyRichEdit`...) work on Qt widgets, and
  every call is made on the GUI thread, as Windows sends the messages of a window to its thread. The dialogs are Qt forms
  (`NppExecForms.cpp`) with the control IDs in `nppId`, whose procedures of the original get their messages from the
  widgets (`NppExecDialog`), as do Notepad++'s messages to them, such as the docking notifications to the console, which
  come as `NppMessageEvent`. The console's key and mouse events reach NppExec's handler as RichEdit's `EN_MSGFILTER`
  notification, and the subclassed edits get theirs through an event filter, so that the input line, history,
  completion and find work as they did.
- **Files:** paths start at `/`, without drives. `%ABSFILE%` of the console's filters matches a path from `/` as well as
  one with a drive. NppExec's text files, such as the saved scripts, are UTF-16LE with a BOM, as on Windows, so they
  move between the two. `NppExec.ini` is read and written by the `GetPrivateProfileString` of `compat/`.
- **Beside the plugin** are `NppExec/`, the headers whose constants scripts may name (Notepad++'s and Scintilla's of this
  build), and `doc/NppExec`, the HTML manual of the Help menu.

## How NppExec differs from the original

- The console encodings ANSI and OEM are both UTF-8, the system's here.
- File > Exit closes Notepad++ without a close event, so NppExec cannot ask there to stop a running program first, as
  it does for the window's close button; it stops it at `NPPN_SHUTDOWN`.
- `$(WORKSPACE_ITEM_PATH)` and `$(WORKSPACE_ITEM_ROOT)` are empty: the original read them from the windows of Folder as
  Workspace.
- The transparency of the floating console is the window's opacity, which Wayland does not give.

## How DSpellCheck differs from the original

- **Dictionaries:** downloads come only from GitHub (LibreOffice's dictionaries), as the FTP source and the zip files
  it gave are dropped. The user's dictionaries are in `plugins/Config/Hunspell` of the settings folder, as on
  Windows, and the system path defaults to `/usr/share/hunspell`, where distributions put theirs.
- **Spellers:** Hunspell and Aspell, which is loaded from `libaspell.so.15` when installed. The Windows speller is
  not there.
- **Words:** as on a Western Windows, East Asian characters do not count as letters, so they are never checked.
- **The suggestions button** over a misspelled word is a child of the editor, and the suggestions of the context
  menu are added to the editor's menu as Notepad++ builds it, through event filters on its widgets in place of
  Windows' hooks and subclassing.
- **Proxy:** the connection settings only offer a web proxy, which applies to the downloads from GitHub.
- **Printing:** the original turns auto-check off while Notepad++ prints, so that the marks of misspellings are not
  printed, by catching the Print commands in its subclass of the main window. The port's commands go straight from
  the menu to Notepad++, so a plugin cannot see them first, and the marks are printed.

## Porting a plugin

What a plugin needs, and what changes, is in `PowerEditor/Test/plugins/README.md`. Each change in the sources here is
marked `Qt port:`. In short:

- **`compat/`** is first on the include path, and its `windows.h` is included before every C++ file, as the plugin
  headers bring it in on Windows. It has types, constants, string, path and memory helpers that work as their Windows
  namesakes, `GetModuleFileName` for the plugin's own file, and `MessageBox`. It does not emulate windows: anything
  that makes a window, uses the clipboard or opens a file dialog is rewritten with Qt in the plugin's source.
- **`DllMain` is not called.** Call it from a function with `__attribute__((constructor))`, or, when the plugin has
  global C++ objects that it sets up, from `setInfo`: a constructor function can run before them, as in Converter.
- **Dialogs are rewritten with Qt,** keeping each control's ID in the `nppId` property, as the dialogs of Notepad++ do.
  Converter's panel is a `QWidget` handed to Notepad++ through `NPPM_DMMREGASDCKDLG`, which puts it in a dock.
- **The clipboard takes MIME types:** `text/html` without the header of Windows' CF_HTML, `text/rtf`, and for the
  bytes of Paste Binary Content `application/x-notepad-plus-plus-binary`.
