// Qt port: the dialog templates of NppExec.rc as Qt forms, with layouts in place of the templates' positions.
//
// Each control has its ID of resource.h in the "nppId" property, through which the dialogs' procedures find it.
// The three pages of the Console Filters dialog are forms of their own, marked with the "nppForm" property, whose
// controls go to the procedures of their pages.

#include "NppExecDialog.h"
#include "NppExecRichEdit.h"
#include "../resource.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

QWidget* nppExecCreateColorButton(QWidget* parent); // PickColorBtn.cpp

namespace
{
	template <class T> T* withId(T* control, int id)
	{
		control->setProperty("nppId", id);
		return control;
	}

	QLabel* label(const QString& text, QWidget* parent, int id = -1)
	{
		auto* l = new QLabel(text, parent);
		if (id != -1)
			withId(l, id);
		return l;
	}

	QPushButton* button(const QString& text, QWidget* parent, int id, bool isDefault = false)
	{
		auto* b = withId(new QPushButton(text, parent), id);
		b->setAutoDefault(false);
		b->setDefault(isDefault);
		return b;
	}

	QCheckBox* check(const QString& text, QWidget* parent, int id)
	{
		return withId(new QCheckBox(text, parent), id);
	}

	QComboBox* combo(QWidget* parent, int id, bool editable, bool sorted = false)
	{
		auto* c = withId(new QComboBox(parent), id);
		c->setEditable(editable);
		if (editable)
			c->setInsertPolicy(QComboBox::NoInsert); // as Windows, where only the program adds items
		c->setProperty("nppSorted", sorted);
		// A list as wide as its items, and an editable one as wide as its layout gives it
		if (editable)
		{
			c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
			c->setMinimumContentsLength(10);
		}
		else
			c->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		return c;
	}

	QFrame* line(QWidget* parent)
	{
		auto* f = new QFrame(parent);
		f->setFrameShape(QFrame::HLine);
		f->setFrameShadow(QFrame::Sunken);
		return f;
	}

	// A row of buttons in the middle, as the templates have them
	QHBoxLayout* buttonRow(std::initializer_list<QPushButton*> buttons)
	{
		auto* row = new QHBoxLayout;
		row->addStretch(1);
		for (QPushButton* b : buttons)
			row->addWidget(b);
		row->addStretch(1);
		return row;
	}

	void doExec(QDialog* form)
	{
		form->setWindowTitle(QStringLiteral("Execute..."));
		form->setSizeGripEnabled(true);
		auto* layout = new QVBoxLayout(form);
		layout->addWidget(label(QStringLiteral("Command(s):"), form));
		// The original's code works on it as on RichEdit
		auto* edit = withId(new NppExecRichEditWidget(form), IDC_ED_CMDLINE);
		edit->lineWithEnd = false; // an edit control on Windows
		edit->setLineWrapMode(QPlainTextEdit::NoWrap);
		edit->setTabChangesFocus(false);
		edit->setContextMenuPolicy(Qt::DefaultContextMenu); // an edit control of Windows has its menu
		layout->addWidget(edit, 1);
		auto* row = new QHBoxLayout;
		auto* scripts = combo(form, IDC_CB_SCRIPT, false, true);
		scripts->setMinimumContentsLength(20);
		row->addWidget(scripts, 1);
		row->addSpacing(8);
		row->addWidget(button(QStringLiteral("OK"), form, IDOK, true));
		row->addWidget(button(QStringLiteral("Save..."), form, IDSAVE));
		row->addWidget(button(QStringLiteral("Cancel"), form, IDCANCEL));
		layout->addLayout(row);
		form->resize(form->fontMetrics().averageCharWidth() * 80, form->fontMetrics().height() * 20);
	}

	void scriptName(QDialog* form)
	{
		form->setWindowTitle(QStringLiteral("Script name"));
		auto* layout = new QVBoxLayout(form);
		auto* group = new QGroupBox(QStringLiteral("Script name:"), form);
		auto* groupLayout = new QVBoxLayout(group);
		auto* names = combo(group, IDC_CB_SCRIPTNAME, true, true);
		names->setMinimumContentsLength(30);
		groupLayout->addWidget(names);
		groupLayout->addLayout(buttonRow({button(QStringLiteral("Save"), group, IDOK, true),
			button(QStringLiteral("Delete"), group, IDC_BT_DELETE), button(QStringLiteral("Cancel"), group, IDCANCEL)}));
		layout->addWidget(group);
	}

