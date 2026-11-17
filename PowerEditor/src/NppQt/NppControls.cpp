// This file is part of Notepad++ project

#include "NppControls.h"

#include <QEvent>
#include <QListWidget>

#include "NppMessages.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QWidget>

QWidget* nppFindControl(const QWidget* form, int id)
{
	if (!form)
		return nullptr;
	const auto children = form->findChildren<QWidget*>();
	for (QWidget* child : children)
	{
		if (child->property("nppId").toInt() == id)
			return child;
	}
	return nullptr;
}

QString nppControlText(const QWidget* control)
{
	if (const auto* b = qobject_cast<const QAbstractButton*>(control))
		return b->text();
	if (const auto* c = qobject_cast<const QComboBox*>(control))
		return c->currentText();
	if (const auto* e = qobject_cast<const QLineEdit*>(control))
		return e->text();
	if (const auto* p = qobject_cast<const QPlainTextEdit*>(control))
		return p->toPlainText();
	if (const auto* l = qobject_cast<const QLabel*>(control))
		return l->text();
	if (const auto* g = qobject_cast<const QGroupBox*>(control))
		return g->title();
	return {};
}

void nppSetControlText(QWidget* control, const QString& text)
{
	if (auto* b = qobject_cast<QAbstractButton*>(control))
		b->setText(text);
	else if (auto* c = qobject_cast<QComboBox*>(control))
		c->setCurrentText(text);
	else if (auto* e = qobject_cast<QLineEdit*>(control))
		e->setText(text);
	else if (auto* p = qobject_cast<QPlainTextEdit*>(control))
		p->setPlainText(text);
	else if (auto* l = qobject_cast<QLabel*>(control))
		l->setText(text);
	else if (auto* g = qobject_cast<QGroupBox*>(control))
		g->setTitle(text);
}

namespace
{
	class FocusReporter final : public QObject
	{
	public:
		FocusReporter(QObject* parent, NppCommandHandler handler, int id, WORD setFocus, WORD killFocus)
			: QObject(parent), _handler(std::move(handler)), _id(id), _setFocus(setFocus), _killFocus(killFocus) {}

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override
		{
			if (event->type() == QEvent::FocusIn)
				_handler(_id, _setFocus);
			else if (event->type() == QEvent::FocusOut)
				_handler(_id, _killFocus);
			return QObject::eventFilter(watched, event);
		}

	private:
		NppCommandHandler _handler;
		int _id;
		WORD _setFocus;
		WORD _killFocus;
	};
}

void nppConnectControlCommands(QWidget* form, NppCommandHandler handler)
{
	const auto controls = form->findChildren<QWidget*>();
	for (QWidget* control : controls)
	{
		const int id = control->property("nppId").toInt();
		if (id <= 0)
			continue;

		if (auto* b = qobject_cast<QAbstractButton*>(control))
		{
			QObject::connect(b, &QAbstractButton::clicked, form, [handler, id]() { handler(id, BN_CLICKED); });
		}
		else if (auto* c = qobject_cast<QComboBox*>(control))
		{
			// activated is user-driven only, as CBN_SELCHANGE is
			QObject::connect(c, &QComboBox::activated, form, [handler, id]() { handler(id, CBN_SELCHANGE); });
			if (c->lineEdit())
			{
				QObject::connect(c->lineEdit(), &QLineEdit::textEdited, form, [handler, id]()
				{
					handler(id, CBN_EDITUPDATE);
					handler(id, CBN_EDITCHANGE);
				});
				c->lineEdit()->installEventFilter(new FocusReporter(c, handler, id, CBN_SETFOCUS, CBN_KILLFOCUS));
			}
		}
		else if (auto* e = qobject_cast<QLineEdit*>(control))
		{
			QObject::connect(e, &QLineEdit::textChanged, form, [handler, id]() { handler(id, EN_CHANGE); });
			e->installEventFilter(new FocusReporter(e, handler, id, EN_SETFOCUS, EN_KILLFOCUS));
		}
		else if (auto* l = qobject_cast<QListWidget*>(control))
		{
			QObject::connect(l, &QListWidget::itemSelectionChanged, form, [handler, id]() { handler(id, LBN_SELCHANGE); });
			QObject::connect(l, &QListWidget::itemDoubleClicked, form, [handler, id]() { handler(id, LBN_DBLCLK); });
		}
	}
}
