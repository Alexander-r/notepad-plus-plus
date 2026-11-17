// Qt port: see NppExecDialog.h

#include "NppExecDialog.h"
#include "NppExecRichEdit.h"

// Here SendMessage and PostMessage are those of Notepad++, which the header's names stand for elsewhere
#undef SendMessage
#undef PostMessage

#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QShowEvent>

#include <map>
#include <mutex>

namespace
{
	// The procedure of each of NppExec's dialogs that is open
	std::mutex g_dialogsLock;
	std::map<QWidget*, DLGPROC> g_dialogs;

	DLGPROC procOf(QWidget* window)
	{
		std::lock_guard<std::mutex> lock(g_dialogsLock);
		const auto it = g_dialogs.find(window);
		return (it != g_dialogs.end()) ? it->second : nullptr;
	}

	void registerDialog(QWidget* dialog, DLGPROC proc)
	{
		{
			std::lock_guard<std::mutex> lock(g_dialogsLock);
			g_dialogs[dialog] = proc;
		}
		QObject::connect(dialog, &QObject::destroyed, [dialog]() {
			std::lock_guard<std::mutex> lock(g_dialogsLock);
			g_dialogs.erase(dialog);
		});
	}

	INT_PTR command(QWidget* dialog, DLGPROC proc, int id, WORD code, QWidget* control)
	{
		return proc(dialog, WM_COMMAND, MAKEWPARAM(static_cast<WORD>(id), code), reinterpret_cast<LPARAM>(control));
	}

	// A modal or modeless dialog: Escape and the close button go to the procedure, as DefDlgProc sends them
	class Dialog : public QDialog
	{
	public:
		Dialog(QWidget* parent) : QDialog(parent) {}

		DLGPROC proc = nullptr;
		bool ended = false;

		// Messages of Notepad++, as its docking notifications to a docked panel, given back as a dialog procedure
		// gives them: DWLP_MSGRESULT when the procedure has handled the message
		bool event(QEvent* event) override
		{
			if (event->type() == NppMessageEvent::eventType && proc)
			{
				auto* message = static_cast<NppMessageEvent*>(event);
				setProperty("nppMsgResult", 0);
				const INT_PTR handled = proc(this, message->message, message->wParam, message->lParam);
				message->result = handled ? static_cast<LRESULT>(property("nppMsgResult").toLongLong()) : 0;
				return true;
			}
			return QDialog::event(event);
		}

		void reject() override
		{
			// Escape, as IDCANCEL, which the procedure ends the dialog for
			if (proc)
				command(this, proc, IDCANCEL, BN_CLICKED, nullptr);
			else
				QDialog::reject();
		}

	protected:
		void closeEvent(QCloseEvent* event) override
		{
			if (ended || !proc)
			{
				QDialog::closeEvent(event);
				return;
			}
			event->ignore();
			if (!proc(this, WM_CLOSE, 0, 0))
				command(this, proc, IDCANCEL, BN_CLICKED, nullptr);
		}

		void showEvent(QShowEvent* event) override
		{
			QDialog::showEvent(event);
			if (proc && !event->spontaneous())
				proc(this, WM_SHOWWINDOW, TRUE, 0);
		}

		void resizeEvent(QResizeEvent* event) override
		{
			QDialog::resizeEvent(event);
			// For the procedures that keep the size; the layout places the controls
			if (proc)
				proc(this, WM_SIZE, 0, MAKELPARAM(static_cast<WORD>(width()), static_cast<WORD>(height())));
		}

		void hideEvent(QHideEvent* event) override
		{
			QDialog::hideEvent(event);
			if (proc && !event->spontaneous())
				proc(this, WM_SHOWWINDOW, FALSE, 0);
		}
	};
}

namespace
{
	// The form a control belongs to: its nearest ancestor with the "nppForm" property, as a page of a tab is one
	QWidget* formOf(QWidget* control)
	{
		for (QWidget* w = control->parentWidget(); w; w = w->parentWidget())
		{
			if (w->property("nppForm").toBool())
				return w;
		}
		return nullptr;
	}
}

