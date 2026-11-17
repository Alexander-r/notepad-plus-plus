// The plugins that come with Notepad++, run in it as a user runs them: from beside the program, through the Plugins
// menu, with their dialogs and panels.
//
// Set NPP_TEST_PLUGINS_DIR=dir for pictures of the plugins' panels. Set NPP_TEST_CONVERTER_INI=1 to start with a
// converter.ini of the user's own, in place of the one that Converter writes the first time.

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAction>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTextBlock>

// NppExec's interface for other plugins: see NppExec/src/PluginCommunication/nppexec_msgs.h
constexpr long NPEM_GETSTATE = 0x0301;
constexpr unsigned int NPE_STATEREADY = 0x01;
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <functional>
#include <map>
#include <memory>
#include <string>

#include <ScintillaEditBase.h>

#include "Notepad_plus_Window.h"
#include "Notepad_plus_msgs.h"
#include "NppControls.h"
#include "NppDarkMode.h"
#include "NppMessageFilter.h"
#include "Parameters.h"
#include "menuCmdID.h"

// The IDs of the Conversion Panel's controls, from a resource.h of the plugin's, whose version is not the program's
#undef VERSION_VALUE
#undef VERSION_DIGITALVALUE
#include "../NppConverter/resource.h"

std::chrono::steady_clock::time_point g_nppStartTimePoint{};

namespace
{
	// What Converter writes the first time, byte for byte as the original does
	const QByteArray converterIniContent =
		"; This section contains the parameters for command ASCII -> Hex\n"
		"; If you modify directly this file, please restart your Notepad++ to take effect.\n"
		"; * insertSpace: this parameter allows you to insert a white space between the generated hex codes. Set the value to 1 to enable it, 0 otherwise.\n"
		"; * uppercase: this parameter allows you to make a-f in UPPERCASE (ie. A-F). Set the value to 1 to enable it , 0 otherwise.\n"
		"; * nbCharPerLine: this parameter allows you to break line. The value you set is the number of ascii character per line. Set the value from 0 to whatever you want.\n"
		"[ascii2Hex]\n"
		"insertSpace=0\n"
		"uppercase=1\n"
		"nbCharPerLine=16\n"
		"\n";

	// A user's own, with names in another case, which Windows does not mind, and a comment in the section
	const QByteArray userConverterIni =
		"[ascii2Hex]\n"
		"; spaced out\n"
		"INSERTSPACE=1\n"
		"uppercase = 0\n"
		"nbCharPerLine=4 characters\n";

	QByteArray readFile(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

	bool writeFile(const QString& path, const QByteArray& content)
	{
		QFile file(path);
		return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
	}
}

class PluginsTest : public QObject
{
	Q_OBJECT

private:
	QTemporaryDir _home;
	QTemporaryDir _files;
	std::unique_ptr<Notepad_plus_Window> _npp;
	CmdLineParams _params;
	HWND _main = nullptr;
	HWND _sub = nullptr;
	const bool _hasUserIni = qEnvironmentVariableIsSet("NPP_TEST_CONVERTER_INI");

	HWND window() const { return _npp->getHSelf(); }
	QMainWindow* mainWindow() const { return _npp->mainWindow(); }

	LRESULT send(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) const
	{
		return ::SendMessage(window(), message, wParam, lParam);
	}

	void command(int id) const
	{
		send(WM_COMMAND, id);
		QCoreApplication::processEvents();
	}

	HWND currentEditor() const
	{
		int which = -1;
		send(NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&which));
		return (which == 0) ? _main : _sub;
	}

	LRESULT sci(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) const
	{
		return ::SendMessage(currentEditor(), message, wParam, lParam);
	}

	QByteArray editorText() const
	{
		const auto length = static_cast<qsizetype>(sci(SCI_GETLENGTH));
		QByteArray text(length + 1, '\0');
		sci(SCI_GETTEXT, static_cast<WPARAM>(length + 1), reinterpret_cast<LPARAM>(text.data()));
		text.resize(length);
		return text;
	}

	QByteArray selectedText() const
	{
		const auto start = sci(SCI_GETSELECTIONSTART);
		const auto end = sci(SCI_GETSELECTIONEND);
		return editorText().mid(start, end - start);
	}

	// A new document with the text, all of it selected
	void newDocument(const QByteArray& text, int eolMode = SC_EOL_LF) const
	{
		command(IDM_FILE_NEW);
		sci(SCI_SETEOLMODE, eolMode);
		sci(SCI_ADDTEXT, static_cast<WPARAM>(text.size()), reinterpret_cast<LPARAM>(text.constData()));
		sci(SCI_SELECTALL);
	}

	QString currentPath() const
	{
		wchar_t path[MAX_PATH]{};
		send(NPPM_GETFULLCURRENTPATH, MAX_PATH, reinterpret_cast<LPARAM>(path));
		return QString::fromWCharArray(path);
	}

	QString pluginsConfigDir() const
	{
		wchar_t dir[MAX_PATH]{};
		send(NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(dir));
		return QString::fromWCharArray(dir);
	}

	// The menu of a plugin in the Plugins menu
	QMenu* pluginMenu(const QString& name) const
	{
		const QList<QAction*> menus = mainWindow()->menuBar()->actions();
		for (const QAction* menu : menus)
		{
			if (!menu->menu() || menu->text().remove(u'&') != QStringLiteral("Plugins"))
				continue;
			const QList<QAction*> plugins = menu->menu()->actions();
			for (const QAction* plugin : plugins)
			{
				if (plugin->menu() && plugin->text().remove(u'&') == name)
					return plugin->menu();
			}
		}
		return nullptr;
	}

	static QStringList itemsOf(const QMenu* menu)
	{
		QStringList names;
		const QList<QAction*> items = menu->actions();
		for (const QAction* item : items)
			names << (item->isSeparator() ? QStringLiteral("---") : item->text().remove(u'&'));
		return names;
	}

	void trigger(const QString& plugin, const QString& item) const
	{
		QMenu* menu = pluginMenu(plugin);
		QVERIFY2(menu, qPrintable(plugin));
		QAction* action = actionOf(menu, item); // also in a submenu
		QVERIFY2(action, qPrintable(plugin + QStringLiteral(": no ") + item));
		action->trigger();
		QCoreApplication::processEvents();
	}

	// The modal dialog that the trigger opens is handed to the step, and closed if the step leaves it open
	void whileModal(const std::function<void(QWidget*)>& step, const std::function<void()>& trigger) const
	{
		bool hasRun = false;
		QTimer::singleShot(0, mainWindow(), [&step, &hasRun]()
		{
			QWidget* modal = QApplication::activeModalWidget();
			if (!modal)
				return;
			hasRun = true;
			step(modal);
			if (QApplication::activeModalWidget() == modal)
				modal->close();
		});
		trigger();
		QVERIFY2(hasRun, "no modal dialog came");
	}

	// As whileModal, for a dialog that comes later than the trigger returns, as those that NppExec opens from its threads
	void whenModal(const std::function<void(QWidget*)>& step, const std::function<void()>& trigger) const
	{
		bool hasRun = false;
		QTimer poll;
		QObject::connect(&poll, &QTimer::timeout, &poll, [&poll, &step, &hasRun]()
		{
			QWidget* modal = QApplication::activeModalWidget();
			if (!modal || hasRun)
				return;
			hasRun = true;
			poll.stop();
			step(modal);
			if (QApplication::activeModalWidget() == modal)
				modal->close();
		});
		poll.start(10);
		trigger();
		QTRY_VERIFY2_WITH_TIMEOUT(hasRun, "no modal dialog came", 10000);
	}