	void console(QDialog* form)
	{
		form->setWindowTitle(QStringLiteral("Console"));
		form->setProperty("nppChildForm", true); // a docked panel
		auto* layout = new QVBoxLayout(form);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(2);
		auto* edit = withId(new NppExecRichEditWidget(form), IDC_RE_CONSOLE);
		edit->setLineWrapMode(QPlainTextEdit::NoWrap);
		layout->addWidget(edit, 1);
		auto* find = new QHBoxLayout;
		find->setContentsMargins(2, 0, 2, 2);
		auto* close = withId(new QToolButton(form), IDC_BT_FINDCLOSE);
		close->setText(QStringLiteral("X"));
		close->setAutoRaise(true);
		find->addWidget(close);
		auto* findEdit = withId(new QLineEdit(form), IDC_ED_FIND);
		findEdit->setMinimumWidth(form->fontMetrics().averageCharWidth() * 30);
		find->addWidget(findEdit);
		find->addWidget(check(QStringLiteral("Match &case"), form, IDC_CH_CASE));
		find->addWidget(check(QStringLiteral("Word &wrap"), form, IDC_CH_WORDWRAP));
		find->addWidget(check(QStringLiteral("Hide &toggled Console"), form, IDC_CH_HIDETOGGLED));
		find->addWidget(check(QStringLiteral("Tab = \\t"), form, IDC_CH_TABASCHAR));
		find->addStretch(1);
		layout->addLayout(find);
	}

	void helpAbout(QDialog* form)
	{
		form->setWindowTitle(QStringLiteral("NppExec Help/About"));
		auto* layout = new QVBoxLayout(form);
		auto* title = label(QStringLiteral("NppExec ver. 0.8.12.1 for Notepad++"), form);
		title->setAlignment(Qt::AlignHCenter);
		layout->addWidget(title);
		auto* copyright = label(QStringLiteral("(C)  Dec 2006 - Jun 2026,  Vitaliy Dovgan aka DV <dvv81 @ ukr.net>"), form);
		copyright->setAlignment(Qt::AlignHCenter);
		copyright->setWordWrap(true);
		layout->addWidget(copyright);
		auto* thanks = label(QStringLiteral("Thanks to:\nDon HO for Notepad++,\nJens Lorenz for the docking feature,\n"
			"Juha Nieminen and Joel Yliluoma for Function Parser\nand myself for this plugin :-)"), form);
		thanks->setAlignment(Qt::AlignHCenter);
		layout->addWidget(thanks);
		auto* info = label(QString(), form, IDC_ED_INFO);
		info->setFrameShape(QFrame::StyledPanel);
		info->setWordWrap(true);
		info->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		info->setTextInteractionFlags(Qt::TextSelectableByMouse);
		info->setMinimumHeight(form->fontMetrics().height() * 12);
		layout->addWidget(info, 1);
		auto* home = new QHBoxLayout;
		home->addWidget(label(QStringLiteral("Home:"), form));
		auto* address = label(QStringLiteral("<a href=\"https://github.com/d0vgan/nppexec/\">https://github.com/d0vgan/nppexec/</a>"), form, IDC_ST_HOME_ADDR);
		address->setOpenExternalLinks(true);
		home->addWidget(address);
		home->addStretch(1);
		layout->addLayout(home);
		layout->addLayout(buttonRow({button(QStringLiteral("OK"), form, IDOK, true)}));
	}

	QWidget* page(QWidget* parent)
	{
		auto* p = new QWidget(parent);
		p->setProperty("nppForm", true);
		return p;
	}