void nppExecConnectControls(QWidget* form, DLGPROC proc)
{
	for (QWidget* control : form->findChildren<QWidget*>())
	{
		const QVariant idValue = control->property("nppId");
		if (!idValue.isValid() || formOf(control) != form)
			continue;
		const int id = idValue.toInt();
		if (auto* button = qobject_cast<QAbstractButton*>(control))
		{
			// clicked, not toggled: Windows notifies the user's clicks, and not BM_SETCHECK
			QObject::connect(button, &QAbstractButton::clicked, form, [form, proc, id, button]() {
				command(form, proc, id, BN_CLICKED, button);
			});
		}
		else if (auto* combo = qobject_cast<QComboBox*>(control))
		{
			QObject::connect(combo, &QComboBox::activated, form, [form, proc, id, combo]() {
				command(form, proc, id, CBN_SELCHANGE, combo);
			});
			if (combo->isEditable())
			{
				QObject::connect(combo->lineEdit(), &QLineEdit::textEdited, form, [form, proc, id, combo]() {
					command(form, proc, id, CBN_EDITCHANGE, combo);
				});
			}
		}
		else if (auto* edit = qobject_cast<QLineEdit*>(control))
		{
			// A single-line edit also notifies a change by the program, as Windows does for WM_SETTEXT
			QObject::connect(edit, &QLineEdit::textChanged, form, [form, proc, id, edit]() {
				command(form, proc, id, EN_CHANGE, edit);
			});
		}
		else if (auto* edit = qobject_cast<QPlainTextEdit*>(control))
		{
			QObject::connect(edit, &QPlainTextEdit::textChanged, form, [form, proc, id, edit]() {
				command(form, proc, id, EN_CHANGE, edit);
			});
		}
		else if (auto* list = qobject_cast<QListWidget*>(control))
		{
			QObject::connect(list, &QListWidget::currentRowChanged, form, [form, proc, id, list]() {
				command(form, proc, id, LBN_SELCHANGE, list);
			});
			QObject::connect(list, &QListWidget::itemDoubleClicked, form, [form, proc, id, list]() {
				command(form, proc, id, LBN_DBLCLK, list);
			});
		}
	}
}

INT_PTR nppExecDialogBox(int idDlg, HWND parent, DLGPROC proc, LPARAM param)
{
	INT_PTR result = -1;
	nppExecOnGuiThread([&]() {
		auto* dialog = new Dialog(parent);
		QWidget* form = nppExecBuildForm(idDlg, dialog);
		if (!form)
		{
			delete dialog;
			return;
		}
		dialog->proc = proc;
		dialog->setModal(true);
		registerDialog(dialog, proc);
		nppExecConnectControls(dialog, proc);
		QPointer<Dialog> guard(dialog);
		proc(dialog, WM_INITDIALOG, 0, param);
		if (guard && !guard->ended)
			result = guard->exec();
		else if (guard)
			result = guard->result();
		delete guard.data();
	});
	return result;
}

HWND nppExecCreateDialog(int idDlg, HWND parent, DLGPROC proc, LPARAM param)
{
	HWND created = nullptr;
	nppExecOnGuiThread([&]() {
		auto* dialog = new Dialog(parent);
		QWidget* form = nppExecBuildForm(idDlg, dialog);
		if (!form)
		{
			delete dialog;
			return;
		}
		dialog->proc = proc;
		// A docked panel, which Notepad++ puts in a dock, is a child widget and not a window
		if (dialog->property("nppChildForm").toBool())
			dialog->setWindowFlags(Qt::Widget);
		registerDialog(dialog, proc);
		nppExecConnectControls(dialog, proc);
		proc(dialog, WM_INITDIALOG, 0, param);
		created = dialog;
	});
	return created;
}

void nppExecInitForm(HWND form, DLGPROC proc, LPARAM param)
{
	nppExecOnGuiThread([&]() {
		registerDialog(form, proc);
		nppExecConnectControls(form, proc);
		proc(form, WM_INITDIALOG, 0, param);
	});
}

BOOL EndDialog(HWND dialog, INT_PTR result)
{
	nppExecOnGuiThread([&]() {
		if (auto* d = dynamic_cast<Dialog*>(dialog))
		{
			d->ended = true;
			d->setResult(static_cast<int>(result));
			if (d->isVisible())
				d->done(static_cast<int>(result));
		}
	});
	return TRUE;
}