	QString messageOf(const std::function<void()>& trigger) const
	{
		QString message;
		whileModal([&message](QWidget* modal)
		{
			auto* box = qobject_cast<QMessageBox*>(modal);
			QVERIFY(box);
			message = box->windowTitle() + u'|' + box->text();
			box->accept();
		}, trigger);
		return message;
	}

	// Exports to the file through the plugin's save dialog
	void exportTo(const QString& item, const QString& path) const
	{
		whileModal([&path](QWidget* modal)
		{
			auto* dialog = qobject_cast<QFileDialog*>(modal);
			QVERIFY(dialog);
			QCOMPARE(dialog->acceptMode(), QFileDialog::AcceptSave);
			dialog->selectFile(path);
			static_cast<QDialog*>(dialog)->accept();
		}, [&]() { trigger(QStringLiteral("NppExport"), item); });
	}

	QDockWidget* dockTitled(const QString& title) const
	{
		const QList<QDockWidget*> docks = mainWindow()->findChildren<QDockWidget*>();
		for (QDockWidget* dock : docks)
		{
			if (dock->windowTitle() == title)
				return dock;
		}
		return nullptr;
	}

	static QWidget* control(QWidget* panel, int id)
	{
		const QList<QWidget*> controls = panel->findChildren<QWidget*>();
		for (QWidget* widget : controls)
		{
			if (widget->property("nppId").toInt() == id)
				return widget;
		}
		return nullptr;
	}

	static QString textOf(QWidget* panel, int id)
	{
		QWidget* widget = control(panel, id);
		if (auto* edit = qobject_cast<QLineEdit*>(widget))
			return edit->text();
		if (auto* label = qobject_cast<QLabel*>(widget))
			return label->text();
		return QStringLiteral("<no control %1>").arg(id);
	}

	// Typed into the field as a user does, after emptying it
	static void type(QWidget* panel, int id, const QString& text)
	{
		auto* edit = qobject_cast<QLineEdit*>(control(panel, id));
		QVERIFY(edit);
		edit->clear();
		QTest::keyClicks(edit, text);
	}

	static void click(QWidget* panel, int id)
	{
		auto* button = qobject_cast<QAbstractButton*>(control(panel, id));
		QVERIFY(button);
		button->click();
		QCoreApplication::processEvents();
	}

private slots:
	void initTestCase()
	{
		nppInstallMessageFilter(); // as winmain does
		QVERIFY(_home.isValid());
		QVERIFY(_files.isValid());
		qputenv("HOME", _home.path().toUtf8());
		qputenv("XDG_CONFIG_HOME", (_home.path() + QStringLiteral("/.config")).toUtf8());

		// A small English dictionary for DSpellCheck, where a user's dictionaries go, under the name of its default language
		{
			const QString hunspellDir = _home.path() + QStringLiteral("/.config/notepad++/plugins/Config/Hunspell");
			QVERIFY(QDir().mkpath(hunspellDir));
			for (const char* file : { "en_GB.aff", "en_GB.dic" })
				QVERIFY(QFile::copy(QStringLiteral(NPP_PLUGINS_TEST_DATA "/hunspell/") + QLatin1String(file), hunspellDir + u'/' + QLatin1String(file)));
		}

		if (_hasUserIni)
		{
			const QString configDir = _home.path() + QStringLiteral("/.config/notepad++/plugins/Config");
			QVERIFY(QDir().mkpath(configDir));
			QVERIFY(writeFile(configDir + QStringLiteral("/converter.ini"), userConverterIni));
		}

		// The menu bar stays in the window, where the tests find the plugins, and the file dialogs are Qt's, which the
		// tests can answer, whatever the desktop offers
		QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
		QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);

		NppParameters::getInstance().load();
		// Only the plugins of this build, whatever the system has installed
		NppParameters::getInstance().setSystemPluginRootDir(std::wstring());
		NppDarkMode::initDarkMode();
		_npp = std::make_unique<Notepad_plus_Window>();
		_npp->init(nullptr, nullptr, L"", &_params);
		mainWindow()->resize(1000, 700);
		QCoreApplication::processEvents();

