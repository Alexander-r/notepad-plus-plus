// End to end tests of the whole application: a real main window, driven through its menu commands, its dialogs'
// controls and the plugin messages. They all share one window, as the application's objects are singletons.
// Set NPP_TEST_APP_DIR=dir to save screenshots there.

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QGuiApplication>
#include <QLabel>
#include <QLibrary>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSlider>
#include <QStandardPaths>
#include <QScrollArea>
#include <QScrollBar>
#include <QProcess>
#include <QSplitter>
#include <QSplitterHandle>
#include <QWheelEvent>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QWindow>
#include <QUrl>
#include <QAbstractScrollArea>
#include <QtMath>
#include <QRegularExpression>
#include <QPushButton>
#include <QTabBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QTimer>
#include <QToolButton>
#include <QToolBar>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include <ScintillaEditBase.h>

#include "FindReplaceDlg_rc.h"
#include "ansiCharPanel_rc.h"
#include "clipboardHistoryPanel_rc.h"
#include "columnEditor_rc.h"
#include "findCharsInRange_rc.h"
#include "fileBrowser_rc.h"
#include "functionListPanel_rc.h"
#include "preference_rc.h"
#include "ProjectPanel_rc.h"
#include "RunDlg_rc.h"
#include "ShortcutMapper_rc.h"
#include "shortcutRc.h"
#include "UserDefineResource.h"
#include "WindowsDlgRc.h"
#include "WordStyleDlgRes.h"
#include "md5Dlgs_rc.h"
#include "NppDarkMode.h"
#include "NppMenu.h"
#include "NppMessageFilter.h"
#include "Notepad_plus_Window.h"
#include "Notepad_plus_msgs.h"
#include "Docking.h"
#include "dockingResource.h"
#include "NppControls.h"
#include "Buffer.h"
#include "Parameters.h"
#include "menuCmdID.h"
#include "resource.h"

std::chrono::steady_clock::time_point g_nppStartTimePoint{};

namespace
{
	const QColor customIconColour(0x10, 0x80, 0x40);
	const QColor customDisabledIconColour(0x40, 0x10, 0x80);

	std::wstring toW(const QString& text)
	{
		return text.toStdWString();
	}

	void writeFile(const QString& path, const QByteArray& content)
	{
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(content);
	}

	QByteArray textOf(HWND editor)
	{
		const auto length = ::SendMessage(editor, SCI_GETLENGTH);
		QByteArray text(length + 1, '\0');
		::SendMessage(editor, SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(text.data()));
		text.chop(1);
		return text;
	}

	QByteArray readFile(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
	}

	// A macro recorded and saved by the original Notepad++ 8.9.9.1 on Windows: it types two lines, uppercases the second
	// with a menu command, and replaces with the Replace dialog. These are the actions of its shortcuts.xml.
	struct MacroAction
	{
		int type;
		int message;
		uptr_t wParam;
		uptr_t lParam;
		const char* sParam;
	};
	const MacroAction windowsMacro[] = {
		{1, 2170, 0, 0, "Hello foo"},
		{0, 2329, 0, 0, ""},
		{1, 2170, 0, 0, "foo bar foo"},
		{0, 2312, 0, 0, ""},
		{0, 2315, 0, 0, ""},
		{2, 0, 42016, 0, ""},
		{3, 1700, 0, 0, ""},
		{3, 1601, 0, 0, "foo"},
		{3, 1625, 0, 0, ""},
		{3, 1602, 0, 0, "baz"},
		{3, 1702, 0, 768, ""},
		{3, 1701, 0, 1609, ""},
	};
	const QByteArray windowsMacroResult = "Hello baz\r\nbaz BAR baz";

	QByteArray windowsMacroXml(const char* name)
	{
		QByteArray xml = QByteArray("\t\t<Macro name=\"") + name + "\" Ctrl=\"no\" Alt=\"no\" Shift=\"no\" Key=\"0\">\r\n";
		for (const MacroAction& action : windowsMacro)
		{
			xml += QStringLiteral("\t\t\t<Action type=\"%1\" message=\"%2\" wParam=\"%3\" lParam=\"%4\" sParam=\"%5\" />\r\n")
				.arg(action.type).arg(action.message).arg(action.wParam).arg(action.lParam).arg(QString::fromUtf8(action.sParam)).toUtf8();
		}
		return xml + "\t\t</Macro>\r\n";
	}
}

class AppTest : public QObject
{
	Q_OBJECT

private:
	QTemporaryDir _home;
	QTemporaryDir _files;
	std::unique_ptr<Notepad_plus_Window> _npp; // made after the settings are loaded, as in winmain
	int _dialogFontSize = 0; // from NPP_TEST_DIALOG_FONT_SIZE, for the layouts with another font
	CmdLineParams _params;

	HWND window() const { return _npp->getHSelf(); }
	QMainWindow* mainWindow() const { return _npp->mainWindow(); }

	LRESULT send(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) const
	{
		return ::SendMessage(window(), message, wParam, lParam);
	}

	// A key press as the desktop sends it, which tries the shortcuts first and comes with the text a QTest::keyClick
	// lacks, like the control character 1 for Ctrl+A
	void pressKey(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString& text = {}) const
	{
		QWindow* handle = window()->windowHandle();
		QTest::simulateEvent(handle, true, key, modifiers, text, false);
		QTest::simulateEvent(handle, false, key, modifiers, text, false);
		QCoreApplication::processEvents();
	}

	// Shortcuts need the active window, and a window manager may keep the focus from a window that only asks for it
	bool mainWindowGetsFocus() const
	{
		mainWindow()->activateWindow();
		return QTest::qWaitForWindowActive(mainWindow()) && QTest::qWaitFor([this]() { return mainWindow()->isActiveWindow(); }, 3000);
	}

	static QString controlText(char letter)
	{
		return QString(QChar(letter - 64));
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

	QByteArray editorText() const
	{
		return textOf(currentEditor());
	}

	QString currentPath() const
	{
		wchar_t path[MAX_PATH]{};
		send(NPPM_GETFULLCURRENTPATH, MAX_PATH, reinterpret_cast<LPARAM>(path));
		return QString::fromWCharArray(path);
	}

	void open(const QString& path) const
	{
		const std::wstring wide = toW(path);
		send(NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(wide.c_str()));
		QCoreApplication::processEvents();
	}

	QWidget* findDialog() const
	{
		const QList<QDialog*> dialogs = mainWindow()->findChildren<QDialog*>();
		for (QDialog* dialog : dialogs)
		{
			if (nppFindControl(dialog, IDFINDWHAT))
				return dialog;
		}
		return nullptr;
	}

	void setComboText(int id, const QString& text) const
	{
		auto* combo = qobject_cast<QComboBox*>(nppFindControl(findDialog(), id));
		QVERIFY(combo);
		combo->setCurrentText(text);
	}

	void click(int id) const
	{
		auto* button = qobject_cast<QAbstractButton*>(nppFindControl(findDialog(), id));
		QVERIFY(button);
		button->click();
		QCoreApplication::processEvents();
	}

	void setChecked(int id, bool checked) const
	{
		auto* button = qobject_cast<QAbstractButton*>(nppFindControl(findDialog(), id));
		QVERIFY(button);
		if (button->isChecked() != checked)
			button->click();
	}

	// The text of the search results panel
	QByteArray finderText() const
	{
		const QList<QDockWidget*> docks = mainWindow()->findChildren<QDockWidget*>();
		for (QDockWidget* dock : docks)
		{
			// Other panels hold editors too, like the Document Map
			if (auto* editor = dock->findChild<ScintillaEditBase*>())
			{
				const QByteArray text = textOf(editor);
				if (text.startsWith("Search "))
					return text;
			}
		}
		return {};
	}

	// A change of the desktop's theme, as KDE announces it. Only for a private session, as the signal goes to every KDE
	// application on the session bus. KDE reads the theme from the folder that this test has made the current one.
	void changeDesktopTheme(bool toDark) const
	{
		QVERIFY(QDir().mkpath(qEnvironmentVariable("XDG_CONFIG_HOME")));
		QFile themeFile(qEnvironmentVariable("XDG_CONFIG_HOME") + QStringLiteral("/kdeglobals"));
		QVERIFY(themeFile.open(QIODevice::WriteOnly));
		const QByteArray background = toDark ? "36,39,44" : "240,241,242";
		const QByteArray foreground = toDark ? "241,241,242" : "20,20,20";
		for (const char* group : { "Window", "View", "Button", "Header", "Tooltip", "Complementary" })
			themeFile.write(QByteArray("[Colors:") + group + "]\nBackgroundNormal=" + background + "\nBackgroundAlternate=" + background + "\nForegroundNormal=" + foreground + "\n\n");
		themeFile.close();
		QCOMPARE(QProcess::execute(QStringLiteral("dbus-send"), { QStringLiteral("--session"), QStringLiteral("--type=signal"), QStringLiteral("/KGlobalSettings"),
			QStringLiteral("org.kde.KGlobalSettings.notifyChange"), QStringLiteral("int32:0"), QStringLiteral("int32:0") }), 0);
		QTRY_COMPARE_WITH_TIMEOUT(NppDarkMode::isDesktopDark(), toDark, 3000);
		QTest::qWait(100);
	}

	// Everything is in the colours that the mode and the desktop ask for
	void checkLooks(bool areControlsDark, bool isEditorDark) const
	{
		QCOMPARE(NppDarkMode::usesDarkWidgets(), areControlsDark);
		QCOMPARE(QApplication::palette().color(QPalette::Window).lightness() < 128, areControlsDark);
		const auto background = static_cast<COLORREF>(::SendMessage(currentEditor(), SCI_STYLEGETBACK, STYLE_DEFAULT, 0));
		QCOMPARE(QColor(GetRValue(background), GetGValue(background), GetBValue(background)).lightness() < 128, isEditorDark);
		checkBarsFollowMode();
		checkTabsAreReadable();
	}

	// Lines meet without a seam of another colour, also at a fractional display scale like 125%, where their edges
	// fall inside device pixels. And the editor paints all of itself, also inside the frame of a style like Fusion.
	void checkEditorHasNoSeams() const
	{
		command(IDM_FILE_NEW);
		HWND editor = currentEditor();
		::SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("\n\n\n\n\nb"));
		::SendMessage(editor, SCI_DOCUMENTEND, 0, 0);
		QTest::qWait(50);
		auto* scrollArea = qobject_cast<QAbstractScrollArea*>(editor);
		QVERIFY(scrollArea);
		const QImage image = scrollArea->viewport()->grab().toImage();
		const qreal scale = image.devicePixelRatio();
		const auto lineHeight = static_cast<int>(::SendMessage(editor, SCI_TEXTHEIGHT, 0, 0));
		const auto lineNumbersEnd = static_cast<int>(::SendMessage(editor, SCI_GETMARGINLEFT, 0, 0) + ::SendMessage(editor, SCI_GETMARGINWIDTHN, 0, 0));
		int textStart = lineNumbersEnd;
		const auto margins = static_cast<int>(::SendMessage(editor, SCI_GETMARGINS, 0, 0));
		for (int margin = 1; margin < margins; ++margin)
			textStart += static_cast<int>(::SendMessage(editor, SCI_GETMARGINWIDTHN, margin, 0));
		const auto background = static_cast<COLORREF>(::SendMessage(editor, SCI_STYLEGETBACK, STYLE_DEFAULT, 0));
		const QColor backColour(GetRValue(background), GetGValue(background), GetBValue(background));

		// The empty lines 1 to 5, above the caret's line, have the background of the text. Beside the line numbers each
		// column is the same from line 2 down, below where the change history's bar begins.
		const int marginTop = qCeil(lineHeight * scale);
		const int bottom = qFloor(5 * lineHeight * scale);
		const int reference = qRound(2.5 * lineHeight * scale);
		QVERIFY(bottom <= image.height());
		for (int y = 0; y < bottom; ++y)
		{
			for (int x = qCeil(lineNumbersEnd * scale); x < image.width(); ++x)
			{
				const bool isMargin = x < qCeil(textStart * scale);
				if (isMargin && y < marginTop)
					continue;
				const QColor expected = isMargin ? image.pixelColor(x, reference) : backColour;
				const QColor pixel = image.pixelColor(x, y);
				QVERIFY2(pixel == expected, qPrintable(QStringLiteral("%1 at %2,%3 of the editor at scale %4, instead of %5")
					.arg(pixel.name()).arg(x).arg(y).arg(scale).arg(expected.name())));
			}
		}
		::SendMessage(editor, SCI_SETSAVEPOINT, 0, 0);
		command(IDM_FILE_CLOSE);
	}

	// The menu bar and the tool bar are in the colours of the mode, which Breeze needs a colour scheme for
	void checkBarsFollowMode() const
	{
		const bool isDark = NppDarkMode::usesDarkWidgets();
		QMenuBar* menuBar = mainWindow()->menuBar();
		const QImage menuImage = menuBar->grab().toImage();
		const QColor menuColour = menuImage.pixelColor(menuImage.width() - 3, menuImage.height() / 2);
		QVERIFY2((menuColour.lightness() < 128) == isDark, qPrintable(menuColour.name()));
		auto* toolbar = mainWindow()->findChild<QToolBar*>(QStringLiteral("mainToolBar"));
		QVERIFY(toolbar);
		const QImage toolImage = toolbar->grab().toImage();
		const QColor toolColour = toolImage.pixelColor(1, 1);
		QVERIFY2((toolColour.lightness() < 128) == isDark, qPrintable(toolColour.name()));

		// The titles of the menus stand out from the bar
		double bestMenuContrast = 1.0;
		for (int y = 0; y < menuImage.height(); ++y)
		{
			for (int x = 0; x < menuImage.width() / 2; ++x)
			{
				const QColor pixel = menuImage.pixelColor(x, y);
				bestMenuContrast = std::max(bestMenuContrast, NppDarkMode::contrastRatio(RGB(pixel.red(), pixel.green(), pixel.blue()), RGB(menuColour.red(), menuColour.green(), menuColour.blue())));
			}
		}
		QVERIFY2(bestMenuContrast >= 3.0, qPrintable(QString::number(bestMenuContrast)));

		// The icons stand out from the bar, so that they are the set for its colour
		const QList<QAction*> actions = toolbar->actions();
		for (QAction* action : actions)
		{
			if (action->isSeparator() || action->icon().isNull() || !action->isEnabled())
				continue;
			const QImage icon = action->icon().pixmap(QSize(16, 16)).toImage();
			double best = 1.0;
			for (int y = 0; y < icon.height(); ++y)
			{
				for (int x = 0; x < icon.width(); ++x)
				{
					const QColor pixel = icon.pixelColor(x, y);
					if (pixel.alpha() > 200)
						best = std::max(best, NppDarkMode::contrastRatio(RGB(pixel.red(), pixel.green(), pixel.blue()), RGB(toolColour.red(), toolColour.green(), toolColour.blue())));
				}
			}
			QVERIFY2(best >= 2.0, qPrintable(action->toolTip() + u' ' + QString::number(best)));
		}
	}

	// The names of the open files must stand out from what they are drawn on
	void checkTabsAreReadable() const
	{
		const auto luminance = [](const QColor& colour)
		{
			const auto channel = [](double value) { return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4); };
			return 0.2126 * channel(colour.redF()) + 0.7152 * channel(colour.greenF()) + 0.0722 * channel(colour.blueF());
		};
		const QList<QTabBar*> bars = mainWindow()->findChildren<QTabBar*>();
		for (QTabBar* bar : bars)
		{
			if (!bar->isVisible())
				continue;
			const QImage image = bar->grab().toImage();
			for (int i = 0; i < bar->count(); ++i)
			{
				const QColor text = bar->tabTextColor(i);
				if (!text.isValid())
					continue;
				const QRect area = bar->tabRect(i);
				const QPoint sample = (QPointF(area.center().x(), area.top() + area.height() - 3) * image.devicePixelRatio()).toPoint();
				if (!image.rect().contains(sample))
					continue; // scrolled out of view
				const QColor background = image.pixelColor(sample);
				const double lighter = std::max(luminance(text), luminance(background));
				const double darker = std::min(luminance(text), luminance(background));
				const double contrast = (lighter + 0.05) / (darker + 0.05);
				QVERIFY2(contrast >= 3.0, qPrintable(QStringLiteral("tab %1 is %2 on %3").arg(bar->tabText(i), text.name(), background.name())));
			}
		}
	}

	// The controls of a dialog that overlap each other or are too small for their text
	static QStringList layoutProblems(QWidget* dialog, const QString& name)
	{
		const auto describe = [](const QWidget* widget)
		{
			return QStringLiteral("%1 %2").arg(QString::fromLatin1(widget->metaObject()->className()), widget->objectName());
		};
		const auto isInsideControl = [dialog](const QWidget* widget)
		{
			for (const QWidget* parent = widget->parentWidget(); parent && parent != dialog; parent = parent->parentWidget())
			{
				if (qobject_cast<const QAbstractScrollArea*>(parent) && !qobject_cast<const QScrollArea*>(parent))
					return true;
				if (qobject_cast<const QComboBox*>(parent) || qobject_cast<const QTabBar*>(parent) || qobject_cast<const QToolButton*>(parent) || qobject_cast<const QLineEdit*>(parent))
					return true;
			}
			return false;
		};

		QStringList problems;
		QList<const QWidget*> controls;
		const QList<QWidget*> children = dialog->findChildren<QWidget*>();
		for (const QWidget* child : children)
		{
			if (!child->isVisibleTo(dialog) || child->isWindow() || isInsideControl(child))
				continue;
			if (!child->findChildren<QWidget*>(Qt::FindDirectChildrenOnly).isEmpty() && !qobject_cast<const QAbstractScrollArea*>(child) && !qobject_cast<const QComboBox*>(child))
				continue; // a container
			if (qobject_cast<const QScrollBar*>(child) || qobject_cast<const QScrollArea*>(child) || child->inherits("QSizeGrip"))
				continue;
			controls.append(child);

			// A label that wraps fits by getting taller, so only the width of the others says anything
			const auto* label = qobject_cast<const QLabel*>(child);
			// The titles of a table's columns
			if (const auto* table = qobject_cast<const QTableWidget*>(child))
			{
				const QHeaderView* header = table->horizontalHeader();
				for (int column = 0; column < table->columnCount(); ++column)
				{
					const QTableWidgetItem* title = table->horizontalHeaderItem(column);
					const int titleWidth = (title && !header->isSectionHidden(column)) ? header->fontMetrics().horizontalAdvance(title->text()) : 0;
					if (header->sectionSize(column) < titleWidth)
						problems << QStringLiteral("%1: column \"%2\" of %3 is %4 wide for a title that needs %5").arg(name, title->text(), describe(child)).arg(header->sectionSize(column)).arg(titleWidth);
				}
			}

			const auto* push = qobject_cast<const QPushButton*>(child);
			const bool hasText = qobject_cast<const QAbstractButton*>(child) || (label && !label->wordWrap() && !label->text().isEmpty());
			// The style's minimum for a push button is wider than its text needs
			const int needed = push ? child->fontMetrics().horizontalAdvance(push->text().remove(u'&')) : child->minimumSizeHint().width();
			if (label && label->wordWrap() && !label->text().isEmpty() && child->height() < label->heightForWidth(child->width()))
				problems << QStringLiteral("%1: %2 is %3 high for wrapped text that needs %4").arg(name, describe(child)).arg(child->height()).arg(label->heightForWidth(child->width()));
			if (hasText && child->width() < needed)
				problems << QStringLiteral("%1: %2 is %3 wide for text that needs %4").arg(name, describe(child)).arg(child->width()).arg(needed);
		}

		for (qsizetype i = 0; i < controls.size(); ++i)
		{
			const QRect first(controls[i]->mapTo(dialog, QPoint(0, 0)), controls[i]->size());
			bool canScroll = false;
			for (const QWidget* parent = controls[i]->parentWidget(); parent && parent != dialog; parent = parent->parentWidget())
				canScroll = canScroll || qobject_cast<const QScrollArea*>(parent);
			if (!canScroll && !dialog->rect().contains(first))
				problems << QStringLiteral("%1: %2 is outside the dialog").arg(name, describe(controls[i]));
			for (qsizetype j = i + 1; j < controls.size(); ++j)
			{
				const QRect second(controls[j]->mapTo(dialog, QPoint(0, 0)), controls[j]->size());
				// The Document Map draws its view zone over the map on purpose
				const bool isEditorOverlay = controls[i]->inherits("ScintillaEditBase") || controls[j]->inherits("ScintillaEditBase");
				if (first.intersects(second) && !isEditorOverlay)
					problems << QStringLiteral("%1: %2 overlaps %3").arg(name, describe(controls[i]), describe(controls[j]));
			}
		}
		return problems;
	}

	void screenshot(const QString& name) const
	{
		const QString dir = qEnvironmentVariable("NPP_TEST_APP_DIR");
		QTest::qWait(50); // for the layout to settle
		if (!dir.isEmpty())
			mainWindow()->grab().save(dir + u'/' + name + QStringLiteral(".png"));
	}

	QString systemPluginDir() const { return _home.path() + QStringLiteral("/systemPlugins"); }

	mutable std::unique_ptr<QString> _asked;
	QString _pluginPath;
	HWND _main = nullptr;
	HWND _sub = nullptr;