	QWidget* outputFilterPage(QWidget* parent)
	{
		QWidget* form = page(parent);
		auto* layout = new QGridLayout(form);
		layout->addWidget(check(QStringLiteral("Enable Console Output Filter"), form, IDC_CH_FILTER_ENABLE), 0, 0, 1, 2);
		auto* masks = new QVBoxLayout;
		auto* exclude = withId(new QGroupBox(QStringLiteral("Exclude mask(s):"), form), IDC_ST_FILTER_EXCLUDE);
		auto* excludeLayout = new QGridLayout(exclude);
		for (int i = 0; i < 5; ++i)
		{
			excludeLayout->addWidget(check(QString(), exclude, IDC_CH_EXCLUDE1 + i), i, 0);
			excludeLayout->addWidget(combo(exclude, IDC_CB_EXCLUDE1 + i, true), i, 1);
		}
		excludeLayout->addWidget(check(QStringLiteral("Exclude duplicate empty lines"), exclude, IDC_CH_EXCLUDE_DUPEMPTY), 5, 0, 1, 2);
		excludeLayout->addWidget(check(QStringLiteral("Exclude all empty lines"), exclude, IDC_CH_EXCLUDE_ALLEMPTY), 6, 0, 1, 2);
		excludeLayout->setColumnStretch(1, 1);
		masks->addWidget(exclude);
		auto* include = withId(new QGroupBox(QStringLiteral("Include mask(s):"), form), IDC_ST_FILTER_INCLUDE);
		auto* includeLayout = new QGridLayout(include);
		for (int i = 0; i < 5; ++i)
		{
			includeLayout->addWidget(check(QString(), include, IDC_CH_INCLUDE1 + i), i, 0);
			includeLayout->addWidget(combo(include, IDC_CB_INCLUDE1 + i, true), i, 1);
		}
		includeLayout->setColumnStretch(1, 1);
		masks->addWidget(include);
		layout->addLayout(masks, 1, 0);
		auto* help = label(QStringLiteral("Quick Reference"), form, IDC_ST_HELP_MASK);
		help->setFrameShape(QFrame::StyledPanel);
		help->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		help->setWordWrap(true);
		help->setMinimumWidth(form->fontMetrics().averageCharWidth() * 36);
		layout->addWidget(help, 1, 1);
		layout->setColumnStretch(0, 1);
		return form;
	}

	QWidget* replaceFilterPage(QWidget* parent)
	{
		QWidget* form = page(parent);
		auto* layout = new QVBoxLayout(form);
		layout->addWidget(check(QStringLiteral("Enable Replacing Filters"), form, IDC_CH_RPLCFILTER_ENABLE));
		for (int i = 0; i < 4; ++i)
		{
			// The check box of a filter is its group's title on Windows; here it is the first row of the group
			auto* group = withId(new QGroupBox(form), IDC_ST_FILTER_RPLC1 + i);
			auto* grid = new QGridLayout(group);
			grid->addWidget(check(QStringLiteral("Replacing Filter %1").arg(i + 1), group, IDC_CH_RPLC1 + i), 0, 0, 1, 3);
			auto* findLabel = label(QStringLiteral("Find what:"), group, IDC_ST_RFIND1 + i);
			findLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			grid->addWidget(findLabel, 1, 0);
			grid->addWidget(combo(group, IDC_CB_RFIND1 + i, true), 1, 1);
			grid->addWidget(check(QStringLiteral("Match case"), group, IDC_CH_RCASE1 + i), 1, 2);
			auto* replaceLabel = label(QStringLiteral("Replace with:"), group, IDC_ST_RRPLC1 + i);
			replaceLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			grid->addWidget(replaceLabel, 2, 0);
			grid->addWidget(combo(group, IDC_CB_RRPLC1 + i, true), 2, 1);
			grid->setColumnStretch(1, 1);
			layout->addWidget(group);
		}
		layout->addWidget(check(QStringLiteral("Exclude empty resulting lines"), form, IDC_CH_EXCLUDE_EMPTYRSLT));
		layout->addStretch(1);
		return form;
	}