		const QList<ScintillaEditBase*> editors = mainWindow()->centralWidget()->findChildren<ScintillaEditBase*>();
		QCOMPARE(editors.size(), 2);
		_main = editors[0]->isVisible() ? editors[0] : editors[1];
		_sub = (_main == editors[0]) ? editors[1] : editors[0];
	}

	// The three plugins of the Windows package, from bin/plugins beside the program, with their commands
	void bundledPlugins()
	{
		const QMenu* mimeTools = pluginMenu(QStringLiteral("MIME Tools"));
		QVERIFY(mimeTools);
		QCOMPARE(itemsOf(mimeTools).size(), qsizetype(22));

		const QMenu* converter = pluginMenu(QStringLiteral("Converter"));
		QVERIFY(converter);
		QCOMPARE(itemsOf(converter), QStringList({ QStringLiteral("ASCII -> HEX"), QStringLiteral("HEX -> ASCII"), QStringLiteral("---"),
			QStringLiteral("Conversion Panel"), QStringLiteral("---"), QStringLiteral("Edit Configuration File"), QStringLiteral("About") }));

		const QMenu* nppExport = pluginMenu(QStringLiteral("NppExport"));
		QVERIFY(nppExport);
		QCOMPARE(itemsOf(nppExport), QStringList({ QStringLiteral("Export to RTF"), QStringLiteral("Export to HTML"),
			QStringLiteral("Copy RTF to clipboard"), QStringLiteral("Copy HTML to clipboard"), QStringLiteral("Copy all formats to clipboard") }));
	}

	// A command of MIME Tools, which the app tests run all of
	void mimeTools()
	{
		newDocument("Hello, world!");
		trigger(QStringLiteral("MIME Tools"), QStringLiteral("Base64 Encode with padding"));
		QCOMPARE(editorText(), QByteArray("SGVsbG8sIHdvcmxkIQ=="));
		QVERIFY(messageOf([this]() { trigger(QStringLiteral("MIME Tools"), QStringLiteral("About")); }).contains(QStringLiteral("Version : 3.2")));
	}

	// Converter writes its settings the first time, and keeps a user's own
	void converterSettingsFile()
	{
		const QString path = pluginsConfigDir() + QStringLiteral("/converter.ini");
		QCOMPARE(readFile(path), _hasUserIni ? userConverterIni : converterIniContent);

		// Which the plugin opens to edit
		trigger(QStringLiteral("Converter"), QStringLiteral("Edit Configuration File"));
		QCOMPARE(currentPath(), path);
		command(IDM_FILE_CLOSE);
	}

	void asciiToHex()
	{
		if (_hasUserIni)
		{
			// Spaced out, in lower case, four characters to a line, after which there is no space
			newDocument("Hello, world");
			trigger(QStringLiteral("Converter"), QStringLiteral("ASCII -> HEX"));
			QCOMPARE(editorText(), QByteArray("48 65 6c 6c\n6f 2c 20 77\n6f 72 6c 64\n"));
			return;
		}

		// Upper case, sixteen characters to a line, with the document's line ending
		newDocument("Hello");
		trigger(QStringLiteral("Converter"), QStringLiteral("ASCII -> HEX"));
		QCOMPARE(editorText(), QByteArray("48656C6C6F"));
		QCOMPARE(selectedText(), QByteArray("48656C6C6F")); // and selected

		newDocument("abcdefghijklmnopqrst", SC_EOL_CRLF);
		trigger(QStringLiteral("Converter"), QStringLiteral("ASCII -> HEX"));
		QCOMPARE(editorText(), QByteArray("6162636465666768696A6B6C6D6E6F70\r\n71727374"));

		// Only the selection, byte by byte
		newDocument("x caf\xC3\xA9 y");
		sci(SCI_SETSEL, 2, 7);
		trigger(QStringLiteral("Converter"), QStringLiteral("ASCII -> HEX"));
		QCOMPARE(editorText(), QByteArray("x 636166C3A9 y"));
	}

	void hexToAscii()
	{
		const auto convert = [this](const QByteArray& hex)
		{
			newDocument(hex);
			trigger(QStringLiteral("Converter"), QStringLiteral("HEX -> ASCII"));
			return editorText();
		};
		QCOMPARE(convert("48656C6C6F"), QByteArray("Hello"));
		QCOMPARE(convert("48 65 6c 6c 6f"), QByteArray("Hello"));
		QCOMPARE(convert("4865\r\n6C6C\n6F"), QByteArray("Hello"));
		QCOMPARE(convert("636166C3A9"), QByteArray("caf\xC3\xA9"));
		QCOMPARE(selectedText(), QByteArray("caf\xC3\xA9"));

		// Text that is not hex, or mixes spaced and unspaced, is left as it is, with the plugin's message
		const QString message = QStringLiteral("Converter plugin|Hex format is not conformed");
		for (const QByteArray& wrong : { QByteArray("48 656C"), QByteArray("4G"), QByteArray("486"), QByteArray("48  65") })
		{
			newDocument(wrong);
			QCOMPARE(messageOf([this]() { trigger(QStringLiteral("Converter"), QStringLiteral("HEX -> ASCII")); }), message);
			QCOMPARE(editorText(), wrong);
		}
	}

	void conversionPanel()
	{
		newDocument("");
		trigger(QStringLiteral("Converter"), QStringLiteral("Conversion Panel"));
		QDockWidget* dock = dockTitled(QStringLiteral("Conversion"));
		QVERIFY(dock);
		QTRY_VERIFY(dock->isVisible());
		QWidget* panel = dock->widget();
		QVERIFY(panel);
		if (const QString dir = qEnvironmentVariable("NPP_TEST_PLUGINS_DIR"); !dir.isEmpty())
			dock->grab().save(dir + QStringLiteral("/conversion-panel.png"));
		QCOMPARE(panel->focusWidget(), control(panel, ID_ASCII_EDIT)); // the ASCII field has the focus in the panel

		// A number in one field shows in all the others
		type(panel, ID_DEC_EDIT, QStringLiteral("65"));
		QCOMPARE(textOf(panel, ID_ASCII_EDIT), QStringLiteral("A"));
		QCOMPARE(textOf(panel, ID_HEX_EDIT), QStringLiteral("41"));
		QCOMPARE(textOf(panel, ID_BIN_EDIT), QStringLiteral("1000001"));
		QCOMPARE(textOf(panel, ID_OCT_EDIT), QStringLiteral("101"));
		QCOMPARE(textOf(panel, ID_ASCII_INFO_STATIC), QString());

		type(panel, ID_ASCII_EDIT, QStringLiteral(" "));
		QCOMPARE(textOf(panel, ID_DEC_EDIT), QStringLiteral("32"));
		QCOMPARE(textOf(panel, ID_ASCII_INFO_STATIC), QStringLiteral("Space"));

		type(panel, ID_HEX_EDIT, QStringLiteral("12c"));
		QCOMPARE(textOf(panel, ID_DEC_EDIT), QStringLiteral("300"));
		QCOMPARE(textOf(panel, ID_BIN_EDIT), QStringLiteral("100101100"));
		QCOMPARE(textOf(panel, ID_OCT_EDIT), QStringLiteral("454"));
		QCOMPARE(textOf(panel, ID_ASCII_EDIT), QString()); // above 255, no character
		QCOMPARE(textOf(panel, ID_ASCII_INFO_STATIC), QString());

		// A character that does not belong is taken back, and the number fields take digits only
		type(panel, ID_HEX_EDIT, QStringLiteral("7g"));
		QCOMPARE(textOf(panel, ID_HEX_EDIT), QStringLiteral("7"));
		QCOMPARE(textOf(panel, ID_DEC_EDIT), QStringLiteral("7"));
		QCOMPARE(textOf(panel, ID_ASCII_INFO_STATIC), QStringLiteral("BEL"));
		type(panel, ID_BIN_EDIT, QStringLiteral("102"));
		QCOMPARE(textOf(panel, ID_BIN_EDIT), QStringLiteral("10"));
		QCOMPARE(textOf(panel, ID_DEC_EDIT), QStringLiteral("2"));
		type(panel, ID_OCT_EDIT, QStringLiteral("1x7"));
		QCOMPARE(textOf(panel, ID_OCT_EDIT), QStringLiteral("17"));
		QCOMPARE(textOf(panel, ID_DEC_EDIT), QStringLiteral("15"));

		// Emptying one field empties the others, but not the name of the character, as in the original
		type(panel, ID_DEC_EDIT, QString());
		for (const int id : { ID_ASCII_EDIT, ID_HEX_EDIT, ID_BIN_EDIT, ID_OCT_EDIT })
			QCOMPARE(textOf(panel, id), QString());
		QCOMPARE(textOf(panel, ID_ASCII_INFO_STATIC), QStringLiteral("SI"));

		// Insert puts the field's text in place of the selection, and for ASCII the byte
		type(panel, ID_DEC_EDIT, QStringLiteral("233"));
		click(panel, ID_HEX_INSERT_BUTTON);
		QCOMPARE(editorText(), QByteArray("E9"));
		sci(SCI_SELECTALL);
		click(panel, ID_BIN_INSERT_BUTTON);
		QCOMPARE(editorText(), QByteArray("11101001"));
		sci(SCI_SELECTALL);
		click(panel, ID_ASCII_INSERT_BUTTON);
		QCOMPARE(editorText(), QByteArray("\xE9"));

		// Copy puts the bytes where Paste Binary Content finds them, and the text for other applications
		const QString binaryFormat = QStringLiteral("application/x-notepad-plus-plus-binary");
		type(panel, ID_DEC_EDIT, QStringLiteral("65"));
		click(panel, ID_OCT_BUTTON);
		QCOMPARE(QApplication::clipboard()->mimeData()->data(binaryFormat), QByteArray("101"));
		QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("101"));
		type(panel, ID_DEC_EDIT, QStringLiteral("0"));
		click(panel, ID_ASCII_BUTTON);
		QCOMPARE(QApplication::clipboard()->mimeData()->data(binaryFormat), QByteArray(1, '\0'));
		newDocument("");
		command(IDM_EDIT_PASTE_BINARY);
		QCOMPARE(editorText(), QByteArray(1, '\0'));

		// The command shows the panel again once it is closed
		dock->close();
		QTRY_VERIFY(!dock->isVisible());
		trigger(QStringLiteral("Converter"), QStringLiteral("Conversion Panel"));
		QTRY_VERIFY(dock->isVisible());
		QCOMPARE(dockTitled(QStringLiteral("Conversion")), dock); // the same panel, not a second one
	}

	void converterAbout()
	{
		QCOMPARE(messageOf([this]() { trigger(QStringLiteral("Converter"), QStringLiteral("About")); }),
			QStringLiteral("Converter Plugin|Version: 4.7\n\nLicense: GPL\n\nAuthor: Don Ho <don.h@free.fr>\n"));
	}

	// HTML and RTF of the document, with its styles, the text escaped as each format needs it. data/ has what the
	// original writes for the same document on Windows, byte for byte, except in one place: see exported.rtf.
	void exportToFiles()
	{
		newDocument("int a < b; // caf\xC3\xA9 {x}\n\tz & \\", SC_EOL_LF);
		send(NPPM_SETCURRENTLANGTYPE, 0, L_CPP);
		sci(SCI_SETSEL, 0, 0); // no selection: the whole document

		const QString htmlPath = _files.filePath(QStringLiteral("out.html"));
		exportTo(QStringLiteral("Export to HTML"), htmlPath);
		QCOMPARE(readFile(htmlPath), readFile(QStringLiteral(NPP_PLUGINS_TEST_DATA "/exported.html")));

		// The tab stops are four spaces of the default font in twips, which depends on the font installed, at 15 twips
		// a pixel at 96 DPI
		const QString rtfPath = _files.filePath(QStringLiteral("out.rtf"));
		exportTo(QStringLiteral("Export to RTF"), rtfPath);
		QByteArray rtf = readFile(rtfPath);
		const QRegularExpression deftab(QStringLiteral("\\\\deftab(\\d+)"));
		const QRegularExpressionMatch match = deftab.match(QString::fromLatin1(rtf));
		QVERIFY2(match.hasMatch(), rtf.constData());
		const int twips = match.captured(1).toInt();
		QVERIFY2(twips > 0 && twips % 15 == 0, rtf.left(40).constData());
		rtf.replace(QByteArray("\\deftab") + match.captured(1).toLatin1(), "\\deftab480");
		QCOMPARE(rtf, readFile(QStringLiteral(NPP_PLUGINS_TEST_DATA "/exported.rtf")));

		// Nothing is written without a file
		whileModal([](QWidget* modal) { qobject_cast<QFileDialog*>(modal)->reject(); },
			[this]() { trigger(QStringLiteral("NppExport"), QStringLiteral("Export to HTML")); });
	}

	void exportToClipboard()
	{
		newDocument("a < b\nc", SC_EOL_LF);
		sci(SCI_SETSEL, 0, 0);

		trigger(QStringLiteral("NppExport"), QStringLiteral("Copy HTML to clipboard"));
		const QMimeData* data = QApplication::clipboard()->mimeData();
		QVERIFY(data->hasFormat(QStringLiteral("text/html")));
		const QByteArray html = data->data(QStringLiteral("text/html"));
		QVERIFY2(html.startsWith("<!DOCTYPE html"), html.constData()); // no CF_HTML header, which is for Windows
		QVERIFY(html.contains("a &lt; b"));
		QVERIFY(!data->hasFormat(QStringLiteral("text/rtf")));

		trigger(QStringLiteral("NppExport"), QStringLiteral("Copy RTF to clipboard"));
		data = QApplication::clipboard()->mimeData();
		QVERIFY(data->data(QStringLiteral("text/rtf")).startsWith("{\\rtf1"));
		QCOMPARE(data->data(QStringLiteral("text/richtext")), data->data(QStringLiteral("text/rtf")));
		QVERIFY(!data->hasFormat(QStringLiteral("text/html")));

		// The selection only, in every format at once
		sci(SCI_SETSEL, 0, 5);
		trigger(QStringLiteral("NppExport"), QStringLiteral("Copy all formats to clipboard"));
		data = QApplication::clipboard()->mimeData();
		QVERIFY(data->data(QStringLiteral("text/rtf")).contains("a < b"));
		QVERIFY(data->data(QStringLiteral("text/html")).contains("a &lt; b"));
		QVERIFY(!data->data(QStringLiteral("text/html")).contains("\nc"));
		QCOMPARE(data->text(), QStringLiteral("a < b"));
	}