BOOL DestroyWindow(HWND window)
{
	if (!window)
		return FALSE;
	nppExecOnGuiThread([&]() {
		if (const DLGPROC proc = procOf(window))
			proc(window, WM_DESTROY, 0, 0);
		window->deleteLater();
	});
	return TRUE;
}

LRESULT nppExecSendMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (const DLGPROC proc = procOf(window))
	{
		LRESULT result = 0;
		nppExecOnGuiThread([&]() { result = proc(window, message, wParam, lParam); });
		return result;
	}
	// The messages that NppExec sends to its controls, which are Qt's
	LRESULT editResult = 0;
	if (nppExecRichEditMessage(window, message, wParam, lParam, &editResult))
		return editResult;
	if (window && !window->property("nppForm").toBool() && window->property("nppId").isValid())
	{
		LRESULT result = 0;
		bool handled = true;
		nppExecOnGuiThread([&]() {
			switch (message)
			{
				case EM_LIMITTEXT:
					if (auto* edit = qobject_cast<QLineEdit*>(window))
						edit->setMaxLength(wParam ? static_cast<int>(wParam) : 32767);
					else if (auto* combo = qobject_cast<QComboBox*>(window); combo && combo->lineEdit())
						combo->lineEdit()->setMaxLength(wParam ? static_cast<int>(wParam) : 32767);
					break;
				case WM_SETREDRAW:
					window->setUpdatesEnabled(wParam != 0);
					break;
				case WM_SETFOCUS:
					window->setFocus(Qt::OtherFocusReason);
					break;
				default:
					handled = false;
					break;
			}
		});
		if (handled)
			return result;
	}
	return ::SendMessage(window, message, wParam, lParam);
}

bool nppExecPostMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (procOf(window))
	{
		QPointer<QWidget> target(window);
		return QMetaObject::invokeMethod(qApp, [target, message, wParam, lParam]() {
			if (target)
			{
				if (const DLGPROC proc = procOf(target))
					proc(target, message, wParam, lParam);
			}
		}, Qt::QueuedConnection);
	}
	return ::PostMessage(window, message, wParam, lParam);
}

LRESULT SendDlgItemMessage(HWND dialog, int id, UINT message, WPARAM wParam, LPARAM lParam)
{
	return nppExecSendMessage(GetDlgItem(dialog, id), message, wParam, lParam);
}

UINT IsDlgButtonChecked(HWND dialog, int id)
{
	UINT checked = BST_UNCHECKED;
	nppExecOnGuiThread([&]() {
		if (auto* button = qobject_cast<QAbstractButton*>(GetDlgItem(dialog, id)))
			checked = button->isChecked() ? BST_CHECKED : BST_UNCHECKED;
	});
	return checked;
}

BOOL CheckDlgButton(HWND dialog, int id, UINT check)
{
	BOOL done = FALSE;
	nppExecOnGuiThread([&]() {
		if (auto* button = qobject_cast<QAbstractButton*>(GetDlgItem(dialog, id)))
		{
			button->setChecked(check == BST_CHECKED);
			done = TRUE;
		}
	});
	return done;
}

BOOL CheckRadioButton(HWND dialog, int firstId, int lastId, int checkId)
{
	nppExecOnGuiThread([&]() {
		for (int id = firstId; id <= lastId; ++id)
		{
			if (auto* button = qobject_cast<QAbstractButton*>(GetDlgItem(dialog, id)))
			{
				// Radio buttons in one group uncheck the others themselves, and these may be in several
				const bool autoExclusive = button->autoExclusive();
				button->setAutoExclusive(false);
				button->setChecked(id == checkId);
				button->setAutoExclusive(autoExclusive);
			}
		}
	});
	return TRUE;
}

BOOL SetDlgItemText(HWND dialog, int id, const wchar_t* text)
{
	return SetWindowText(GetDlgItem(dialog, id), text);
}

UINT GetDlgItemText(HWND dialog, int id, wchar_t* text, int size)
{
	return static_cast<UINT>(GetWindowText(GetDlgItem(dialog, id), text, size));
}
