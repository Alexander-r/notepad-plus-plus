What is Notepad++ ?
===================

Notepad++ is a free source code editor and Notepad replacement that supports
several programming languages and natural languages.

This repository is a port of Notepad++ from the Windows API to
Qt 6 QWidgets, so that it builds and runs on Linux, under Wayland and X11.
It follows upstream Notepad++ 8.9.9.1. This is not an official Notepad++
release, so the problems with the port should be reported here and not to
the upstream project.

It is under the [GPL License](LICENSE), version 3, same as Notepad++.


State of the port
-----------------

The application builds and runs, and every dialog and panel is ported.

What stays as on Windows:

- The editor is the same Scintilla and Lexilla, with Scintilla's own Qt layer.
- Find, Replace, Find in Files and Mark use the same Boost regex search, which
  sees the text as Windows does, so search results are the same.
- Languages, themes, User Defined Languages, the Function List, macros,
  `shortcuts.xml`, sessions, context menus and the translations work as on
  Windows, and their XML files have the same format.
- Plugins have the same API: the same exports and the same `NPPM_*` and `SCI_*`
  messages.

What is different on Linux:

- **Look:** the windows follow the desktop's theme in light and in dark mode.
- **Settings** are kept in `~/.config/notepad++/`. The preferences that were in
  `config.xml` are now in `config.ini`, and an existing `config.xml` is imported
  once. The other settings files stay XML. A `doLocalConf.xml` file beside the
  program still keeps everything beside it, for a portable setup.
- **Plugins** must be recompiled as Linux libraries, and are loaded from
  `plugins/Name/Name.so` or `plugins/Name/libName.so`, beside the program or in
  the settings folder. Code without a user interface usually compiles with few
  changes, while dialogs made from Windows resources have to be rewritten with
  Qt. [PowerEditor/Test/plugins](PowerEditor/Test/plugins/README.md) explains
  how, with MIME Tools as a ported example.
- **Adapted:** the commands that opened Explorer or `cmd` open the desktop's
  file manager and terminal. "Save as administrator" asks for the password
  through `pkexec`. File associations are made by the
  [desktop file](PowerEditor/src/notepad-plus-plus.desktop) instead of a page
  of the preferences.

Known gaps:

- The tab bar has only one row, so the multi-line tab bar option does nothing.
- Dropping a tab onto another running Notepad++ opens a new instance.
- Folder as Workspace watches each folder with inotify, so a very large tree can
  run out of watches.


Build Notepad++
---------------

You need CMake 3.21 or later, a C++20 compiler, and Qt 6 with the Widgets,
Core5Compat, Network, PrintSupport and Test modules. Scintilla, Lexilla
and Boost regex are part of this repository.

```
cmake -S . -B build
cmake --build build -j
build/bin/notepad++
```

The build puts the languages, themes, translations, function list and
auto-completion files beside the program in `build/bin`, where it finds them.

To run the tests, build all targets and then run:

```
ctest --test-dir build --output-on-failure
```

Without a display, set `QT_QPA_PLATFORM=offscreen`. The app tests use the
clipboard and take the focus, so run them offscreen or in a private session
rather than on your desktop.