private:
	// DSpellCheck: the misspellings of the document underlined with its indicator, once its dictionary has loaded
	static constexpr int spellCheckIndicator = 19;

	bool isMarked(int position) const
	{
		return sci(SCI_INDICATORVALUEAT, spellCheckIndicator, position) != 0;
	}

	QAction* actionOf(QMenu* menu, const QString& text) const
	{
		const QList<QAction*> actions = menu->actions();
		for (QAction* action : actions)
		{
			if (action->text().remove(u'&') == text)
				return action;
			if (action->menu())
			{
				if (QAction* found = actionOf(action->menu(), text))
					return found;
			}
		}
		return nullptr;
	}

private slots:
	void dspellcheckMenu()
	{
		QMenu* menu = pluginMenu(QStringLiteral("DSpellCheck"));
		QVERIFY(menu);
		for (const char* item : { "Spell Check Document Automatically", "Find Next Misspelling", "Find Previous Misspelling", "Change Current Language",
			"Settings...", "Online Manual", "About" })
			QVERIFY2(actionOf(menu, QString::fromLatin1(item)), item);

		// Moved into a submenu when Notepad++ is ready, as on Windows
		QAction* additional = actionOf(menu, QStringLiteral("Additional Actions"));
		QVERIFY(additional && additional->menu());
		QVERIFY(actionOf(additional->menu(), QStringLiteral("Copy All Misspelled Words to Clipboard")));
		QVERIFY(actionOf(additional->menu(), QStringLiteral("Bookmark Lines with Misspelling")));
		QVERIFY(!menu->actions().contains(actionOf(additional->menu(), QStringLiteral("Erase All Misspelled Words"))));

		// The dictionary of the user's folder, chosen in the languages' submenu
		QAction* languages = actionOf(menu, QStringLiteral("Change Current Language"));
		QTRY_VERIFY(languages->menu());
		QAction* english = actionOf(languages->menu(), QStringLiteral("English (United Kingdom)"));
		QVERIFY2(english, qPrintable(itemsOf(languages->menu()).join(u',')));
		QVERIFY(english->isChecked());
		QVERIFY(actionOf(languages->menu(), QStringLiteral("Download More Languages...")));
	}

	void dspellcheckMarksMisspellings()
	{
		newDocument("hello wrold this is a tset");
		QTRY_VERIFY_WITH_TIMEOUT(isMarked(6), 5000); // wrold
		QVERIFY(isMarked(10));
		QVERIFY(!isMarked(0)); // hello
		QVERIFY(!isMarked(12)); // this
		QVERIFY(isMarked(23)); // tset

		// Typing a correction takes the mark away
		sci(SCI_SETSEL, 6, 11);
		sci(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("world"));
		QTRY_VERIFY_WITH_TIMEOUT(!isMarked(6), 3000);
		QVERIFY(isMarked(23));

		// Copy All Misspelled Words
		trigger(QStringLiteral("DSpellCheck"), QStringLiteral("Copy All Misspelled Words to Clipboard"));
		QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("tset\n"));

		// Switching automatic checking off erases the marks
		trigger(QStringLiteral("DSpellCheck"), QStringLiteral("Spell Check Document Automatically"));
		QTRY_VERIFY(!isMarked(23));
		trigger(QStringLiteral("DSpellCheck"), QStringLiteral("Spell Check Document Automatically"));
		QTRY_VERIFY_WITH_TIMEOUT(isMarked(23), 3000);
	}

