// This file is part of Notepad++ project
//
// Generic access to form controls by their numeric ID, stored in the nppId property. The translation
// files address controls by these numbers. Application code uses the typed members of its Ui class.

#pragma once

#include <QString>

#include <functional>

#include "NppPlatform.h"

class QWidget;

QWidget* nppFindControl(const QWidget* form, int id);
QString nppControlText(const QWidget* control);
void nppSetControlText(QWidget* control, const QString& text);

// Reports user actions on the form's controls as (control ID, Win32 notification code), for logic written around
// those IDs: clicks, combo box selections and edits, focus changes of edit fields, and text changes.
using NppCommandHandler = std::function<void(int id, WORD notification)>;
void nppConnectControlCommands(QWidget* form, NppCommandHandler handler);