	QWidget* highlightFilterPage(QWidget* parent)
	{
		QWidget* form = page(parent);
		auto* layout = new QVBoxLayout(form);
		auto* group = withId(new QGroupBox(QStringLiteral("HighLight mask(s):"), form), IDC_ST_FILTER_HIGHLIGHT);
		auto* grid = new QGridLayout(group);
		// The headers of the columns of check boxes, group boxes around them on Windows
		const int ids[3] = {IDC_ITALIC, IDC_BOLD, IDC_UNDERLINED};
		const QString names[3] = {QStringLiteral("I"), QStringLiteral("B"), QStringLiteral("U")};
		for (int c = 0; c < 3; ++c)
		{
			auto* header = label(names[c], group, ids[c]);
			header->setAlignment(Qt::AlignHCenter);
			grid->addWidget(header, 0, 3 + c);
		}
		for (int i = 0; i < 10; ++i)
		{
			grid->addWidget(check(QString(), group, IDC_CH_HIGHLIGHT1 + i), i + 1, 0);
			grid->addWidget(combo(group, IDC_CB_HIGHLIGHT1 + i, true), i + 1, 1);
			grid->addWidget(withId(nppExecCreateColorButton(group), IDC_HIGHLIGHT_COLOR1 + i), i + 1, 2);
			grid->addWidget(check(QString(), group, IDC_CH_HIGHLIGHT_I1 + i), i + 1, 3, Qt::AlignHCenter);
			grid->addWidget(check(QString(), group, IDC_CH_HIGHLIGHT_B1 + i), i + 1, 4, Qt::AlignHCenter);
			grid->addWidget(check(QString(), group, IDC_CH_HIGHLIGHT_U1 + i), i + 1, 5, Qt::AlignHCenter);
		}
		grid->setColumnStretch(1, 1);
		layout->addWidget(group);
		auto* help = label(QStringLiteral("Quick Reference"), form, IDC_ST_HELP_HIGHLIGHT);
		help->setFrameShape(QFrame::StyledPanel);
		help->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		help->setWordWrap(true);
		layout->addWidget(help, 1);
		return form;
	}

	void consoleFilter(QDialog* form)
	{
		form->setWindowTitle(QStringLiteral("NppExec Console Filters"));
		auto* layout = new QVBoxLayout(form);
		auto* tabs = new QTabWidget(form);
		tabs->setObjectName(QStringLiteral("nppExecFilterTabs"));
		// In the order of the original's tabs
		tabs->addTab(outputFilterPage(tabs), QStringLiteral("Filter"));
		tabs->addTab(replaceFilterPage(tabs), QStringLiteral("Replace"));
		tabs->addTab(highlightFilterPage(tabs), QStringLiteral("Highlight"));
		layout->addWidget(tabs, 1);
		layout->addLayout(buttonRow({button(QStringLiteral("OK"), form, IDOK, true), button(QStringLiteral("Cancel"), form, IDCANCEL)}));
	}