private:
	// The editor's context menu with the suggestions for the word under the mouse, as Notepad++ makes it
	QStringList contextMenuAt(int position, const std::function<void(QMenu*)>& use) const
	{
		HWND editor = currentEditor();
		sci(SCI_SETEMPTYSELECTION, 0); // a right click in the selection keeps it, and the caret where it is
		auto* scrollArea = qobject_cast<QAbstractScrollArea*>(editor);
		const QPoint inViewport(static_cast<int>(sci(SCI_POINTXFROMPOSITION, 0, position)) + 2,
			static_cast<int>(sci(SCI_POINTYFROMPOSITION, 0, position)) + static_cast<int>(sci(SCI_TEXTHEIGHT, 0)) / 2);
		const QPoint point = scrollArea->viewport()->mapTo(editor, inViewport);
		QStringList items;
		QTimer::singleShot(0, mainWindow(), [&]() {
			auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			if (!menu)
				return;
			items = itemsOf(menu);
			use(menu);
			if (QApplication::activePopupWidget() == menu)
				menu->close();
		});
		QContextMenuEvent event(QContextMenuEvent::Mouse, point, editor->mapToGlobal(point));
		QApplication::sendEvent(editor, &event);
		QCoreApplication::processEvents();
		return items;
	}

private slots:
	void dspellcheckContextMenu()
	{
		newDocument("hello wrold this");
		QTRY_VERIFY_WITH_TIMEOUT(isMarked(6), 5000);

		const auto click = [this](const QString& text)
		{
			return [this, text](QMenu* menu)
			{
				if (QAction* action = actionOf(menu, text))
					action->trigger();
			};
		};
		const QStringList items = contextMenuAt(8, click(QStringLiteral("world")));
		QVERIFY2(items.size() > 6, qPrintable(items.join(u'|')));
		QCOMPARE(items.mid(0, 6), QStringList({ QStringLiteral("world"), QStringLiteral("---"), QStringLiteral("Replace All \"wrold\" with"),
			QStringLiteral("Ignore \"wrold\" for Current Session"), QStringLiteral("Add \"wrold\" to Dictionary"), QStringLiteral("---") }));
		QVERIFY(items.contains(QStringLiteral("Cut"))); // Notepad++'s own items follow
		QCOMPARE(editorText(), QByteArray("hello world this"));

		// A correct word adds nothing
		const QStringList correct = contextMenuAt(1, [](QMenu*) {});
		QVERIFY2(!correct.isEmpty() && correct.first() != QStringLiteral("world"), qPrintable(correct.join(u'|')));
		QVERIFY(!correct.contains(QStringLiteral("Add \"hello\" to Dictionary")));

		// Ignore for the session, and Add to Dictionary, which writes the user's dictionary
		newDocument("zorp blix");
		QTRY_VERIFY_WITH_TIMEOUT(isMarked(0) && isMarked(5), 5000);
		contextMenuAt(1, click(QStringLiteral("Ignore \"zorp\" for Current Session")));
		QTRY_VERIFY(!isMarked(0));
		QVERIFY(isMarked(5));
		contextMenuAt(6, click(QStringLiteral("Add \"blix\" to Dictionary")));
		QTRY_VERIFY(!isMarked(5));
		const QString userDictionary = pluginsConfigDir() + QStringLiteral("/Hunspell/en_GB.usr");
		QTRY_VERIFY(readFile(userDictionary).contains("blix\n"));
	}

	void dspellcheckSettings()
	{
		trigger(QStringLiteral("DSpellCheck"), QStringLiteral("Settings..."));
		QWidget* dialog = nullptr;
		for (QWidget* widget : QApplication::topLevelWidgets())
		{
			if (widget->isVisible() && widget->windowTitle() == QStringLiteral("DSpellCheck Settings"))
				dialog = widget;
		}
		QVERIFY(dialog);
		if (const QString dir = qEnvironmentVariable("NPP_TEST_PLUGINS_DIR"); !dir.isEmpty())
			dialog->grab().save(dir + QStringLiteral("/dspellcheck-settings.png"));
		auto* language = qobject_cast<QComboBox*>(control(dialog, 1005)); // IDC_COMBO_LANGUAGE
		QVERIFY(language);
		QCOMPARE(language->currentText(), QStringLiteral("English (United Kingdom)"));
		auto* library = qobject_cast<QComboBox*>(control(dialog, 1003)); // IDC_LIBRARY
		QVERIFY(library);
		QStringList libraries;
		for (int i = 0; i < library->count(); ++i)
			libraries << library->itemText(i);
		QCOMPARE(libraries, QStringList({ QStringLiteral("Aspell"), QStringLiteral("Hunspell") })); // no Windows speller
		QCOMPARE(library->currentText(), QStringLiteral("Hunspell"));

		// A setting goes to DSpellCheck.ini with OK
		auto* tabs = dialog->findChild<QTabWidget*>();
		QVERIFY(tabs);
		QCOMPARE(tabs->tabText(0), QStringLiteral("Simple"));
		QWidget* simple = tabs->widget(0); // as IDs are only unique within a page
		auto* suggestions = qobject_cast<QLineEdit*>(control(simple, 1001)); // IDC_SUGGESTIONS_NUM
		QVERIFY(suggestions);
		suggestions->setText(QStringLiteral("7"));
		qobject_cast<QAbstractButton*>(control(dialog, IDOK))->click();
		QVERIFY(!dialog->isVisible());
		QTRY_VERIFY(readFile(pluginsConfigDir() + QStringLiteral("/DSpellCheck.ini")).contains("Suggestions_Number=7"));
	}

private:
	// In place of GitHub, for DSpellCheck's downloads: each request of https://host/path comes as /host/path
	class TestServer : public QTcpServer
	{
	public:
		std::map<QByteArray, QByteArray> files;
		QStringList requests;

		TestServer()
		{
			QObject::connect(this, &QTcpServer::newConnection, this, [this]() {
				while (QTcpSocket* socket = nextPendingConnection())
				{
					QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
						const QByteArray request = socket->readAll();
						const QByteArray path = request.split(' ').value(1);
						requests << QString::fromLatin1(path);
						const auto found = files.find(path);
						const QByteArray body = found != files.end() ? found->second : QByteArray("{\"message\": \"Not Found\"}");
						socket->write((found != files.end() ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n") + QByteArray("Content-Length: ") +
							QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
						socket->disconnectFromHost();
					});
					QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
				}
			});
		}
	};

