# Plugins for the Qt port

`mimeTools/` is [MIME Tools](https://github.com/npp-plugins/mimetools) by Don Ho (GPL), from upstream commit `7a54297`, built for the Qt port. It shows that a plugin written for Windows works with few changes, and the app tests run all of its commands. Follow it as an example. The plugins that come with Notepad++, MIME Tools among them, are in `plugins/` at the top of the repository.

## What a plugin needs

- **Same API.** `setInfo`, `getName`, `getFuncsArray`, `beNotified`, `messageProc` and `isUnicode`, and the `NPPM_*` and `SCI_*` messages sent with `SendMessage`, all work as on Windows.
- **Recompiling.** A Windows DLL cannot load. A plugin is a `.so`, installed as `plugins/Name/Name.so` in the user's config folder, built with a compatible Qt since `HWND` is a `QWidget*`.
- **The port's headers.** Use its `PluginInterface.h`, `Notepad_plus_msgs.h`, `menuCmdID.h` and `Scintilla.h`, not the copies a plugin carries.
- **`compat/` first on the include path.** Its `windows.h` has the types and macros that plain plugin code uses, and `MessageBox`.
- **`UNICODE` defined,** as the Windows project files do. Plugins commonly define `isUnicode` only then, and Notepad++ refuses a plugin without it.

## What has to change in the source

Each change in `mimeTools/mimeTools.cpp` is marked with `Qt port:`. There are three kinds:

- **`DllMain` is not called.** Call it from a function with `__attribute__((constructor))`, or move what it does to `setInfo`.
- **Dialogs from Windows resources have to be rewritten with Qt.** The `compat` header deliberately has no `CreateDialog` and the like. This is the real cost of porting: a plugin with much UI needs real work, and one built on MFC or COM, like XML Tools with MSXML, is close to a rewrite.
- **Code that only served such a dialog goes away,** like theming it for dark mode.
- **Messages to the plugin's own widgets come as an event.** `SendMessage` and `PostMessage` to a widget that Notepad++
  has no handler for, such as a plugin's docked panel, deliver the message as an `NppMessageEvent` (`NppMessages.h`),
  which the widget handles in `event()` or an event filter, setting `result` as its window procedure would. So a panel
  hears Notepad++'s `WM_NOTIFY` with `DMN_CLOSE`, `DMN_FLOAT`, `DMN_DOCK` and the others, and stays open by answering
  `DMN_CLOSE` with nonzero, as on Windows. Converter's `DockingDlgInterface` in `plugins/` hands them to `run_dlgProc`.

Link with `Qt6::Widgets` if the plugin shows anything. `CMakeLists.txt` here has all of it. Code that does not touch the UI, like the conversions here, should compile unchanged.