	void advancedOptions(QDialog* form)
	{
		form->setWindowTitle(QStringLiteral("NppExec Advanced Options"));
		auto* layout = new QVBoxLayout(form);
		auto* columns = new QHBoxLayout;

		auto* left = new QVBoxLayout;
		auto* menuItems = withId(new QGroupBox(QStringLiteral("Menu items *"), form), IDC_GB_MENUITEMS);
		auto* menuLayout = new QVBoxLayout(menuItems);
		menuLayout->addWidget(check(QStringLiteral("Place to the Macros submenu"), menuItems, IDC_CH_MACROS_SUBMENU));
		menuLayout->addWidget(withId(new QListWidget(menuItems), IDC_LB_MENUITEMS), 1);
		auto* moves = new QHBoxLayout;
		moves->addWidget(button(QStringLiteral("Move up"), menuItems, IDC_BT_MOVEUP));
		moves->addStretch(1);
		moves->addWidget(button(QStringLiteral("Move down"), menuItems, IDC_BT_MOVEDOWN));
		menuLayout->addLayout(moves);
		auto* hint = label(QStringLiteral("* Assign shortcut keys using Settings:  Shortcut Mapper -> Plugin commands"), menuItems, IDC_ST_MENUITEMSHINT);
		hint->setAlignment(Qt::AlignHCenter);
		hint->setWordWrap(true);
		menuLayout->addWidget(hint);
		left->addWidget(menuItems, 1);
		auto* menuItem = withId(new QGroupBox(QStringLiteral("Menu item"), form), IDC_GB_MENUITEM);
		auto* itemLayout = new QVBoxLayout(menuItem);
		itemLayout->addWidget(label(QStringLiteral("Item name:"), menuItem, IDC_ST_ITEMNAME));
		itemLayout->addWidget(withId(new QLineEdit(menuItem), IDC_ED_ITEMNAME));
		itemLayout->addWidget(label(QStringLiteral("Associated script:"), menuItem, IDC_ST_ITEMSCRIPT));
		itemLayout->addWidget(combo(menuItem, IDC_CB_ITEMSCRIPT, false));
		auto* itemButtons = new QHBoxLayout;
		itemButtons->addWidget(button(QStringLiteral("Add/Modify"), menuItem, IDC_BT_ITEMNEW));
		itemButtons->addStretch(1);
		itemButtons->addWidget(button(QStringLiteral("Delete"), menuItem, IDC_BT_ITEMDELETE));
		itemLayout->addLayout(itemButtons);
		left->addWidget(menuItem);
		columns->addLayout(left, 1);

		auto* options = withId(new QGroupBox(QStringLiteral("Advanced options"), form), IDC_GB_ADVANCEDOPTIONS);
		auto* optionsLayout = new QVBoxLayout(options);
		optionsLayout->addWidget(label(QStringLiteral("Execute this script when Notepad++ starts:"), options, IDC_ST_OPT_SCRIPTNPPSTART));
		optionsLayout->addWidget(combo(options, IDC_CB_OPT_SCRIPTNPPSTART, false));
		optionsLayout->addWidget(label(QStringLiteral("Execute this script when Notepad++ exits:"), options, IDC_ST_OPT_SCRIPTNPPEXIT));
		optionsLayout->addWidget(combo(options, IDC_CB_OPT_SCRIPTNPPEXIT, false));

		const auto optionRow = [](QGroupBox* group, QGridLayout* grid, int row, const QString& text, int labelId, QWidget* control) {
			grid->addWidget(label(text, group, labelId), row, 0);
			grid->addWidget(control, row, 1);
		};
		auto* optionsGroup = withId(new QGroupBox(QStringLiteral("[Options]"), options), IDC_GB_OPT_OPTIONS);
		auto* optionsGrid = new QGridLayout(optionsGroup);
		optionRow(optionsGroup, optionsGrid, 0, QStringLiteral("HotKey"), IDC_ST_OPT_HOTKEY, combo(optionsGroup, IDC_CB_OPT_HOTKEY, false));
		optionRow(optionsGroup, optionsGrid, 1, QStringLiteral("ToolbarBtn"), IDC_ST_OPT_TOOLBARBTN, combo(optionsGroup, IDC_CB_OPT_TOOLBARBTN, false));
		optionsGrid->setColumnStretch(0, 1);
		optionsLayout->addWidget(optionsGroup);

		auto* consoleGroup = withId(new QGroupBox(QStringLiteral("[Console]"), options), IDC_GB_OPT_CONSOLE);
		auto* consoleGrid = new QGridLayout(consoleGroup);
		optionRow(consoleGroup, consoleGrid, 0, QStringLiteral("Visible (on start)"), IDC_ST_OPT_CONVISIBLE, combo(consoleGroup, IDC_CB_OPT_CONVISIBLE, false));
		optionRow(consoleGroup, consoleGrid, 1, QStringLiteral("ShowHelp (on start)"), IDC_ST_OPT_SHOWHELP, combo(consoleGroup, IDC_CB_OPT_SHOWHELP, false));
		optionRow(consoleGroup, consoleGrid, 2, QStringLiteral("SaveCmdHistory"), IDC_ST_OPT_SAVECMDHST, combo(consoleGroup, IDC_CB_OPT_SAVECMDHST, false));
		auto* delimiter = withId(new QLineEdit(consoleGroup), IDC_ED_OPT_COMMENTDELIM);
		delimiter->setMaximumWidth(consoleGroup->fontMetrics().averageCharWidth() * 10);
		optionRow(consoleGroup, consoleGrid, 3, QStringLiteral("CommentDelimiter"), IDC_ST_OPT_COMMENTDELIM, delimiter);
		optionRow(consoleGroup, consoleGrid, 4, QStringLiteral("TextColorNormal"), IDC_ST_OPT_TEXTCOLORNORM,
			withId(nppExecCreateColorButton(consoleGroup), IDC_ED_OPT_TEXTCOLORNORM));
		optionRow(consoleGroup, consoleGrid, 5, QStringLiteral("TextColorError"), IDC_ST_OPT_TEXTCOLORERR,
			withId(nppExecCreateColorButton(consoleGroup), IDC_ED_OPT_TEXTCOLORERR));
		optionRow(consoleGroup, consoleGrid, 6, QStringLiteral("TextColorMessage"), IDC_ST_OPT_TEXTCOLORMSG,
			withId(nppExecCreateColorButton(consoleGroup), IDC_ED_OPT_TEXTCOLORMSG));
		optionRow(consoleGroup, consoleGrid, 7, QStringLiteral("BackgroundColor"), IDC_ST_OPT_BKCOLOR,
			withId(nppExecCreateColorButton(consoleGroup), IDC_ED_OPT_BKCOLOR));
		consoleGrid->addWidget(check(QStringLiteral("Use Editor colors in the Console"), consoleGroup, IDC_CH_OPT_USEEDITORCOLORS_CONSOLE), 8, 0, 1, 2);
		consoleGrid->addWidget(check(QStringLiteral("Use Editor colors in the \"Execute\" dialog"), consoleGroup, IDC_CH_OPT_USEEDITORCOLORS_EXECDLG), 9, 0, 1, 2);
		consoleGrid->setColumnStretch(0, 1);
		optionsLayout->addWidget(consoleGroup);
		optionsLayout->addStretch(1);
		columns->addWidget(options, 1);

		layout->addLayout(columns, 1);
		layout->addLayout(buttonRow({button(QStringLiteral("OK"), form, IDOK, true), button(QStringLiteral("Cancel"), form, IDCANCEL)}));
	}