private slots:
	void dspellcheckDownload()
	{
		TestServer server;
		QVERIFY(server.listen(QHostAddress::LocalHost));
		server.files["/api.github.com/rate_limit"] = "{\"resources\": {\"core\": {\"limit\": 60, \"remaining\": 59, \"reset\": 0}}}";
		server.files["/api.github.com/repos/LibreOffice/dictionaries/git/trees/master?recursive=1"] =
			"{\"tree\": [{\"path\": \"fr_FR/fr.aff\", \"type\": \"blob\"}, {\"path\": \"fr_FR/fr.dic\", \"type\": \"blob\"},"
			" {\"path\": \"xx/alone.aff\", \"type\": \"blob\"}, {\"path\": \"fr_FR\", \"type\": \"tree\"}]}";
		server.files["/raw.githubusercontent.com/LibreOffice/dictionaries/master/fr_FR/fr.aff"] = "SET UTF-8\n";
		server.files["/raw.githubusercontent.com/LibreOffice/dictionaries/master/fr_FR/fr.dic"] = "2\nbonjour\nmonde\n";
		qputenv("NPP_TEST_DSPELLCHECK_SERVER", QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());

		trigger(QStringLiteral("DSpellCheck"), QStringLiteral("Download More Languages..."));
		QWidget* dialog = nullptr;
		QTRY_VERIFY((dialog = [](){
			for (QWidget* widget : QApplication::topLevelWidgets())
			{
				if (widget->isVisible() && widget->windowTitle() == QStringLiteral("Download Dictionaries Dialog"))
					return widget;
			}
			return static_cast<QWidget*>(nullptr);
		}()) != nullptr);
		auto* list = qobject_cast<QListWidget*>(control(dialog, 1010)); // IDC_FILE_LIST
		QVERIFY(list);
		QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1, 5000); // a pair of .aff and .dic only
		QCOMPARE(list->item(0)->text(), QStringLiteral("French"));
		QCOMPARE(textOf(dialog, 1011), QStringLiteral("Status: List of available files was successfully loaded")); // IDC_SERVER_STATUS
		if (const QString dir = qEnvironmentVariable("NPP_TEST_PLUGINS_DIR"); !dir.isEmpty())
			dialog->grab().save(dir + QStringLiteral("/dspellcheck-download.png"));

		auto* install = qobject_cast<QAbstractButton*>(control(dialog, IDOK));
		QVERIFY(!install->isEnabled()); // nothing chosen
		list->item(0)->setCheckState(Qt::Checked);
		QVERIFY(install->isEnabled());

		// The message at the end, from the GUI thread once both files have come, which opens with exec()
		QString message;
		QTimer answers;
		QObject::connect(&answers, &QTimer::timeout, &answers, [&message]()
		{
			if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
			{
				message = box->text();
				box->accept();
			}
		});
		answers.start(10);
		install->click();
		QTRY_VERIFY_WITH_TIMEOUT(!message.isEmpty(), 5000);
		answers.stop();
		QVERIFY2(message.contains(QStringLiteral("French")), qPrintable(message));
		const QString hunspellDir = pluginsConfigDir() + QStringLiteral("/Hunspell");
		QCOMPARE(readFile(hunspellDir + QStringLiteral("/fr.aff")), QByteArray("SET UTF-8\n"));
		QCOMPARE(readFile(hunspellDir + QStringLiteral("/fr.dic")), QByteArray("2\nbonjour\nmonde\n"));

		// And French among the languages
		QMenu* languages = actionOf(pluginMenu(QStringLiteral("DSpellCheck")), QStringLiteral("Change Current Language"))->menu();
		QTRY_VERIFY(languages && actionOf(languages, QStringLiteral("French")));
		qunsetenv("NPP_TEST_DSPELLCHECK_SERVER");
	}

private:
	// NppExec's console, shown through its menu
	QDockWidget* nppExecDock() const
	{
		for (QDockWidget* dock : mainWindow()->findChildren<QDockWidget*>())
		{
			if (qobject_cast<QPlainTextEdit*>(control(dock, 1002))) // IDC_RE_CONSOLE
				return dock;
		}
		return nullptr;
	}

	QPlainTextEdit* nppExecConsole() const
	{
		QDockWidget* dock = nppExecDock();
		if (!dock || !dock->isVisible())
		{
			trigger(QStringLiteral("NppExec"), QStringLiteral("Show NppExec Console"));
			dock = nppExecDock();
		}
		return dock ? qobject_cast<QPlainTextEdit*>(control(dock, 1002)) : nullptr;
	}

	// NppExec's state, through its interface for other plugins
	DWORD nppExecState() const
	{
		DWORD state = 0;
		CommunicationInfo info{NPEM_GETSTATE, L"pluginsTest", &state};
		send(NPPM_MSGTOPLUGIN, reinterpret_cast<WPARAM>(L"NppExec.so"), reinterpret_cast<LPARAM>(&info));
		return state;
	}

	// Types the command in the console and presses Enter, as a user does, once NppExec is ready, and waits for it to
	// be ready again
	void runInConsole(QPlainTextEdit* console, const QString& command) const
	{
		QTRY_COMPARE_WITH_TIMEOUT(nppExecState(), static_cast<DWORD>(NPE_STATEREADY), 10000);
		console->setFocus();
		console->moveCursor(QTextCursor::End);
		QTest::keyClicks(console, command);
		QTest::keyClick(console, Qt::Key_Return);
		QTRY_VERIFY_WITH_TIMEOUT(nppExecState() != NPE_STATEREADY || console->toPlainText().contains(command), 10000);
		QTRY_COMPARE_WITH_TIMEOUT(nppExecState(), static_cast<DWORD>(NPE_STATEREADY), 10000);
	}