private slots:
	void initTestCase()
	{
		nppInstallMessageFilter(); // as winmain does
		QVERIFY(_home.isValid());
		QVERIFY(_files.isValid());
		qputenv("HOME", _home.path().toUtf8());
		qputenv("XDG_CONFIG_HOME", (_home.path() + QStringLiteral("/.config")).toUtf8());

		// A plugin in the user's folder, where a user would put one
		const QString pluginDir = _home.path() + QStringLiteral("/.config/notepad++/plugins/testPlugin");
		QVERIFY(QDir().mkpath(pluginDir));
		QVERIFY(QFile::copy(QStringLiteral(NPP_TEST_PLUGIN), pluginDir + QStringLiteral("/testPlugin.so")));
		_pluginPath = pluginDir + QStringLiteral("/testPlugin.so");

		// And a real plugin from Windows, built for the port (see Test/plugins), where the system's packages put plugins.
		// That folder comes after the user's, so its copy of the test plugin is not loaded again.
		const QString mimeToolsDir = systemPluginDir() + QStringLiteral("/mimeTools");
		QVERIFY(QDir().mkpath(mimeToolsDir));
		QVERIFY(QFile::copy(QStringLiteral(NPP_TEST_MIME_TOOLS_PLUGIN), mimeToolsDir + QStringLiteral("/mimeTools.so")));
		QVERIFY(QDir().mkpath(systemPluginDir() + QStringLiteral("/testPlugin")));
		QVERIFY(QFile::copy(QStringLiteral(NPP_TEST_PLUGIN), systemPluginDir() + QStringLiteral("/testPlugin/testPlugin.so")));

		// A toolbar icon set in the user's folder, with an unreadable file in it
		const QString configDir = _home.path() + QStringLiteral("/.config/notepad++");
		const QString iconDir = configDir + QStringLiteral("/toolbarIcons/testSet");
		QVERIFY(QDir().mkpath(iconDir));
		const auto writeIcon = [&iconDir](const QString& name, const QColor& colour)
		{
			QImage image(16, 16, QImage::Format_ARGB32);
			image.fill(colour);
			return image.save(iconDir + QLatin1Char('/') + name, "ICO");
		};
		QVERIFY(writeIcon(QStringLiteral("save-macro.ico"), customIconColour));
		QVERIFY(writeIcon(QStringLiteral("save-macro_disabled.ico"), customDisabledIconColour));
		QVERIFY(writeIcon(QStringLiteral("cut.ico"), customIconColour));
		QFile unreadable(iconDir + QStringLiteral("/open.ico"));
		QVERIFY(unreadable.open(QIODevice::WriteOnly));
		unreadable.write("not an icon");
		unreadable.close();
		QFile buttonsConf(configDir + QStringLiteral("/toolbarButtonsConf.xml"));
		QVERIFY(buttonsConf.open(QIODevice::WriteOnly));
		buttonsConf.write("<NotepadPlus><ToolBarIcons icoFolderName=\"testSet\" /></NotepadPlus>");
		buttonsConf.close();

		// The shortcuts.xml of a user who has the macro recorded on Windows, after the default macro
		QByteArray shortcuts(SHORTCUT_XML_CONTENT);
		const qsizetype macrosEnd = shortcuts.indexOf("\t</Macros>");
		QVERIFY(macrosEnd != -1);
		shortcuts.insert(macrosEnd, windowsMacroXml("Windows macro"));
		writeFile(configDir + QStringLiteral("/shortcuts.xml"), shortcuts);

		// A user with recent files, and documents from the last session in the main view
		if (qEnvironmentVariableIsSet("NPP_TEST_RESTORED_SESSION"))
		{
			const QString first = _files.filePath(QStringLiteral("sessionOne.txt"));
			const QString second = _files.filePath(QStringLiteral("sessionTwo.txt"));
			writeFile(first, "one\n");
			writeFile(second, "two\n");
			QFile config(_home.path() + QStringLiteral("/.config/notepad++/config.xml"));
			QVERIFY(config.open(QIODevice::WriteOnly));
			config.write(QStringLiteral("<NotepadPlus><GUIConfigs /><History nbMaxFile=\"10\" inSubMenu=\"no\" customLength=\"-1\"><File filename=\"%1\" /></History></NotepadPlus>").arg(first).toUtf8());
			config.close();
			QFile session(_home.path() + QStringLiteral("/.config/notepad++/session.xml"));
			QVERIFY(session.open(QIODevice::WriteOnly));
			session.write(QStringLiteral("<NotepadPlus><Session activeView=\"0\"><mainView activeIndex=\"1\"><File firstVisibleLine=\"0\" xOffset=\"0\" scrollWidth=\"100\" startPos=\"0\" endPos=\"0\" selMode=\"0\" offset=\"0\" wrapCount=\"1\" lang=\"None (Normal Text)\" encoding=\"-1\" userReadOnly=\"no\" filename=\"%1\" backupFilePath=\"\" originalFileLastModifTimestamp=\"0\" originalFileLastModifTimestampHigh=\"0\" tabColourId=\"-1\" RTL=\"no\" tabPinned=\"no\" /><File firstVisibleLine=\"0\" xOffset=\"0\" scrollWidth=\"100\" startPos=\"0\" endPos=\"0\" selMode=\"0\" offset=\"0\" wrapCount=\"1\" lang=\"None (Normal Text)\" encoding=\"-1\" userReadOnly=\"no\" filename=\"%2\" backupFilePath=\"\" originalFileLastModifTimestamp=\"0\" originalFileLastModifTimestampHigh=\"0\" tabColourId=\"-1\" RTL=\"no\" tabPinned=\"no\" /></mainView><subView activeIndex=\"0\" /></Session></NotepadPlus>").arg(first, second).toUtf8());
		}

		// The state of a user who chose Follow system earlier, so that this session has not opened the preferences
		if (qEnvironmentVariableIsSet("NPP_TEST_THEME_CHANGE"))
		{
			QFile config(_home.path() + QStringLiteral("/.config/notepad++/config.xml"));
			QVERIFY(config.open(QIODevice::WriteOnly));
			config.write("<NotepadPlus><GUIConfigs><GUIConfig name=\"DarkMode\" enable=\"yes\" enableWindowsMode=\"yes\" /></GUIConfigs></NotepadPlus>");
		}

		// The menu bar stays in the window, whatever the desktop offers. With a menu registrar on the session bus, as
		// for KDE's menu button in the title bar, the KDE platform theme would hand it to the desktop, which crashes Qt
		// on the offscreen platform, and the tests look at the menu bar in the window.
		QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);

		NppParameters::getInstance().load();
		NppParameters::getInstance().setSystemPluginRootDir(systemPluginDir().toStdWString());
		NppDarkMode::initDarkMode(); // as winmain does
		_dialogFontSize = qEnvironmentVariableIntValue("NPP_TEST_DIALOG_FONT_SIZE");
		if (_dialogFontSize > 0)
			NppParameters::getInstance().getNppGUI()._fontDlgSize = static_cast<WORD>(_dialogFontSize);
		_npp = std::make_unique<Notepad_plus_Window>();
		_npp->init(nullptr, nullptr, L"", &_params);
		mainWindow()->resize(1000, 700);
		QCoreApplication::processEvents();

		// The two document editors are the ones in the panes of the splitter of the central area
		const QList<ScintillaEditBase*> editors = mainWindow()->centralWidget()->findChildren<ScintillaEditBase*>();
		QCOMPARE(editors.size(), 2);
		_main = editors[0]->isVisible() ? editors[0] : editors[1];
		_sub = (_main == editors[0]) ? editors[1] : editors[0];
	}

	void mainWindowIsComplete()
	{
		QVERIFY(mainWindow()->isVisible());
		QVERIFY(mainWindow()->menuBar()->actions().size() >= 10);
		QVERIFY(mainWindow()->centralWidget());
		QVERIFY(_main->isVisible());
		QVERIFY(!_sub->isVisible()); // one view until the user asks for two
		QVERIFY(_main->width() > 200);
		QVERIFY(_main->height() > 200);
		screenshot(QStringLiteral("start"));
	}

	void typeAndUndo()
	{
		command(IDM_FILE_NEW);
		::SendMessage(_main, SCI_ADDTEXT, 5, reinterpret_cast<LPARAM>("hello"));
		QCOMPARE(editorText(), QByteArray("hello"));
		command(IDM_EDIT_UNDO);
		QCOMPARE(editorText(), QByteArray());
		command(IDM_EDIT_REDO);
		QCOMPARE(editorText(), QByteArray("hello"));
	}

	// Paste is only enabled with text to paste, as SCI_CANPASTE says on Windows
	void pasteFollowsClipboard()
	{
		const HMENU mainMenu = reinterpret_cast<HMENU>(send(NPPM_INTERNAL_GETMENU));
		QAction* paste = nppFindMenuAction(mainMenu, IDM_EDIT_PASTE);
		QVERIFY(paste);
		QApplication::clipboard()->clear();
		QTRY_VERIFY(!paste->isEnabled());
		QApplication::clipboard()->setText(QStringLiteral("to paste"));
		QTRY_VERIFY(paste->isEnabled());
	}

	void openEditAndSave()
	{
		const QString path = _files.filePath(QStringLiteral("sample.cpp"));
		writeFile(path, "int main()\r\n{\r\n}\r\n");
		open(path);

		QCOMPARE(currentPath(), path);
		QCOMPARE(editorText(), QByteArray("int main()\r\n{\r\n}\r\n"));

		LangType language = L_TEXT;
		send(NPPM_GETCURRENTLANGTYPE, 0, reinterpret_cast<LPARAM>(&language));
		QCOMPARE(language, L_CPP);
		QVERIFY(mainWindow()->windowTitle().contains(QStringLiteral("sample.cpp")));

		::SendMessage(_main, SCI_GOTOPOS, 0);
		::SendMessage(_main, SCI_ADDTEXT, 3, reinterpret_cast<LPARAM>("// "));
		QCOMPARE(editorText(), QByteArray("// int main()\r\n{\r\n}\r\n"));
		QVERIFY(::SendMessage(_main, SCI_GETMODIFY));
		QVERIFY(send(NPPM_SAVECURRENTFILE));
		QVERIFY(!::SendMessage(_main, SCI_GETMODIFY));
		QCOMPARE(readFile(path), QByteArray("// int main()\r\n{\r\n}\r\n"));
		screenshot(QStringLiteral("sample"));
	}

	// Plugins come from the user's folder and the system's packages, each of them once
	void pluginsOfEveryFolder()
	{
		QStringList names;
		const QList<QAction*> menus = mainWindow()->menuBar()->actions();
		for (QAction* menu : menus)
		{
			if (menu->menu() && menu->text().remove(u'&') == QStringLiteral("Plugins"))
			{
				const QList<QAction*> plugins = menu->menu()->actions();
				for (QAction* plugin : plugins)
				{
					if (plugin->menu())
						names.append(plugin->text());
				}
			}
		}
		QCOMPARE(names.count(QStringLiteral("Test Plugin")), 1);
		QCOMPARE(names.count(QStringLiteral("MIME Tools")), 1);
	}

	// The user defined languages for Markdown, which the Windows installer puts in the user's folder, are copied there on
	// the first run
	void markdownLanguagesOnFirstRun()
	{
		const QDir udlDir(_home.path() + QStringLiteral("/.config/notepad++/userDefineLangs"));
		QVERIFY(udlDir.exists(QStringLiteral("markdown._preinstalled.udl.xml")));
		QVERIFY(udlDir.exists(QStringLiteral("markdown._preinstalled_DM.udl.xml")));
		const NppParameters& params = NppParameters::getInstance();
		QVERIFY(params.getULCFromName(L"Markdown (preinstalled)"));
		QVERIFY(params.getULCFromName(L"Markdown (preinstalled dark mode)"));
	}

	void pluginMessagesAboutPaths()
	{
		wchar_t text[MAX_PATH]{};
		send(NPPM_GETFILENAME, MAX_PATH, reinterpret_cast<LPARAM>(text));
		QCOMPARE(QString::fromWCharArray(text), QStringLiteral("sample.cpp"));
		send(NPPM_GETNAMEPART, MAX_PATH, reinterpret_cast<LPARAM>(text));
		QCOMPARE(QString::fromWCharArray(text), QStringLiteral("sample"));
		send(NPPM_GETEXTPART, MAX_PATH, reinterpret_cast<LPARAM>(text));
		QCOMPARE(QString::fromWCharArray(text), QStringLiteral(".cpp"));
		send(NPPM_GETCURRENTDIRECTORY, MAX_PATH, reinterpret_cast<LPARAM>(text));
		QCOMPARE(QString::fromWCharArray(text), _files.path());
	}

	void findAllInCurrentDocument()
	{
		command(IDM_SEARCH_FIND);
		QVERIFY(findDialog());
		QVERIFY(findDialog()->isVisible());

		setComboText(IDFINDWHAT, QStringLiteral("main"));
		click(IDC_FINDALL_CURRENTFILE);

		const QByteArray results = finderText();
		QVERIFY2(results.contains("sample.cpp"), results.constData());
		QVERIFY2(results.contains("1 hit"), results.constData());
		screenshot(QStringLiteral("findAll"));
	}

	void findInFiles()
	{
		QDir(_files.path()).mkdir(QStringLiteral("sub"));
		writeFile(_files.filePath(QStringLiteral("alpha.txt")), "one needle\nno\nneedle two\n");
		writeFile(_files.filePath(QStringLiteral("beta.log")), "needle in a log\n");
		writeFile(_files.filePath(QStringLiteral("sub/gamma.txt")), "deep needle\n");
		writeFile(_files.filePath(QStringLiteral("zeta.txt")), "nothing\n");

		command(IDM_SEARCH_FINDINFILES);
		setComboText(IDFINDWHAT, QStringLiteral("needle"));
		setComboText(IDD_FINDINFILES_FILTERS_COMBO, QStringLiteral("*.txt"));
		setComboText(IDD_FINDINFILES_DIR_COMBO, _files.path());
		setChecked(IDD_FINDINFILES_RECURSIVE_CHECK, true);
		click(IDD_FINDINFILES_FIND_BUTTON);

		QTRY_VERIFY_WITH_TIMEOUT(finderText().contains("gamma.txt"), 10000);
		const QByteArray results = finderText();
		QVERIFY2(results.contains("alpha.txt"), results.constData());
		QVERIFY2(!results.contains("beta.log"), results.constData());
		QVERIFY2(!results.contains("zeta.txt"), results.constData());
		QVERIFY2(results.contains("3 hits"), results.constData());

		const QList<QDockWidget*> docks = mainWindow()->findChildren<QDockWidget*>();
		bool isResultPanelShown = false;
		for (const QDockWidget* dock : docks)
		{
			if (dock->isVisible() && dock->findChild<ScintillaEditBase*>())
			{
				isResultPanelShown = true;
				QVERIFY(!dock->isFloating());
				QCOMPARE(mainWindow()->dockWidgetArea(const_cast<QDockWidget*>(dock)), Qt::BottomDockWidgetArea);
			}
		}
		QVERIFY(isResultPanelShown);

		screenshot(QStringLiteral("findInFiles"));
	}

	void replaceAllInCurrentDocument()
	{
		const QString path = _files.filePath(QStringLiteral("replace.txt"));
		writeFile(path, "aaa bbb aaa\n");
		open(path);

		command(IDM_SEARCH_REPLACE);
		setComboText(IDFINDWHAT, QStringLiteral("a+"));
		setComboText(IDREPLACEWITH, QStringLiteral("[$0]"));
		auto* regex = qobject_cast<QAbstractButton*>(nppFindControl(findDialog(), IDREGEXP));
		QVERIFY(regex);
		if (!regex->isChecked())
			regex->click();
		click(IDREPLACEALL);

		QCOMPARE(editorText(), QByteArray("[aaa] bbb [aaa]\n"));
		regex->setChecked(false);
		findDialog()->hide();
	}

	void twoViews()
	{
		const QString path = currentPath();
		const LRESULT mainDocs = send(NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW);

		command(IDM_VIEW_GOTO_ANOTHER_VIEW);
		QVERIFY(_sub->isVisible());
		QVERIFY(_main->isVisible());
		QVERIFY(_sub->width() > 100);
		QCOMPARE(send(NPPM_GETCURRENTVIEW), LRESULT(SUB_VIEW));
		QCOMPARE(currentPath(), path);
		QCOMPARE(send(NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW), mainDocs - 1);
		screenshot(QStringLiteral("twoViews"));
		QCOMPARE(send(NPPM_GETCURRENTVIEW), LRESULT(SUB_VIEW));

		// Moving the only document back closes the second view
		command(IDM_VIEW_GOTO_ANOTHER_VIEW);
		QCOMPARE(send(NPPM_GETCURRENTVIEW), LRESULT(MAIN_VIEW));
		QCOMPARE(currentPath(), path);
		QCOMPARE(send(NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW), mainDocs);
		QVERIFY(_main->isVisible());
		QVERIFY(!_sub->isVisible());
	}

	// Answers the first modal dialog that comes up by clicking the button with this ID, and tells what it asked
	QString answerModalDialog(int buttonId) const
	{
		auto* asked = new QString;
		QTimer::singleShot(0, mainWindow(), [asked, buttonId]()
		{
			QWidget* modal = QApplication::activeModalWidget();
			if (!modal)
				return;
			if (auto* messageBox = qobject_cast<QMessageBox*>(modal))
			{
				*asked = messageBox->text();
				const QMessageBox::StandardButton standard = (buttonId == IDYES) ? QMessageBox::Yes : (buttonId == IDNO) ? QMessageBox::No : (buttonId == IDOK) ? QMessageBox::Ok : QMessageBox::Cancel;
				messageBox->button(standard)->click();
				return;
			}
			if (const auto* label = modal->findChild<QLabel*>())
				*asked = label->text();
			if (auto* button = qobject_cast<QAbstractButton*>(nppFindControl(modal, buttonId)))
				button->click();
		});
		_asked.reset(asked);
		return {};
	}

	void closingModifiedDocumentAsks()
	{
		const QString path = _files.filePath(QStringLiteral("askMe.txt"));

		// "No" discards the change
		writeFile(path, "before\n");
		open(path);
		::SendMessage(currentEditor(), SCI_ADDTEXT, 7, reinterpret_cast<LPARAM>("change "));
		answerModalDialog(IDNO);
		command(IDM_FILE_CLOSE);
		QVERIFY(_asked);
		QVERIFY2(_asked->contains(QStringLiteral("askMe.txt")), qPrintable(*_asked));
		QCOMPARE(readFile(path), QByteArray("before\n"));
		QVERIFY(currentPath() != path);

		// "Cancel" keeps the document open
		open(path);
		::SendMessage(currentEditor(), SCI_ADDTEXT, 7, reinterpret_cast<LPARAM>("change "));
		answerModalDialog(IDCANCEL);
		command(IDM_FILE_CLOSE);
		QCOMPARE(currentPath(), path);

		// "Yes" saves it
		answerModalDialog(IDYES);
		command(IDM_FILE_CLOSE);
		QVERIFY(currentPath() != path);
		QCOMPARE(readFile(path), QByteArray("change before\n"));
	}

	void aboutAndDebugInfo()
	{
		command(IDM_ABOUT);

		QWidget* about = visibleDialogWith(IDC_LICENCE_EDIT);
		QVERIFY(about);
		about->hide();

		command(IDM_DEBUGINFO);
		QWidget* debugInfo = visibleDialogWith(IDC_DEBUGINFO_EDIT);
		QVERIFY(debugInfo);
		const QString text = qobject_cast<QPlainTextEdit*>(nppFindControl(debugInfo, IDC_DEBUGINFO_EDIT))->toPlainText();
		QVERIFY2(text.contains(QStringLiteral("Notepad++ v")), qPrintable(text));
		QVERIFY2(text.contains(QStringLiteral("Scintilla/Lexilla included: 5.")), qPrintable(text));
		QVERIFY2(text.contains(QStringLiteral("Boost Regex included: 1")), qPrintable(text));
		debugInfo->hide();

		command(IDM_CMDLINEARGUMENTS);
		QWidget* arguments = visibleDialogWith(IDC_COMMANDLINEARGS_EDIT);
		QVERIFY(arguments);
		arguments->hide();
	}

	// The visible dialog holding a control with this ID
	QWidget* visibleDialogWith(int id) const
	{
		const QList<QDialog*> dialogs = mainWindow()->findChildren<QDialog*>();
		for (QDialog* dialog : dialogs)
		{
			if (dialog->isVisible() && nppFindControl(dialog, id))
				return dialog;
		}
		return nullptr;
	}

	void clickIn(QWidget* dialog, int id) const
	{
		auto* button = qobject_cast<QAbstractButton*>(nppFindControl(dialog, id));
		QVERIFY(button);
		button->click();
		QCoreApplication::processEvents();
	}

	void setLineEdit(QWidget* dialog, int id, const QString& text) const
	{
		auto* edit = qobject_cast<QLineEdit*>(nppFindControl(dialog, id));
		QVERIFY(edit);
		edit->setText(text);
	}

	void openWithContent(const QString& name, const QByteArray& content) const
	{
		const QString path = _files.filePath(name);
		writeFile(path, content);
		open(path);
	}

	void goToLine()
	{
		openWithContent(QStringLiteral("lines.txt"), "one\ntwo\nthree\nfour\n");

		command(IDM_SEARCH_GOTOLINE);
		QWidget* dialog = visibleDialogWith(ID_GOLINE_EDIT);
		QVERIFY(dialog);
		QCOMPARE(qobject_cast<QLineEdit*>(nppFindControl(dialog, ID_CURRLINE_EDIT))->text(), QStringLiteral("1"));

		setLineEdit(dialog, ID_GOLINE_EDIT, QStringLiteral("3"));
		clickIn(dialog, IDOK);
		QVERIFY(!dialog->isVisible());
		QCOMPARE(::SendMessage(currentEditor(), SCI_LINEFROMPOSITION, ::SendMessage(currentEditor(), SCI_GETCURRENTPOS)), LRESULT(2));
	}

	void findCharactersInRange()
	{
		openWithContent(QStringLiteral("chars.txt"), "plain caf\xC3\xA9 text\n");
		::SendMessage(currentEditor(), SCI_GOTOPOS, 0);

		command(IDM_SEARCH_FINDCHARINRANGE);
		QWidget* dialog = visibleDialogWith(ID_FINDCHAR_NEXT);
		QVERIFY(dialog);
		clickIn(dialog, ID_FINDCHAR_NEXT); // non ASCII is the default choice
		QCOMPARE(::SendMessage(currentEditor(), SCI_GETSELECTIONSTART), LRESULT(9));
		dialog->hide();
	}

	void columnEditorInsertsNumbers()
	{
		openWithContent(QStringLiteral("column.txt"), "a\nb\nc\n");
		::SendMessage(currentEditor(), SCI_GOTOPOS, 0);

		command(IDM_EDIT_COLUMNMODE);
		QWidget* dialog = visibleDialogWith(IDC_COL_INITNUM_EDIT);
		QVERIFY(dialog);
		clickIn(dialog, IDC_COL_NUM_RADIO);
		setLineEdit(dialog, IDC_COL_INITNUM_EDIT, QStringLiteral("5"));
		setLineEdit(dialog, IDC_COL_INCREASENUM_EDIT, QStringLiteral("2"));
		setLineEdit(dialog, IDC_COL_REPEATNUM_EDIT, QStringLiteral("1"));
		clickIn(dialog, IDOK);
		QCOMPARE(editorText(), QByteArray("5 a\n7 b\n9 c\n11")); // padded to the width of the largest number
	}

	void hashFromText()
	{
		command(IDM_TOOL_MD5_GENERATE);
		QWidget* dialog = visibleDialogWith(IDC_HASH_TEXT_EDIT);
		QVERIFY(dialog);

		auto* input = qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_HASH_TEXT_EDIT));
		auto* output = qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_HASH_RESULT_FOMTEXT_EDIT));
		input->setPlainText(QStringLiteral("abc"));
		QCOMPARE(output->toPlainText(), QStringLiteral("900150983cd24fb0d6963f7d28e17f72"));
		dialog->hide();

		command(IDM_TOOL_SHA256_GENERATE);
		dialog = visibleDialogWith(IDC_HASH_TEXT_EDIT);
		QVERIFY(dialog);
		input = qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_HASH_TEXT_EDIT));
		output = qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_HASH_RESULT_FOMTEXT_EDIT));
		input->setPlainText(QStringLiteral("abc"));
		QCOMPARE(output->toPlainText(), QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
		dialog->hide();
	}

	// The list of a dock widget that shows a table with this ID
	QTableWidget* panelList(int listId) const
	{
		const QList<QTableWidget*> tables = mainWindow()->findChildren<QTableWidget*>();
		for (QTableWidget* table : tables)
		{
			if (table->property("nppId").toInt() == listId)
				return table;
		}
		return nullptr;
	}

	void asciiPanelInsertsCharacters()
	{
		openWithContent(QStringLiteral("ascii.txt"), "");

		command(IDM_EDIT_CHAR_PANEL);
		QTableWidget* list = panelList(IDC_LIST_ANSICHAR);
		QVERIFY(list);
		QTRY_VERIFY(list->isVisible());
		QCOMPARE(list->rowCount(), 256);
		QCOMPARE(list->item(65, 0)->text(), QStringLiteral("65"));
		QCOMPARE(list->item(65, 1)->text(), QStringLiteral("41"));
		QCOMPARE(list->item(65, 2)->text(), QStringLiteral("A"));
		QCOMPARE(list->item(38, 3)->text(), QStringLiteral("&amp;"));

		emit list->cellDoubleClicked(65, 2);
		QCOMPARE(editorText(), QByteArray("A"));
		emit list->cellDoubleClicked(38, 3);
		QCOMPARE(editorText(), QByteArray("A&amp;"));

		command(IDM_EDIT_CHAR_PANEL);
		QTRY_VERIFY(!list->isVisible());
	}

	void clipboardHistoryPanel()
	{
		openWithContent(QStringLiteral("clipboard.txt"), "");

		command(IDM_EDIT_CLIPBOARDHISTORY_PANEL);
		QTableWidget* list = panelList(IDC_LIST_CLIPBOARD);
		QVERIFY(list);
		QTRY_VERIFY(list->isVisible());

		QApplication::clipboard()->setText(QStringLiteral("first text"));
		QApplication::clipboard()->setText(QStringLiteral("second text"));
		QTRY_COMPARE(list->item(0, 0)->text(), QStringLiteral("second text"));
		QCOMPARE(list->item(1, 0)->text(), QStringLiteral("first text"));

		emit list->cellDoubleClicked(1, 0);
		QCOMPARE(editorText(), QByteArray("first text"));

		// Copying something again moves it to the top instead of repeating it
		const int count = list->rowCount();
		QApplication::clipboard()->setText(QStringLiteral("first text"));
		QTRY_COMPARE(list->item(0, 0)->text(), QStringLiteral("first text"));
		QCOMPARE(list->rowCount(), count);

		command(IDM_EDIT_CLIPBOARDHISTORY_PANEL);
		QTRY_VERIFY(!list->isVisible());
	}

	void documentListPanel()
	{
		openWithContent(QStringLiteral("listOne.cpp"), "one\n");
		openWithContent(QStringLiteral("listTwo.txt"), "two\n");

		command(IDM_VIEW_DOCLIST);
		QTreeWidget* list = nullptr;
		const QList<QDockWidget*> docks = mainWindow()->findChildren<QDockWidget*>();
		for (const QDockWidget* dock : docks)
		{
			if (auto* tree = dock->findChild<QTreeWidget*>())
				list = tree;
		}
		QVERIFY(list);
		QTRY_VERIFY(list->isVisible());
		QCOMPARE(list->headerItem()->text(0), QStringLiteral("Name"));

		const auto rowOf = [list](const QString& name) -> QTreeWidgetItem*
		{
			const auto found = list->findItems(name, Qt::MatchExactly | Qt::MatchRecursive);
			return found.isEmpty() ? nullptr : found.first();
		};

		QTreeWidgetItem* row = rowOf(QStringLiteral("listTwo"));
		QVERIFY(row);
		QCOMPARE(row->text(1), QStringLiteral(".txt"));
		QVERIFY(row->isSelected());

		// A click activates the document
		row = rowOf(QStringLiteral("listOne"));
		QVERIFY(row);
		emit list->itemClicked(row, 0);
		QCOMPARE(currentPath(), _files.filePath(QStringLiteral("listOne.cpp")));

		// Activating a document elsewhere follows in the list
		open(_files.filePath(QStringLiteral("listTwo.txt")));
		QVERIFY(rowOf(QStringLiteral("listTwo"))->isSelected());
		QVERIFY(!rowOf(QStringLiteral("listOne"))->isSelected());

		// A modified document changes its row, a closed one leaves
		const int before = list->topLevelItemCount();
		open(_files.filePath(QStringLiteral("listOne.cpp")));
		command(IDM_FILE_CLOSE);
		QVERIFY(!rowOf(QStringLiteral("listOne")));
		QCOMPARE(list->topLevelItemCount(), before - 1);

		command(IDM_VIEW_DOCLIST);
		QTRY_VERIFY(!list->isVisible());
	}

	// A right click in the editor, with the texts of the menu it brought up. A given item is then chosen from it.
	QStringList rightClick(const QPoint& point, const QString& choose = {}, QContextMenuEvent::Reason reason = QContextMenuEvent::Mouse) const
	{
		QStringList texts;
		QTimer::singleShot(0, mainWindow(), [&texts, &choose]()
		{
			auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			if (!menu)
				return;
			QAction* chosen = nullptr;
			const QList<QAction*> actions = menu->actions();
			for (QAction* action : actions)
			{
				const QString text = action->text().remove(u'&');
				if (!action->isSeparator())
					texts << text;
				if (!choose.isEmpty() && text == choose)
					chosen = action;
			}
			menu->close();
			if (chosen)
				chosen->trigger();
		});
		HWND editor = currentEditor();
		QContextMenuEvent event(reason, point, editor->mapToGlobal(point));
		QApplication::sendEvent(editor, &event);
		QCoreApplication::processEvents();
		return texts;
	}

	QPoint pointOfPosition(int position) const
	{
		HWND editor = currentEditor();
		return QPoint(static_cast<int>(::SendMessage(editor, SCI_POINTXFROMPOSITION, 0, position)) + 1,
			static_cast<int>(::SendMessage(editor, SCI_POINTYFROMPOSITION, 0, position)) + 2);
	}

	void editorContextMenu()
	{
		using Span = std::pair<LRESULT, LRESULT>;
		openWithContent(QStringLiteral("rightClick.txt"), "first line\nsecond line\nthird line\n");
		HWND editor = currentEditor();

		// The items come from contextMenu.xml, and they do what they say
		const QStringList items = rightClick(pointOfPosition(3));
		for (const char* item : { "Cut", "Copy", "Paste", "Delete", "Select All" })
			QVERIFY2(items.contains(QString::fromLatin1(item)), qPrintable(items.join(u',')));
		rightClick(pointOfPosition(3), QStringLiteral("Select All"));
		QCOMPARE(::SendMessage(editor, SCI_GETSELECTIONEMPTY, 0, 0), LRESULT{0});
		QCOMPARE(::SendMessage(editor, SCI_GETSELECTIONSTART, 0, 0), LRESULT{0});
		QCOMPARE(::SendMessage(editor, SCI_GETSELECTIONEND, 0, 0), ::SendMessage(editor, SCI_GETLENGTH, 0, 0));

		// A click in the selection keeps it, so that its menu acts on it
		setSelection(0, 10);
		rightClick(pointOfPosition(4));
		QCOMPARE(selection(), Span(0, 10));

		// A click outside of it moves the caret there
		rightClick(pointOfPosition(14));
		QCOMPARE(selection(), Span(14, 14));

		// Unless the preference says otherwise, which does not go for the margins
		bool& keepsSelection = const_cast<ScintillaViewParams&>(NppParameters::getInstance().getSVP())._rightClickKeepsSelection;
		keepsSelection = true;
		setSelection(0, 10);
		rightClick(pointOfPosition(14));
		QCOMPARE(selection(), Span(0, 10));
		rightClick(QPoint(2, pointOfPosition(14).y()));
		QCOMPARE(::SendMessage(editor, SCI_GETSELECTIONEMPTY, 0, 0), LRESULT{1});
		keepsSelection = false;

		// The menu key leaves the selection alone and opens the menu at the caret
		setSelection(0, 10);
		const QStringList byKeyboard = rightClick(QPoint(1, 1), {}, QContextMenuEvent::Keyboard);
		QVERIFY(byKeyboard.contains(QStringLiteral("Copy")));
		QCOMPARE(selection(), Span(0, 10));
	}

	// Coming back to the application notices what others did to the open files
	void fileChangedOutsideIsNoticed()
	{
		const QString path = _files.filePath(QStringLiteral("changedOutside.txt"));
		writeFile(path, "before\n");
		open(path);
		QCOMPARE(editorText(), QByteArray("before\n"));

		const auto changeOnDisk = [&path](const QByteArray& content, int secondsLater)
		{
			writeFile(path, content);
			QFile file(path);
			QVERIFY(file.open(QIODevice::ReadWrite));
			QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(secondsLater), QFileDevice::FileModificationTime));
		};

		NppGUI& gui = NppParameters::getInstance().getNppGUI();
		const int detection = gui._fileAutoDetection;

		// Without asking when set to update silently
		gui._fileAutoDetection = cdEnabled4CurrentFile | cdAutoUpdate;
		changeOnDisk("silently\n", 10);
		Q_EMIT qApp->applicationStateChanged(Qt::ApplicationActive);
		QTRY_COMPARE(editorText(), QByteArray("silently\n"));

		// By default it asks, and "No" keeps what is in the editor
		gui._fileAutoDetection = cdEnabled4CurrentFile;
		changeOnDisk("asked\n", 20);
		QString asked;
		QTimer watch;
		QObject::connect(&watch, &QTimer::timeout, &watch, [&]()
		{
			auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
			if (!prompt)
				return;
			watch.stop();
			asked = prompt->text();
			prompt->button(QMessageBox::No)->click();
		});
		watch.start(20);
		Q_EMIT qApp->applicationStateChanged(Qt::ApplicationActive);
		QTRY_VERIFY(!asked.isEmpty());
		watch.stop();
		QVERIFY2(asked.contains(QStringLiteral("changedOutside.txt")), qPrintable(asked));
		QCOMPARE(editorText(), QByteArray("silently\n"));

		// And not at all when turned off
		gui._fileAutoDetection = cdDisabled;
		changeOnDisk("ignored\n", 30);
		Q_EMIT qApp->applicationStateChanged(Qt::ApplicationActive);
		QTest::qWait(100);
		QVERIFY(!QApplication::activeModalWidget());
		QCOMPARE(editorText(), QByteArray("silently\n"));

		gui._fileAutoDetection = detection;
	}

	void documentSwitcher()
	{
		openWithContent(QStringLiteral("switchOne.txt"), "one\n");
		openWithContent(QStringLiteral("switchTwo.txt"), "two\n");

		// With Ctrl up, as here, the switcher activates the next document at once
		NppParameters::getInstance().getNppGUI()._doTaskList = true;
		command(IDC_NEXT_DOC);
		QTRY_VERIFY(!QApplication::activeModalWidget());
		QVERIFY(currentPath() != _files.filePath(QStringLiteral("switchTwo.txt")));
	}

	void functionList()
	{
		openWithContent(QStringLiteral("functions.cpp"), "int zebra(int a)\n{\n\treturn a;\n}\n\nvoid apple()\n{\n}\n\nint mango()\n{\n\treturn 1;\n}\n");

		command(IDM_VIEW_FUNC_LIST);
		QTreeWidget* tree = nullptr;
		QTreeWidget* searchTree = nullptr;
		const QList<QTreeWidget*> trees = mainWindow()->findChildren<QTreeWidget*>();
		for (QTreeWidget* candidate : trees)
		{
			if (candidate->property("nppId").toInt() == IDC_LIST_FUNCLIST)
				tree = candidate;
			else if (candidate->property("nppId").toInt() == IDC_LIST_FUNCLIST_AUX)
				searchTree = candidate;
		}
		QVERIFY(tree);
		QVERIFY(searchTree);
		QTRY_VERIFY(tree->isVisible());

		QTRY_VERIFY(tree->topLevelItem(0));
		QTreeWidgetItem* root = tree->topLevelItem(0);
		QCOMPARE(root->text(0), QStringLiteral("functions.cpp"));
		QVERIFY(root->isExpanded());
		QCOMPARE(root->childCount(), 3);

		// In the order of the file until sorted
		QCOMPARE(root->child(0)->text(0), QStringLiteral("zebra"));
		QCOMPARE(root->child(1)->text(0), QStringLiteral("apple"));
		QCOMPARE(root->child(2)->text(0), QStringLiteral("mango"));

		// Going to a function
		tree->setCurrentItem(root->child(2));
		emit tree->itemDoubleClicked(root->child(2), 0);
		QCOMPARE(::SendMessage(currentEditor(), SCI_LINEFROMPOSITION, ::SendMessage(currentEditor(), SCI_GETCURRENTPOS)), LRESULT(9));

		// The sort button and back
		const QWidget* panel = tree->parentWidget();
		auto* sortButton = qobject_cast<QToolButton*>(nppFindControl(panel, IDC_SORTBUTTON_FUNCLIST));
		QVERIFY(sortButton);
		sortButton->click();
		QCOMPARE(root->child(0)->text(0), QStringLiteral("apple"));
		QCOMPARE(root->child(2)->text(0), QStringLiteral("zebra"));
		sortButton->click();
		QCOMPARE(root->child(0)->text(0), QStringLiteral("zebra"));

		// The filter shows the matching functions in the other tree
		auto* filter = qobject_cast<QLineEdit*>(nppFindControl(panel, IDC_SEARCHFIELD_FUNCLIST));
		QVERIFY(filter);
		filter->setText(QStringLiteral("MAN"));
		QTRY_VERIFY(searchTree->isVisible());
		QVERIFY(!tree->isVisible());
		QCOMPARE(searchTree->topLevelItem(0)->childCount(), 1);
		QCOMPARE(searchTree->topLevelItem(0)->child(0)->text(0), QStringLiteral("mango"));
		filter->setText(QStringLiteral("nothingLikeThis"));
		QVERIFY(filter->styleSheet().contains(QStringLiteral("background-color")));
		filter->clear();
		QTRY_VERIFY(tree->isVisible());
		screenshot(QStringLiteral("functionList"));

		command(IDM_VIEW_FUNC_LIST);
		QTRY_VERIFY(!tree->isVisible());
	}

	// A panel of a plugin is a widget with no message handler of Notepad++'s: the docking notifications and the
	// messages sent or posted to it come as NppMessageEvent, and it stays open by answering DMN_CLOSE with nonzero
	void pluginPanelNotifications()
	{
		class Panel : public QWidget
		{
		public:
			std::vector<UINT> notifications;
			std::vector<UINT> messages;
			bool stayOpen = false;

		protected:
			bool event(QEvent* event) override
			{
				if (event->type() != NppMessageEvent::eventType)
					return QWidget::event(event);
				auto* message = static_cast<NppMessageEvent*>(event);
				messages.push_back(message->message);
				if (message->message == WM_NOTIFY)
				{
					const UINT code = LOWORD(reinterpret_cast<const NMHDR*>(message->lParam)->code);
					notifications.push_back(code);
					if (code == DMN_CLOSE && stayOpen)
						message->result = 1;
				}
				return true;
			}
		};
		const auto has = [](const std::vector<UINT>& list, UINT value) { return std::find(list.begin(), list.end(), value) != list.end(); };

		auto* panel = new Panel;
		tTbData data{};
		data.hClient = panel;
		data.pszName = L"Notified Panel";
		data.dlgID = -1;
		data.uMask = DWS_DF_CONT_RIGHT;
		data.pszModuleName = L"notifiedPanel.so";
		send(NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&data));
		send(NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(panel));
		QDockWidget* dock = nullptr;
		for (QWidget* w = panel->parentWidget(); w && !dock; w = w->parentWidget())
			dock = qobject_cast<QDockWidget*>(w);
		QVERIFY(dock);
		QTRY_VERIFY(dock->isVisible());

		// The dock's close button asks the panel first
		panel->stayOpen = true;
		dock->close();
		QVERIFY(has(panel->notifications, DMN_CLOSE));
		QVERIFY(dock->isVisible());
		panel->stayOpen = false;
		dock->close();
		QTRY_VERIFY(!dock->isVisible());

		// Floating and docking it again
		send(NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(panel));
		QTRY_VERIFY(dock->isVisible());
		panel->notifications.clear();
		dock->setFloating(true);
		QTRY_VERIFY(has(panel->notifications, DMN_FLOAT));
		dock->setFloating(false);
		QTRY_VERIFY(has(panel->notifications, DMN_DOCK));

		// A plugin's own messages to its widgets, sent and posted
		constexpr UINT ownMessage = WM_USER + 77;
		::SendMessage(panel, ownMessage, 0, 0);
		QVERIFY(has(panel->messages, ownMessage));
		panel->messages.clear();
		QVERIFY(::PostMessage(panel, ownMessage, 0, 0));
		QVERIFY(!has(panel->messages, ownMessage));
		QTRY_VERIFY(has(panel->messages, ownMessage));

		send(NPPM_DMMHIDE, 0, reinterpret_cast<LPARAM>(panel));
	}

	void pluginIsLoadedAndWorks()
	{
		QLibrary plugin(_pluginPath); // the same library as the application loaded, not a second copy
		QVERIFY2(plugin.load(), qPrintable(plugin.errorString()));

		using IntGetter = int (*)();
		const auto readyCount = reinterpret_cast<IntGetter>(plugin.resolve("testPluginReadyCount"));
		const auto bufferActivatedCount = reinterpret_cast<IntGetter>(plugin.resolve("testPluginBufferActivatedCount"));
		const auto lastVersion = reinterpret_cast<long (*)()>(plugin.resolve("testPluginLastVersion"));
		const auto commandId = reinterpret_cast<int (*)(int)>(plugin.resolve("testPluginCommandId"));
		QVERIFY(readyCount && bufferActivatedCount && lastVersion && commandId);

		// Notifications arrive
		QCOMPARE(readyCount(), 1);
		const int activatedBefore = bufferActivatedCount();
		openWithContent(QStringLiteral("plugin.txt"), "text ");
		QVERIFY(bufferActivatedCount() > activatedBefore);

		// The plugin's menu sits in the Plugins menu, with its command, separator, and shortcut
		const int greetingId = commandId(0);
		QVERIFY(greetingId >= ID_PLUGINS_CMD);
		const HMENU mainMenu = reinterpret_cast<HMENU>(send(NPPM_INTERNAL_GETMENU));
		QAction* greeting = nppFindMenuAction(mainMenu, greetingId);
		QVERIFY(greeting);
		QCOMPARE(greeting->text(), QStringLiteral("Insert greeting"));
		QCOMPARE(greeting->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+Shift+G")));
		QCOMPARE(nppFindMenuAction(mainMenu, commandId(2))->text(), QStringLiteral("Ask version"));
		QCOMPARE(greeting->parentWidget()->actions().size(), 3);
		QVERIFY(greeting->parentWidget()->actions().at(1)->isSeparator());

		// Its command edits the document through a Scintilla message
		::SendMessage(currentEditor(), SCI_DOCUMENTEND);
		command(greetingId);
		QCOMPARE(editorText(), QByteArray("text greetings from a plugin"));

		// And it can ask the application things with the public messages
		command(commandId(2));
		QCOMPARE(lastVersion(), static_cast<long>(send(NPPM_GETNPPVERSION)));
		QVERIFY(lastVersion() != 0);
	}

	void documentMap()
	{
		QByteArray content;
		for (int i = 1; i <= 400; ++i)
			content += "line " + QByteArray::number(i) + " of the long file\n";
		openWithContent(QStringLiteral("map.txt"), content);

		command(IDM_VIEW_DOC_MAP);
		QDockWidget* mapDock = nullptr;
		const QList<QDockWidget*> docks = mainWindow()->findChildren<QDockWidget*>();
		for (QDockWidget* dock : docks)
		{
			if (dock->windowTitle() == QStringLiteral("Document Map"))
				mapDock = dock;
		}
		QVERIFY(mapDock);
		QTRY_VERIFY(mapDock->isVisible());
		QTest::qWait(50);

		auto* mapEditor = mapDock->findChild<ScintillaEditBase*>();
		QVERIFY(mapEditor);
		QCOMPARE(textOf(mapEditor), content); // it shows the current document
		// The right area has the width of config.xml, which panels made later than the window take as on Windows
		QCOMPARE(mapDock->width(), NppParameters::getInstance().getNppGUI()._dockingData._rightWidth);
		QCOMPARE(mapEditor->width(), mapDock->widget()->width());

		// The zone covers the map, and clicking far down in it scrolls the document
		QWidget* zone = nullptr;
		const QList<QWidget*> children = mapDock->widget()->findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
		for (QWidget* child : children)
		{
			if (child != mapEditor)
				zone = child;
		}
		QVERIFY(zone);
		QCOMPARE(zone->geometry(), mapEditor->geometry());

		const LRESULT before = ::SendMessage(currentEditor(), SCI_GETFIRSTVISIBLELINE);
		QTest::mouseClick(zone, Qt::LeftButton, Qt::NoModifier, QPoint(10, zone->height() - 5));
		QVERIFY(::SendMessage(currentEditor(), SCI_GETFIRSTVISIBLELINE) > before);
		screenshot(QStringLiteral("documentMap"));

		// The area keeps the width that the user gives it while no panel is in it
		mainWindow()->resizeDocks({ mapDock }, { 260 }, Qt::Horizontal);
		QTRY_COMPARE(mapDock->width(), 260);
		command(IDM_VIEW_DOC_MAP);
		QTRY_VERIFY(!mapDock->isVisible());
		command(IDM_VIEW_DOC_MAP);
		QTRY_VERIFY(mapDock->isVisible());
		QTRY_COMPARE(mapDock->width(), 260);

		command(IDM_VIEW_DOC_MAP);
		QTRY_VERIFY(!mapDock->isVisible());
	}

	void folderAsWorkspace()
	{
		QTemporaryDir folder;
		QVERIFY(folder.isValid());
		const QDir root(folder.path());
		QVERIFY(root.mkpath(QStringLiteral("zdir/inner")));
		QVERIFY(root.mkpath(QStringLiteral("adir")));
		QFile(root.filePath(QStringLiteral("a.txt"))).open(QIODevice::WriteOnly);
		const auto write = [](const QString& path, const QByteArray& content)
		{
			QFile file(path);
			QVERIFY(file.open(QIODevice::WriteOnly));
			file.write(content);
		};
		write(root.filePath(QStringLiteral("b.txt")), "in the workspace\n");
		write(root.filePath(QStringLiteral("zdir/inner/deep.txt")), "deep\n");

		const std::vector<std::wstring> dropped{ folder.path().toStdWString() };
		send(WM_DROPFILES, reinterpret_cast<WPARAM>(&dropped));

		QTreeWidget* tree = nullptr;
		const QList<QTreeWidget*> trees = mainWindow()->findChildren<QTreeWidget*>();
		for (QTreeWidget* candidate : trees)
		{
			if (candidate->property("nppId").toInt() == ID_FILEBROWSERTREEVIEW)
				tree = candidate;
		}
		QVERIFY(tree);
		QTRY_VERIFY(tree->isVisible());
		QCOMPARE(tree->topLevelItemCount(), 1);

		const auto names = [](QTreeWidgetItem* parent)
		{
			QStringList result;
			for (int i = 0; i < parent->childCount(); ++i)
				result << parent->child(i)->text(0);
			return result;
		};

		// Folders first, then files, each sorted
		QTreeWidgetItem* rootItem = tree->topLevelItem(0);
		QCOMPARE(rootItem->text(0), QFileInfo(folder.path()).fileName());
		QCOMPARE(names(rootItem), (QStringList{ QStringLiteral("adir"), QStringLiteral("zdir"), QStringLiteral("a.txt"), QStringLiteral("b.txt") }));
		QCOMPARE(names(rootItem->child(1)), QStringList{ QStringLiteral("inner") });

		// Opening a file from the tree
		tree->setCurrentItem(rootItem->child(3));
		QMetaObject::invokeMethod(tree, [tree, rootItem]() { Q_EMIT tree->itemDoubleClicked(rootItem->child(3), 0); }, Qt::DirectConnection);
		QTRY_COMPARE(currentPath(), root.filePath(QStringLiteral("b.txt")));
		QCOMPARE(editorText(), QByteArray("in the workspace\n"));

		// Changes on the disk reach the tree
		write(root.filePath(QStringLiteral("c.txt")), "new\n");
		QTRY_VERIFY(names(rootItem).contains(QStringLiteral("c.txt")));
		QCOMPARE(names(rootItem).last(), QStringLiteral("c.txt"));

		write(root.filePath(QStringLiteral("zdir/inner/deeper.txt")), "new\n");
		QTRY_VERIFY(rootItem->child(1)->child(0)->childCount() == 2);

		QVERIFY(root.mkpath(QStringLiteral("ndir/sub")));
		QTRY_VERIFY(names(rootItem).contains(QStringLiteral("ndir")));
		QCOMPARE(names(rootItem).indexOf(QStringLiteral("ndir")), 1);
		QTRY_COMPARE(names(rootItem->child(1)), QStringList{ QStringLiteral("sub") });

		QVERIFY(QFile::remove(root.filePath(QStringLiteral("a.txt"))));
		QTRY_VERIFY(!names(rootItem).contains(QStringLiteral("a.txt")));

		QVERIFY(QFile::rename(root.filePath(QStringLiteral("c.txt")), root.filePath(QStringLiteral("d.txt"))));
		QTRY_VERIFY(names(rootItem).contains(QStringLiteral("d.txt")));
		QTRY_VERIFY(!names(rootItem).contains(QStringLiteral("c.txt")));

		// The panel's buttons: locate the current file, fold all, expand all
		const QList<QToolButton*> tools = tree->parentWidget()->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly);
		QCOMPARE(tools.size(), 3);
		tools[0]->click();
		QCOMPARE(tree->currentItem()->text(0), QStringLiteral("b.txt"));
		QVERIFY(rootItem->isExpanded());
		tools[1]->click();
		QVERIFY(!rootItem->isExpanded());
		tools[2]->click();
		QVERIFY(rootItem->isExpanded());
		QVERIFY(rootItem->child(2)->isExpanded() || rootItem->child(2)->childCount() == 0);

		screenshot(QStringLiteral("folder-as-workspace"));

		command(IDM_VIEW_FILEBROWSER);
		QTRY_VERIFY(!tree->isVisible());
		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	// Clicks a button that pops up a menu, and chooses the item that starts with the given text
	bool chooseFromMenu(QToolButton* button, const QString& text) const
	{
		bool isFound = false;
		QTimer::singleShot(0, button, [text, &isFound]()
		{
			auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			if (!menu)
				return;

			QAction* chosen = nullptr;
			const QList<QAction*> actions = menu->actions();
			for (QAction* action : actions)
			{
				if (!chosen && action->text().startsWith(text))
					chosen = action;
			}
			menu->close();
			if (chosen)
			{
				isFound = true;
				chosen->trigger();
			}
		});
		button->click();
		return isFound;
	}

	void projectPanel()
	{
		QTemporaryDir folder;
		QVERIFY(folder.isValid());
		const QDir root(folder.path());
		QVERIFY(root.mkpath(QStringLiteral("src/deep")));
		const auto write = [](const QString& path, const QByteArray& content)
		{
			QFile file(path);
			QVERIFY(file.open(QIODevice::WriteOnly));
			file.write(content);
		};
		write(root.filePath(QStringLiteral("src/one.txt")), "one\n");
		write(root.filePath(QStringLiteral("src/deep/two.txt")), "two needleproject\n");

		// A workspace written elsewhere: relative with Windows separators, absolute, and missing
		const QString absolute = root.filePath(QStringLiteral("src/one.txt"));
		const QString workspace = root.filePath(QStringLiteral("test.workspace"));
		write(workspace, QByteArrayLiteral("<NotepadPlus><Project name=\"Proj\"><Folder name=\"Dir\"><File name=\"src\\deep\\two.txt\" /></Folder>")
			+ "<File name=\"" + absolute.toUtf8() + "\" /><File name=\"gone.txt\" /></Project></NotepadPlus>");

		NppParameters::getInstance().setWorkSpaceFilePath(0, workspace.toStdWString().c_str());
		command(IDM_VIEW_PROJECT_PANEL_1);
		QTreeWidget* tree = nullptr;
		const QList<QTreeWidget*> trees = mainWindow()->findChildren<QTreeWidget*>();
		for (QTreeWidget* candidate : trees)
		{
			if (candidate->property("nppId").toInt() == ID_PROJECTTREEVIEW)
				tree = candidate;
		}
		QVERIFY(tree);
		QTRY_VERIFY(tree->isVisible());
		QCOMPARE(tree->topLevelItemCount(), 1);
		QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("test.workspace"));
		QTreeWidgetItem* project = tree->topLevelItem(0)->child(0);
		QCOMPARE(project->text(0), QStringLiteral("Proj"));
		QCOMPARE(project->childCount(), 3);
		QCOMPARE(project->child(0)->text(0), QStringLiteral("Dir"));
		QCOMPARE(project->child(0)->child(0)->text(0), QStringLiteral("two.txt"));
		QCOMPARE(project->child(1)->text(0), QStringLiteral("one.txt"));
		QCOMPARE(project->child(2)->text(0), QStringLiteral("gone.txt"));

		// Opening a file from the tree
		tree->setCurrentItem(project->child(0)->child(0));
		Q_EMIT tree->itemDoubleClicked(project->child(0)->child(0), 0);
		QTRY_COMPARE(currentPath(), root.filePath(QStringLiteral("src/deep/two.txt")));

		// A renamed label renames the file's path, and the workspace needs saving
		tree->setCurrentItem(project->child(1));
		tree->editItem(project->child(1));

		// A finished editor stays around, hidden, until it is deleted later
		const auto openEditor = [tree]() -> QLineEdit*
		{
			const QList<QLineEdit*> editors = tree->findChildren<QLineEdit*>();
			for (QLineEdit* candidate : editors)
			{
				if (candidate->isVisible())
					return candidate;
			}
			return nullptr;
		};
		QLineEdit* editor = openEditor();
		QVERIFY(editor);
		editor->setText(QStringLiteral("two.txt"));
		QTest::keyClick(editor, Qt::Key_Return);
		QTRY_COMPARE(project->child(1)->text(0), QStringLiteral("two.txt"));

		const auto reader = [&workspace]()
		{
			QFile file(workspace);
			return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
		};

		const QList<QToolButton*> buttons = tree->parentWidget()->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly);
		QCOMPARE(buttons.size(), 2);
		QToolButton* workspaceButton = buttons[0];
		QToolButton* editButton = buttons[1];

		// The menus of the panel's buttons act on the selected item
		tree->setCurrentItem(project->child(2));

		// The dialog for the path of a file comes up with that path, in order
		// It opens from a menu, so it is not up yet when the menu's own event loop starts
		QStringList relocateProblems;
		QString relocatePath;
		QTimer relocateWatch;
		QObject::connect(&relocateWatch, &QTimer::timeout, &relocateWatch, [&]()
		{
			QWidget* relocate = QApplication::activeModalWidget();
			if (!relocate)
				return;
			relocateWatch.stop();
			relocateProblems = layoutProblems(relocate, QStringLiteral("relocate"));
			if (auto* path = qobject_cast<QLineEdit*>(nppFindControl(relocate, IDC_EDIT_FILEFULLPATHNAME)))
				relocatePath = path->text();
			relocate->close();
		});
		relocateWatch.start(20);
		QVERIFY(chooseFromMenu(editButton, QStringLiteral("Modify File Path")));
		relocateWatch.stop();
		QVERIFY2(relocateProblems.isEmpty(), qPrintable(relocateProblems.join(u'\n')));
		QVERIFY2(relocatePath.endsWith(QStringLiteral("gone.txt")), qPrintable(relocatePath));

		// A compositor gives focus back after a while, and that would close a label editor opened before it
		QTest::qWaitForWindowActive(mainWindow(), 1000);
		QTest::qWait(100);

		QVERIFY(chooseFromMenu(editButton, QStringLiteral("Move Up")));
		QCOMPARE(project->child(1)->text(0), QStringLiteral("gone.txt"));
		QVERIFY(chooseFromMenu(editButton, QStringLiteral("Move Down")));
		QCOMPARE(project->child(2)->text(0), QStringLiteral("gone.txt"));

		// A new folder starts with its label being edited
		tree->setCurrentItem(project);
		QVERIFY(chooseFromMenu(editButton, QStringLiteral("Add Folder")));
		QCOMPARE(project->childCount(), 4);
		editor = openEditor();
		QVERIFY(editor);
		editor->setText(QStringLiteral("Named"));
		QTest::keyClick(editor, Qt::Key_Return);
		QTRY_COMPARE(project->child(3)->text(0), QStringLiteral("Named")); // the delegate commits through a queued call

		// Removing an empty folder asks nothing
		tree->setCurrentItem(project->child(3));
		QVERIFY(chooseFromMenu(editButton, QStringLiteral("Remove")));
		QCOMPARE(project->childCount(), 3);

		// Saving writes paths relative to the workspace
		tree->setCurrentItem(tree->topLevelItem(0));
		QVERIFY(chooseFromMenu(workspaceButton, QStringLiteral("Save")));
		const QByteArray saved = reader();
		QVERIFY2(saved.contains("src/two.txt"), saved.constData());
		QVERIFY2(saved.contains("src/deep/two.txt"), saved.constData());
		QVERIFY2(!saved.contains(folder.path().toUtf8()), saved.constData());

		// Find in Projects searches the files of the workspace, and only those that exist
		QVERIFY(chooseFromMenu(workspaceButton, QStringLiteral("Find in Projects")));
		QTRY_VERIFY(findDialog()->isVisible());
		setComboText(IDFINDWHAT, QStringLiteral("needleproject"));
		setComboText(IDD_FINDINFILES_FILTERS_COMBO, QStringLiteral("*.*"));
		click(IDD_FINDINFILES_FIND_BUTTON);
		QTRY_VERIFY_WITH_TIMEOUT(finderText().contains("two needleproject"), 10000);
		QVERIFY2(finderText().contains("1 hit in 1 file"), finderText().constData());
		findDialog()->hide();

		screenshot(QStringLiteral("project-panel"));

		command(IDM_VIEW_PROJECT_PANEL_1);
		QTRY_VERIFY(!tree->isVisible());
		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	// Runs a step while the Windows dialog is open, which then has to close it
	void withWindowsDialog(const std::function<void(QDialog*, QTableWidget*)>& step) const
	{
		bool hasRun = false;
		QTimer::singleShot(0, mainWindow(), [&step, &hasRun]()
		{
			auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
			QVERIFY(dialog);
			auto* table = qobject_cast<QTableWidget*>(nppFindControl(dialog, IDC_WINDOWS_LIST));
			QVERIFY(table);
			hasRun = true;
			step(dialog, table);
			if (QApplication::activeModalWidget() == dialog)
				dialog->reject();
		});
		command(IDM_WINDOW_WINDOWS);
		QVERIFY(hasRun);
	}

	void windowsDialog()
	{
		const auto clickButton = [](QDialog* dialog, int id)
		{
			auto* button = qobject_cast<QAbstractButton*>(nppFindControl(dialog, id));
			QVERIFY(button);
			QVERIFY(button->isEnabled());
			button->click();
		};
		const auto rowOf = [](QTableWidget* table, const QString& name)
		{
			for (int row = 0; row < table->rowCount(); ++row)
			{
				if (table->item(row, 0)->text() == name)
					return row;
			}
			return -1;
		};

		openWithContent(QStringLiteral("winC.txt"), "ccc\n");
		openWithContent(QStringLiteral("winA10.txt"), "a\n");
		openWithContent(QStringLiteral("winA9.txt"), "aa\n");
		openWithContent(QStringLiteral("winB.cpp"), "bbbb\n");
		const int nbDocs = static_cast<int>(send(NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW));

		withWindowsDialog([&](QDialog* dialog, QTableWidget* table)
		{
			if (const QString dir = qEnvironmentVariable("NPP_TEST_APP_DIR"); !dir.isEmpty())
				dialog->grab().save(QDir(dir).filePath(QStringLiteral("windows-dialog.png")));

			QCOMPARE(table->rowCount(), nbDocs);
			QVERIFY(dialog->windowTitle().endsWith(QString::number(nbDocs)));

			// The current document starts selected, and every column is filled
			const int current = rowOf(table, QStringLiteral("winB.cpp"));
			QVERIFY(current >= 0);
			QVERIFY(table->item(current, 0)->isSelected());
			QCOMPARE(table->item(current, 1)->text() + QStringLiteral("winB.cpp"), _files.filePath(QStringLiteral("winB.cpp")));
			QCOMPARE(table->item(current, 2)->text(), QStringLiteral("cpp"));
			QCOMPARE(table->item(current, 3)->text(), QStringLiteral("5"));
			QVERIFY(QDateTime::fromString(table->item(current, 4)->text(), QStringLiteral("yyyy-MM-dd HH:mm:ss")).isValid());

			// Sorted by name, with numbers in their order, then reversed
			Q_EMIT table->horizontalHeader()->sectionClicked(0);
			QVERIFY(table->horizontalHeaderItem(0)->text().startsWith(QString::fromUtf8("▽")));
			QVERIFY(rowOf(table, QStringLiteral("winA9.txt")) < rowOf(table, QStringLiteral("winA10.txt")));
			QVERIFY(rowOf(table, QStringLiteral("winA10.txt")) < rowOf(table, QStringLiteral("winB.cpp")));
			QVERIFY(table->item(rowOf(table, QStringLiteral("winB.cpp")), 0)->isSelected()); // the selection follows the document
			Q_EMIT table->horizontalHeader()->sectionClicked(0);
			QVERIFY(table->horizontalHeaderItem(0)->text().startsWith(QString::fromUtf8("△")));
			QVERIFY(rowOf(table, QStringLiteral("winB.cpp")) < rowOf(table, QStringLiteral("winA10.txt")));

			// By size, which is a number and not text
			Q_EMIT table->horizontalHeader()->sectionClicked(3);
			QVERIFY(rowOf(table, QStringLiteral("winA10.txt")) < rowOf(table, QStringLiteral("winA9.txt")));
			QVERIFY(rowOf(table, QStringLiteral("winA9.txt")) < rowOf(table, QStringLiteral("winB.cpp")));

			// Typing goes through the names that start with the character
			table->setFocus();
			QTest::keyClick(table, 'w');
			QCOMPARE(table->selectionModel()->selectedRows().size(), 1);
			const int firstMatch = table->selectionModel()->selectedRows().first().row();
			QVERIFY(table->item(firstMatch, 0)->text().startsWith(QLatin1Char('w')));
			QTest::keyClick(table, 'w');
			QVERIFY(table->selectionModel()->selectedRows().first().row() != firstMatch);
			QTest::keyClick(table, '#');
			QCOMPARE(table->selectionModel()->selectedRows().size(), 1); // no match leaves the selection

			// With several selected, there is no activating
			table->clearSelection();
			auto* activate = qobject_cast<QAbstractButton*>(nppFindControl(dialog, IDOK));
			QVERIFY(!activate->isEnabled());
			table->selectRow(rowOf(table, QStringLiteral("winC.txt")));
			QVERIFY(activate->isEnabled());
			clickButton(dialog, IDOK);
		});
		QCOMPARE(currentPath(), _files.filePath(QStringLiteral("winC.txt")));

		// Closing documents from the list
		withWindowsDialog([&](QDialog* dialog, QTableWidget* table)
		{
			const QItemSelectionModel::SelectionFlags addRow = QItemSelectionModel::Select | QItemSelectionModel::Rows;
			table->clearSelection();
			table->selectionModel()->select(table->model()->index(rowOf(table, QStringLiteral("winA9.txt")), 0), addRow);
			table->selectionModel()->select(table->model()->index(rowOf(table, QStringLiteral("winA10.txt")), 0), addRow);
			clickButton(dialog, IDC_WINDOWS_CLOSE);
			QCOMPARE(table->rowCount(), nbDocs - 2);
			QCOMPARE(rowOf(table, QStringLiteral("winA9.txt")), -1);
			QCOMPARE(rowOf(table, QStringLiteral("winA10.txt")), -1);
			QVERIFY(rowOf(table, QStringLiteral("winB.cpp")) >= 0);
			QVERIFY(dialog->windowTitle().endsWith(QString::number(nbDocs - 2)));
		});
		QCOMPARE(send(NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW), nbDocs - 2);

		// The tabs sorted from the menu, keeping the current document
		QTabBar* tabs = nullptr;
		const QList<QTabBar*> bars = mainWindow()->findChildren<QTabBar*>();
		for (QTabBar* bar : bars)
		{
			for (int i = 0; i < bar->count(); ++i)
			{
				if (bar->tabText(i) == QStringLiteral("winC.txt"))
					tabs = bar;
			}
		}
		QVERIFY(tabs);
		const auto tabIndex = [tabs](const QString& name)
		{
			for (int i = 0; i < tabs->count(); ++i)
			{
				if (tabs->tabText(i) == name)
					return i;
			}
			return -1;
		};
		command(IDM_WINDOW_SORT_FN_ASC);
		QVERIFY(tabIndex(QStringLiteral("winB.cpp")) < tabIndex(QStringLiteral("winC.txt")));
		QCOMPARE(currentPath(), _files.filePath(QStringLiteral("winC.txt")));
		command(IDM_WINDOW_SORT_FN_DSC);
		QVERIFY(tabIndex(QStringLiteral("winC.txt")) < tabIndex(QStringLiteral("winB.cpp")));
		QCOMPARE(currentPath(), _files.filePath(QStringLiteral("winC.txt")));

		// The Window menu lists the documents and switches to them, as does the list at the end of the menu bar
		for (int menuIndex : { MENUINDEX_WINDOW, MENUINDEX_LIST })
		{
			QMenu* menu = mainWindow()->menuBar()->actions().at(menuIndex)->menu();
			QVERIFY(menu);
			Q_EMIT menu->aboutToShow();

			QAction* other = nullptr;
			QAction* current = nullptr;
			const QList<QAction*> actions = menu->actions();
			for (QAction* action : actions)
			{
				if (action->text().contains(QStringLiteral("winB.cpp")))
					other = action;
				else if (action->text().contains(QStringLiteral("winC.txt")))
					current = action;
			}
			QVERIFY(other);
			QVERIFY(current);
			QVERIFY(current->isChecked());
			QVERIFY(!other->isChecked());

			other->trigger();
			QTRY_COMPARE(currentPath(), _files.filePath(QStringLiteral("winB.cpp")));

			command(IDM_FILE_NEW); // a list only shrinks once it is shown again
			Q_EMIT menu->aboutToShow();
			send(WM_COMMAND, IDM_FILE_CLOSE);
			Q_EMIT menu->aboutToShow();

			const QString cPath = _files.filePath(QStringLiteral("winC.txt"));
			send(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(cPath.toStdWString().c_str()));
			QCOMPARE(currentPath(), cPath);
		}

		send(WM_COMMAND, IDM_FILE_CLOSE);
		const QString bPath = _files.filePath(QStringLiteral("winB.cpp"));
		send(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(bPath.toStdWString().c_str()));
		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	void styleConfigurator()
	{
		openWithContent(QStringLiteral("style.cpp"), "int main()\n{\n}\n");

		command(IDM_LANGSTYLE_CONFIG_DLG);
		QWidget* dialog = visibleDialogWith(IDC_LANGUAGES_COMBO);
		QVERIFY(dialog);

		auto* languages = qobject_cast<QComboBox*>(nppFindControl(dialog, IDC_LANGUAGES_COMBO));
		auto* styles = qobject_cast<QListWidget*>(nppFindControl(dialog, IDC_STYLES_LIST));
		auto* themes = qobject_cast<QComboBox*>(nppFindControl(dialog, IDC_SWITCH2THEME_COMBO));
		auto* foreground = dialog->findChild<QAbstractButton*>(QStringLiteral("fgColour"));
		auto* background = dialog->findChild<QAbstractButton*>(QStringLiteral("bgColour"));
		auto* save = qobject_cast<QAbstractButton*>(nppFindControl(dialog, IDC_SAVECLOSE_BUTTON));
		auto* cancel = qobject_cast<QAbstractButton*>(nppFindControl(dialog, IDCANCEL));
		QVERIFY(languages && styles && themes && foreground && background && save && cancel);

		QCOMPARE(languages->itemText(0), QStringLiteral("Global Styles"));
		QVERIFY(languages->findText(QStringLiteral("C++")) > 0);
		QVERIFY(styles->count() > 10);
		QCOMPARE(styles->currentRow(), 0);
		QVERIFY(!save->isEnabled());

		const auto selectStyle = [&](const QString& language, const QString& style)
		{
			const int index = languages->findText(language);
			QVERIFY(index >= 0);
			languages->setCurrentIndex(index);
			Q_EMIT languages->activated(index);
			const QList<QListWidgetItem*> found = styles->findItems(style, Qt::MatchExactly);
			QVERIFY(!found.isEmpty());
			styles->setCurrentItem(found.first());
		};
		const auto editorStyle = [this](int message, int style)
		{
			return ::SendMessage(currentEditor(), message, style, 0);
		};

		// Each kind of style shows only its own controls
		selectStyle(QStringLiteral("C++"), QStringLiteral("INSTRUCTION WORD"));
		QVERIFY(nppFindControl(dialog, IDC_USER_KEYWORDS_EDIT)->isVisible());
		QVERIFY(nppFindControl(dialog, IDC_USER_EXT_EDIT)->isVisible());
		QVERIFY(!nppFindControl(dialog, IDC_GLOBAL_FG_CHECK)->isVisible());
		QVERIFY(qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_DEF_KEYWORDS_EDIT))->toPlainText().contains(QStringLiteral("while")));
		QVERIFY(qobject_cast<QLineEdit*>(nppFindControl(dialog, IDC_DEF_EXT_EDIT))->text().contains(QStringLiteral("cpp")));
		QVERIFY(!save->isEnabled()); // looking changes nothing
		selectStyle(QStringLiteral("Global Styles"), QStringLiteral("Global override"));
		QVERIFY(nppFindControl(dialog, IDC_GLOBAL_FG_CHECK)->isVisible());
		QVERIFY(!nppFindControl(dialog, IDC_USER_KEYWORDS_EDIT)->isVisible());
		QVERIFY(!nppFindControl(dialog, IDC_USER_EXT_EDIT)->isVisible());
		if (const QString dir = qEnvironmentVariable("NPP_TEST_APP_DIR"); !dir.isEmpty())
			dialog->grab().save(QDir(dir).filePath(QStringLiteral("style-global.png")));

		// Changes show in the editor at once
		selectStyle(QStringLiteral("C++"), QStringLiteral("INSTRUCTION WORD"));
		if (const QString dir = qEnvironmentVariable("NPP_TEST_APP_DIR"); !dir.isEmpty())
			dialog->grab().save(QDir(dir).filePath(QStringLiteral("style-cpp.png")));
		const int wordStyle = 5; // SCE_C_WORD
		const LRESULT wasBold = editorStyle(SCI_STYLEGETBOLD, wordStyle);
		const LRESULT wasColour = editorStyle(SCI_STYLEGETFORE, wordStyle);
		qobject_cast<QAbstractButton*>(nppFindControl(dialog, IDC_BOLD_CHECK))->click();
		QCOMPARE(editorStyle(SCI_STYLEGETBOLD, wordStyle) != 0, wasBold == 0);
		QVERIFY(save->isEnabled());

		// A colour from the popup
		QCOMPARE(pickColour(foreground, RGB(0, 128, 128)), true);
		QCOMPARE(editorStyle(SCI_STYLEGETFORE, wordStyle), static_cast<LRESULT>(RGB(0, 128, 128)));

		// Cancelling goes back to how things were
		cancel->click();
		QVERIFY(!dialog->isVisible());
		QCOMPARE(editorStyle(SCI_STYLEGETBOLD, wordStyle), wasBold);
		QCOMPARE(editorStyle(SCI_STYLEGETFORE, wordStyle), wasColour);

		// Another theme, which is also undone by cancelling
		command(IDM_LANGSTYLE_CONFIG_DLG);
		QVERIFY(dialog->isVisible());
		QVERIFY(themes->count() > 1);
		const int zenburn = themes->findText(QStringLiteral("Zenburn"));
		QVERIFY(zenburn > 0);
		const LRESULT lightBackground = editorStyle(SCI_STYLEGETBACK, 32);
		themes->setCurrentIndex(zenburn);
		Q_EMIT themes->activated(zenburn);
		QVERIFY(editorStyle(SCI_STYLEGETBACK, 32) != lightBackground);
		cancel->click();
		QCOMPARE(editorStyle(SCI_STYLEGETBACK, 32), lightBackground);

		// Saving writes the theme's file
		command(IDM_LANGSTYLE_CONFIG_DLG);
		selectStyle(QStringLiteral("C++"), QStringLiteral("INSTRUCTION WORD"));
		QCOMPARE(pickColour(foreground, RGB(0, 128, 64)), true);
		auto* userKeywords = qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_USER_KEYWORDS_EDIT));
		userKeywords->setPlainText(QStringLiteral("myownkeyword"));
		save->click();
		QVERIFY(!dialog->isVisible());
		QCOMPARE(editorStyle(SCI_STYLEGETFORE, wordStyle), static_cast<LRESULT>(RGB(0, 128, 64)));

		QFile stylers(_home.path() + QStringLiteral("/.config/notepad++/stylers.xml"));
		QVERIFY2(stylers.open(QIODevice::ReadOnly), qPrintable(stylers.fileName()));
		const QByteArray written = stylers.readAll();
		QVERIFY(written.contains("myownkeyword"));
		QVERIFY(written.contains("008040"));

		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	// Clicks a colour button and chooses the colour in its popup
	bool pickColour(QAbstractButton* button, COLORREF colour) const
	{
		bool isFound = false;
		QTimer::singleShot(0, button, [colour, &isFound]()
		{
			QWidget* popup = QApplication::activePopupWidget();
			auto* list = popup ? popup->findChild<QListWidget*>() : nullptr;
			if (!list)
				return;
			for (int i = 0; i < list->count(); ++i)
			{
				if (list->item(i)->data(Qt::UserRole).toUInt() == colour)
				{
					isFound = true;
					Q_EMIT list->itemClicked(list->item(i));
					return;
				}
			}
			popup->close();
		});
		button->click();
		QTest::qWait(0);
		QCoreApplication::processEvents();
		return isFound;
	}

	// Runs a step while the modal dialog opened by the trigger is up, closing it if the step did not
	void whileModal(const std::function<void(QWidget*)>& step, const std::function<void()>& trigger) const
	{
		bool hasRun = false;
		QTimer::singleShot(0, mainWindow(), [&step, &hasRun]()
		{
			QWidget* modal = QApplication::activeModalWidget();
			QVERIFY(modal);
			hasRun = true;
			step(modal);
			if (QApplication::activeModalWidget() == modal)
				modal->close();
		});
		trigger();
		QVERIFY(hasRun);
	}

	// Sets the keys in a shortcut dialog and accepts it
	static void acceptShortcut(QWidget* dialog, bool ctrl, bool alt, bool shift, const QString& key)
	{
		const auto check = [dialog](int id, bool isChecked)
		{
			auto* box = qobject_cast<QAbstractButton*>(nppFindControl(dialog, id));
			QVERIFY(box);
			if (box->isChecked() != isChecked)
				box->click();
		};
		check(IDC_CTRL_CHECK, ctrl);
		check(IDC_ALT_CHECK, alt);
		check(IDC_SHIFT_CHECK, shift);

		auto* keys = qobject_cast<QComboBox*>(nppFindControl(dialog, IDC_KEY_COMBO));
		QVERIFY(keys);
		const int index = keys->findText(key);
		QVERIFY2(index >= 0, qPrintable(key));
		keys->setCurrentIndex(index);
		Q_EMIT keys->activated(index);

		auto* ok = qobject_cast<QAbstractButton*>(nppFindControl(dialog, IDOK));
		QVERIFY(ok);
		QVERIFY(ok->isEnabled());
		ok->click();
	}

	void shortcutMapper()
	{
		const auto buttonIn = [](QWidget* dialog, int id) { return qobject_cast<QAbstractButton*>(nppFindControl(dialog, id)); };
		HMENU mainMenu = reinterpret_cast<HMENU>(send(NPPM_INTERNAL_GETMENU));

		whileModal([&](QWidget* dialog)
		{
			auto* table = qobject_cast<QTableWidget*>(nppFindControl(dialog, IDD_BABYGRID_ID1));
			auto* tabs = qobject_cast<QTabBar*>(nppFindControl(dialog, IDC_BABYGRID_TABBAR));
			auto* filter = qobject_cast<QLineEdit*>(nppFindControl(dialog, IDC_BABYGRID_FILTER));
			auto* info = qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_BABYGRID_INFO));
			QVERIFY(table && tabs && filter && info);

			QCOMPARE(tabs->count(), 5);
			QCOMPARE(tabs->tabText(0), QStringLiteral("Main menu"));
			QVERIFY(table->rowCount() > 100);
			QCOMPARE(table->columnCount(), 3);
			QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("Category"));
			if (const QString dir = qEnvironmentVariable("NPP_TEST_APP_DIR"); !dir.isEmpty())
				dialog->grab().save(QDir(dir).filePath(QStringLiteral("shortcut-mapper.png")));

			// Every word of the filter has to match the name or the keys
			filter->setText(QStringLiteral("save ctrl+shift"));
			QCOMPARE(table->rowCount(), 1);
			QVERIFY(table->item(0, 0)->text().contains(QStringLiteral("Save All"), Qt::CaseInsensitive));
			QCOMPARE(table->item(0, 1)->text(), QStringLiteral("Ctrl+Shift+S"));
			filter->setText(QStringLiteral("nothing has this name"));
			QCOMPARE(table->rowCount(), 0);
			buttonIn(dialog, IDC_BABYGRID_FILTER_CLEAR)->click();
			QVERIFY(table->rowCount() > 100);

			// A new shortcut reaches the list and the menu
			filter->setText(QStringLiteral("Save a Copy"));
			QCOMPARE(table->rowCount(), 1);
			QCOMPARE(table->item(0, 1)->text(), QString());
			QVERIFY(info->toPlainText().contains(QStringLiteral("No shortcut conflicts")));
			whileModal([](QWidget* editor) { acceptShortcut(editor, true, true, true, QStringLiteral("K")); },
				[&]() { buttonIn(dialog, IDM_BABYGRID_MODIFY)->click(); });
			QCOMPARE(table->item(0, 1)->text(), QStringLiteral("Ctrl+Alt+Shift+K"));
			QCOMPARE(nppFindMenuAction(mainMenu, IDM_FILE_SAVECOPYAS)->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+Shift+K")));

			// A key used elsewhere is marked, and the information says where
			const QColor plainRow = table->item(0, 0)->background().color();
			whileModal([](QWidget* editor) { acceptShortcut(editor, true, false, false, QStringLiteral("N")); },
				[&]() { Q_EMIT table->cellDoubleClicked(0, 0); });
			QCOMPARE(table->item(0, 1)->text(), QStringLiteral("Ctrl+N"));
			QVERIFY(table->item(0, 0)->background().color() != plainRow);
			QVERIFY2(info->toPlainText().contains(QStringLiteral("New")), qPrintable(info->toPlainText()));

			// Clearing takes it away again
			buttonIn(dialog, IDM_BABYGRID_CLEAR)->click();
			QCOMPARE(table->item(0, 1)->text(), QString());
			QVERIFY(nppFindMenuAction(mainMenu, IDM_FILE_SAVECOPYAS)->shortcut().isEmpty());
			filter->clear();

			// What each tab allows
			QVERIFY(buttonIn(dialog, IDM_BABYGRID_CLEAR)->isEnabled());
			QVERIFY(!buttonIn(dialog, IDM_BABYGRID_DELETE)->isEnabled());
			tabs->setCurrentIndex(4); // Scintilla commands
			QCOMPARE(table->columnCount(), 2);
			QVERIFY(table->rowCount() > 50);
			QVERIFY(buttonIn(dialog, IDM_BABYGRID_MODIFY)->isEnabled());
			QVERIFY(!buttonIn(dialog, IDM_BABYGRID_CLEAR)->isEnabled());
			tabs->setCurrentIndex(1); // the macros of shortcuts.xml
			QCOMPARE(table->rowCount(), static_cast<int>(NppParameters::getInstance().getMacroList().size()));
			QVERIFY(buttonIn(dialog, IDM_BABYGRID_DELETE)->isEnabled());
			filter->setText(QStringLiteral("nothing has this name"));
			QCOMPARE(table->rowCount(), 0);
			QVERIFY(info->toPlainText().isEmpty());
			filter->clear();
		}, [this]() { command(IDM_SETTING_SHORTCUT_MAPPER); });

		// A saved Run command is listed, and deleting it takes it off the menu
		HMENU runMenu = nppSubMenuAt(mainMenu, MENUINDEX_RUN);
		const int runMenuItems = nppMenuItemCount(runMenu);

		command(IDM_EXECUTE);
		QWidget* run = visibleDialogWith(IDC_COMBO_RUN_PATH);
		QVERIFY(run);
		qobject_cast<QComboBox*>(nppFindControl(run, IDC_COMBO_RUN_PATH))->setEditText(QStringLiteral("xdg-open x"));
		whileModal([&](QWidget* editor)
		{
			qobject_cast<QLineEdit*>(nppFindControl(editor, IDC_NAME_EDIT))->setText(QStringLiteral("myRunCommand"));
			acceptShortcut(editor, true, true, true, QStringLiteral("L"));
		}, [&]() { buttonIn(run, IDC_BUTTON_SAVE)->click(); });
		run->hide();
		QVERIFY(nppMenuItemCount(runMenu) > runMenuItems);

		whileModal([&](QWidget* dialog)
		{
			auto* table = qobject_cast<QTableWidget*>(nppFindControl(dialog, IDD_BABYGRID_ID1));
			QCOMPARE(qobject_cast<QTabBar*>(nppFindControl(dialog, IDC_BABYGRID_TABBAR))->currentIndex(), 2);
			qobject_cast<QLineEdit*>(nppFindControl(dialog, IDC_BABYGRID_FILTER))->setText(QStringLiteral("myRunCommand"));
			QCOMPARE(table->rowCount(), 1);
			QCOMPARE(table->item(0, 0)->text(), QStringLiteral("myRunCommand"));
			QCOMPARE(table->item(0, 1)->text(), QStringLiteral("Ctrl+Alt+Shift+L"));
			QVERIFY(buttonIn(dialog, IDM_BABYGRID_DELETE)->isEnabled());

			answerModalDialog(IDOK);
			buttonIn(dialog, IDM_BABYGRID_DELETE)->click();
			QCOMPARE(table->rowCount(), 0);
		}, [this]() { command(IDM_SETTING_SHORTCUT_MAPPER_RUN); });
		QCOMPARE(nppMenuItemCount(runMenu), runMenuItems);
	}

	void preferences()
	{
		command(IDM_SETTING_PREFERENCE);
		QWidget* dialog = visibleDialogWith(IDC_LIST_DLGTITLE);
		QVERIFY(dialog);
		auto* pages = dialog->findChild<QStackedWidget*>();
		auto* categories = qobject_cast<QListWidget*>(nppFindControl(dialog, IDC_LIST_DLGTITLE));
		QVERIFY(pages && categories);
		QCOMPARE(pages->count(), categories->count());
		QCOMPARE(categories->item(0)->text(), QStringLiteral("General"));
		QVERIFY(categories->findItems(QStringLiteral("File Association"), Qt::MatchExactly).isEmpty());

		const QString shots = qEnvironmentVariable("NPP_TEST_APP_DIR");
		for (int i = 0; i < categories->count(); ++i)
		{
			categories->setCurrentRow(i);
			QCOMPARE(pages->currentIndex(), i);
			QVERIFY(pages->currentWidget()->isVisible());
			if (!shots.isEmpty())
				dialog->grab().save(QDir(shots).filePath(QStringLiteral("pref-%1.png").arg(i, 2, 10, QLatin1Char('0'))));
		}

		const auto showPage = [&](const QString& name)
		{
			const QList<QListWidgetItem*> found = categories->findItems(name, Qt::MatchExactly);
			QVERIFY2(!found.isEmpty(), qPrintable(name));
			categories->setCurrentItem(found.first());
		};
		const auto control = [dialog](int id) { return nppFindControl(dialog, id); };
		const auto clickCheck = [&](int id)
		{
			auto* box = qobject_cast<QAbstractButton*>(control(id));
			QVERIFY(box);
			box->click();
		};
		const auto toggled = [&](int id, const std::function<bool()>& isOn)
		{
			const bool before = isOn();
			clickCheck(id);
			QCOMPARE(isOn(), !before);
			clickCheck(id);
			QCOMPARE(isOn(), before);
		};
		NppParameters& params = NppParameters::getInstance();

		// The toolbar's and the tab bar's settings act on the window at once
		showPage(QStringLiteral("Toolbar"));
		toggled(IDC_CHECK_HIDE, [this]() { return mainWindow()->findChild<QToolBar*>()->isVisible(); });

		// Radio buttons exclude each other in their own group only
		auto* standard = qobject_cast<QAbstractButton*>(control(IDC_RADIO_STANDARD));
		auto* small = qobject_cast<QAbstractButton*>(control(IDC_RADIO_SMALLICON));
		auto* red = qobject_cast<QAbstractButton*>(control(IDC_RADIO_RED));
		auto* blue = qobject_cast<QAbstractButton*>(control(IDC_RADIO_BLUE));
		small->click();
		QVERIFY(small->isChecked() && !standard->isChecked());
		QVERIFY(red->isEnabled());
		red->click();
		QVERIFY(red->isChecked() && small->isChecked());
		blue->click();
		QVERIFY(blue->isChecked() && !red->isChecked() && small->isChecked());
		standard->click();
		QVERIFY(!red->isEnabled());

		// The margins of the editors
		showPage(QStringLiteral("Margins/Border/Edge"));
		toggled(IDC_CHECK_LINENUMBERMARGE, [this]() { return ::SendMessage(currentEditor(), SCI_GETMARGINWIDTHN, 0, 0) != 0; });
		toggled(IDC_CHECK_BOOKMARKMARGE, [this]() { return ::SendMessage(currentEditor(), SCI_GETMARGINWIDTHN, 1, 0) != 0; });

		auto* border = qobject_cast<QSlider*>(control(IDC_BORDERWIDTH_SLIDER));
		QVERIFY(border);
		const int borderBefore = border->value();
		border->setValue(borderBefore + 3);
		QCOMPARE(params.getSVP()._borderWidth, borderBefore + 3);
		QCOMPARE(qobject_cast<QLabel*>(control(IDC_BORDERWIDTHVAL_STATIC))->text(), QString::number(borderBefore + 3));
		border->setValue(borderBefore);

		// The Scintilla settings of the first editing page
		showPage(QStringLiteral("Editing 1"));
		toggled(IDC_CHECK_SMOOTHFONT, [this]() { return ::SendMessage(currentEditor(), SCI_GETFONTQUALITY, 0, 0) == SC_EFF_QUALITY_LCD_OPTIMIZED; });
		auto* frame = qobject_cast<QSlider*>(control(IDC_CARETLINEFRAME_WIDTH_SLIDER));
		QVERIFY(frame);
		frame->setValue(3);
		QCOMPARE(static_cast<int>(params.getSVP()._currentLineFrameWidth), 3);

		// The default of new documents
		showPage(QStringLiteral("New Document"));
		auto* unixEol = qobject_cast<QAbstractButton*>(control(IDC_RADIO_F_UNIX));
		auto* windowsEol = qobject_cast<QAbstractButton*>(control(IDC_RADIO_F_WIN));
		QVERIFY(unixEol && windowsEol);
		windowsEol->click();
		command(IDM_FILE_NEW);
		QCOMPARE(::SendMessage(currentEditor(), SCI_GETEOLMODE, 0, 0), static_cast<LRESULT>(SC_EOL_CRLF));
		send(WM_COMMAND, IDM_FILE_CLOSE);
		unixEol->click();
		command(IDM_FILE_NEW);
		QCOMPARE(::SendMessage(currentEditor(), SCI_GETEOLMODE, 0, 0), static_cast<LRESULT>(SC_EOL_LF));
		send(WM_COMMAND, IDM_FILE_CLOSE);

		// The tab settings
		showPage(QStringLiteral("Indentation"));
		auto* tabSize = qobject_cast<QLineEdit*>(control(IDC_EDIT_TABSIZEVAL));
		QVERIFY(tabSize);
		QVERIFY(tabSize->validator());
		const int tabBefore = params.getNppGUI()._tabSize;
		tabSize->setText(QStringLiteral("7"));
		Q_EMIT tabSize->editingFinished();
		QCOMPARE(params.getNppGUI()._tabSize, 7);
		QCOMPARE(::SendMessage(currentEditor(), SCI_GETTABWIDTH, 0, 0), static_cast<LRESULT>(7));
		tabSize->setText(QString::number(tabBefore));
		Q_EMIT tabSize->editingFinished();

		// The tab size has to be reachable for every entry of the list
		QVERIFY(tabSize->isVisible());
		auto* languageTabs = qobject_cast<QListWidget*>(control(IDC_LIST_TABSETTNG));
		languageTabs->setCurrentRow(1);
		QVERIFY(control(IDC_CHECK_DEFAULTTABVALUE)->isVisible());
		languageTabs->setCurrentRow(0);
		QVERIFY(!control(IDC_CHECK_DEFAULTTABVALUE)->isVisible());
		QVERIFY(tabSize->isVisible());

		// A language moved off the menu and back
		showPage(QStringLiteral("Language"));
		auto* enabled = qobject_cast<QListWidget*>(control(IDC_LIST_ENABLEDLANG));
		auto* disabled = qobject_cast<QListWidget*>(control(IDC_LIST_DISABLEDLANG));
		QVERIFY(enabled && disabled);
		HMENU mainMenu = reinterpret_cast<HMENU>(send(NPPM_INTERNAL_GETMENU));
		QVERIFY(nppFindMenuAction(mainMenu, IDM_LANG_ADA));
		const QList<QListWidgetItem*> ada = enabled->findItems(QStringLiteral("Ada"), Qt::MatchExactly);
		QCOMPARE(ada.size(), 1);
		const int enabledBefore = enabled->count();
		enabled->setCurrentItem(ada.first());
		clickCheck(IDC_BUTTON_REMOVE);
		QCOMPARE(enabled->count(), enabledBefore - 1);
		QCOMPARE(disabled->count(), 1);
		QCOMPARE(disabled->item(0)->text(), QStringLiteral("Ada"));
		QVERIFY(!nppFindMenuAction(mainMenu, IDM_LANG_ADA));
		disabled->setCurrentRow(0);
		clickCheck(IDC_BUTTON_RESTORE);
		QCOMPARE(enabled->count(), enabledBefore);
		QCOMPARE(disabled->count(), 0);
		QVERIFY(nppFindMenuAction(mainMenu, IDM_LANG_ADA));

		// Word characters
		showPage(QStringLiteral("Delimiter"));
		clickCheck(IDC_RADIO_WORDCHAR_CUSTOM);
		qobject_cast<QLineEdit*>(control(IDC_WORDCHAR_CUSTOM_EDIT))->setText(QStringLiteral("@"));
		QCOMPARE(params.getNppGUI()._customWordChars, std::string("@"));
		clickCheck(IDC_RADIO_WORDCHAR_DEFAULT);

		// Search engine and cloud fields keep what is typed
		showPage(QStringLiteral("Backup"));
		const QList<QLineEdit*> backupEdits = pages->currentWidget()->findChildren<QLineEdit*>();
		for (const QLineEdit* edit : backupEdits)
			QVERIFY2(!edit->text().contains(QLatin1Char('\\')), qPrintable(edit->text()));

		showPage(QStringLiteral("Search Engine"));
		clickCheck(IDC_SEARCHENGINE_CUSTOM_RADIO);
		qobject_cast<QLineEdit*>(control(IDC_SEARCHENGINE_EDIT))->setText(QStringLiteral("https://example.org/$(CURRENT_WORD)"));
		QCOMPARE(params.getNppGUI()._searchEngineCustom, std::wstring(L"https://example.org/$(CURRENT_WORD)"));

		// Dark mode changes the palette of the whole application, and its colours can be picked
		showPage(QStringLiteral("Dark Mode"));
		const QColor lightWindow = QApplication::palette().color(QPalette::Window);
		const LRESULT lightBackground = ::SendMessage(currentEditor(), SCI_STYLEGETBACK, STYLE_DEFAULT, 0);
		const LRESULT lightForeground = ::SendMessage(currentEditor(), SCI_STYLEGETFORE, STYLE_DEFAULT, 0);
		clickCheck(IDC_RADIO_DARKMODE_DARKMODE);
		QVERIFY(params.getNppGUI()._darkmode._isEnabled);
		QVERIFY(QApplication::palette().color(QPalette::Window) != lightWindow);
		clickCheck(IDC_RADIO_DARKMODE_LIGHTMODE);
		QVERIFY(!params.getNppGUI()._darkmode._isEnabled);
		QCOMPARE(QApplication::palette().color(QPalette::Window), lightWindow);
		QCOMPARE(::SendMessage(currentEditor(), SCI_STYLEGETBACK, STYLE_DEFAULT, 0), lightBackground);
		QCOMPARE(::SendMessage(currentEditor(), SCI_STYLEGETFORE, STYLE_DEFAULT, 0), lightForeground);

		// The same dialog can be reached with a section to go to
		dialog->hide();
		send(NPPM_INTERNAL_LAUNCHPREFERENCES, 5, IDC_RADIO_DARKMODE_DARKMODE);
		QVERIFY(dialog->isVisible());
		QCOMPARE(categories->currentRow(), 5);
		QCOMPARE(pages->currentIndex(), 5);
		dialog->hide();
	}

	void userDefinedLanguage()
	{
		const auto buttonIn = [](QWidget* dialog, int id) { return qobject_cast<QAbstractButton*>(nppFindControl(dialog, id)); };
		const auto editorStyle = [this](int message, int style) { return ::SendMessage(currentEditor(), message, style, 0); };
		HMENU mainMenu = reinterpret_cast<HMENU>(send(NPPM_INTERNAL_GETMENU));
		NppParameters& params = NppParameters::getInstance();
		const int nbLanguages = params.getNbUserLang();
		const int languageCmd = IDM_LANG_USER + nbLanguages + 1;

		openWithContent(QStringLiteral("sample.myudl"), "zebrakw other\n# zebrakw in comment\n");
		const QString docPath = currentPath();

		command(IDM_LANG_USER_DLG);
		QWidget* dialog = visibleDialogWith(IDC_LANGNAME_COMBO);
		QVERIFY(dialog);
		auto* tabs = dialog->findChild<QTabWidget*>();
		auto* languages = qobject_cast<QComboBox*>(nppFindControl(dialog, IDC_LANGNAME_COMBO));
		QVERIFY(tabs && languages);
		QCOMPARE(tabs->count(), 4);
		QCOMPARE(languages->itemText(0), QStringLiteral("User Defined Language"));
		QCOMPARE(languages->currentIndex(), 0);
		QVERIFY(!nppFindControl(dialog, IDC_RENAME_BUTTON)->isVisible());

		// The default status is docked, beside the editors
		QVERIFY(!dialog->isWindow());
		QVERIFY(currentEditor()->isVisible());
		for (int i = 0; i < tabs->count(); ++i)
		{
			tabs->setCurrentIndex(i);
			if (const QString dir = qEnvironmentVariable("NPP_TEST_APP_DIR"); !dir.isEmpty())
				dialog->grab().save(QDir(dir).filePath(QStringLiteral("udl-%1.png").arg(i)));
		}

		// A new language reaches the list and the Language menu
		whileModal([](QWidget* ask)
		{
			qobject_cast<QLineEdit*>(nppFindControl(ask, IDC_STRING_EDIT))->setText(QStringLiteral("TestLang"));
			qobject_cast<QAbstractButton*>(nppFindControl(ask, IDOK))->click();
		}, [&]() { buttonIn(dialog, IDC_ADDNEW_BUTTON)->click(); });
		QCOMPARE(languages->currentText(), QStringLiteral("TestLang"));
		QCOMPARE(params.getNbUserLang(), nbLanguages + 1);
		QVERIFY(nppFindControl(dialog, IDC_RENAME_BUTTON)->isVisible());
		QVERIFY(nppFindMenuAction(mainMenu, languageCmd));
		QCOMPARE(nppMenuCommandText(mainMenu, languageCmd), std::wstring(L"TestLang"));

		// Keywords and comments colour the document that uses the language
		command(languageCmd);
		const auto setEdit = [&](int id, const QString& text)
		{
			auto* edit = qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, id));
			QVERIFY(edit);
			edit->setPlainText(text);
		};
		setEdit(IDC_KEYWORD1_EDIT, QStringLiteral("zebrakw"));
		setEdit(IDC_COMMENTLINE_OPEN_EDIT, QStringLiteral("#"));

		const auto checkColouring = [this]()
		{
			::SendMessage(currentEditor(), SCI_COLOURISE, 0, -1);
			QCOMPARE(::SendMessage(currentEditor(), SCI_GETSTYLEAT, 0, 0), static_cast<LRESULT>(SCE_USER_STYLE_KEYWORD1));
			const LRESULT commentStart = ::SendMessage(currentEditor(), SCI_POSITIONFROMLINE, 1, 0);
			QCOMPARE(::SendMessage(currentEditor(), SCI_GETSTYLEAT, commentStart + 3, 0), static_cast<LRESULT>(SCE_USER_STYLE_COMMENTLINE));
		};
		checkColouring();

		// Going to another language and back keeps everything, as filling the fields is not an edit
		const auto chooseLanguage = [&](int index)
		{
			languages->setCurrentIndex(index);
			Q_EMIT languages->activated(index);
		};
		chooseLanguage(0);
		QCOMPARE(qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_KEYWORD1_EDIT))->toPlainText(), QString());
		chooseLanguage(nbLanguages + 1);
		QCOMPARE(qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_KEYWORD1_EDIT))->toPlainText(), QStringLiteral("zebrakw"));
		QCOMPARE(qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_COMMENTLINE_OPEN_EDIT))->toPlainText(), QStringLiteral("#"));
		QCOMPARE(qobject_cast<QPlainTextEdit*>(nppFindControl(dialog, IDC_COMMENTLINE_CLOSE_EDIT))->toPlainText(), QString());
		send(WM_COMMAND, languageCmd); // the document has to read the language's lists again
		checkColouring();

		// The style dialog, which applies at once and is undone by cancelling
		const LRESULT wasBold = editorStyle(SCI_STYLEGETBOLD, SCE_USER_STYLE_KEYWORD1);
		whileModal([&](QWidget* styler)
		{
			if (const QString dir = qEnvironmentVariable("NPP_TEST_APP_DIR"); !dir.isEmpty())
				styler->grab().save(QDir(dir).filePath(QStringLiteral("udl-styler.png")));
			buttonIn(styler, IDC_STYLER_CHECK_BOLD)->click();
			QCOMPARE(editorStyle(SCI_STYLEGETBOLD, SCE_USER_STYLE_KEYWORD1) != 0, wasBold == 0);
			buttonIn(styler, IDCANCEL)->click();
		}, [&]() { buttonIn(dialog, IDC_KEYWORD1_STYLER)->click(); });
		QCOMPARE(editorStyle(SCI_STYLEGETBOLD, SCE_USER_STYLE_KEYWORD1), wasBold);

		whileModal([&](QWidget* styler)
		{
			buttonIn(styler, IDC_STYLER_CHECK_BOLD)->click();
			auto* foreground = styler->findChild<QAbstractButton*>(QStringLiteral("fgColour"));
			QVERIFY(foreground);
			QVERIFY(pickColour(foreground, RGB(0, 128, 128)));
			buttonIn(styler, IDOK)->click();
		}, [&]() { buttonIn(dialog, IDC_KEYWORD1_STYLER)->click(); });
		QCOMPARE(editorStyle(SCI_STYLEGETBOLD, SCE_USER_STYLE_KEYWORD1) != 0, wasBold == 0);
		QCOMPARE(editorStyle(SCI_STYLEGETFORE, SCE_USER_STYLE_KEYWORD1), static_cast<LRESULT>(RGB(0, 128, 128)));

		// Undocking makes it a window, and docking puts it back beside the editors
		auto* dock = buttonIn(dialog, IDC_DOCK_BUTTON);
		dock->click();
		QVERIFY(dialog->isWindow());
		QVERIFY(dialog->isVisible());
		QCOMPARE(dock->text(), QStringLiteral("Dock"));
		QTRY_VERIFY(currentEditor()->isVisible());
		dock->click();
		QVERIFY(!dialog->isWindow());
		QVERIFY(dialog->isVisible());
		QCOMPARE(dock->text(), QStringLiteral("Undock"));
		QTRY_VERIFY(currentEditor()->isVisible());

		// Hiding and showing it while docked leaves the editors in place
		command(IDM_LANG_USER_DLG);
		QTRY_VERIFY(!dialog->isVisible());
		QVERIFY(currentEditor()->isVisible());
		command(IDM_LANG_USER_DLG);
		QTRY_VERIFY(dialog->isVisible());
		QVERIFY(currentEditor()->isVisible());

		// Renaming follows into the menu, and the document stays with the language
		whileModal([](QWidget* ask)
		{
			auto* edit = qobject_cast<QLineEdit*>(nppFindControl(ask, IDC_STRING_EDIT));
			QCOMPARE(edit->text(), QStringLiteral("TestLang"));
			edit->setText(QStringLiteral("Renamed"));
			qobject_cast<QAbstractButton*>(nppFindControl(ask, IDOK))->click();
		}, [&]() { buttonIn(dialog, IDC_RENAME_BUTTON)->click(); });
		QCOMPARE(languages->currentText(), QStringLiteral("Renamed"));
		QCOMPARE(nppMenuCommandText(mainMenu, languageCmd), std::wstring(L"Renamed"));
		QCOMPARE(std::wstring(params.getULCFromIndex(nbLanguages)->getName()), std::wstring(L"Renamed"));

		// Removing it from the list and the menu
		answerModalDialog(IDYES);
		buttonIn(dialog, IDC_REMOVELANG_BUTTON)->click();
		QCOMPARE(params.getNbUserLang(), nbLanguages);
		QCOMPARE(languages->count(), nbLanguages + 1);
		QVERIFY(!nppFindMenuAction(mainMenu, languageCmd));

		command(IDM_LANG_USER_DLG);
		QTRY_VERIFY(!dialog->isVisible());
		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	void hiddenMenuBar()
	{
		QMainWindow* window = mainWindow();
		window->activateWindow();
		if (!QTest::qWaitForWindowActive(window))
		{
			// A compositor can refuse a window focus that the user did not ask for
			QVERIFY(QGuiApplication::platformName() != QStringLiteral("offscreen"));
			QSKIP("the window manager did not give the window focus");
		}
		QMenuBar* menuBar = window->menuBar();
		QVERIFY(menuBar->isVisible());
		currentEditor()->setFocus();

		// A shortcut, with the menu bar showing and hidden
		const auto newDocumentByKey = [this, window]()
		{
			const LRESULT before = send(NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW);
			QTest::keySequence(window, QKeySequence(QStringLiteral("Ctrl+N")));
			QCoreApplication::processEvents();
			QCOMPARE(send(NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW), before + 1);
			send(WM_COMMAND, IDM_FILE_CLOSE);
		};
		newDocumentByKey();

		send(NPPM_HIDEMENU, 0, TRUE);
		QVERIFY(menuBar->isHidden());
		newDocumentByKey();

		// Alt alone brings it back and puts it away, and F10 too
		QTest::keyClick(window, Qt::Key_Alt);
		QVERIFY(menuBar->isVisible());
		QTest::keyClick(window, Qt::Key_Alt);
		QVERIFY(menuBar->isHidden());
		QTest::keyClick(window, Qt::Key_F10);
		QVERIFY(menuBar->isVisible());

		// Choosing from it hides it again, and it stays hidden in the settings
		QAction* statusBar = nppFindMenuAction(reinterpret_cast<HMENU>(send(NPPM_INTERNAL_GETMENU)), IDM_VIEW_ALWAYSONTOP);
		QVERIFY(statusBar);
		Q_EMIT menuBar->triggered(statusBar);
		QVERIFY(menuBar->isHidden());
		QVERIFY(!NppParameters::getInstance().getNppGUI()._menuBarShow);

		// Alt as part of another key does nothing
		QTest::keyPress(window, Qt::Key_Alt, Qt::AltModifier);
		QTest::keyClick(window, Qt::Key_X, Qt::AltModifier);
		QTest::keyRelease(window, Qt::Key_Alt);
		QVERIFY(menuBar->isHidden());

		send(NPPM_HIDEMENU, 0, FALSE);
		QVERIFY(menuBar->isVisible());
		QTest::keyClick(window, Qt::Key_Alt);
		QVERIFY(menuBar->isVisible());
	}

	void desktopLabels()
	{
		HMENU mainMenu = reinterpret_cast<HMENU>(send(NPPM_INTERNAL_GETMENU));
		QCOMPARE(nppMenuCommandText(mainMenu, IDM_FILE_OPEN_FOLDER), std::wstring(L"File Manager"));
		QCOMPARE(nppMenuCommandText(mainMenu, IDM_FILE_OPEN_CMD), std::wstring(L"Terminal"));
		const bool hasPowerShell = !QStandardPaths::findExecutable(QStringLiteral("pwsh")).isEmpty();
		QCOMPARE(nppFindMenuAction(mainMenu, IDM_FILE_OPEN_POWERSHELL) != nullptr, hasPowerShell);
	}

	// The cases of PowerEditor/Test/UrlDetection: a line "u text u" followed by "m mask m", in which a 1 says that the
	// character of the text above it is part of a URL. Upstream runs these with a Lua plugin, and this writes the same report.
	QByteArray urlDetectionReport(const QString& document)
	{
		send(NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(document.toStdWString().c_str()));
		if (!QTest::qWaitFor([&]() { return currentPath() == document; }, 5000))
			return {};

		HWND editor = currentEditor();
		const auto line = [editor](LRESULT number)
		{
			const LRESULT length = ::SendMessage(editor, SCI_LINELENGTH, number, 0);
			QByteArray text(length, '\0');
			::SendMessage(editor, SCI_GETLINE, number, reinterpret_cast<LPARAM>(text.data()));
			while (text.endsWith('\n') || text.endsWith('\r'))
				text.chop(1);
			return text;
		};

		QByteArray report;
		const LRESULT lines = ::SendMessage(editor, SCI_GETLINECOUNT, 0, 0);
		for (LRESULT number = 0; number + 1 < lines; ++number)
		{
			const QByteArray text = line(number);
			if (!text.startsWith("u ") || !text.endsWith(" u") || text.size() < 4)
				continue;
			const QByteArray expected = line(number + 1);
			if (!expected.startsWith("m ") || !expected.endsWith(" m"))
				continue;

			const LRESULT start = ::SendMessage(editor, SCI_POSITIONFROMLINE, number, 0);
			const LRESULT end = start + text.size() - 2;

			// Links are only marked in the lines showing, a moment after they come into view
			::SendMessage(editor, SCI_SCROLLRANGE, ::SendMessage(editor, SCI_POSITIONFROMLINE, number + 1, 0), start);
			send(NPPM_INTERNAL_UPDATECLICKABLELINKS, 0, 0);
			QCoreApplication::processEvents();

			QByteArray mask = "m ";
			for (LRESULT position = start + 2; position < end; position = ::SendMessage(editor, SCI_POSITIONAFTER, position, 0))
				mask += (::SendMessage(editor, SCI_INDICATORVALUEAT, URL_INDIC, position) != 0) ? '1' : '0';
			mask += " m";

			if (mask == expected)
			{
				report += "OK\t" + text + "\n";
			}
			else
			{
				report += "KO\t" + text + "\n";
				report += "ok\t" + expected + "\n";
				report += "ko\t" + mask + "\n";
			}
			++number;
		}
		send(WM_COMMAND, IDM_FILE_CLOSE);
		return report;
	}

	void urlDetection_data()
	{
		QTest::addColumn<QString>("name");
		QTest::newRow("1a") << QStringLiteral("verifyUrlDetection_1a");
		QTest::newRow("1b") << QStringLiteral("verifyUrlDetection_1b");
	}

	void urlDetection()
	{
		QFETCH(QString, name);

		// A copy, as the application may rewrite nothing in the repository
		const QDir source(QStringLiteral(NPP_TEST_DATA_DIR "/UrlDetection"));
		const QString document = _files.filePath(name);
		QVERIFY(QFile::copy(source.filePath(name), document));

		const QByteArray report = urlDetectionReport(document);
		QVERIFY(!report.isEmpty());

		// In any order, as upstream's Compare-Object does, because the expected file is not in the order of the document
		QByteArray expected = readFile(source.filePath(name + QStringLiteral(".expected.result")));
		expected.replace("\r\n", "\n");
		QList<QByteArray> got = report.split('\n');
		QList<QByteArray> want = expected.split('\n');
		got.removeAll(QByteArray());
		want.removeAll(QByteArray());
		std::sort(got.begin(), got.end());
		std::sort(want.begin(), want.end());

		for (const QByteArray& entry : got)
		{
			if (!want.contains(entry))
				qWarning("unexpected: %s", entry.constData());
		}
		for (const QByteArray& entry : want)
		{
			if (!got.contains(entry))
				qWarning("missing: %s", entry.constData());
		}
		QCOMPARE(got, want);
	}

	// The selection in the current editor, which is where a search leaves its match
	std::pair<LRESULT, LRESULT> selection() const
	{
		return { ::SendMessage(currentEditor(), SCI_GETSELECTIONSTART, 0, 0), ::SendMessage(currentEditor(), SCI_GETSELECTIONEND, 0, 0) };
	}

	void setSelection(LRESULT start, LRESULT end) const
	{
		::SendMessage(currentEditor(), SCI_SETSEL, start, end);
	}

	QString findStatus() const
	{
		QString text;
		const QList<QLabel*> labels = findDialog()->findChild<QStatusBar*>()->findChildren<QLabel*>();
		for (const QLabel* label : labels)
			text += label->text();
		return text;
	}

	// The options a test may leave changed, back to plain searching
	void resetFindOptions() const
	{
		setChecked(IDNORMAL, true);
		setChecked(IDWHOLEWORD, false);
		setChecked(IDMATCHCASE, false);
		setChecked(IDWRAP, true);
		setChecked(IDC_BACKWARDDIRECTION, false);
		setChecked(IDREDOTMATCHNL, false);
		setChecked(IDC_IN_SELECTION_CHECK, false);
	}

	void findModes()
	{
		using Span = std::pair<LRESULT, LRESULT>;
		openWithContent(QStringLiteral("modes.txt"), "Alpha beta alphabet\nALPHA gamma\nbeta alpha\n");

		command(IDM_SEARCH_FIND);
		resetFindOptions();
		setComboText(IDFINDWHAT, QStringLiteral("alpha"));

		// The dialog's transparency keeps its setting, which the slider's range once clamped to its minimum
		QCOMPARE(NppParameters::getInstance().getFindHistory()._transparency, 150);
		auto* transparency = qobject_cast<QSlider*>(nppFindControl(findDialog(), IDC_PERCENTAGE_SLIDER));
		QVERIFY(transparency);
		QCOMPARE(transparency->value(), 150);

		// Forward through every match, without case, and around
		setSelection(0, 0);
		click(IDOK);
		QCOMPARE(selection(), Span(0, 5));
		click(IDOK);
		QCOMPARE(selection(), Span(11, 16));
		click(IDOK);
		QCOMPARE(selection(), Span(20, 25));
		click(IDOK);
		QCOMPARE(selection(), Span(37, 42));
		click(IDOK);
		QCOMPARE(selection(), Span(0, 5));
		QVERIFY2(findStatus().contains(QStringLiteral("Reached")), qPrintable(findStatus()));

		// Stopping at the end without wrapping
		setChecked(IDWRAP, false);
		setSelection(37, 42);
		click(IDOK);
		QVERIFY2(findStatus().contains(QStringLiteral("Can't find")), qPrintable(findStatus()));
		setChecked(IDWRAP, true);

		// Whole words
		setChecked(IDWHOLEWORD, true);
		setSelection(0, 5);
		click(IDOK);
		QCOMPARE(selection(), Span(20, 25));
		setChecked(IDWHOLEWORD, false);

		// With case
		setChecked(IDMATCHCASE, true);
		setComboText(IDFINDWHAT, QStringLiteral("ALPHA"));
		setSelection(0, 0);
		click(IDOK);
		QCOMPARE(selection(), Span(20, 25));
		setChecked(IDMATCHCASE, false);

		// Backward
		setComboText(IDFINDWHAT, QStringLiteral("beta"));
		setChecked(IDC_BACKWARDDIRECTION, true);
		setSelection(25, 25);
		click(IDOK);
		QCOMPARE(selection(), Span(6, 10));
		setChecked(IDC_BACKWARDDIRECTION, false);

		// Count
		setComboText(IDFINDWHAT, QStringLiteral("alpha"));
		click(IDCCOUNTALL);
		QVERIFY2(findStatus().contains(QStringLiteral("4 matches")), qPrintable(findStatus()));
		setChecked(IDWHOLEWORD, true);
		click(IDCCOUNTALL);
		QVERIFY2(findStatus().contains(QStringLiteral("3 matches")), qPrintable(findStatus()));
		setChecked(IDWHOLEWORD, false);

		// Extended: the escapes in the text
		setChecked(IDEXTENDED, true);
		setComboText(IDFINDWHAT, QStringLiteral("gamma\\nbeta"));
		setSelection(0, 0);
		click(IDOK);
		QCOMPARE(selection(), Span(26, 36));
		setComboText(IDFINDWHAT, QStringLiteral("\\x41lpha"));
		setSelection(0, 0);
		setChecked(IDMATCHCASE, true);
		click(IDOK);
		QCOMPARE(selection(), Span(0, 5));
		setChecked(IDMATCHCASE, false);

		// Regular expressions: anchors, groups, and the option for a dot to match a line end
		setChecked(IDREGEXP, true);
		setComboText(IDFINDWHAT, QStringLiteral("^beta"));
		setSelection(0, 0);
		click(IDOK);
		QCOMPARE(selection(), Span(32, 36));
		setComboText(IDFINDWHAT, QStringLiteral("(alpha)bet"));
		setSelection(0, 0);
		click(IDOK);
		QCOMPARE(selection(), Span(11, 19));
		setComboText(IDFINDWHAT, QStringLiteral("gamma.beta"));
		setSelection(0, 0);
		setChecked(IDREDOTMATCHNL, false);
		click(IDOK);
		QVERIFY2(findStatus().contains(QStringLiteral("Can't find")), qPrintable(findStatus()));
		setChecked(IDREDOTMATCHNL, true);
		click(IDOK);
		QCOMPARE(selection(), Span(26, 36));
		setChecked(IDREDOTMATCHNL, false);

		// A broken expression says so instead of matching nothing
		setComboText(IDFINDWHAT, QStringLiteral("(unclosed"));
		click(IDOK);
		QVERIFY2(findStatus().contains(QStringLiteral("Invalid")) || findStatus().contains(QStringLiteral("regular expression")), qPrintable(findStatus()));

		// The commands that work from the main window use the same search
		resetFindOptions();
		setComboText(IDFINDWHAT, QStringLiteral("gamma"));
		click(IDOK);
		findDialog()->hide();
		setSelection(0, 0);
		command(IDM_SEARCH_FINDNEXT);
		QCOMPARE(selection(), Span(26, 31));
		command(IDM_SEARCH_FINDPREV);
		QCOMPARE(selection(), Span(26, 31));

		// The selected word as the text to look for
		setSelection(6, 10);
		command(IDM_SEARCH_SETANDFINDNEXT);
		QCOMPARE(selection(), Span(32, 36));

		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	void markAll()
	{
		openWithContent(QStringLiteral("mark.txt"), "one two\nthree two\nfour\ntwo two\n");
		HWND editor = currentEditor();

		const auto markedRanges = [editor]()
		{
			int count = 0;
			const LRESULT length = ::SendMessage(editor, SCI_GETLENGTH, 0, 0);
			for (LRESULT position = 0; position < length;)
			{
				const LRESULT end = ::SendMessage(editor, SCI_INDICATOREND, SCE_UNIVERSAL_FOUND_STYLE, position);
				if (::SendMessage(editor, SCI_INDICATORVALUEAT, SCE_UNIVERSAL_FOUND_STYLE, position) != 0)
					++count;
				if (end <= position)
					break;
				position = end;
			}
			return count;
		};
		const auto bookmarkedLines = [editor]()
		{
			int count = 0;
			for (LRESULT line = ::SendMessage(editor, SCI_MARKERNEXT, 0, 1 << MARK_BOOKMARK); line >= 0; line = ::SendMessage(editor, SCI_MARKERNEXT, line + 1, 1 << MARK_BOOKMARK))
				++count;
			return count;
		};

		command(IDM_SEARCH_MARK);
		resetFindOptions();
		setComboText(IDFINDWHAT, QStringLiteral("two"));
		setChecked(IDC_MARKLINE_CHECK, false);
		setChecked(IDC_PURGE_CHECK, true);
		click(IDCMARKALL);
		QCOMPARE(markedRanges(), 4);
		QCOMPARE(bookmarkedLines(), 0);
		QVERIFY2(findStatus().contains(QStringLiteral("4 matches")), qPrintable(findStatus()));

		// Purging replaces the last marks, and bookmarking takes the lines
		setComboText(IDFINDWHAT, QStringLiteral("four"));
		setChecked(IDC_MARKLINE_CHECK, true);
		click(IDCMARKALL);
		QCOMPARE(markedRanges(), 1);
		QCOMPARE(bookmarkedLines(), 1);

		setChecked(IDC_PURGE_CHECK, false);
		setComboText(IDFINDWHAT, QStringLiteral("one"));
		click(IDCMARKALL);
		QCOMPARE(markedRanges(), 2);
		QCOMPARE(bookmarkedLines(), 2);

		// What was marked can be copied
		// Not cleared first: a clipboard manager on a real desktop restores an empty clipboard, racing the copy
		QApplication::clipboard()->setText(QStringLiteral("before"));
		click(IDC_COPY_MARKED_TEXT);
		QTRY_VERIFY2(QApplication::clipboard()->text().contains(QStringLiteral("one")) && QApplication::clipboard()->text().contains(QStringLiteral("four")), qPrintable(QApplication::clipboard()->text()));

		click(IDC_CLEAR_ALL);
		QCOMPARE(markedRanges(), 0);
		QCOMPARE(bookmarkedLines(), 0);

		setChecked(IDC_MARKLINE_CHECK, false);
		setChecked(IDC_PURGE_CHECK, false);
		findDialog()->hide();
		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	// A macro saved by the original on Windows runs the same, once the user has confirmed the shortcuts.xml they wrote
	void macroFromWindows()
	{
		NppParameters& nppParams = NppParameters::getInstance();
		std::vector<MacroShortcut>& macros = nppParams.getMacroList();
		QVERIFY(macros.size() >= 2);
		QCOMPARE(QString::fromStdString(macros[1].getName()), QStringLiteral("Windows macro"));
		QCOMPARE(macros[1].getMacro().size(), std::size(windowsMacro));

		command(IDM_FILE_NEW);
		command(IDM_FORMAT_TODOS); // the line ending of the original's new documents, which an earlier test may have changed
		const LRESULT document = send(NPPM_GETCURRENTBUFFERID);

		// shortcuts.xml was not written by Notepad++, so it is opened for review instead of running the macro
		answerModalDialog(IDOK);
		command(ID_MACRO + 1);
		QVERIFY(_asked);
		QVERIFY2(_asked->contains(QStringLiteral("shortcuts.xml")), qPrintable(*_asked));
		QVERIFY(send(NPPM_GETCURRENTBUFFERID) != document);
		command(IDM_FILE_CLOSE);
		QCOMPARE(send(NPPM_GETCURRENTBUFFERID), document);
		QCOMPARE(textOf(currentEditor()), QByteArray());

		command(IDM_EXECUTE_VALIDATE_SHORTCUTSXML);
		command(ID_MACRO + 1);
		QCOMPARE(textOf(currentEditor()), windowsMacroResult);

		::SendMessage(currentEditor(), SCI_SETSAVEPOINT, 0, 0);
		command(IDM_FILE_CLOSE);
	}

	// Files in various encodings open, save and convert as the original did on Windows, which ran the same samples
	void encodingsAsOnWindows()
	{
		const QByteArray utf8 = "h\xC3\xA9llo w\xC3\xB6rld \xE2\x82\xAC \xE2\x9C\x93\r\nsecond line\r\n";
		const QString unicode = QString::fromUtf8(utf8);
		const QByteArray utf16le(reinterpret_cast<const char*>(unicode.utf16()), unicode.size() * 2);
		QByteArray utf16be = utf16le;
		for (qsizetype i = 0; i + 1 < utf16be.size(); i += 2)
			std::swap(utf16be[i], utf16be[i + 1]);
		const QString ascii = QStringLiteral("plain text in utf-16\r\nsecond line\r\n");
		const QByteArray utf16leNoBom(reinterpret_cast<const char*>(ascii.utf16()), ascii.size() * 2);
		const QByteArray shiftJis = QByteArray::fromStdString(wstring2string(L"日本語のテキストです。これはテストです。\r\n", 932));
		const QByteArray cp1251 = QByteArray::fromStdString(wstring2string(L"Привет, мир! Съешь же ещё этих мягких французских булок\r\n", 1251));
		const QByteArray cp1252 = QByteArray::fromStdString(wstring2string(L"café naïve € déjà vu, garçon, naïveté\r\nsecond line\r\n", 1252));

		struct Sample
		{
			const char* name;
			QByteArray bytes;
			int uniMode;
			int codePage;
			QByteArray text; // as Scintilla has it, in UTF-8
		};
		const Sample samples[] = {
			{"ascii.txt", "plain ascii\r\nsecond line\r\n", uniUTF8_NoBOM, -1, "plain ascii\r\nsecond line\r\n"},
			{"utf8.txt", utf8, uniUTF8_NoBOM, -1, utf8},
			{"utf8bom.txt", "\xEF\xBB\xBF" + utf8, uniUTF8, -1, utf8},
			{"utf16le.txt", "\xFF\xFE" + utf16le, uni16LE, -1, utf8},
			{"utf16be.txt", "\xFE\xFF" + utf16be, uni16BE, -1, utf8},
			{"utf16le-nobom.txt", utf16leNoBom, uni16LE_NoBOM, -1, ascii.toUtf8()},
			{"shiftjis.txt", shiftJis, uniUTF8_NoBOM, 932, "日本語のテキストです。これはテストです。\r\n"},
			// Detection takes this Windows-1251 text for Mac Cyrillic, as on Windows, and reads it as Windows does
			{"cp1251.txt", cp1251, uniUTF8_NoBOM, 10007, "ѕривет, мир! —ъешь же ещЄ этих м¤гких французских булок\r\n"},
			// The original opens this as ANSI. The system's code page is UTF-8 here, so it opens as UTF-8 with its bytes as they are
			{"cp1252.txt", cp1252, uniUTF8_NoBOM, -1, cp1252},
		};

		const auto currentBuffer = [this]()
		{
			return MainFileManager.getBufferByID(reinterpret_cast<BufferID>(send(NPPM_GETCURRENTBUFFERID)));
		};
		const auto saveAs = [this](const QString& path)
		{
			QFile::remove(path);
			const std::wstring wide = toW(path);
			QVERIFY(send(NPPM_SAVECURRENTFILEAS, 0, reinterpret_cast<LPARAM>(wide.c_str())));
		};

		// Opened and saved, each keeps its bytes
		for (const Sample& sample : samples)
		{
			const QString name = QString::fromLatin1(sample.name);
			const QString path = _files.filePath(QStringLiteral("encoding-") + name);
			writeFile(path, sample.bytes);
			open(path);
			QVERIFY2(send(NPPM_GETBUFFERENCODING, send(NPPM_GETCURRENTBUFFERID)) == sample.uniMode, sample.name);
			QVERIFY2(currentBuffer()->getEncoding() == sample.codePage, sample.name);
			QVERIFY2(textOf(currentEditor()) == sample.text, sample.name);

			const QString saved = _files.filePath(QStringLiteral("saved-") + name);
			saveAs(saved);
			QVERIFY2(readFile(saved) == sample.bytes, sample.name);
			command(IDM_FILE_CLOSE);
		}

		// A character set chosen in the menu keeps even its undefined bytes, which Windows maps to private-use characters
		QByteArray allHigh;
		for (int b = 0x80; b < 0x100; ++b)
			allHigh += static_cast<char>(b);
		const QString highPath = _files.filePath(QStringLiteral("encoding-high.txt"));
		writeFile(highPath, allHigh);
		open(highPath);
		command(IDM_FORMAT_WIN_1253);
		QCOMPARE(currentBuffer()->getEncoding(), 1253);
		QVERIFY(textOf(currentEditor()).contains("\xEF\xA3\xBA")); // 0xD2 as U+F8FA, as on Windows
		const QString highSaved = _files.filePath(QStringLiteral("saved-high.txt"));
		saveAs(highSaved);
		QCOMPARE(readFile(highSaved), allHigh);
		command(IDM_FILE_CLOSE);

		// Converting writes what the original wrote
		const QString utf8Path = _files.filePath(QStringLiteral("encoding-utf8.txt"));
		const std::pair<int, QByteArray> conversions[] = {
			{IDM_FORMAT_CONV2_UTF_8, "\xEF\xBB\xBF" + utf8},
			{IDM_FORMAT_CONV2_UTF_16BE, "\xFE\xFF" + utf16be},
			{IDM_FORMAT_CONV2_UTF_16LE, "\xFF\xFE" + utf16le},
		};
		for (const auto& [commandId, expected] : conversions)
		{
			open(utf8Path);
			command(commandId);
			const QString converted = _files.filePath(QStringLiteral("converted-%1.txt").arg(commandId));
			saveAs(converted);
			QVERIFY2(readFile(converted) == expected, qPrintable(QString::number(commandId)));
			command(IDM_FILE_CLOSE);
		}
		const QString utf16Path = _files.filePath(QStringLiteral("encoding-utf16le.txt"));
		open(utf16Path);
		command(IDM_FORMAT_CONV2_AS_UTF_8);
		const QString backToUtf8 = _files.filePath(QStringLiteral("converted-back.txt"));
		saveAs(backToUtf8);
		QCOMPARE(readFile(backToUtf8), utf8);
		command(IDM_FILE_CLOSE);
	}

	// Recording the same actions as the macro from Windows gives the same steps, which play back and are saved the same
	void macroRecording()
	{
		command(IDM_FILE_NEW);
		command(IDM_FORMAT_TODOS); // the line ending of the original's new documents, which an earlier test may have changed
		HWND editor = currentEditor();
		command(IDM_MACRO_STARTRECORDINGMACRO);
		::SendMessage(editor, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("Hello foo"));
		::SendMessage(editor, SCI_NEWLINE, 0, 0);
		::SendMessage(editor, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("foo bar foo"));
		::SendMessage(editor, SCI_HOME, 0, 0);
		::SendMessage(editor, SCI_LINEENDEXTEND, 0, 0);
		command(IDM_EDIT_UPPERCASE);
		command(IDM_SEARCH_REPLACE);
		resetFindOptions();
		setComboText(IDFINDWHAT, QStringLiteral("foo"));
		setComboText(IDREPLACEWITH, QStringLiteral("baz"));
		click(IDREPLACEALL);
		findDialog()->hide();
		command(IDM_MACRO_STOPRECORDINGMACRO);
		QCOMPARE(textOf(editor), windowsMacroResult);

		// Played back in another document
		::SendMessage(editor, SCI_SETSAVEPOINT, 0, 0);
		command(IDM_FILE_NEW);
		command(IDM_FORMAT_TODOS);
		command(IDM_MACRO_PLAYBACKRECORDEDMACRO);
		QCOMPARE(textOf(currentEditor()), windowsMacroResult);
		::SendMessage(currentEditor(), SCI_SETSAVEPOINT, 0, 0);
		command(IDM_FILE_CLOSE);

		// Saved with a name, it has the steps of the macro from Windows
		whileModal([](QWidget* dialog)
		{
			auto* name = qobject_cast<QLineEdit*>(nppFindControl(dialog, IDC_NAME_EDIT));
			QVERIFY(name);
			name->setText(QStringLiteral("Recorded macro"));
			qobject_cast<QAbstractButton*>(nppFindControl(dialog, IDOK))->click();
		}, [this]() { command(IDM_MACRO_SAVECURRENTMACRO); });

		NppParameters& nppParams = NppParameters::getInstance();
		MacroShortcut& saved = nppParams.getMacroList().back();
		QCOMPARE(QString::fromStdString(saved.getName()), QStringLiteral("Recorded macro"));
		const Macro& steps = saved.getMacro();
		QCOMPARE(steps.size(), std::size(windowsMacro));
		for (size_t i = 0; i < steps.size(); ++i)
		{
			const MacroAction& expected = windowsMacro[i];
			const QString where = QStringLiteral("step %1").arg(i);
			QVERIFY2(static_cast<int>(steps[i]._macroType) == expected.type, qPrintable(where));
			QVERIFY2(steps[i]._message == expected.message, qPrintable(where));
			QVERIFY2(steps[i]._wParameter == expected.wParam, qPrintable(where));
			QVERIFY2(steps[i]._lParameter == expected.lParam, qPrintable(where));
			QVERIFY2(steps[i]._sParameter == expected.sParam, qPrintable(where));
		}

		// And written to shortcuts.xml as the original writes it
		nppParams.writeShortcuts();
		const QByteArray written = readFile(QString::fromStdWString(nppParams.getShortcutsPath()));
		const auto actionsOf = [](const QByteArray& xml, const QByteArray& name)
		{
			QList<QByteArray> actions;
			const qsizetype start = xml.indexOf("<Macro name=\"" + name + "\"");
			const qsizetype end = xml.indexOf("</Macro>", start);
			if (start == -1 || end == -1)
				return actions;
			for (const QByteArray& line : xml.mid(start, end - start).split('\n'))
			{
				if (line.contains("<Action"))
					actions << line.trimmed();
			}
			return actions;
		};
		QCOMPARE(actionsOf(written, "Recorded macro"), actionsOf(windowsMacroXml("Recorded macro"), "Recorded macro"));
		QCOMPARE(actionsOf(written, "Recorded macro").size(), static_cast<qsizetype>(std::size(windowsMacro)));

		::SendMessage(editor, SCI_SETSAVEPOINT, 0, 0);
		command(IDM_FILE_CLOSE);
	}

	// The button between "Find what" and "Replace with" swaps them, and its arrow has a menu to copy one to the other
	void swapFindAndReplace()
	{
		command(IDM_SEARCH_REPLACE);
		auto* swap = qobject_cast<QToolButton*>(nppFindControl(findDialog(), IDD_FINDREPLACE_SWAP_BUTTON));
		QVERIFY(swap);
		QCOMPARE(swap->width(), swap->sizeHint().width()); // as narrow as on Windows, not stretched over its column

		auto* findWhat = qobject_cast<QComboBox*>(nppFindControl(findDialog(), IDFINDWHAT));
		auto* replaceWith = qobject_cast<QComboBox*>(nppFindControl(findDialog(), IDREPLACEWITH));
		setComboText(IDFINDWHAT, QStringLiteral("one"));
		setComboText(IDREPLACEWITH, QStringLiteral("two"));
		click(IDD_FINDREPLACE_SWAP_BUTTON);
		QCOMPARE(findWhat->currentText(), QStringLiteral("two"));
		QCOMPARE(replaceWith->currentText(), QStringLiteral("one"));

		QMenu* menu = swap->menu();
		QVERIFY(menu);
		QCOMPARE(menu->actions().size(), 3);
		nppFindMenuAction(menu, IDC_COPY_FIND2REPLACE)->trigger();
		QCOMPARE(replaceWith->currentText(), QStringLiteral("two"));
		QCOMPARE(swap->text(), QStringLiteral("⤵"));

		// The button then does what was chosen last
		setComboText(IDFINDWHAT, QStringLiteral("three"));
		click(IDD_FINDREPLACE_SWAP_BUTTON);
		QCOMPARE(replaceWith->currentText(), QStringLiteral("three"));
		nppFindMenuAction(menu, IDC_SWAP_FIND_REPLACE)->trigger();
		QCOMPARE(swap->text(), QStringLiteral("⇅"));

		setComboText(IDFINDWHAT, QString());
		setComboText(IDREPLACEWITH, QString());
		findDialog()->hide();
	}

	// The option beside "Find what" highlights its matches while the dialog shows the Find or Replace tab
	void findDialogHighlightsMatches()
	{
		openWithContent(QStringLiteral("highlight.txt"), "alpha beta Alpha\nbeta alpha\n");
		HWND editor = currentEditor();

		const auto highlighted = [this]()
		{
			HWND view = currentEditor();
			int count = 0;
			const LRESULT length = ::SendMessage(view, SCI_GETLENGTH, 0, 0);
			for (LRESULT position = 0; position < length;)
			{
				const LRESULT end = ::SendMessage(view, SCI_INDICATOREND, FINDDLG_HIGHLIGHT_INDIC, position);
				if (::SendMessage(view, SCI_INDICATORVALUEAT, FINDDLG_HIGHLIGHT_INDIC, position) != 0)
					++count;
				if (end <= position)
					break;
				position = end;
			}
			return count;
		};

		// No word at the caret for the dialog to take
		const LRESULT docEnd = ::SendMessage(editor, SCI_GETLENGTH, 0, 0);
		::SendMessage(editor, SCI_SETSEL, docEnd, docEnd);
		command(IDM_SEARCH_FIND);
		resetFindOptions();
		setChecked(IDC_HIGHLIGHT_MODE, false);
		setComboText(IDFINDWHAT, QString());

		// Off by default, and then each typed letter updates the highlighting a moment later
		auto* findWhat = qobject_cast<QComboBox*>(nppFindControl(findDialog(), IDFINDWHAT));
		QVERIFY(findWhat);
		QTest::keyClicks(findWhat->lineEdit(), QStringLiteral("alpha"));
		QTest::qWait(400);
		QCOMPARE(highlighted(), 0);
		setChecked(IDC_HIGHLIGHT_MODE, true);
		QCOMPARE(highlighted(), 3);
		QTest::keyClick(findWhat->lineEdit(), Qt::Key_Backspace);
		QTest::keyClicks(findWhat->lineEdit(), QStringLiteral("a b"));
		QTRY_COMPARE(highlighted(), 1);
		findWhat->lineEdit()->clear();
		QTest::keyClicks(findWhat->lineEdit(), QStringLiteral("alpha"));
		QTRY_COMPARE(highlighted(), 3);

		// The options of the dialog
		setChecked(IDMATCHCASE, true);
		QCOMPARE(highlighted(), 2);
		setChecked(IDMATCHCASE, false);
		setChecked(IDREGEXP, true);
		setComboText(IDFINDWHAT, QStringLiteral("^(alpha|beta)"));
		QTest::keyClicks(findWhat->lineEdit(), QStringLiteral(" "));
		QTRY_COMPARE(highlighted(), 2);
		resetFindOptions();
		setComboText(IDFINDWHAT, QStringLiteral("alpha"));
		QTest::keyClicks(findWhat->lineEdit(), QStringLiteral(" "));
		QTest::keyClick(findWhat->lineEdit(), Qt::Key_Backspace);
		QTRY_COMPARE(highlighted(), 3);

		// Not on the other tabs
		command(IDM_SEARCH_MARK);
		QCOMPARE(highlighted(), 0);
		command(IDM_SEARCH_REPLACE);
		QCOMPARE(highlighted(), 3);
		command(IDM_SEARCH_FINDINFILES);
		QCOMPARE(highlighted(), 0);
		command(IDM_SEARCH_FIND);
		QCOMPARE(highlighted(), 3);

		// An edit of the document
		::SendMessage(editor, SCI_INSERTTEXT, 0, reinterpret_cast<LPARAM>("alpha "));
		QTRY_COMPARE(highlighted(), 4);
		::SendMessage(editor, SCI_UNDO, 0, 0);
		QTRY_COMPARE(highlighted(), 3);

		// Only while the dialog is shown
		findDialog()->hide();
		QCOMPARE(highlighted(), 0);
		::SendMessage(editor, SCI_SETSEL, docEnd, docEnd);
		command(IDM_SEARCH_FIND);
		QCOMPARE(highlighted(), 3);

		// It follows the current document
		openWithContent(QStringLiteral("highlight2.txt"), "alpha\n");
		QCOMPARE(highlighted(), 1);
		send(WM_COMMAND, IDM_FILE_CLOSE);
		QCOMPARE(highlighted(), 3);

		setChecked(IDC_HIGHLIGHT_MODE, false);
		QCOMPARE(highlighted(), 0);
		findDialog()->hide();
		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	void replaceVariants()
	{
		using Span = std::pair<LRESULT, LRESULT>;
		openWithContent(QStringLiteral("replace2.txt"), "cat dog cat\nbird cat\n");

		command(IDM_SEARCH_REPLACE);
		resetFindOptions();
		setComboText(IDFINDWHAT, QStringLiteral("cat"));
		setComboText(IDREPLACEWITH, QStringLiteral("lion"));

		// One at a time: the first press finds, the next one replaces and moves on
		setSelection(0, 0);
		click(IDREPLACE);
		QCOMPARE(selection(), Span(0, 3));
		QCOMPARE(editorText(), QByteArray("cat dog cat\nbird cat\n"));
		click(IDREPLACE);
		QCOMPARE(editorText(), QByteArray("lion dog cat\nbird cat\n"));
		QCOMPARE(selection(), Span(9, 12));

		// Undone in one step
		click(IDREPLACEALL);
		QCOMPARE(editorText(), QByteArray("lion dog lion\nbird lion\n"));
		QVERIFY2(findStatus().contains(QStringLiteral("2 occurrences")), qPrintable(findStatus()));
		::SendMessage(currentEditor(), SCI_UNDO, 0, 0);
		QCOMPARE(editorText(), QByteArray("lion dog cat\nbird cat\n"));

		// Only in the selection. The check box follows the selection when the dialog is activated, which a user
		// does by coming back to it from the editor, and a compositor may not do for a test.
		setSelection(0, 13);
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
		QApplication::setActiveWindow(mainWindow());
		QApplication::setActiveWindow(findDialog());
QT_WARNING_POP
		QVERIFY(nppFindControl(findDialog(), IDC_IN_SELECTION_CHECK)->isEnabled());
		setChecked(IDC_IN_SELECTION_CHECK, true);
		click(IDREPLACEALL);
		QCOMPARE(editorText(), QByteArray("lion dog lion\nbird cat\n"));
		setChecked(IDC_IN_SELECTION_CHECK, false);

		// Groups in a regular expression, and escapes in extended mode
		setChecked(IDREGEXP, true);
		setComboText(IDFINDWHAT, QStringLiteral("(\\w+) (\\w+)$"));
		setComboText(IDREPLACEWITH, QStringLiteral("$2 \\U$1"));
		click(IDREPLACEALL);
		QCOMPARE(editorText(), QByteArray("lion lion DOG\ncat BIRD\n"));
		setChecked(IDEXTENDED, true);
		setComboText(IDFINDWHAT, QStringLiteral("\\n"));
		setComboText(IDREPLACEWITH, QStringLiteral("\\t"));
		click(IDREPLACEALL);
		QCOMPARE(editorText(), QByteArray("lion lion DOG\tcat BIRD\t"));

		// In all the open documents
		resetFindOptions();
		const QString firstPath = currentPath();
		openWithContent(QStringLiteral("replace3.txt"), "lion again\n");
		setComboText(IDFINDWHAT, QStringLiteral("lion"));
		setComboText(IDREPLACEWITH, QStringLiteral("tiger"));
		whileModal([](QWidget* ask)
		{
			if (auto* box = qobject_cast<QMessageBox*>(ask))
				box->button(QMessageBox::Ok) ? box->button(QMessageBox::Ok)->click() : box->button(QMessageBox::Yes)->click();
		}, [this]() { click(IDC_REPLACE_OPENEDFILES); });
		QCOMPARE(editorText(), QByteArray("tiger again\n"));
		answerModalDialog(IDNO);
		send(WM_COMMAND, IDM_FILE_CLOSE);
		QCOMPARE(currentPath(), firstPath);
		QVERIFY(editorText().startsWith("tiger tiger DOG"));

		findDialog()->hide();
		answerModalDialog(IDNO);
		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	void findInFilesFilters()
	{
		QTemporaryDir folder;
		QVERIFY(folder.isValid());
		const QDir root(folder.path());
		QVERIFY(root.mkpath(QStringLiteral("sub/deeper")));
		QVERIFY(root.mkpath(QStringLiteral(".hidden")));
		writeFile(root.filePath(QStringLiteral("top.txt")), "needle top\n");
		writeFile(root.filePath(QStringLiteral("top.log")), "needle log\n");
		writeFile(root.filePath(QStringLiteral("sub/inner.txt")), "needle inner\n");
		writeFile(root.filePath(QStringLiteral("sub/deeper/low.txt")), "needle low\n");
		writeFile(root.filePath(QStringLiteral(".hidden/secret.txt")), "needle secret\n");

		// Other encodings are searched by their text
		QByteArray utf16le("\xFF\xFE", 2);
		const QString wide = QStringLiteral("needle wide \u00E9t\u00E9\n");
		utf16le.append(reinterpret_cast<const char*>(wide.utf16()), wide.size() * 2);
		writeFile(root.filePath(QStringLiteral("wide.txt")), utf16le);
		writeFile(root.filePath(QStringLiteral("utf8.txt")), "needle \xC3\xA9t\xC3\xA9\n");

		const auto search = [&](const QString& what, const QString& filters, bool isRecursive, bool inHidden)
		{
			command(IDM_SEARCH_FINDINFILES);
			resetFindOptions();
			setComboText(IDFINDWHAT, what);
			setComboText(IDD_FINDINFILES_FILTERS_COMBO, filters);
			setComboText(IDD_FINDINFILES_DIR_COMBO, folder.path());
			setChecked(IDD_FINDINFILES_RECURSIVE_CHECK, isRecursive);
			setChecked(IDD_FINDINFILES_INHIDDENDIR_CHECK, inHidden);
			click(IDD_FINDINFILES_FIND_BUTTON);
			QCoreApplication::processEvents();

			// The newest search is at the top of the panel
			const QByteArray text = finderText();
			const qsizetype next = text.indexOf("\nSearch \"", 1);
			return next < 0 ? text : text.left(next);
		};

		QByteArray results = search(QStringLiteral("needle"), QStringLiteral("*.txt"), false, false);
		QVERIFY2(results.contains("top.txt") && results.contains("wide.txt") && results.contains("utf8.txt"), results.constData());
		QVERIFY2(!results.contains("top.log") && !results.contains("inner.txt") && !results.contains("secret.txt"), results.constData());

		results = search(QStringLiteral("needle"), QStringLiteral("*.txt"), true, false);
		QVERIFY2(results.contains("inner.txt") && results.contains("low.txt"), results.constData());
		QVERIFY2(!results.contains("secret.txt"), results.constData());

		results = search(QStringLiteral("needle"), QStringLiteral("*.txt"), true, true);
		QVERIFY2(results.contains("secret.txt"), results.constData());

		// Several filters, and leaving out by name and by folder
		results = search(QStringLiteral("needle"), QStringLiteral("*.txt *.log"), false, false);
		QVERIFY2(results.contains("top.txt") && results.contains("top.log"), results.constData());
		results = search(QStringLiteral("needle"), QStringLiteral("*.* !*.log"), false, false);
		QVERIFY2(results.contains("top.txt") && !results.contains("top.log"), results.constData());
		results = search(QStringLiteral("needle"), QStringLiteral("*.txt !+\\deeper"), true, false);
		QVERIFY2(results.contains("inner.txt") && !results.contains("low.txt"), results.constData());

		// Non-ASCII text is found in every encoding, without case
		results = search(QString::fromUtf8("\xC3\x89T\xC3\x89"), QStringLiteral("*.txt"), false, false);
		QVERIFY2(results.contains("wide.txt"), results.constData());
		QVERIFY2(results.contains("utf8.txt"), results.constData());

		// Replacing in the files keeps their encoding
		command(IDM_SEARCH_FINDINFILES);
		resetFindOptions();
		setComboText(IDFINDWHAT, QStringLiteral("wide"));
		setComboText(IDREPLACEWITH, QStringLiteral("narrow"));
		setComboText(IDD_FINDINFILES_FILTERS_COMBO, QStringLiteral("wide.txt"));
		setComboText(IDD_FINDINFILES_DIR_COMBO, folder.path());
		setChecked(IDD_FINDINFILES_RECURSIVE_CHECK, false);
		whileModal([](QWidget* ask)
		{
			if (auto* box = qobject_cast<QMessageBox*>(ask))
				box->button(QMessageBox::Ok) ? box->button(QMessageBox::Ok)->click() : box->button(QMessageBox::Yes)->click();
		}, [this]() { click(IDD_FINDINFILES_REPLACEINFILES); });
		QVERIFY2(findStatus().contains(QStringLiteral("1 occurrence")), qPrintable(findStatus()));

		QByteArray expected("\xFF\xFE", 2);
		const QString replaced = QStringLiteral("needle narrow \u00E9t\u00E9\n");
		expected.append(reinterpret_cast<const char*>(replaced.utf16()), replaced.size() * 2);
		QCOMPARE(readFile(root.filePath(QStringLiteral("wide.txt"))), expected);

		findDialog()->hide();
	}

	void findResultsNavigation()
	{
		openWithContent(QStringLiteral("navigate.txt"), "first needle\nnothing\nthird needle\n");

		command(IDM_SEARCH_FIND);
		resetFindOptions();
		setComboText(IDFINDWHAT, QStringLiteral("needle"));
		click(IDC_FINDALL_CURRENTFILE);
		QVERIFY(finderText().contains("2 hits"));
		findDialog()->hide();

		// The next and previous results move the caret in the document
		const auto currentLine = [this]()
		{
			const LRESULT position = ::SendMessage(currentEditor(), SCI_GETCURRENTPOS, 0, 0);
			return ::SendMessage(currentEditor(), SCI_LINEFROMPOSITION, position, 0);
		};
		command(IDM_SEARCH_GOTONEXTFOUND);
		const LRESULT firstHit = currentLine();
		command(IDM_SEARCH_GOTONEXTFOUND);
		const LRESULT secondHit = currentLine();
		QCOMPARE(firstHit, static_cast<LRESULT>(0));
		QCOMPARE(secondHit, static_cast<LRESULT>(2));
		command(IDM_SEARCH_GOTOPREVFOUND);
		QCOMPARE(currentLine(), static_cast<LRESULT>(0));

		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	void tabDecorations()
	{
		openWithContent(QStringLiteral("tabA.txt"), "a\n");
		const QString aPath = currentPath();
		openWithContent(QStringLiteral("tabB.txt"), "b\n");
		const QString bPath = currentPath();
		openWithContent(QStringLiteral("tabC.txt"), "c\n");

		QTabBar* tabs = nullptr;
		const QList<QTabBar*> bars = mainWindow()->findChildren<QTabBar*>();
		for (QTabBar* bar : bars)
		{
			for (int i = 0; i < bar->count(); ++i)
			{
				if (bar->tabText(i) == QStringLiteral("tabC.txt"))
					tabs = bar;
			}
		}
		QVERIFY(tabs);
		const auto tabIndex = [tabs](const QString& name)
		{
			for (int i = 0; i < tabs->count(); ++i)
			{
				if (tabs->tabText(i) == name)
					return i;
			}
			return -1;
		};
		tabs->setIconSize(tabs->iconSize());

		// Colour for a tab, seen once another one is current
		const auto sampleTab = [tabs](int index)
		{
			QTest::qWait(0);
			const QRect area = tabs->tabRect(index);
			const QImage image = tabs->grab().toImage();
			return image.pixelColor((QPointF(area.left() + 3, area.top() + area.height() - 4) * image.devicePixelRatio()).toPoint());
		};
		send(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(aPath.toStdWString().c_str()));
		command(IDM_VIEW_TAB_COLOUR_1);
		send(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(bPath.toStdWString().c_str()));
		const QColor coloured = sampleTab(tabIndex(QStringLiteral("tabA.txt")));
		const QColor plain = sampleTab(tabIndex(QStringLiteral("tabC.txt")));
		QVERIFY2(coloured != plain, qPrintable(coloured.name() + QLatin1Char(' ') + plain.name()));

		// Each tab has a pin, and a click pins the document and moves its tab to the front
		const int cIndex = tabIndex(QStringLiteral("tabC.txt"));
		auto* pin = qobject_cast<QAbstractButton*>(tabs->tabButton(cIndex, QTabBar::LeftSide));
		QVERIFY(pin);
		QVERIFY(tabs->tabButton(cIndex, QTabBar::RightSide));
		screenshot(QStringLiteral("tabs"));
		pin->click();
		QCOMPARE(tabIndex(QStringLiteral("tabC.txt")), 0);
		auto* movedPin = qobject_cast<QAbstractButton*>(tabs->tabButton(0, QTabBar::LeftSide));
		QVERIFY(movedPin);
		QVERIFY(movedPin->isVisibleTo(tabs));
		screenshot(QStringLiteral("tabs-pinned"));
		movedPin->click();
		QVERIFY(tabIndex(QStringLiteral("tabC.txt")) > 0);

		// Removing the colour
		send(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(aPath.toStdWString().c_str()));
		command(IDM_VIEW_TAB_COLOUR_NONE);
		send(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(bPath.toStdWString().c_str()));
		QCOMPARE(sampleTab(tabIndex(QStringLiteral("tabA.txt"))), plain);

		// The option takes the pins away
		NppGUI& gui = NppParameters::getInstance().getNppGUI();
		const int status = gui._tabStatus;
		gui._tabStatus &= ~TAB_PINBUTTON;
		send(NPPM_INTERNAL_DRAWTABBARPINBUTTON);
		QVERIFY(!tabs->tabButton(tabIndex(QStringLiteral("tabC.txt")), QTabBar::LeftSide));
		gui._tabStatus = status;
		send(NPPM_INTERNAL_DRAWTABBARPINBUTTON);
		QVERIFY(tabs->tabButton(tabIndex(QStringLiteral("tabC.txt")), QTabBar::LeftSide));

		for (const QString& name : { QStringLiteral("tabA.txt"), QStringLiteral("tabB.txt"), QStringLiteral("tabC.txt") })
		{
			const QString path = _files.filePath(name);
			send(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(path.toStdWString().c_str()));
			send(WM_COMMAND, IDM_FILE_CLOSE);
		}
	}

	// Through the menu's own actions, as a click does, and not by sending the command
	void menuCheckMarks()
	{
		HMENU mainMenu = reinterpret_cast<HMENU>(send(NPPM_INTERNAL_GETMENU));
		openWithContent(QStringLiteral("menumarks.txt"), "text\n");

		const auto trigger = [mainMenu](int cmdID)
		{
			QAction* action = nppFindMenuAction(mainMenu, cmdID);
			QVERIFY(action);
			action->trigger();
			QCoreApplication::processEvents();
		};

		// Every item with a mark goes on and off, however often
		const bool wasWrapped = ::SendMessage(currentEditor(), SCI_GETWRAPMODE, 0, 0) != 0;
		for (int round = 0; round < 4; ++round)
		{
			trigger(IDM_VIEW_WRAP);
			QCOMPARE(::SendMessage(currentEditor(), SCI_GETWRAPMODE, 0, 0) != 0, (round % 2 == 0) ? !wasWrapped : wasWrapped);
			QCOMPARE(nppIsMenuCommandChecked(mainMenu, IDM_VIEW_WRAP), (round % 2 == 0) ? !wasWrapped : wasWrapped);
		}

		const bool wasShown = ::SendMessage(currentEditor(), SCI_GETVIEWWS, 0, 0) != 0;
		for (int round = 0; round < 4; ++round)
		{
			trigger(IDM_VIEW_TAB_SPACE);
			QCOMPARE(::SendMessage(currentEditor(), SCI_GETVIEWWS, 0, 0) != 0, (round % 2 == 0) ? !wasShown : wasShown);
		}

		const bool wasEol = ::SendMessage(currentEditor(), SCI_GETVIEWEOL, 0, 0) != 0;
		trigger(IDM_VIEW_EOL);
		QCOMPARE(::SendMessage(currentEditor(), SCI_GETVIEWEOL, 0, 0) != 0, !wasEol);
		trigger(IDM_VIEW_EOL);
		QCOMPARE(::SendMessage(currentEditor(), SCI_GETVIEWEOL, 0, 0) != 0, wasEol);

		// A panel from the menu, on and off
		trigger(IDM_VIEW_FUNC_LIST);
		const bool isListed = nppIsMenuCommandChecked(mainMenu, IDM_VIEW_FUNC_LIST);
		trigger(IDM_VIEW_FUNC_LIST);
		QCOMPARE(nppIsMenuCommandChecked(mainMenu, IDM_VIEW_FUNC_LIST), !isListed);

		send(WM_COMMAND, IDM_FILE_CLOSE);
	}

	void toolbarAndPanelIcons()
	{
		QToolBar* toolbar = mainWindow()->findChild<QToolBar*>(QStringLiteral("mainToolBar"));
		QVERIFY(toolbar);
		QAction* saveAction = nullptr;
		QAction* allCharacters = nullptr;
		const QList<QAction*> actions = toolbar->actions();
		const auto iconImage = [](const QIcon& icon) { return icon.pixmap(16, 16).toImage(); };

		// The button with a menu, and any other button with an icon
		for (QAction* action : actions)
		{
			auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(action));
			if (button && button->menu())
				allCharacters = action;
			else if (button && !saveAction && !action->icon().isNull())
				saveAction = action;
		}
		QVERIFY(saveAction);
		QVERIFY(allCharacters);

		// The symbols button: its menu has the neighbours in it, which stay in step with the main menu
		auto* symbols = qobject_cast<QToolButton*>(toolbar->widgetForAction(allCharacters));
		QCOMPARE(symbols->popupMode(), QToolButton::MenuButtonPopup);
		const QList<QAction*> symbolItems = symbols->menu()->actions();
		QCOMPARE(symbolItems.size(), 6);
		QVERIFY(symbolItems[4]->isSeparator());
		const bool wasShown = symbolItems[0]->isChecked();
		symbolItems[0]->trigger();
		QCOMPARE(symbolItems[0]->isChecked(), !wasShown);
		symbolItems[0]->trigger();
		QCOMPARE(symbolItems[0]->isChecked(), wasShown);

		// Fluent icons in a colour of the preferences
		command(IDM_SETTING_PREFERENCE);
		QWidget* dialog = visibleDialogWith(IDC_LIST_DLGTITLE);
		QVERIFY(dialog);
		auto* categories = qobject_cast<QListWidget*>(nppFindControl(dialog, IDC_LIST_DLGTITLE));
		categories->setCurrentRow(categories->row(categories->findItems(QStringLiteral("Toolbar"), Qt::MatchExactly).first()));
		const auto clickRadio = [dialog](int id)
		{
			auto* radio = qobject_cast<QAbstractButton*>(nppFindControl(dialog, id));
			QVERIFY(radio);
			radio->click();
		};

		clickRadio(IDC_RADIO_SMALLICON);
		clickRadio(IDC_RADIO_DEFAULTCOLOR);
		clickRadio(IDC_RADIO_PARTIAL);
		const QImage defaultIcon = iconImage(saveAction->icon());
		clickRadio(IDC_RADIO_RED);
		const QImage partlyRed = iconImage(saveAction->icon());
		QVERIFY(partlyRed != defaultIcon);
		clickRadio(IDC_RADIO_COMPLETE);
		const QImage allRed = iconImage(saveAction->icon());
		QVERIFY(allRed != partlyRed);
		bool isEveryPixelRed = true;
		for (int y = 0; y < allRed.height(); ++y)
		{
			for (int x = 0; x < allRed.width(); ++x)
			{
				const QColor pixel = allRed.pixelColor(x, y);
				// The edges of a scaled icon blend, so only the solid part says it
				if (pixel.alpha() == 255 && (qAbs(pixel.red() - 0xE8) > 3 || qAbs(pixel.green() - 0x11) > 3 || qAbs(pixel.blue() - 0x23) > 3))
					isEveryPixelRed = false;
			}
		}
		QVERIFY(isEveryPixelRed);
		clickRadio(IDC_RADIO_PARTIAL);
		clickRadio(IDC_RADIO_DEFAULTCOLOR);
		QCOMPARE(iconImage(saveAction->icon()), defaultIcon);

		// The panels' icons follow dark mode. The Project Panel has been opened by its test when run in order.
		QTreeWidget* tree = nullptr;
		const QList<QTreeWidget*> trees = mainWindow()->findChildren<QTreeWidget*>();
		for (QTreeWidget* candidate : trees)
		{
			if (candidate->property("nppId").toInt() == ID_PROJECTTREEVIEW)
				tree = candidate;
		}

		// As on Windows, the trees take a new icon set with the next change of dark mode, so that is done once first
		categories->setCurrentRow(categories->row(categories->findItems(QStringLiteral("Dark Mode"), Qt::MatchExactly).first()));
		clickRadio(IDC_RADIO_DARKMODE_DARKMODE);
		clickRadio(IDC_RADIO_DARKMODE_LIGHTMODE);
		const QImage lightTreeIcon = (tree && tree->topLevelItemCount() > 0) ? iconImage(tree->topLevelItem(0)->icon(0)) : QImage();

		clickRadio(IDC_RADIO_DARKMODE_DARKMODE);
		if (!lightTreeIcon.isNull())
			QVERIFY(iconImage(tree->topLevelItem(0)->icon(0)) != lightTreeIcon);
		clickRadio(IDC_RADIO_DARKMODE_LIGHTMODE);
		if (!lightTreeIcon.isNull())
			QCOMPARE(iconImage(tree->topLevelItem(0)->icon(0)), lightTreeIcon);

		categories->setCurrentRow(categories->row(categories->findItems(QStringLiteral("Toolbar"), Qt::MatchExactly).first()));
		clickRadio(IDC_RADIO_STANDARD);
		dialog->hide();
	}

	void customToolbarIcons()
	{
		QToolBar* toolbar = mainWindow()->findChild<QToolBar*>(QStringLiteral("mainToolBar"));
		QVERIFY(toolbar);

		// The buttons by their place, without the separators. A plugin's button comes after them.
		QList<QAction*> buttons;
		const QList<QAction*> actions = toolbar->actions();
		for (QAction* action : actions)
		{
			if (!action->isSeparator())
				buttons.append(action);
		}
		QVERIFY(buttons.size() >= 32);
		QAction* newFile = buttons[0];
		QAction* open = buttons[1];
		QAction* cut = buttons[7];
		QAction* saveMacro = buttons[31];

		const auto colourOf = [](QAction* action, QIcon::Mode mode)
		{
			return action->icon().pixmap(QSize(16, 16), mode).toImage().pixelColor(8, 8);
		};
		const auto isOneColour = [](QAction* action, QIcon::Mode mode)
		{
			const QImage image = action->icon().pixmap(QSize(16, 16), mode).toImage();
			for (int y = 0; y < image.height(); ++y)
			{
				for (int x = 0; x < image.width(); ++x)
				{
					if (image.pixelColor(x, y) != image.pixelColor(0, 0))
						return false;
				}
			}
			return true;
		};
		const auto checkSet = [&]()
		{
			QCOMPARE(colourOf(saveMacro, QIcon::Normal), customIconColour);
			QCOMPARE(colourOf(saveMacro, QIcon::Disabled), customDisabledIconColour);
			QCOMPARE(colourOf(cut, QIcon::Normal), customIconColour);

			// No disabled file: the built-in one, not a grey copy of the custom one
			QVERIFY(!isOneColour(cut, QIcon::Disabled));

			// Not in the set, or not an icon: the built-in one
			QVERIFY(!isOneColour(newFile, QIcon::Normal));
			QVERIFY(!isOneColour(open, QIcon::Normal));
		};

		command(IDM_SETTING_PREFERENCE);
		QWidget* dialog = visibleDialogWith(IDC_LIST_DLGTITLE);
		QVERIFY(dialog);
		auto* categories = qobject_cast<QListWidget*>(nppFindControl(dialog, IDC_LIST_DLGTITLE));
		const auto showPage = [categories](const QString& name)
		{
			categories->setCurrentRow(categories->row(categories->findItems(name, Qt::MatchExactly).first()));
		};
		const auto clickRadio = [dialog](int id)
		{
			auto* radio = qobject_cast<QAbstractButton*>(nppFindControl(dialog, id));
			QVERIFY(radio);
			radio->click();
		};

		// The standard set gives way to the small one, which the custom icons replace
		showPage(QStringLiteral("Toolbar"));
		clickRadio(IDC_RADIO_STANDARD);
		QCOMPARE(::SendMessage(mainWindow(), NPPM_GETTOOLBARICONSETCHOICE, 0, 0), static_cast<LRESULT>(TB_SMALL));
		checkSet();

		// In each set, and without taking the colour of the preferences
		for (const int iconSet : { IDC_RADIO_SMALLICON, IDC_RADIO_BIGICON, IDC_RADIO_SMALLICON2, IDC_RADIO_BIGICON2 })
		{
			clickRadio(iconSet);
			checkSet();
		}
		clickRadio(IDC_RADIO_SMALLICON);
		clickRadio(IDC_RADIO_RED);
		checkSet();
		clickRadio(IDC_RADIO_DEFAULTCOLOR);

		// And in dark mode
		showPage(QStringLiteral("Dark Mode"));
		clickRadio(IDC_RADIO_DARKMODE_DARKMODE);
		checkSet();
		clickRadio(IDC_RADIO_DARKMODE_LIGHTMODE);
		checkSet();
		dialog->hide();
	}

	// Every dialog as it is used, with its translation and everything the code adds to the form: its controls
	// must not overlap or cut off their text. NPP_TEST_APP_DIR gets a picture of each.
	void dialogLayouts()
	{
		const QString dir = qEnvironmentVariable("NPP_TEST_APP_DIR");
		QStringList problems;

		// A desktop with a dark theme: light mode is light all the same, and Follow system has the desktop's colours
		if (qEnvironmentVariableIsSet("NPP_TEST_DARK_DESKTOP"))
		{
			if (!NppDarkMode::isDesktopDark())
				QSKIP("this needs the KDE platform theme");
			QVERIFY(QApplication::palette().color(QPalette::Window).lightness() >= 128);
			QVERIFY(!NppDarkMode::usesDarkWidgets());
			checkBarsFollowMode();

			command(IDM_SETTING_PREFERENCE);
			QWidget* preferences = visibleDialogWith(IDC_LIST_DLGTITLE);
			QVERIFY(preferences);
			qobject_cast<QAbstractButton*>(nppFindControl(preferences, IDC_RADIO_DARKMODE_FOLLOWWINDOWS))->click();
			QTest::qWait(50);
			QCOMPARE(QApplication::palette().color(QPalette::Window), QColor(36, 39, 44));
			QVERIFY(NppDarkMode::isEnabled());
			checkBarsFollowMode();
			qobject_cast<QAbstractButton*>(nppFindControl(preferences, IDC_RADIO_DARKMODE_LIGHTMODE))->click();
			QTest::qWait(50);
			QVERIFY(QApplication::palette().color(QPalette::Window).lightness() >= 128);
			QVERIFY(!NppDarkMode::isEnabled());
			checkBarsFollowMode();
			preferences->hide();
		}

		// As a user does: dark mode from the preferences
		if (qEnvironmentVariableIsSet("NPP_TEST_DARK_MODE"))
		{
			command(IDM_SETTING_PREFERENCE);
			QWidget* preferences = visibleDialogWith(IDC_LIST_DLGTITLE);
			QVERIFY(preferences);
			qobject_cast<QAbstractButton*>(nppFindControl(preferences, IDC_RADIO_DARKMODE_DARKMODE))->click();
			preferences->hide();
			if (qEnvironmentVariableIsSet("NPP_TEST_DARK_DESKTOP"))
			{
				QCOMPARE(QApplication::palette().color(QPalette::Window), QColor(36, 39, 44));
				QCOMPARE(NppDarkMode::getDlgBackgroundColor(), COLORREF{RGB(36, 39, 44)});
			}
		}


		// The Dialog Font of the preferences, or the system's, on everything but the system's message boxes.
		// The panels keep the system's font, as on Windows.
		const QFont systemFont = QApplication::font();
		const int fontSize = _dialogFontSize;
		const auto save = [&dir, &problems, &systemFont, fontSize](QWidget* widget, const QString& name)
		{
			QTest::qWait(50); // for the layout to settle
			if (!dir.isEmpty())
				widget->grab().save(dir + QStringLiteral("/dlg-") + name + QStringLiteral(".png"));
			problems += layoutProblems(widget, name);
			const bool isPanel = name.startsWith(QStringLiteral("panel-"));
			const int expected = (fontSize > 0 && !isPanel) ? fontSize : systemFont.pointSize();
			if (!qobject_cast<QMessageBox*>(widget) && !qobject_cast<QMenu*>(widget) && widget->font().pointSize() != expected)
				problems << QStringLiteral("%1: font size %2 instead of %3").arg(name).arg(widget->font().pointSize()).arg(expected);
		};
		const auto visibleDialogs = []()
		{
			QSet<QWidget*> dialogs;
			const QWidgetList widgets = QApplication::topLevelWidgets();
			for (QWidget* widget : widgets)
			{
				if (qobject_cast<QDialog*>(widget) && widget->isVisible())
					dialogs.insert(widget);
			}
			return dialogs;
		};
		// A command's dialog, modal or not, is shown to the callback and closed
		const auto withDialog = [&](int cmd, const QString& name, const std::function<void(QWidget*)>& step)
		{
			const QSet<QWidget*> before = visibleDialogs();
			bool wasModal = false;
			const auto use = [&](QWidget* dialog)
			{
				save(dialog, name);
				if (step)
					step(dialog);
			};
			QTimer::singleShot(150, mainWindow(), [&]()
			{
				if (QWidget* modal = QApplication::activeModalWidget())
				{
					wasModal = true;
					use(modal);
					if (auto* asDialog = qobject_cast<QDialog*>(modal))
						asDialog->reject();
					else
						modal->close();
				}
			});
			command(cmd);
			QTest::qWait(250);
			if (wasModal)
				return;
			const QSet<QWidget*> shown = visibleDialogs() - before;
			for (QWidget* dialog : shown)
			{
				use(dialog);
				dialog->hide();
			}
		};

		command(IDM_FILE_NEW);
		// A theme's font that is not installed still shows in the Style Configurator
		withDialog(IDM_LANGSTYLE_CONFIG_DLG, QStringLiteral("styles"), [&](QWidget* styles)
		{
			const QList<QComboBox*> combos = styles->findChildren<QComboBox*>();
			bool isFontShown = false;
			for (const QComboBox* comboBox : combos)
				isFontShown = isFontShown || comboBox->currentText() == QStringLiteral("Courier New");
			QVERIFY(isFontShown || qEnvironmentVariableIsSet("NPP_TEST_DARK_MODE")); // the dark theme has another font
		});

		const std::pair<int, const char*> simple[]{
			{ IDM_ABOUT, "about" }, { IDM_CMDLINEARGUMENTS, "cmdline" }, { IDM_DEBUGINFO, "debuginfo" },
			{ IDM_EDIT_COLUMNMODE, "column" }, { IDM_SEARCH_GOTOLINE, "goto" }, { IDM_SEARCH_FINDCHARINRANGE, "findchars" },
			{ IDM_TOOL_MD5_GENERATE, "hash-text" }, { IDM_TOOL_MD5_GENERATEFROMFILE, "hash-files" },
			{ IDM_EXECUTE, "run" }, { IDM_MACRO_RUNMULTIMACRODLG, "runmacro" }, { IDM_WINDOW_WINDOWS, "windows" },
		};
		for (const auto& [cmd, name] : simple)
			withDialog(cmd, QString::fromLatin1(name), {});

		withDialog(IDM_SEARCH_FIND, QStringLiteral("find"), [&](QWidget* dialog)
		{
			auto* tabs = dialog->findChild<QTabBar*>();
			QVERIFY(tabs);

			// Transparency is offered only where it works
			QCOMPARE(nppFindControl(dialog, IDC_TRANSPARENT_CHECK)->isVisible(), !QGuiApplication::platformName().startsWith(QStringLiteral("wayland")));

			// The tabs share one size, and so one position of the buttons
			const int height = dialog->height();
			QHash<int, int> buttonsLeft;

			// The reduced mode takes only the height it needs, on each tab
			auto* toggle = qobject_cast<QAbstractButton*>(nppFindControl(dialog, IDD_RESIZE_TOGGLE_BUTTON));
			QVERIFY(toggle);
			toggle->click();
			QTRY_VERIFY(dialog->height() < height);
			for (int i = 0; i < tabs->count(); ++i)
			{
				QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->tabRect(i).center());
				save(dialog, QStringLiteral("find-less-%1").arg(i));
			}
			QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->tabRect(0).center());
			toggle->click();
			QTRY_COMPARE(dialog->height(), height);
			for (int i = 0; i < tabs->count(); ++i)
			{
				QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->tabRect(i).center()); // the dialog follows clicks
				save(dialog, QStringLiteral("find-%1").arg(i));
				QCOMPARE(dialog->height(), height);
				buttonsLeft[i] = nppFindControl(dialog, IDCANCEL)->mapTo(dialog, QPoint(0, 0)).x();
				QVERIFY2(buttonsLeft[i] == buttonsLeft[0], qPrintable(QStringLiteral("tab %1: buttons at %2, on the first tab at %3, dialog %4 wide, minimum %5")
					.arg(i).arg(buttonsLeft[i]).arg(buttonsLeft[0]).arg(dialog->width()).arg(dialog->minimumWidth())));
			}
		});

		withDialog(IDM_SETTING_PREFERENCE, QStringLiteral("preferences"), [&](QWidget* dialog)
		{
			auto* categories = qobject_cast<QListWidget*>(nppFindControl(dialog, IDC_LIST_DLGTITLE));
			QVERIFY(categories);
			QVERIFY(!categories->verticalScrollBar()->isVisible());
			QVERIFY(categories->viewport()->width() >= categories->sizeHintForColumn(0));
			for (int i = 0; i < categories->count(); ++i)
			{
				categories->setCurrentRow(i);
				save(dialog, QStringLiteral("pref-%1-%2").arg(i, 2, 10, QLatin1Char('0')).arg(categories->item(i)->text().remove(u' ').remove(u'/')));
			}

			// Through dark mode and back, to see below that everything follows it. The style stays the same, so that
			// the two modes differ only in their colours.
			if (!qEnvironmentVariableIsSet("NPP_TEST_DARK_MODE"))
			{
				const QStyle* style = QApplication::style();
				categories->setCurrentRow(categories->row(categories->findItems(QStringLiteral("Dark Mode"), Qt::MatchExactly).first()));
				qobject_cast<QAbstractButton*>(nppFindControl(dialog, IDC_RADIO_DARKMODE_DARKMODE))->click();
				QVERIFY(QApplication::style() == style);
				QTest::qWait(50);
				checkTabsAreReadable();
				checkBarsFollowMode();
				qobject_cast<QAbstractButton*>(nppFindControl(dialog, IDC_RADIO_DARKMODE_LIGHTMODE))->click();
				QVERIFY(QApplication::style() == style);
				QTest::qWait(50);
				checkTabsAreReadable();
				checkBarsFollowMode();
			}
		});

		// Docked by default, so not a window of its own
		QWidget* udl = visibleDialogWith(IDC_LANGNAME_COMBO);
		const bool wasShowing = udl != nullptr;
		if (!wasShowing)
			command(IDM_LANG_USER_DLG);
		udl = visibleDialogWith(IDC_LANGNAME_COMBO);
		QVERIFY(udl);
		auto* udlPages = udl->findChild<QTabWidget*>();
		QVERIFY(udlPages);
		for (int i = 0; i < udlPages->count(); ++i)
		{
			udlPages->setCurrentIndex(i);
			save(udl, QStringLiteral("udl-%1").arg(i));
		}

		// The styler of a UDL item
		whileModal([&](QWidget* styler) { save(styler, QStringLiteral("udl-styler")); },
			[&]() { qobject_cast<QAbstractButton*>(nppFindControl(udl, IDC_DEFAULT_STYLER))->click(); });

		if (!wasShowing)
			command(IDM_LANG_USER_DLG);

		// The shortcut dialogs of the Shortcut Mapper, for a command and for a Scintilla command with its list of keys
		withDialog(IDM_SETTING_SHORTCUT_MAPPER, QStringLiteral("shortcuts"), [&](QWidget* mapper)
		{
			auto* tabs = mapper->findChild<QTabBar*>();
			auto* modify = qobject_cast<QAbstractButton*>(nppFindControl(mapper, IDM_BABYGRID_MODIFY));
			QVERIFY(tabs && modify);
			whileModal([&](QWidget* editor) { save(editor, QStringLiteral("shortcut-edit")); }, [&]() { modify->click(); });
			tabs->setCurrentIndex(tabs->count() - 1);
			QCoreApplication::processEvents();
			whileModal([&](QWidget* editor) { save(editor, QStringLiteral("shortcut-scintilla")); }, [&]() { modify->click(); });
			tabs->setCurrentIndex(0);
		});

		// The incremental search bar in the main window
		command(IDM_SEARCH_FINDINCREMENT);
		{
			QWidget* bar = nullptr;
			const QList<QLineEdit*> edits = mainWindow()->findChildren<QLineEdit*>();
			for (QLineEdit* edit : edits)
			{
				if (edit->property("nppId").toInt() == IDC_INCFINDTEXT && edit->isVisible())
					bar = edit->parentWidget();
			}
			QVERIFY(bar);
			save(bar, QStringLiteral("incremental"));
			QCOMPARE(bar->palette().color(QPalette::Window), QApplication::palette().color(QPalette::Window));
			command(IDM_SEARCH_FINDINCREMENT);
		}

		// The panels. Those a test before this one left open are seen too.
		const auto visibleDocks = [this]()
		{
			QSet<QDockWidget*> docks;
			const QList<QDockWidget*> all = mainWindow()->findChildren<QDockWidget*>();
			for (QDockWidget* dock : all)
			{
				if (dock->isVisible())
					docks.insert(dock);
			}
			return docks;
		};
		for (const int cmd : { IDM_EDIT_CHAR_PANEL, IDM_EDIT_CLIPBOARDHISTORY_PANEL, IDM_VIEW_DOCLIST, IDM_VIEW_FUNC_LIST, IDM_VIEW_DOC_MAP, IDM_VIEW_PROJECT_PANEL_1 })
		{
			const QSet<QDockWidget*> before = visibleDocks();
			command(cmd);
			QTest::qWait(50);
			const QSet<QDockWidget*> opened = visibleDocks() - before;
			for (QDockWidget* dock : opened)
				save(dock->widget(), QStringLiteral("panel-%1").arg(cmd));
			if (!opened.isEmpty())
				command(cmd);
		}

		// Find in Search Results, from the context menu of the results
		setComboText(IDFINDWHAT, QStringLiteral("x"));
		command(IDM_FILE_NEW);
		::SendMessage(currentEditor(), SCI_ADDTEXT, 1, reinterpret_cast<LPARAM>("x"));
		command(IDM_SEARCH_FIND);
		setComboText(IDFINDWHAT, QStringLiteral("x"));
		click(IDC_FINDALL_CURRENTFILE);
		QTRY_VERIFY(!finderText().isEmpty());
		{
			QDockWidget* results = nullptr;
			const QList<QDockWidget*> docks = mainWindow()->findChildren<QDockWidget*>();
			for (QDockWidget* dock : docks)
			{
				const auto* editor = dock->findChild<ScintillaEditBase*>();
				if (editor && textOf(const_cast<ScintillaEditBase*>(editor)).startsWith("Search "))
					results = dock;
			}
			QVERIFY(results);
			// The caret line is on the title of the search, and must not hide it
			auto* resultsEditor = results->findChild<ScintillaEditBase*>();
			QVERIFY(resultsEditor);
			const auto caretLine = static_cast<COLORREF>(::SendMessage(resultsEditor, SCI_GETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK, 0)) & 0xFFFFFF;
			const auto titleColour = static_cast<COLORREF>(::SendMessage(resultsEditor, SCI_STYLEGETFORE, SCE_SEARCHRESULT_SEARCH_HEADER, 0)) & 0xFFFFFF;
			QVERIFY2(NppDarkMode::contrastRatio(caretLine, titleColour) >= 3.0, qPrintable(QString::number(caretLine, 16) + u' ' + QString::number(titleColour, 16)));

			whileModal([&](QWidget* dialog) { save(dialog, QStringLiteral("find-in-results")); },
				[&]() { ::SendMessage(results->widget(), WM_COMMAND, NPPM_INTERNAL_FINDINFINDERDLG, 0); });
		}

		// The confirmation of Save All, which "No" leaves unsaved. It only comes for more than one modified document.
		command(IDM_FILE_NEW);
		::SendMessage(currentEditor(), SCI_ADDTEXT, 1, reinterpret_cast<LPARAM>("y"));
		NppParameters::getInstance().getNppGUI()._saveAllConfirm = true;
		whileModal([&](QWidget* ask)
		{
			save(ask, QStringLiteral("save-all"));
			auto* no = qobject_cast<QAbstractButton*>(nppFindControl(ask, IDNO));
			QVERIFY2(no, qPrintable(QStringLiteral("%1 \"%2\"").arg(QString::fromLatin1(ask->metaObject()->className()), ask->windowTitle())));
			no->click();
		}, [&]() { command(IDM_FILE_SAVEALL); });

		// The default is the system's font, named as such
		if (fontSize == 0)
		{
			command(IDM_SETTING_PREFERENCE);
			QWidget* preferences = visibleDialogWith(IDC_LIST_DLGTITLE);
			QVERIFY(preferences);
			auto* size = qobject_cast<QComboBox*>(nppFindControl(preferences, IDC_COMBO_DIALOG_FONT_SIZE));
			auto* family = qobject_cast<QComboBox*>(nppFindControl(preferences, IDC_COMBO_DIALOG_FONT_NAME));
			QVERIFY(size && family);
			QCOMPARE(size->currentText(), QStringLiteral("System default"));
			QCOMPARE(family->currentText(), QStringLiteral("System default"));
			QCOMPARE(family->findText(QStringLiteral("MS Shell Dlg")), -1);

			// A choice is stored, and the first item goes back to the system's
			NppGUI& gui = NppParameters::getInstance().getNppGUI();
			size->setCurrentIndex(size->findText(QStringLiteral("12")));
			Q_EMIT size->activated(size->currentIndex());
			QCOMPARE(gui._fontDlgSize, WORD{12});
			family->setCurrentIndex(1);
			Q_EMIT family->activated(1);
			QCOMPARE(QString::fromStdWString(gui._fontDlgName), family->itemText(1));
			size->setCurrentIndex(0);
			Q_EMIT size->activated(0);
			QCOMPARE(gui._fontDlgSize, WORD{0});
			family->setCurrentIndex(0);
			Q_EMIT family->activated(0);
			QVERIFY(gui._fontDlgName.empty());
			preferences->hide();
		}

		// What Windows wrote for its default means the default here too, while a real choice stays
		{
			WORD size = 8;
			std::wstring name = L"MS Shell Dlg";
			NppParameters::normalizeDialogFont(size, name);
			QCOMPARE(size, WORD{0});
			QVERIFY(name.empty());
			size = 12;
			name = L"MS Shell Dlg";
			NppParameters::normalizeDialogFont(size, name);
			QCOMPARE(size, WORD{12});
			QVERIFY(name.empty());
			size = 8;
			name = L"Noto Sans";
			NppParameters::normalizeDialogFont(size, name);
			QCOMPARE(size, WORD{8});
			QVERIFY(name == L"Noto Sans");
		}

		// The menus, with the arrows of their submenus
		const QList<QAction*> menuBarActions = mainWindow()->menuBar()->actions();
		for (QAction* topLevel : menuBarActions)
		{
			QMenu* menu = topLevel->menu();
			if (!menu)
				continue;
			QString title = topLevel->text().remove(u'&');
			title.remove(u' ');
			menu->popup(mainWindow()->mapToGlobal(QPoint(0, 0)));
			QTest::qWait(30);
			save(menu, QStringLiteral("menu-") + title);

			// A title with a submenu needs room beyond its text for the arrow
			const QList<QAction*> items = menu->actions();
			for (QAction* item : items)
			{
				if (!item->menu())
					continue;
				const int textWidth = menu->fontMetrics().horizontalAdvance(item->text().remove(u'&'));
				const int arrowRoom = 2 * menu->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, menu);
				if (menu->actionGeometry(item).width() < textWidth + arrowRoom)
					problems << QStringLiteral("menu %1: \"%2\" has %3 for %4 of text").arg(title, item->text()).arg(menu->actionGeometry(item).width()).arg(textWidth);
			}
			menu->close();
		}

		// The editor's right click menu
		{
			bool wasShown = false;
			QTimer::singleShot(0, mainWindow(), [&]()
			{
				if (QWidget* popup = QApplication::activePopupWidget())
				{
					wasShown = true;
					save(popup, QStringLiteral("editor-menu"));
					popup->close();
				}
			});
			const QPoint point(100, 10);
			QContextMenuEvent event(QContextMenuEvent::Mouse, point, currentEditor()->mapToGlobal(point));
			QApplication::sendEvent(currentEditor(), &event);
			QVERIFY(wasShown);
		}

		// The main window with its tabs and panels
		screenshot(QStringLiteral("main-window"));
		checkTabsAreReadable();
		checkBarsFollowMode();
		checkEditorHasNoSeams();

		// The save prompt
		command(IDM_FILE_NEW);
		::SendMessage(currentEditor(), SCI_ADDTEXT, 1, reinterpret_cast<LPARAM>("x"));
		whileModal([&](QWidget* ask)
		{
			save(ask, QStringLiteral("save-prompt"));
			auto* messageBox = qobject_cast<QMessageBox*>(ask);
			QVERIFY(messageBox);
			messageBox->button(QMessageBox::No)->click();
		}, [&]() { command(IDM_FILE_CLOSE); });

		QVERIFY2(problems.isEmpty(), qPrintable(problems.join(u'\n')));
	}

	// The desktop's theme changes while the application runs, as a user does it in the system settings
	void desktopThemeChanges()
	{
		if (!qEnvironmentVariableIsSet("NPP_TEST_THEME_CHANGE"))
			QSKIP("this needs a private session, see CLAUDE.md");
		if (!NppDarkMode::isDesktopDark())
			QSKIP("this needs the KDE platform theme");

		// Follow system, with the preferences never opened in this session: the editor goes along
		QVERIFY(NppDarkMode::isWindowsModeEnabled());
		QVERIFY(NppDarkMode::isEnabled());
		command(IDM_FILE_NEW);
		checkLooks(true, true);
		changeDesktopTheme(false);
		QVERIFY(!NppDarkMode::isEnabled());
		checkLooks(false, false);
		changeDesktopTheme(true);
		QVERIFY(NppDarkMode::isEnabled());
		checkLooks(true, true);

		// The same with the preferences opened, which takes another way through the code
		command(IDM_SETTING_PREFERENCE);
		QWidget* preferences = visibleDialogWith(IDC_LIST_DLGTITLE);
		QVERIFY(preferences);
		preferences->hide();
		changeDesktopTheme(false);
		QVERIFY(!NppDarkMode::isEnabled());
		checkLooks(false, false);
		changeDesktopTheme(true);
		QVERIFY(NppDarkMode::isEnabled());
		checkLooks(true, true);

		// A chosen mode stays as it is whatever the desktop does. KDE tells an application with a colour scheme of its
		// own nothing about the desktop, so only these two changes can reach it: those from the desktop's colours.
		qobject_cast<QAbstractButton*>(nppFindControl(preferences, IDC_RADIO_DARKMODE_DARKMODE))->click();
		QTest::qWait(50);
		QCOMPARE(QApplication::palette().color(QPalette::Window), QColor(36, 39, 44));
		checkLooks(true, true);
		changeDesktopTheme(false);
		checkLooks(true, true);
		qobject_cast<QAbstractButton*>(nppFindControl(preferences, IDC_RADIO_DARKMODE_LIGHTMODE))->click();
		QTest::qWait(50);
		checkLooks(false, false);
		changeDesktopTheme(true);
		checkLooks(false, false);
	}

	// Edit > Auto-Completion > Path, with Linux paths
	void pathCompletion()
	{
		QVERIFY(QDir(_files.path()).mkpath(QStringLiteral("completion/subfolder")));
		writeFile(_files.filePath(QStringLiteral("completion/needle.txt")), "x");
		writeFile(_home.filePath(QStringLiteral("inHome.txt")), "x");
		const QString folder = _files.filePath(QStringLiteral("completion"));

		command(IDM_FILE_NEW);
		HWND editor = currentEditor();
		const auto complete = [this, editor](const QString& typed)
		{
			::SendMessage(editor, SCI_CLEARALL, 0, 0);
			const QByteArray text = typed.toUtf8();
			::SendMessage(editor, SCI_ADDTEXT, text.size(), reinterpret_cast<LPARAM>(text.constData()));
			command(IDM_EDIT_AUTOCOMPLETE_PATH);
			const bool isActive = ::SendMessage(editor, SCI_AUTOCACTIVE, 0, 0) != 0;
			if (isActive)
				::SendMessage(editor, SCI_AUTOCCOMPLETE, 0, 0);
			return isActive ? QString::fromUtf8(editorText()) : QString();
		};

		// Part of a name, a whole folder, and a path after other text and in quotes
		QCOMPARE(complete(folder + QStringLiteral("/need")), folder + QStringLiteral("/needle.txt"));
		QCOMPARE(complete(folder + QStringLiteral("/sub")), folder + QStringLiteral("/subfolder/"));
		QVERIFY(complete(folder + u'/').startsWith(folder + u'/'));
		QCOMPARE(complete(QStringLiteral("see ") + folder + QStringLiteral("/need")), QStringLiteral("see ") + folder + QStringLiteral("/needle.txt"));
		QCOMPARE(complete(u'"' + folder + QStringLiteral("/need")), u'"' + folder + QStringLiteral("/needle.txt"));

		// The home folder and the root
		QCOMPARE(complete(QStringLiteral("~/inHo")), QStringLiteral("~/inHome.txt"));
		QVERIFY(complete(QStringLiteral("/us")).startsWith(QStringLiteral("/usr")));

		// Not a path, or a file that is complete already
		QVERIFY(complete(QStringLiteral("just/a/word")).isEmpty());
		QVERIFY(complete(QStringLiteral("nothing here")).isEmpty());
		QVERIFY(complete(folder + QStringLiteral("/needle.txt")).isEmpty());

		::SendMessage(editor, SCI_CLEARALL, 0, 0);
		::SendMessage(editor, SCI_SETSAVEPOINT, 0, 0);
	}

	// The last session comes back with its tabs, and recent files do not get in the way of starting
	void restoredSession()
	{
		if (!qEnvironmentVariableIsSet("NPP_TEST_RESTORED_SESSION"))
			QSKIP("this needs a session from before the start");

		QTest::qWait(50);
		QTabBar* bar = nullptr;
		const QList<QTabBar*> bars = mainWindow()->findChildren<QTabBar*>();
		for (QTabBar* candidate : bars)
		{
			if (candidate->count() >= 2)
				bar = candidate;
		}
		QVERIFY(bar);
		QVERIFY(bar->isVisible());
		QVERIFY(bar->height() > 5);
		QCOMPARE(bar->tabText(0), QStringLiteral("sessionOne.txt"));
		QCOMPARE(bar->tabText(1), QStringLiteral("sessionTwo.txt"));
		QCOMPARE(bar->currentIndex(), 1);
		QCOMPARE(editorText(), QByteArray("two\n"));
		screenshot(QStringLiteral("restored-session"));
	}

	// The tab of the current document as it is seen, with real clicks through the window
	QTabBar* currentTabBar() const
	{
		const QList<QTabBar*> bars = mainWindow()->findChildren<QTabBar*>();
		for (QTabBar* candidate : bars)
		{
			if (candidate->isVisible() && candidate->count() > 0 && candidate->parentWidget() && candidate->parentWidget()->isAncestorOf(currentEditor()))
				return candidate;
		}
		for (QTabBar* candidate : bars)
		{
			if (candidate->isVisible() && candidate->count() > 0)
				return candidate;
		}
		return nullptr;
	}

	// A new document's tab shows its close and pin buttons at once, as the active tab, without the mouse over it
	void newTabShowsItsButtons()
	{
		QTabBar* bar = currentTabBar();
		QVERIFY(bar);
		const int countBefore = bar->count();
		for (int i = 0; i < 2; ++i)
		{
			command(IDM_FILE_NEW);
			QCOMPARE(bar->count(), countBefore + i + 1);
			const int current = bar->currentIndex();
			QWidget* close = bar->tabButton(current, QTabBar::RightSide);
			QWidget* pin = bar->tabButton(current, QTabBar::LeftSide);
			QVERIFY(close);
			QVERIFY(pin);
			QVERIFY(close->isVisible());
			QVERIFY(pin->isVisible());

			// The tab that was active only shows them under the mouse
			if (i == 1)
			{
				const int previous = current - 1;
				QVERIFY(!bar->tabButton(previous, QTabBar::RightSide)->isVisible());
				QVERIFY(!bar->tabButton(previous, QTabBar::LeftSide)->isVisible());
			}
		}

		// Going to another tab moves them along
		const int last = bar->currentIndex();
		command(IDM_VIEW_TAB_PREV);
		QCOMPARE(bar->currentIndex(), last - 1);
		QVERIFY(bar->tabButton(last - 1, QTabBar::RightSide)->isVisible());
		QVERIFY(!bar->tabButton(last, QTabBar::RightSide)->isVisible());
		command(IDM_VIEW_TAB_NEXT);
		QVERIFY(bar->tabButton(last, QTabBar::RightSide)->isVisible());

		command(IDM_FILE_CLOSE);
		command(IDM_FILE_CLOSE);
	}

	// Right clicking a tab brings up its menu, and only that
	void tabRightClickOneMenu()
	{
		// A compositor refuses to show a popup that does not come from real input
		if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland")))
			QSKIP("Wayland shows no menu for a simulated click");

		openWithContent(QStringLiteral("rightTabA.txt"), "a\n");
		openWithContent(QStringLiteral("rightTabB.txt"), "b\n");
		QTabBar* bar = currentTabBar();
		QVERIFY(bar);

		QStringList firstItems;
		int menus = 0;
		QTimer watch;
		QObject::connect(&watch, &QTimer::timeout, &watch, [&]()
		{
			auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			if (!menu)
				return;
			++menus;
			if (!menu->actions().isEmpty())
				firstItems << menu->actions().first()->text().remove(u'&');
			menu->close();
		});
		watch.start(10);

		const QPoint inWindow = bar->mapTo(mainWindow(), bar->tabRect(bar->currentIndex()).center());
		QWindow* window = mainWindow()->windowHandle();
		QVERIFY(window);
		QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, inWindow);
		QTest::qWait(150);
		watch.stop();
		QVERIFY2(menus == 1, qPrintable(QStringLiteral("%1 menus: %2").arg(menus).arg(firstItems.join(u','))));
		QVERIFY2(!firstItems.first().startsWith(QStringLiteral("Cut")), qPrintable(firstItems.join(u',')));

		// And the same click in the editor brings up the editor's
		menus = 0;
		firstItems.clear();
		watch.start(10);
		QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, currentEditor()->mapTo(mainWindow(), QPoint(100, 40)));
		QTest::qWait(150);
		watch.stop();
		QCOMPARE(menus, 1);
		QVERIFY(firstItems.first().startsWith(QStringLiteral("Cut")));
	}

	// Files dragged from a file manager open wherever they are dropped on the window, the editor included
	void filesDroppedOnEditorOpen()
	{
		const QString path = _files.filePath(QStringLiteral("droppedFile.txt"));
		writeFile(path, "dropped\n");
		command(IDM_FILE_NEW);

		// Drops land on the viewport of the editor, as it is a scroll area. Sent to the editor itself they would go on
		// to the main window, past the case that matters.
		QWidget* viewport = qobject_cast<QAbstractScrollArea*>(currentEditor())->viewport();
		QVERIFY(viewport);

		QMimeData data;
		data.setUrls({ QUrl::fromLocalFile(path) });
		QDragEnterEvent enter(QPoint(50, 50), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
		QApplication::sendEvent(viewport, &enter);
		QVERIFY(enter.isAccepted());
		QDropEvent drop(QPointF(50, 50), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
		QApplication::sendEvent(viewport, &drop);
		QCoreApplication::processEvents();
		QCOMPARE(currentPath(), path);
		QCOMPARE(editorText(), QByteArray("dropped\n"));

		// Dragged text still goes in where it is dropped
		command(IDM_FILE_NEW);
		::SendMessage(currentEditor(), SCI_ADDTEXT, 5, reinterpret_cast<LPARAM>("hello"));
		QMimeData text;
		text.setText(QStringLiteral("XY"));
		const QPointF inText(::SendMessage(currentEditor(), SCI_POINTXFROMPOSITION, 0, 2), ::SendMessage(currentEditor(), SCI_POINTYFROMPOSITION, 0, 2) + 3);
		QDragEnterEvent textEnter(inText.toPoint(), Qt::CopyAction, &text, Qt::LeftButton, Qt::NoModifier);
		QApplication::sendEvent(viewport, &textEnter);
		QDropEvent textDrop(inText, Qt::CopyAction, &text, Qt::LeftButton, Qt::NoModifier);
		QApplication::sendEvent(viewport, &textDrop);
		QTRY_VERIFY2(editorText().contains("XY"), editorText().constData());
	}

	// While a tab is dragged, the bar of the current tab goes along with it
	void tabHighlightFollowsDrag()
	{
		openWithContent(QStringLiteral("dragA.txt"), "a\n");
		openWithContent(QStringLiteral("dragB.txt"), "b\n");
		openWithContent(QStringLiteral("dragC.txt"), "c\n");
		QTabBar* bar = currentTabBar();
		QVERIFY(bar);
		NppGUI& gui = NppParameters::getInstance().getNppGUI();
		const int status = gui._tabStatus;
		gui._tabStatus |= TAB_DRAGNDROP | TAB_DRAWTOPBAR;

		const QPoint start = bar->tabRect(bar->currentIndex()).center();
		QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, start);
		QPoint now = start;
		for (int i = 0; i < 6; ++i)
		{
			now.rx() -= 12;
			QTest::mouseMove(bar, now);
			QCoreApplication::processEvents();
		}

		QTest::qWait(50); // Qt places the picture of the tab when it paints
		if (const QString dir = qEnvironmentVariable("NPP_TEST_APP_DIR"); !dir.isEmpty())
			mainWindow()->grab().save(dir + QStringLiteral("/dragging-tab.png"));

		QWidget* moving = nullptr;
		QWidget* highlight = nullptr;
		const QList<QWidget*> children = bar->findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
		for (QWidget* child : children)
		{
			if (child->objectName() == QStringLiteral("movingTabHighlight"))
				highlight = child;
			else if (child->metaObject() == &QWidget::staticMetaObject && child->isVisible())
				moving = child;
		}
		QVERIFY(moving && moving->isVisible());
		QVERIFY(highlight && highlight->isVisible());
		QCOMPARE(highlight->x(), moving->x());
		QCOMPARE(highlight->width(), moving->width());
		QVERIFY(highlight->geometry().bottom() <= moving->geometry().top() + 8);

		QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, now);
		QTest::qWait(300);
		QVERIFY(!highlight->isVisible());

		// A tab with a gap to the one on its left: only the bar's background shows in its empty place, and not the edge
		// that the style gives the neighbour, which the tab normally covers
		bar->setCurrentIndex(1);
		QTest::qWait(50);
		const QPoint middle = bar->tabRect(1).center();
		QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, middle);
		QPoint right = middle;
		for (int i = 0; i < 4; ++i)
		{
			right.rx() += 8;
			QTest::mouseMove(bar, right);
			QCoreApplication::processEvents();
		}
		QTest::qWait(60);
		const QImage withGap = bar->grab().toImage();
		const qreal scale = withGap.devicePixelRatio();
		const int slotLeft = qCeil(bar->tabRect(bar->currentIndex()).left() * scale);
		const int middleY = qRound(bar->height() / 2 * scale);
		const QColor background = bar->palette().color(QPalette::Window);
		QStringList wrongPixels;
		for (int x = slotLeft; x < slotLeft + qFloor(8 * scale); ++x)
		{
			if (x >= 0 && x < withGap.width() && withGap.pixelColor(x, middleY) != background)
				wrongPixels << withGap.pixelColor(x, middleY).name();
		}
		QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, right);
		QTest::qWait(300);
		gui._tabStatus = status;
		QVERIFY2(wrongPixels.isEmpty(), qPrintable(wrongPixels.join(u' ') + QStringLiteral(" instead of ") + background.name()));
		gui._tabStatus = status;
	}

	void tabBarWheel()
	{
		// Three documents, the bar with them being the visible one
		for (const char* name : { "wheelA.txt", "wheelB.txt", "wheelC.txt" })
			openWithContent(QString::fromLatin1(name), "x\n");
		QTabBar* bar = nullptr;
		const QList<QTabBar*> bars = mainWindow()->findChildren<QTabBar*>();
		for (QTabBar* candidate : bars)
		{
			if (candidate->isVisible() && candidate->count() >= 3 && candidate->tabText(candidate->currentIndex()) == QStringLiteral("wheelC.txt"))
				bar = candidate;
		}
		QVERIFY(bar);
		const auto wheel = [bar](int delta, Qt::KeyboardModifiers modifiers)
		{
			QWheelEvent event(QPointF(5, 5), bar->mapToGlobal(QPointF(5, 5)), QPoint(), QPoint(0, delta), Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
			QApplication::sendEvent(bar, &event);
			QCoreApplication::processEvents();
		};
		const int last = bar->count() - 1;
		constexpr int towardsUser = -120;
		constexpr int awayFromUser = 120;

		// Without a key it only scrolls, and leaves the current document alone
		bar->setCurrentIndex(1);
		wheel(towardsUser, Qt::NoModifier);
		wheel(awayFromUser, Qt::NoModifier);
		QCOMPARE(bar->currentIndex(), 1);

		// Ctrl goes through them endlessly
		bar->setCurrentIndex(last);
		wheel(towardsUser, Qt::ControlModifier);
		QCOMPARE(bar->currentIndex(), 0);
		wheel(awayFromUser, Qt::ControlModifier);
		QCOMPARE(bar->currentIndex(), last);

		// Both go to the first and the last one
		bar->setCurrentIndex(1);
		wheel(awayFromUser, Qt::ControlModifier | Qt::ShiftModifier);
		QCOMPARE(bar->currentIndex(), 0);
		wheel(towardsUser, Qt::ControlModifier | Qt::ShiftModifier);
		QCOMPARE(bar->currentIndex(), last);

		// Shift moves the tab, and the document stays with it
		NppGUI& gui = NppParameters::getInstance().getNppGUI();
		const int status = gui._tabStatus;
		gui._tabStatus |= TAB_DRAGNDROP;
		bar->setCurrentIndex(last);
		const QString name = bar->tabText(last);
		const QString path = currentPath();
		wheel(awayFromUser, Qt::ShiftModifier);
		QCOMPARE(bar->tabText(last - 1), name);
		QCOMPARE(bar->currentIndex(), last - 1);
		QCOMPARE(currentPath(), path);
		wheel(towardsUser, Qt::ShiftModifier);
		QCOMPARE(bar->tabText(last), name);

		// Without dragging it goes through them, and stops at the ends
		gui._tabStatus &= ~TAB_DRAGNDROP;
		bar->setCurrentIndex(last);
		wheel(towardsUser, Qt::ShiftModifier);
		QCOMPARE(bar->currentIndex(), last);
		wheel(awayFromUser, Qt::ShiftModifier);
		QCOMPARE(bar->currentIndex(), last - 1);
		gui._tabStatus = status;
	}

	void splitterBetweenViews()
	{
		openWithContent(QStringLiteral("splitA.txt"), "a\n");
		openWithContent(QStringLiteral("splitB.txt"), "b\n");
		command(IDM_VIEW_GOTO_ANOTHER_VIEW);
		QVERIFY(_sub->isVisible());

		QSplitter* splitter = nullptr;
		const QList<QSplitter*> splitters = mainWindow()->findChildren<QSplitter*>();
		for (QSplitter* candidate : splitters)
		{
			if (candidate->count() == 2 && candidate->widget(0)->isAncestorOf(_main) && candidate->widget(1)->isAncestorOf(_sub))
				splitter = candidate;
		}
		QVERIFY(splitter);
		QWidget* first = splitter->widget(0);
		QWidget* second = splitter->widget(1);
		QSplitterHandle* handle = splitter->handle(1);
		QVERIFY(handle);

		const auto chooseOnDivider = [handle](const QString& text)
		{
			QStringList items;
			QTimer::singleShot(0, handle, [&items, &text]()
			{
				auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
				if (!menu)
					return;
				QAction* chosen = nullptr;
				const QList<QAction*> actions = menu->actions();
				for (QAction* action : actions)
				{
					items << action->text();
					if (action->text() == text)
						chosen = action;
				}
				if (chosen)
					menu->setActiveAction(chosen);
				QTest::keyClick(menu, Qt::Key_Return);
			});
			QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(1, 1), handle->mapToGlobal(QPoint(1, 1)));
			QApplication::sendEvent(handle, &event);
			return items;
		};

		// Rotating turns the divider, and "left" from side by side has the panes change places
		const Qt::Orientation sideBySide = splitter->orientation();
		QCOMPARE(chooseOnDivider(QStringLiteral("Rotate to left")), QStringList({ QStringLiteral("Rotate to right"), QStringLiteral("Rotate to left") }));
		QVERIFY(splitter->orientation() != sideBySide);
		QCOMPARE(splitter->widget(0), second);
		QCOMPARE(splitter->widget(1), first);

		// And "right" back from above and below too, which brings them back
		chooseOnDivider(QStringLiteral("Rotate to right"));
		QCOMPARE(splitter->orientation(), sideBySide);
		QCOMPARE(splitter->widget(0), first);
		QCOMPARE(splitter->widget(1), second);

		// A double click gives both the same size. Through the window, so that Qt decides what is a double click, and
		// twice: Breeze's helper widget over the divider only comes after the first, and swallows double clicks.
		QWindow* window = mainWindow()->windowHandle();
		QVERIFY(window);
		const auto handleCentre = [handle, this]() { return handle->mapTo(mainWindow(), handle->rect().center()); };
		const auto areEqual = [splitter]() { return qAbs(splitter->sizes().at(0) - splitter->sizes().at(1)) <= 2; };
		const bool isSideBySide = splitter->orientation() == Qt::Horizontal;
		for (int attempt = 1; attempt <= 3; ++attempt)
		{
			// As a user does: drag it away, and then go back onto it with the mouse, which brings the helper
			splitter->setSizes({ 400, 400 });
			QCoreApplication::processEvents();
			const QPoint before = handleCentre();
			const QPoint away = before + (isSideBySide ? QPoint(-180, 0) : QPoint(0, -100));
			QTest::mouseMove(window, before);
			QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, before);
			QTest::mouseMove(window, away);
			QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, away);
			QVERIFY(!areEqual());
			QTest::qWait(500); // not a double click with the clicks of the attempt before
			const QPoint onHandle = handleCentre();
			QTest::mouseMove(window, onHandle + (isSideBySide ? QPoint(30, 0) : QPoint(0, 30)));
			QTest::qWait(30);
			QTest::mouseMove(window, onHandle);
			QTest::qWait(30);
			QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, onHandle, 20);
			QVERIFY(!areEqual()); // one click alone leaves it where it is
			QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, onHandle, 20);
			QVERIFY2(areEqual(), qPrintable(QStringLiteral("attempt %1: %2 and %3").arg(attempt).arg(splitter->sizes().at(0)).arg(splitter->sizes().at(1))));
		}

		// The same for what the helper leaves of it: a release without a press
		splitter->setSizes({ 200, 600 });
		QCoreApplication::processEvents();
		const auto mouse = [handle](QEvent::Type type, const QPointF& onScreen, Qt::MouseButtons buttons)
		{
			const Qt::MouseButton button = (type == QEvent::MouseMove) ? Qt::NoButton : Qt::LeftButton;
			QMouseEvent event(type, handle->mapFromGlobal(onScreen), onScreen, button, buttons, Qt::NoModifier);
			QApplication::sendEvent(handle, &event);
		};
		const QPointF cursor = handle->mapToGlobal(QPointF(1, 1));
		mouse(QEvent::MouseButtonPress, cursor, Qt::LeftButton);
		mouse(QEvent::MouseButtonRelease, cursor, Qt::NoButton);
		QVERIFY(!areEqual());
		mouse(QEvent::MouseButtonRelease, cursor, Qt::NoButton);
		QVERIFY(areEqual());

		// And it can still be dragged after that
		const int centred = splitter->sizes().at(0);
		const QPointF start = handle->mapToGlobal(QPointF(handle->rect().center()));
		const QPointF moved = start + (splitter->orientation() == Qt::Horizontal ? QPointF(-60, 0) : QPointF(0, -60));
		splitter->setOpaqueResize(true);
		mouse(QEvent::MouseButtonPress, start, Qt::LeftButton);
		mouse(QEvent::MouseMove, moved, Qt::LeftButton);
		mouse(QEvent::MouseButtonRelease, moved, Qt::NoButton);
		QVERIFY(splitter->sizes().at(0) < centred);

		command(IDM_VIEW_GOTO_ANOTHER_VIEW);
	}

	// A panel that is torn off goes on working, as it is told about that
	void panelFloats()
	{
		openWithContent(QStringLiteral("floats.txt"), QByteArray(5000, 'x'));
		const bool wasOpen = mainWindow()->findChild<QDockWidget*>() != nullptr;
		Q_UNUSED(wasOpen);
		command(IDM_VIEW_DOC_MAP);
		QDockWidget* map = nullptr;
		const QList<QDockWidget*> docks = mainWindow()->findChildren<QDockWidget*>();
		for (QDockWidget* dock : docks)
		{
			if (dock->isVisible() && dock->findChild<ScintillaEditBase*>() && !textOf(dock->findChild<ScintillaEditBase*>()).startsWith("Search "))
				map = dock;
		}
		QVERIFY(map);
		map->setFloating(true);
		QTRY_VERIFY(map->isFloating());
		QVERIFY(map->isVisible());
		map->setFloating(false);
		QTRY_VERIFY(!map->isFloating());
		QVERIFY(map->isVisible());
		command(IDM_VIEW_DOC_MAP);
	}

	// One message of Breeze and Wayland stays out of the console, and all the others get through
	void messageFilter()
	{
		QTest::failOnWarning(QRegularExpression(QStringLiteral("This plugin supports grabbing")));
		qWarning("This plugin supports grabbing the mouse only for popup windows");

		QTest::ignoreMessage(QtWarningMsg, "This plugin supports grabbing the mouse, and something else");
		qWarning("This plugin supports grabbing the mouse, and something else");
		QTest::ignoreMessage(QtInfoMsg, "This plugin supports grabbing the mouse only for popup windows");
		qInfo("This plugin supports grabbing the mouse only for popup windows");
	}

	// MIME Tools, a real plugin from Windows, with all of its commands. The expected results come from Python's base64,
	// quopri, urllib and zlib, and not from running the plugin.
	void realPluginFromWindows()
	{
		QMenu* mimeTools = nullptr;
		const QList<QAction*> menus = mainWindow()->menuBar()->actions();
		for (QAction* menu : menus)
		{
			if (menu->menu() && menu->text().remove(u'&') == QStringLiteral("Plugins"))
			{
				const QList<QAction*> plugins = menu->menu()->actions();
				for (QAction* plugin : plugins)
				{
					if (plugin->menu() && plugin->text() == QStringLiteral("MIME Tools"))
						mimeTools = plugin->menu();
				}
			}
		}
		QVERIFY(mimeTools);

		QStringList names;
		const QList<QAction*> items = mimeTools->actions();
		for (const QAction* item : items)
		{
			if (!item->isSeparator())
				names << item->text();
		}
		QCOMPARE(names, QStringList({
			QStringLiteral("Base64 Encode"), QStringLiteral("Base64 Encode with padding"), QStringLiteral("Base64 Encode with Unix EOL"),
			QStringLiteral("Base64 Encode by line"), QStringLiteral("Base64 Decode"), QStringLiteral("Base64 Decode strict"),
			QStringLiteral("Base64 Decode by line"), QStringLiteral("Quoted-printable Encode"), QStringLiteral("Quoted-printable Decode"),
			QStringLiteral("URL Encode (RFC1738)"), QStringLiteral("URL Encode (RFC1738) by line"), QStringLiteral("URL Encode (Extended)"),
			QStringLiteral("URL Encode (Extended) by line"), QStringLiteral("URL Encode (Full)"), QStringLiteral("URL Encode (Full) by line"),
			QStringLiteral("URL Decode"), QStringLiteral("SAML Decode"), QStringLiteral("About") }));

		const auto trigger = [&items](const QString& name)
		{
			for (QAction* item : items)
			{
				if (item->text() == name)
					item->trigger();
			}
		};
		command(IDM_FILE_NEW);
		const auto run = [&](const QString& name, const QByteArray& input)
		{
			HWND editor = currentEditor();
			::SendMessage(editor, SCI_CLEARALL, 0, 0);
			::SendMessage(editor, SCI_ADDTEXT, input.size(), reinterpret_cast<LPARAM>(input.constData()));
			::SendMessage(editor, SCI_SELECTALL, 0, 0);
			trigger(name);
			QCoreApplication::processEvents();
			return editorText();
		};

		const QByteArray hello = "Hello, world!";
		const QByteArray cafe = "caf\xC3\xA9 = ok";
		QCOMPARE(run(QStringLiteral("Base64 Encode"), hello), QByteArray("SGVsbG8sIHdvcmxkIQ"));
		QCOMPARE(run(QStringLiteral("Base64 Encode with padding"), hello), QByteArray("SGVsbG8sIHdvcmxkIQ=="));
		QCOMPARE(run(QStringLiteral("Base64 Decode"), "SGVsbG8sIHdvcmxkIQ=="), hello);
		QCOMPARE(run(QStringLiteral("Base64 Decode strict"), "SGVsbG8sIHdvcmxkIQ=="), hello);
		QCOMPARE(run(QStringLiteral("Quoted-printable Encode"), cafe), QByteArray("caf=C3=A9 =3D ok"));
		QCOMPARE(run(QStringLiteral("Quoted-printable Decode"), "caf=C3=A9 =3D ok"), cafe);
		QCOMPARE(run(QStringLiteral("URL Decode"), "a%20b%26c"), QByteArray("a b&c"));
		QCOMPARE(run(QStringLiteral("SAML Decode"), "sylOzM0psApKLS7IzytOVfB0sVVKTEpW0rcDAA%3D%3D"), QByteArray("<samlp:Response ID=\"abc\"/>"));

		// Long text is broken into lines and comes back whole
		const QByteArray longText(60, 'x');
		const QByteArray wrapped = run(QStringLiteral("Base64 Encode with Unix EOL"), longText);
		QVERIFY(wrapped.contains('\n'));
		QVERIFY(!wrapped.contains('\r'));
		QCOMPARE(QByteArray::fromBase64(QByteArray(wrapped).replace("\n", "")), longText);
		QCOMPARE(run(QStringLiteral("Base64 Decode"), wrapped), longText);

		// By line, each line on its own
		const QByteArray byLine = run(QStringLiteral("Base64 Encode by line"), "Hello\nworld");
		const QList<QByteArray> encodedLines = byLine.split('\n');
		QCOMPARE(encodedLines.size(), qsizetype(2));
		QCOMPARE(QByteArray::fromBase64(encodedLines[0]), QByteArray("Hello"));
		QCOMPARE(QByteArray::fromBase64(encodedLines[1]), QByteArray("world"));
		QCOMPARE(run(QStringLiteral("Base64 Decode by line"), "SGVsbG8=\nd29ybGQ="), QByteArray("Hello\nworld"));

		// The three kinds of URL encoding take in more and more characters
		const QByteArray url = "a b&c";
		const QByteArray rfc = run(QStringLiteral("URL Encode (RFC1738)"), url);
		QCOMPARE(rfc, QByteArray("a%20b%26c"));
		QCOMPARE(run(QStringLiteral("URL Encode (RFC1738)"), "a!b(c)"), QByteArray("a!b(c)"));
		QCOMPARE(run(QStringLiteral("URL Encode (Extended)"), "a!b(c)"), QByteArray("a%21b%28c%29"));
		const QByteArray full = run(QStringLiteral("URL Encode (Full)"), url);
		QCOMPARE(full, QByteArray("%61%20%62%26%63"));
		QCOMPARE(run(QStringLiteral("URL Decode"), full), url);
		QCOMPARE(run(QStringLiteral("URL Encode (RFC1738) by line"), "a b\nc d"), QByteArray("a%20b\nc%20d"));
		QCOMPARE(run(QStringLiteral("URL Encode (Extended) by line"), "a!b\nc d"), QByteArray("a%21b\nc%20d"));
		QCOMPARE(run(QStringLiteral("URL Encode (Full) by line"), "ab\nc"), QByteArray("%61%62\n%63"));

		// The plugin's own message for wrong input, which leaves the text as it is
		QString message;
		whileModal([&message](QWidget* box)
		{
			message = qobject_cast<QMessageBox*>(box)->text();
		}, [&]() { run(QStringLiteral("Base64 Decode strict"), "%%%"); });
		QCOMPARE(message, QStringLiteral("Problem!"));
		QCOMPARE(editorText(), QByteArray("%%%"));

		// Its one dialog, which had to be written anew for Qt
		whileModal([&message](QWidget* box)
		{
			message = qobject_cast<QMessageBox*>(box)->text();
		}, [&]() { trigger(QStringLiteral("About")); });
		QVERIFY2(message.contains(QStringLiteral("Don HO")), qPrintable(message));

		::SendMessage(currentEditor(), SCI_CLEARALL, 0, 0);
		::SendMessage(currentEditor(), SCI_SETSAVEPOINT, 0, 0);
	}

	// The keys that Scintilla handles or that are in the Edit menu, as the desktop sends them
	void editingKeys()
	{
		if (!mainWindowGetsFocus())
			QSKIP("the window manager did not give the window focus");
		command(IDM_FILE_NEW);
		HWND editor = currentEditor();
		editor->setFocus();
		const auto set = [editor](const QByteArray& text, int caret)
		{
			::SendMessage(editor, SCI_CLEARALL, 0, 0);
			::SendMessage(editor, SCI_ADDTEXT, text.size(), reinterpret_cast<LPARAM>(text.constData()));
			::SendMessage(editor, SCI_GOTOPOS, caret, 0);
			::SendMessage(editor, SCI_EMPTYUNDOBUFFER, 0, 0);
		};
		const auto selected = [editor]()
		{
			return ::SendMessage(editor, SCI_GETSELECTIONEND, 0, 0) - ::SendMessage(editor, SCI_GETSELECTIONSTART, 0, 0);
		};
		const auto caret = [editor]() { return ::SendMessage(editor, SCI_GETCURRENTPOS, 0, 0); };
		const Qt::KeyboardModifiers ctrl = Qt::ControlModifier;
		const Qt::KeyboardModifiers ctrlShift = Qt::ControlModifier | Qt::ShiftModifier;

		set("one two", 0);
		pressKey(Qt::Key_A, ctrl, controlText('A'));
		QCOMPARE(selected(), LRESULT(7));

		// Undo and redo, with both of their keys
		set("one", 3);
		pressKey(Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
		QCOMPARE(editorText(), QByteArray("onex"));
		pressKey(Qt::Key_Z, ctrl, controlText('Z'));
		QCOMPARE(editorText(), QByteArray("one"));
		pressKey(Qt::Key_Y, ctrl, controlText('Y'));
		QCOMPARE(editorText(), QByteArray("onex"));
		pressKey(Qt::Key_Backspace, Qt::AltModifier, QStringLiteral("\b"));
		QCOMPARE(editorText(), QByteArray("one"));
		pressKey(Qt::Key_Z, ctrlShift, controlText('Z'));
		QCOMPARE(editorText(), QByteArray("onex"));

		// Deleting
		set("one two", 0);
		pressKey(Qt::Key_Delete, Qt::NoModifier, QStringLiteral("\x7f"));
		QCOMPARE(editorText(), QByteArray("ne two"));
		set("one two", 7);
		pressKey(Qt::Key_Backspace, Qt::NoModifier, QStringLiteral("\b"));
		QCOMPARE(editorText(), QByteArray("one tw"));
		set("one two", 0);
		pressKey(Qt::Key_Delete, ctrl, QStringLiteral("\x7f"));
		QCOMPARE(editorText(), QByteArray("two"));
		set("one two", 7);
		pressKey(Qt::Key_Backspace, ctrl, QStringLiteral("\b"));
		QCOMPARE(editorText(), QByteArray("one "));
		set("one two", 4);
		pressKey(Qt::Key_Backspace, ctrlShift, QStringLiteral("\b"));
		QCOMPARE(editorText(), QByteArray("two"));
		set("one two", 3);
		pressKey(Qt::Key_Delete, ctrlShift, QStringLiteral("\x7f"));
		QCOMPARE(editorText(), QByteArray("one"));

		// Lines
		set("one\ntwo\n", 0);
		pressKey(Qt::Key_L, ctrlShift, controlText('L'));
		QCOMPARE(editorText(), QByteArray("two\n"));
		set("one\ntwo", 4);
		pressKey(Qt::Key_T, ctrl, controlText('T'));
		QVERIFY2(editorText().startsWith("two"), editorText().constData());
		set("one", 0);
		pressKey(Qt::Key_D, ctrl, controlText('D'));
		QCOMPARE(editorText().count("one"), qsizetype(2));

		// The clipboard
		set("one two", 0);
		::SendMessage(editor, SCI_SETSEL, 0, 4);
		pressKey(Qt::Key_X, ctrl, controlText('X'));
		QCOMPARE(editorText(), QByteArray("two"));
		QTRY_COMPARE(QApplication::clipboard()->text(), QStringLiteral("one ")); // the desktop's answer comes later on Wayland
		pressKey(Qt::Key_V, ctrl, controlText('V'));
		QCOMPARE(editorText(), QByteArray("one two"));
		::SendMessage(editor, SCI_SETSEL, 0, 3);
		pressKey(Qt::Key_C, ctrl, controlText('C'));
		QTRY_COMPARE(QApplication::clipboard()->text(), QStringLiteral("one"));
		::SendMessage(editor, SCI_GOTOPOS, 7, 0);
		pressKey(Qt::Key_V, ctrl, controlText('V'));
		QCOMPARE(editorText(), QByteArray("one twoone"));

		// Moving with the punctuation keys, which have other codes on Windows
		set("one\n\ntwo", 0);
		pressKey(Qt::Key_BracketRight, ctrl, controlText(']'));
		QCOMPARE(caret(), LRESULT(5));
		pressKey(Qt::Key_BracketLeft, ctrl, controlText('['));
		QCOMPARE(caret(), LRESULT(0));
		pressKey(Qt::Key_BraceRight, ctrlShift, controlText(']'));
		QCOMPARE(selected(), LRESULT(5));
		set("oneTwo", 0);
		pressKey(Qt::Key_Backslash, ctrl, controlText('\\'));
		QCOMPARE(caret(), LRESULT(3));
		pressKey(Qt::Key_Slash, ctrl, controlText('/'));
		QCOMPARE(caret(), LRESULT(0));

		// Moving by words and in the document
		set("one two", 0);
		pressKey(Qt::Key_Right, ctrl);
		QCOMPARE(caret(), LRESULT(4));
		pressKey(Qt::Key_End, ctrl);
		QCOMPARE(caret(), LRESULT(7));
		pressKey(Qt::Key_Home, ctrl);
		QCOMPARE(caret(), LRESULT(0));

		// Zooming
		::SendMessage(editor, SCI_SETZOOM, 0, 0);
		pressKey(Qt::Key_Plus, ctrl | Qt::KeypadModifier, QStringLiteral("+"));
		QCOMPARE(::SendMessage(editor, SCI_GETZOOM, 0, 0), LRESULT(1));
		pressKey(Qt::Key_Minus, ctrl | Qt::KeypadModifier, QStringLiteral("-"));
		QCOMPARE(::SendMessage(editor, SCI_GETZOOM, 0, 0), LRESULT(0));
		pressKey(Qt::Key_Plus, ctrl | Qt::KeypadModifier, QStringLiteral("+"));
		pressKey(Qt::Key_Slash, ctrl | Qt::KeypadModifier, QStringLiteral("/"));
		QCOMPARE(::SendMessage(editor, SCI_GETZOOM, 0, 0), LRESULT(0));

		// Keys that the Shortcut Mapper can give to Scintilla's commands, by their codes on Windows
		const Qt::KeyboardModifiers ctrlAlt = Qt::ControlModifier | Qt::AltModifier;
		const auto mapped = [&](int code, int key, Qt::KeyboardModifiers modifiers, const QString& text)
		{
			const WPARAM keyDefinition = code + ((SCMOD_CTRL | SCMOD_ALT | ((modifiers & Qt::ShiftModifier) ? SCMOD_SHIFT : 0)) << 16);
			::SendMessage(editor, SCI_ASSIGNCMDKEY, keyDefinition, SCI_DOCUMENTEND);
			set("one", 0);
			pressKey(key, modifiers, text);
			const bool isDone = caret() == 3;
			::SendMessage(editor, SCI_CLEARCMDKEY, keyDefinition, 0);
			return isDone;
		};
		QVERIFY(mapped(VK_F7, Qt::Key_F7, ctrlAlt, {}));
		QVERIFY(mapped(VK_OEM_1, Qt::Key_Semicolon, ctrlAlt, QStringLiteral(";")));
		QVERIFY(mapped(VK_OEM_1, Qt::Key_Colon, ctrlAlt | Qt::ShiftModifier, QStringLiteral(":")));
		QVERIFY(mapped(VK_NUMPAD5, Qt::Key_5, ctrlAlt | Qt::KeypadModifier, QStringLiteral("5")));

		// A control character is not typed by a key that has no other use, unless the option for that is off
		set("one", 3);
		pressKey(Qt::Key_E, ctrl, controlText('E'));
		QCOMPARE(editorText(), QByteArray("one"));
		bool& noControlCharacters = const_cast<ScintillaViewParams&>(NppParameters::getInstance().getSVP())._npcNoInputC0;
		noControlCharacters = false;
		pressKey(Qt::Key_E, ctrl, controlText('E'));
		QCOMPARE(editorText(), QByteArray("one\x05"));
		pressKey(Qt::Key_A, ctrl, controlText('A'));
		QCOMPARE(selected(), LRESULT(4));
		QCOMPARE(editorText(), QByteArray("one\x05"));
		noControlCharacters = true;

		// The Edit menu shows the keys that Scintilla handles, as those are not its own shortcuts
		const HMENU mainMenu = reinterpret_cast<HMENU>(send(NPPM_INTERNAL_GETMENU));
		const auto menuKey = [mainMenu](int id)
		{
			QAction* action = nppFindMenuAction(mainMenu, id);
			return action ? action->shortcut().toString() : QStringLiteral("no item");
		};
		QCOMPARE(menuKey(IDM_EDIT_SELECTALL), QStringLiteral("Ctrl+A"));
		QCOMPARE(menuKey(IDM_EDIT_UNDO), QStringLiteral("Ctrl+Z"));
		QCOMPARE(menuKey(IDM_EDIT_REDO), QStringLiteral("Ctrl+Y"));
		QCOMPARE(menuKey(IDM_EDIT_DELETE), QStringLiteral("Del"));

		set("", 0);
		::SendMessage(editor, SCI_SETSAVEPOINT, 0, 0);
		command(IDM_FILE_CLOSE);
	}

	// Those keys belong to the editor that has the focus, and the results of a search are in one too
	void editingKeysInResultsPanel()
	{
		openWithContent(QStringLiteral("keysInPanel.txt"), "needle\nother\n");
		HWND editor = currentEditor();
		command(IDM_SEARCH_FIND);
		QVERIFY(findDialog());
		setComboText(IDFINDWHAT, QStringLiteral("needle"));
		click(IDC_FINDALL_CURRENTFILE);
		findDialog()->hide();
		QCOMPARE(finderText().contains("1 hit"), true);

		ScintillaEditBase* panel = nullptr;
		const QList<QDockWidget*> docks = mainWindow()->findChildren<QDockWidget*>();
		for (QDockWidget* dock : docks)
		{
			auto* candidate = dock->findChild<ScintillaEditBase*>();
			if (dock->isVisible() && candidate && textOf(candidate).startsWith("Search "))
				panel = candidate;
		}
		QVERIFY(panel);
		if (!mainWindowGetsFocus())
			QSKIP("the window manager did not give the window focus");
		panel->setFocus();
		QTRY_COMPARE(QApplication::focusWidget(), static_cast<QWidget*>(panel));
		::SendMessage(editor, SCI_GOTOPOS, 0, 0);

		pressKey(Qt::Key_A, Qt::ControlModifier, controlText('A'));
		QVERIFY(panel->send(SCI_GETSELECTIONEND) > panel->send(SCI_GETSELECTIONSTART));
		QCOMPARE(::SendMessage(editor, SCI_GETSELECTIONEND, 0, 0), ::SendMessage(editor, SCI_GETSELECTIONSTART, 0, 0));

		editor->setFocus();
		::SendMessage(editor, SCI_SETSAVEPOINT, 0, 0);
		command(IDM_FILE_CLOSE);
	}

	// The button that leaves full screen and Post-It is part of the main window, in its top right corner
	void restoreButton()
	{
		openWithContent(QStringLiteral("restoreButton.txt"), "text\n");
		QMainWindow* win = mainWindow();
		if (!mainWindowGetsFocus())
			QSKIP("the window manager did not give the window focus");
		const QRect normal = win->geometry();

		const auto findButton = [win]() { return win->findChild<QPushButton*>(QStringLiteral("restoreButton")); };
		const auto inCorner = [win](const QWidget* button, int widths)
		{
			const QPoint topRight = button->mapTo(win, QPoint(button->width() * widths, widths > 1 ? -1 : 0));
			return topRight == QPoint(win->width(), 0);
		};

		// F11, as the key really comes
		pressKey(Qt::Key_F11);
		QTRY_VERIFY(win->isFullScreen());
		QPushButton* button = findButton();
		QVERIFY(button);
		QVERIFY(button->isVisible());
		QCOMPARE(button->window(), static_cast<QWidget*>(win));
		QVERIFY(!button->parentWidget()->isWindow());
		QVERIFY(button->width() < 60 && button->height() < 60);
		const QStringList problems = layoutProblems(button->parentWidget(), QStringLiteral("restoreButton"));
		QVERIFY2(problems.isEmpty(), qPrintable(problems.join(u'\n')));
		QTRY_VERIFY(inCorner(button, 1));
		QVERIFY(win->childAt(win->width() - 1, 0) == button);
		QVERIFY(currentEditor()->hasFocus() || !win->isActiveWindow());
		screenshot(QStringLiteral("restoreButton"));

		// A click leaves it, and gives the editor back its keys
		QTest::mouseClick(button, Qt::LeftButton);
		QTRY_VERIFY(!win->isFullScreen());
		QVERIFY(!button->isVisible());
		QTRY_VERIFY(win->menuBar()->isVisible());
		QVERIFY(currentEditor()->hasFocus() || !win->isActiveWindow());

		// So does the key. The desktop takes its time to answer a change of state, which no one could beat with a key.
		QTest::qWait(300);
		QTRY_VERIFY(win->isActiveWindow());
		pressKey(Qt::Key_F11);
		QTRY_VERIFY(win->isFullScreen());
		QVERIFY(button->isVisible());
		QTest::qWait(300);
		pressKey(Qt::Key_F11);
		QTRY_VERIFY(!win->isFullScreen());
		QVERIFY(!button->isVisible());

		// Post-It has it one width further in, and it follows a change of the window's size
		command(IDM_VIEW_POSTIT);
		QTRY_VERIFY(win->windowFlags() & Qt::FramelessWindowHint);
		QVERIFY(button->isVisible());
		QTRY_VERIFY(inCorner(button, 2));
		win->resize(win->width() - 40, win->height());
		QTRY_VERIFY(inCorner(button, 2));
		QTest::mouseClick(button, Qt::LeftButton);
		QTRY_VERIFY(!(win->windowFlags() & Qt::FramelessWindowHint));
		QVERIFY(!button->isVisible());
		win->resize(normal.size());

		// With both, the first click leaves Post-It and the second full screen
		command(IDM_VIEW_FULLSCREENTOGGLE);
		QTRY_VERIFY(win->isFullScreen());
		command(IDM_VIEW_POSTIT);
		QVERIFY(button->isVisible());
		QTest::mouseClick(button, Qt::LeftButton);
		QTRY_VERIFY(!(win->windowFlags() & Qt::FramelessWindowHint));
		const bool tracksStateOverFlags = QGuiApplication::platformName() != QStringLiteral("offscreen"); // only a real desktop reports it
		if (tracksStateOverFlags)
			QTRY_VERIFY(win->isFullScreen());
		QVERIFY(button->isVisible());
		QTRY_VERIFY(inCorner(button, 2));
		QTest::mouseClick(button, Qt::LeftButton);
		QTRY_VERIFY(!win->isFullScreen());
		QVERIFY(!button->isVisible());

		// Always on Top keeps a window maximized
		win->showMaximized();
		QTRY_VERIFY(win->isMaximized());
		command(IDM_VIEW_ALWAYSONTOP);
		QVERIFY(win->windowFlags() & Qt::WindowStaysOnTopHint);
		if (tracksStateOverFlags)
			QTRY_VERIFY(win->isMaximized());
		command(IDM_VIEW_ALWAYSONTOP);
		QVERIFY(!(win->windowFlags() & Qt::WindowStaysOnTopHint));
		if (tracksStateOverFlags)
			QTRY_VERIFY(win->isMaximized());
		win->showNormal();
		QTRY_VERIFY(!win->isMaximized());

		// Distraction Free is left as a whole
		command(IDM_VIEW_DISTRACTIONFREE);
		QTRY_VERIFY(win->isFullScreen());
		QVERIFY(button->isVisible());
		QTest::mouseClick(button, Qt::LeftButton);
		QTRY_VERIFY(!win->isFullScreen());
		QVERIFY(!button->isVisible());
		QVERIFY(!(win->windowFlags() & Qt::FramelessWindowHint));

		// After all of that the keys still go to the editor
		QTRY_VERIFY(win->isActiveWindow());
		QTRY_VERIFY(currentEditor()->hasFocus());
		QTest::qWait(300);
		QVERIFY(win->isActiveWindow());
		pressKey(Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
		QVERIFY2(editorText().contains('x'), editorText().constData());

		win->resize(normal.size());
		::SendMessage(currentEditor(), SCI_SETSAVEPOINT, 0, 0);
		command(IDM_FILE_CLOSE);
	}

	void shortcutsReachCommands()
	{
		const QList<QAction*> actions = mainWindow()->findChildren<QAction*>();
		bool hasSave = false;
		for (const QAction* action : actions)
		{
			if (action->data().toInt() == IDM_FILE_SAVE)
				hasSave = hasSave || action->shortcut() == QKeySequence(QStringLiteral("Ctrl+S"));
		}
		QVERIFY(hasSave);
	}

	// Last, as it ends the application
	void closeSavesSettings()
	{
		// Answer "no" for every document these tests left modified
		QTimer answers;
		QObject::connect(&answers, &QTimer::timeout, &answers, []()
		{
			QWidget* modal = QApplication::activeModalWidget();
			if (auto* messageBox = qobject_cast<QMessageBox*>(modal))
				messageBox->button(QMessageBox::No)->click();
			else if (auto* button = modal ? qobject_cast<QAbstractButton*>(nppFindControl(modal, IDNO)) : nullptr)
				button->click();
		});
		answers.start(10);

		command(IDM_FILE_CLOSEALL);
		send(WM_CLOSE);
		answers.stop();
		QCoreApplication::processEvents();

		const QString configDir = _home.path() + QStringLiteral("/.config/notepad++");
		QVERIFY2(QFile::exists(configDir + QStringLiteral("/config.ini")), qPrintable(configDir));
		QVERIFY(QFile::exists(configDir + QStringLiteral("/session.xml")));
	}
};

QTEST_MAIN(AppTest)
#include "appTest.moc"