	void inputBox(QDialog* form)
	{
		form->setWindowTitle(QStringLiteral("NppExec InputBox"));
		auto* layout = new QVBoxLayout(form);
		layout->addWidget(label(QString(), form, IDC_ST_VARMESSAGE));
		layout->addWidget(line(form));
		auto* row = new QHBoxLayout;
		auto* name = label(QStringLiteral("$(INPUT) ="), form, IDC_ST_VARNAME);
		name->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		row->addWidget(name);
		auto* value = combo(form, IDC_CB_VARVALUE, true);
		value->setMinimumContentsLength(50);
		row->addWidget(value, 1);
		layout->addLayout(row);
		layout->addWidget(line(form));
		layout->addLayout(buttonRow({button(QStringLiteral("Kill"), form, IDC_BT_KILL), button(QStringLiteral("OK"), form, IDOK, true),
			button(QStringLiteral("Cancel"), form, IDCANCEL)}));
	}

	void consoleEncoding(QDialog* form)
	{
		form->setWindowTitle(QStringLiteral("Console Output/Input encoding"));
		auto* layout = new QVBoxLayout(form);
		auto* groups = new QHBoxLayout;
		const auto encodingGroup = [form](const QString& title, int ansi, int oem, int utf8) {
			auto* group = new QGroupBox(title, form);
			auto* v = new QVBoxLayout(group);
			v->addWidget(withId(new QRadioButton(QStringLiteral("ANSI (Windows)"), group), ansi));
			v->addWidget(withId(new QRadioButton(QStringLiteral("OEM (DOS)"), group), oem));
			v->addWidget(withId(new QRadioButton(QStringLiteral("UTF-8"), group), utf8));
			return group;
		};
		groups->addWidget(encodingGroup(QStringLiteral("Console Output is..."), IDC_RB_OUTPUT_ANSI, IDC_RB_OUTPUT_OEM, IDC_RB_OUTPUT_UTF8));
		groups->addWidget(encodingGroup(QStringLiteral("Console Input is..."), IDC_RB_INPUT_ANSI, IDC_RB_INPUT_OEM, IDC_RB_INPUT_UTF8));
		layout->addLayout(groups);
		layout->addWidget(check(QStringLiteral("Console Input is the same as Output"), form, IDC_CH_INPUT_AS_OUTPUT));
		layout->addLayout(buttonRow({button(QStringLiteral("OK"), form, IDOK, true), button(QStringLiteral("Cancel"), form, IDCANCEL)}));
	}
}

QWidget* nppExecBuildForm(int idDlg, QWidget* parent)
{
	auto* form = qobject_cast<QDialog*>(parent);
	if (!form)
		return nullptr;
	form->setProperty("nppForm", true);
	switch (idDlg)
	{
		case IDD_DOEXEC: doExec(form); break;
		case IDD_SCRIPTNAME: scriptName(form); break;
		case IDD_CONSOLE: console(form); break;
		case IDD_HELP_ABOUT: helpAbout(form); break;
		case IDD_CONSOLE_FILTER: consoleFilter(form); break;
		case IDD_ADVANCEDOPTIONS: advancedOptions(form); break;
		case IDD_INPUTBOX: inputBox(form); break;
		case IDD_CONSOLE_ENCODING: consoleEncoding(form); break;
		default: return nullptr;
	}
	return form;
}