private slots:
	void nppexecMenu()
	{
		const QStringList items = itemsOf(pluginMenu(QStringLiteral("NppExec")));
		QVERIFY2(items.contains(QStringLiteral("Execute NppExec Script...")) || items.contains(QStringLiteral("Execute Script...")),
			qPrintable(items.join(u'|')));
		QVERIFY2(items.contains(QStringLiteral("Show NppExec Console")), qPrintable(items.join(u'|')));
	}

	void nppexecConsoleCommand()
	{
		QPlainTextEdit* console = nppExecConsole();
		QVERIFY(console);
		QTRY_VERIFY(nppExecDock()->isVisible());
		runInConsole(console, QStringLiteral("echo hello from NppExec"));
		QTRY_VERIFY2(console->toPlainText().split(u'\n').contains(QStringLiteral("hello from NppExec")), qPrintable(console->toPlainText()));
	}

	void nppexecChildProcess()
	{
		QPlainTextEdit* console = nppExecConsole();
		QVERIFY(console);
		runInConsole(console, QStringLiteral("sh -c \"echo out; echo err 1>&2; exit 3\""));
		QTRY_VERIFY2_WITH_TIMEOUT(console->toPlainText().contains(QStringLiteral("<<< Process finished")), qPrintable(console->toPlainText()), 10000);
		const QStringList lines = console->toPlainText().split(u'\n');
		QVERIFY2(lines.contains(QStringLiteral("out")), qPrintable(console->toPlainText()));
		QVERIFY2(lines.contains(QStringLiteral("err")), qPrintable(console->toPlainText()));
		QVERIFY2(console->toPlainText().contains(QStringLiteral("(Exit code 3)")), qPrintable(console->toPlainText()));
	}

	void nppexecChildInput()
	{
		// What is typed while a program runs is its input
		QPlainTextEdit* console = nppExecConsole();
		QVERIFY(console);
		QTRY_COMPARE_WITH_TIMEOUT(nppExecState(), static_cast<DWORD>(NPE_STATEREADY), 10000);
		console->setFocus();
		console->moveCursor(QTextCursor::End);
		QTest::keyClicks(console, QStringLiteral("sh -c \"read x; echo got:$x\""));
		QTest::keyClick(console, Qt::Key_Return);
		QTRY_VERIFY_WITH_TIMEOUT(nppExecState() & 0x20, 10000); // NPE_STATECHILDPROC
		// As a user, who types once the program has started: NppExec prints that, and then moves the start of the
		// input past it in another call, which typing at once could come before
		QTRY_VERIFY_WITH_TIMEOUT(console->toPlainText().contains(QStringLiteral("Process started")), 10000);
		QTest::qWait(200);
		console->moveCursor(QTextCursor::End);
		QTest::keyClicks(console, QStringLiteral("typed input"));
		QTest::keyClick(console, Qt::Key_Return);
		QTRY_VERIFY2_WITH_TIMEOUT(console->toPlainText().split(u'\n').contains(QStringLiteral("got:typed input")),
			qPrintable(console->toPlainText()), 10000);
		QTRY_COMPARE_WITH_TIMEOUT(nppExecState(), static_cast<DWORD>(NPE_STATEREADY), 10000);
	}

	void nppexecGoToError()
	{
		// A line of a compiler's output, found by the warning filter, opens the file at the line on a double click
		const QString file = _home.path() + QStringLiteral("/broken.c");
		QVERIFY(writeFile(file, "int a;\nint b;\nint c\n"));
		QPlainTextEdit* console = nppExecConsole();
		QVERIFY(console);

		// gcc's mask, from NppExec's Quick Reference, in the Console Filters dialog, whose Highlight tab is first
		whileModal([](QWidget* dialog)
		{
			auto* enable = qobject_cast<QAbstractButton*>(control(dialog, 1071)); // IDC_CH_HIGHLIGHT1
			auto* mask = qobject_cast<QComboBox*>(control(dialog, 1081)); // IDC_CB_HIGHLIGHT1
			QVERIFY(enable && mask && mask->isEditable());
			if (!enable->isChecked())
				enable->click();
			mask->setEditText(QStringLiteral("%ABSFILE%:%LINE%: *error*"));
			click(dialog, IDOK);
		}, [this]() { trigger(QStringLiteral("NppExec"), QStringLiteral("Console Output Filters...")); });

		runInConsole(console, QStringLiteral("echo %1:3: error: expected ';'").arg(file));
		const QString text = console->toPlainText();
		const int line = text.split(u'\n').indexOf(QStringLiteral("%1:3: error: expected ';'").arg(file));
		QVERIFY2(line >= 0, qPrintable(text));
		const QTextBlock block = console->document()->findBlockByNumber(line);
		QTextCursor cursor(block);
		console->setTextCursor(cursor);
		const QPoint at = console->cursorRect().center();
		QTest::mouseDClick(console->viewport(), Qt::LeftButton, Qt::NoModifier, at);
		QTRY_COMPARE(currentPath(), file);
		QTRY_COMPARE(sci(SCI_LINEFROMPOSITION, sci(SCI_GETCURRENTPOS)), 2);
	}

	void nppexecExecuteDialog()
	{
		// A script typed in the Execute dialog, saved under a name through the prompt for it, and run with OK
		QPlainTextEdit* console = nppExecConsole();
		QVERIFY(console);
		QTRY_COMPARE_WITH_TIMEOUT(nppExecState(), static_cast<DWORD>(NPE_STATEREADY), 10000);
		const QString dir = qEnvironmentVariable("NPP_TEST_PLUGINS_DIR");
		if (!dir.isEmpty())
			nppExecDock()->grab().save(dir + QStringLiteral("/nppexec-console.png"));

		bool saved = false;
		whenModal([&saved, &dir](QWidget* dialog)
		{
			auto* script = qobject_cast<QPlainTextEdit*>(control(dialog, 1001)); // IDC_ED_CMDLINE
			QVERIFY(script);
			// lines that end without "\r", as the edit control of Windows gives them: "abc" has 3 characters
			script->setPlainText(QStringLiteral("echo first line\nset n ~ strlen abc\necho n=$(n)\necho second line"));
			if (!dir.isEmpty())
				dialog->grab().save(dir + QStringLiteral("/nppexec-execute.png"));

			// Save... asks for the name in a dialog of its own
			QTimer::singleShot(0, dialog, [&saved]()
			{
				QWidget* prompt = QApplication::activeModalWidget();
				auto* name = prompt ? qobject_cast<QComboBox*>(control(prompt, 1005)) : nullptr; // IDC_CB_SCRIPTNAME
				QVERIFY(name);
				name->setEditText(QStringLiteral("my script"));
				click(prompt, IDOK);
				saved = true;
			});
			click(dialog, 1003); // IDSAVE
			QVERIFY(saved);
			auto* scripts = qobject_cast<QComboBox*>(control(dialog, 1004)); // IDC_CB_SCRIPT
			QVERIFY(scripts && scripts->findText(QStringLiteral("my script")) >= 0);
			QCOMPARE(scripts->currentText(), QStringLiteral("my script"));
			click(dialog, IDOK);
		}, [this]() { trigger(QStringLiteral("NppExec"), QStringLiteral("Execute NppExec Script...")); });

		QTRY_VERIFY2_WITH_TIMEOUT(console->toPlainText().split(u'\n').contains(QStringLiteral("second line")), qPrintable(console->toPlainText()), 10000);
		QVERIFY(console->toPlainText().split(u'\n').contains(QStringLiteral("first line")));
		QVERIFY2(console->toPlainText().split(u'\n').contains(QStringLiteral("n=3")), qPrintable(console->toPlainText()));
	}

	void nppexecDialogs()
	{
		// Each dialog of the menu opens, with its controls, and closes with Cancel or OK
		const QString dir = qEnvironmentVariable("NPP_TEST_PLUGINS_DIR");
		const struct
		{
			const char* item;
			const char* picture;
			int control; // one that the dialog has
			int close;
		} dialogs[] = {
			{"Console Output Filters...", "nppexec-filters", 1081, IDCANCEL}, // IDC_CB_HIGHLIGHT1
			{"Advanced Options...", "nppexec-options", 1301, IDCANCEL}, // IDC_GB_MENUITEMS
			{"Help/About...", "nppexec-about", 1007, IDOK}, // IDC_ED_INFO
		};
		for (const auto& d : dialogs)
		{
			whenModal([&](QWidget* dialog)
			{
				if (!dir.isEmpty())
					dialog->grab().save(dir + u'/' + QLatin1String(d.picture) + QStringLiteral(".png"));
				QVERIFY2(dialog->isVisible(), d.item);
				QVERIFY2(control(dialog, d.control), d.item);
				click(dialog, d.close);
			}, [this, &d]() { trigger(QStringLiteral("NppExec"), QString::fromLatin1(d.item)); });
		}

		// The console's encoding, whose menu item shows what is chosen
		QMenu* menu = pluginMenu(QStringLiteral("NppExec"));
		QAction* encoding = nullptr;
		for (QAction* action : menu->actions())
		{
			if (action->text().startsWith(QStringLiteral("Console Output...")))
				encoding = action;
		}
		QVERIFY(encoding);
		whenModal([&](QWidget* dialog)
		{
			if (!dir.isEmpty())
				dialog->grab().save(dir + QStringLiteral("/nppexec-encoding.png"));
			click(dialog, 1392); // IDC_RB_OUTPUT_UTF8
			click(dialog, IDOK);
		}, [encoding]() { encoding->trigger(); });
		QTRY_VERIFY2(encoding->text().contains(QStringLiteral("{UTF8/")), qPrintable(encoding->text()));
	}

	void nppexecScriptCommands()
	{
		// Commands of NppExec's scripts that work on Notepad++, Scintilla and NppExec's variables
		const QString file = _home.path() + QStringLiteral("/script commands.txt");
		QVERIFY(writeFile(file, "twelve chars"));
		QPlainTextEdit* console = nppExecConsole();
		QVERIFY(console);
		const auto lines = [console]() { return console->toPlainText().split(u'\n'); };

		runInConsole(console, QStringLiteral("npp_open \"%1\"").arg(file));
		QTRY_COMPARE(currentPath(), file);

		runInConsole(console, QStringLiteral("echo [$(FILE_NAME)] [$(CURRENT_DIRECTORY)]"));
		QVERIFY2(lines().contains(QStringLiteral("[script commands.txt] [%1]").arg(_home.path())), qPrintable(console->toPlainText()));

		runInConsole(console, QStringLiteral("sci_sendmsg SCI_GETLENGTH"));
		runInConsole(console, QStringLiteral("echo length=$(MSG_RESULT)"));
		QVERIFY2(lines().contains(QStringLiteral("length=12")), qPrintable(console->toPlainText()));

		runInConsole(console, QStringLiteral("set n ~ 6 * 7"));
		runInConsole(console, QStringLiteral("echo n=$(n)"));
		QVERIFY2(lines().contains(QStringLiteral("n=42")), qPrintable(console->toPlainText()));

		// sel_settext puts the text in the editor
		runInConsole(console, QStringLiteral("sel_settext replaced"));
		QTRY_COMPARE(editorText(), QByteArray("replacedtwelve chars"));
	}

	void nppexecConsoleCompletionAndHistory()
	{
		// Tab completes a path of the input, and Up brings back the last command
		const QString file = _home.path() + QStringLiteral("/completion-target.txt");
		QVERIFY(writeFile(file, "x"));
		QPlainTextEdit* console = nppExecConsole();
		QVERIFY(console);
		QTRY_COMPARE_WITH_TIMEOUT(nppExecState(), static_cast<DWORD>(NPE_STATEREADY), 10000);
		console->setFocus();
		console->moveCursor(QTextCursor::End);
		QTest::keyClicks(console, QStringLiteral("npp_open %1/completion-ta").arg(_home.path()));
		QTest::keyClick(console, Qt::Key_Tab);
		QTRY_VERIFY2(console->toPlainText().endsWith(QStringLiteral("npp_open ") + file), qPrintable(console->toPlainText().right(200)));
		QTest::keyClick(console, Qt::Key_Return);
		QTRY_COMPARE(currentPath(), file);
		QTRY_COMPARE_WITH_TIMEOUT(nppExecState(), static_cast<DWORD>(NPE_STATEREADY), 10000);

		console->setFocus();
		console->moveCursor(QTextCursor::End);
		QTest::keyClick(console, Qt::Key_Up);
		QTRY_VERIFY2(console->toPlainText().endsWith(QStringLiteral("npp_open ") + file), qPrintable(console->toPlainText().right(200)));
		QTest::keyClick(console, Qt::Key_Return); // runs it again
		QTRY_COMPARE_WITH_TIMEOUT(nppExecState(), static_cast<DWORD>(NPE_STATEREADY), 10000);
	}

	void nppexecCloseConsole()
	{
		QVERIFY(nppExecConsole());
		QDockWidget* dock = nppExecDock();
		QTRY_VERIFY(dock->isVisible());
		// NppExec keeps its console while a script runs
		QTRY_COMPARE_WITH_TIMEOUT(nppExecState(), static_cast<DWORD>(NPE_STATEREADY), 10000);
		QAction* show = actionOf(pluginMenu(QStringLiteral("NppExec")), QStringLiteral("Show NppExec Console"));
		QVERIFY(show);
		QTRY_VERIFY(show->isChecked());

		// The dock's close button, which NppExec hears as DMN_CLOSE, as on Windows
		dock->close();
		QTRY_VERIFY(!dock->isVisible());
		QTRY_VERIFY(!show->isChecked());

		// And it comes back with the menu
		QVERIFY(nppExecConsole());
		QTRY_VERIFY(nppExecDock()->isVisible());
		QTRY_VERIFY(show->isChecked());
	}

	// Last, as it ends the application with the Conversion Panel open, whose widgets have to go before the plugin
	void closeApplication()
	{
		QDockWidget* dock = dockTitled(QStringLiteral("Conversion"));
		QVERIFY(dock && dock->isVisible());
		const QPointer<QWidget> panel = dock->widget();

		QTimer answers;
		QObject::connect(&answers, &QTimer::timeout, &answers, []()
		{
			QWidget* modal = QApplication::activeModalWidget();
			if (auto* messageBox = qobject_cast<QMessageBox*>(modal))
				messageBox->button(QMessageBox::No)->click();
			else if (auto* button = modal ? qobject_cast<QAbstractButton*>(nppFindControl(modal, IDNO)) : nullptr)
				button->click(); // the save prompt of Notepad++
		});
		answers.start(10);
		command(IDM_FILE_CLOSEALL);
		send(WM_CLOSE);
		answers.stop();
		QCoreApplication::processEvents();
		_npp.reset();
		QCoreApplication::processEvents();

		// The main window is not deleted, so the panel, whose class is the plugin's, is still there and the plugin has to
		// be too. Unloaded, this call into its code crashes, as Qt 6.8's accessibility does when the application ends.
		if (panel)
			QVERIFY(panel->metaObject());

		// NppExec's script of nppexecExecuteDialog is kept in its file of saved scripts
		// in UTF-16LE with its BOM, as on Windows
		const QByteArray scriptsFile = readFile(_home.path() + QStringLiteral("/.config/notepad++/plugins/Config/npes_saved.txt"));
		QVERIFY(scriptsFile.startsWith("\xFF\xFE"));
		const QString scripts = QString::fromUtf16(reinterpret_cast<const char16_t*>(scriptsFile.constData() + 2), (scriptsFile.size() - 2) / 2);
		QVERIFY2(scripts.contains(QStringLiteral("::my script")) && scripts.contains(QStringLiteral("echo second line")), qPrintable(scripts));

		// The panels' places are kept for the next start, numbered as there are two: Converter's and NppExec's
		const QString config = QString::fromUtf8(readFile(_home.path() + QStringLiteral("/.config/notepad++/config.ini")));
		for (const QString& plugin : {QStringLiteral("NppConverter.so"), QStringLiteral("NppExec.so")})
		{
			const QRegularExpression entry(QStringLiteral("PluginDlg\\\\\\d+\\\\pluginName=") + QRegularExpression::escape(plugin));
			QVERIFY2(config.contains(entry), qPrintable(config));
		}
	}
};

QTEST_MAIN(PluginsTest)
#include "pluginsTest.moc"
