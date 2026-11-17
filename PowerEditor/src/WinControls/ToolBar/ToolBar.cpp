// This file is part of Notepad++ project
// Copyright (C)2021 Don HO <don.h@free.fr>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#include "ToolBar.h"

#include <cstdlib>
#include <QToolButton>
#include <QMenu>
#include <QImage>
#include <QLayout>
#include <QMainWindow>
#include <QAction>
#include <QPixmap>
#include <QVBoxLayout>

#include <cstring>
#include <filesystem>
#include <string_view>

#include "NppDarkMode.h"
#include "Notepad_plus_msgs.h"
#include "NppConstants.h"
#include "NppResources.h"
#include "Parameters.h"
#include "menuCmdID.h"
#include "resource.h"

namespace
{
	struct ToolbarIconIdUnit
	{
		std::wstring_view _id;
		bool _hasDisabledIcon = false;
	};

	// In the order of the buttons without the separators, as listed in toolbarButtonsConf_example.xml
	constexpr ToolbarIconIdUnit toolbarIconIDs[]{
		{ L"new", false },
		{ L"open", false },
		{ L"save", true },
		{ L"save-all", true },
		{ L"close", false },
		{ L"close-all", false },
		{ L"print", false },
		{ L"cut", true },
		{ L"copy", true },
		{ L"paste", true },
		{ L"undo", true },
		{ L"redo", true },
		{ L"find", false },
		{ L"replace", false },
		{ L"zoom-in", false },
		{ L"zoom-out", false },
		{ L"sync-vertical", false },
		{ L"sync-horizontal", false },
		{ L"word-wrap", false },
		{ L"all-chars", false },
		{ L"indent-guide", false },
		{ L"udl-dlg", false },
		{ L"doc-map", false },
		{ L"doc-list", false },
		{ L"function-list", false },
		{ L"folder-as-workspace", false },
		{ L"monitoring", true },
		{ L"record", true },
		{ L"stop-record", true },
		{ L"playback", true },
		{ L"playback-multiple", true },
		{ L"save-macro", true }
	};

	// The path if the file is there. Whether it is a usable icon shows when it is loaded.
	QString existingFile(const std::filesystem::path& path)
	{
		const std::wstring file = path.wstring();
		return doesFileExist(file.c_str()) ? QString::fromStdWString(file) : QString();
	}

	QIcon iconFromResource(int id, int disabledId)
	{
		QIcon icon;
		if (const char* path = nppImageResourcePath(id))
			icon.addFile(QString::fromLatin1(path));
		if (disabledId != id)
		{
			if (const char* path = nppImageResourcePath(disabledId))
				icon.addFile(QString::fromLatin1(path), QSize(), QIcon::Disabled);
		}
		return icon;
	}

	// The colour the settings put on the fluent icons, in place of a colour in the icon or of everything when that is 0
	bool fluentColours(COLORREF& from, COLORREF& to)
	{
		const bool isDark = NppDarkMode::usesDarkWidgets();
		const COLORREF main = isDark ? g_cDefaultMainDark : g_cDefaultMainLight;
		const COLORREF secondary = isDark ? g_cDefaultSecondaryDark : g_cDefaultSecondaryLight;
		const TbIconInfo& info = NppParameters::getInstance().getNppGUI()._tbIconInfo;

		from = info._tbUseMono ? 0 : secondary;
		switch (info._tbColor)
		{
			case FluentColor::accent:
				to = NppDarkMode::getAccentColor();
				return true;
			case FluentColor::red:
				to = RGB(0xE8, 0x11, 0x23);
				return true;
			case FluentColor::green:
				to = RGB(0x00, 0x8B, 0x00);
				return true;
			case FluentColor::blue:
				to = RGB(0x00, 0x78, 0xD4);
				return true;
			case FluentColor::purple:
				to = RGB(0xB1, 0x46, 0xC2);
				return true;
			case FluentColor::cyan:
				to = RGB(0x00, 0xB7, 0xC3);
				return true;
			case FluentColor::olive:
				to = RGB(0x49, 0x82, 0x05);
				return true;
			case FluentColor::yellow:
				to = RGB(0xFF, 0xB9, 0x00);
				return true;
			case FluentColor::custom:
				if (info._tbCustomColor != 0)
				{
					to = info._tbCustomColor;
					return true;
				}
				[[fallthrough]];
			case FluentColor::defaultColor:
				if (info._tbUseMono)
				{
					to = main;
					return true;
				}
				[[fallthrough]];
			default:
				return false;
		}
	}

	QPixmap recoloured(const QPixmap& pixmap, COLORREF from, COLORREF to)
	{
		constexpr int tolerance = 3;
		QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
		const QRgb target = qRgb(GetRValue(to), GetGValue(to), GetBValue(to));
		for (int y = 0; y < image.height(); ++y)
		{
			auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
			for (int x = 0; x < image.width(); ++x)
			{
				if (qAlpha(line[x]) == 0)
					continue;

				const bool isChanged = (from == 0)
					|| (std::abs(qRed(line[x]) - static_cast<int>(GetRValue(from))) <= tolerance
						&& std::abs(qGreen(line[x]) - static_cast<int>(GetGValue(from))) <= tolerance
						&& std::abs(qBlue(line[x]) - static_cast<int>(GetBValue(from))) <= tolerance);
				if (isChanged)
					line[x] = (target & 0x00FFFFFF) | (static_cast<QRgb>(qAlpha(line[x])) << 24);
			}
		}
		return QPixmap::fromImage(image);
	}

	// A fluent icon in the colour of the settings. The disabled one stays as it is.
	QIcon fluentIcon(int id, int disabledId)
	{
		COLORREF from = 0;
		COLORREF to = 0;
		const QIcon original = iconFromResource(id, disabledId);
		if (!fluentColours(from, to))
			return original;

		QIcon icon;
		const QList<QSize> sizes = original.availableSizes();
		for (const QSize& size : sizes)
		{
			icon.addPixmap(recoloured(original.pixmap(size), from, to));
			icon.addPixmap(original.pixmap(size, QIcon::Disabled), QIcon::Disabled);
		}
		return icon;
	}
}

void ToolBar::setButtonMenus(int cmdID, QMenu* dropDown, QMenu* contextMenu)
{
	auto* button = qobject_cast<QToolButton*>(static_cast<QToolBar*>(_hSelf)->widgetForAction(findAction(cmdID)));
	if (!button)
		return;

	button->setMenu(dropDown);
	button->setPopupMode(QToolButton::MenuButtonPopup);
	button->setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(button, &QWidget::customContextMenuRequested, button, [button, contextMenu](const QPoint& position)
	{
		contextMenu->exec(button->mapToGlobal(position));
	});
}

void ToolBar::initTheme(NppXml::Document toolIconsDocRoot)
{
	NppXml::Element toolIcons = NppXml::firstChildElement(toolIconsDocRoot, "NotepadPlus");
	if (toolIcons)
		toolIcons = NppXml::firstChildElement(toolIcons, "ToolBarIcons");
	if (!toolIcons)
		return;

	namespace fs = std::filesystem;
	fs::path iconFolderDir = NppParameters::getInstance().getUserPath();
	iconFolderDir /= L"toolbarIcons";

	const char* folderName = NppXml::attribute(toolIcons, "icoFolderName");
	iconFolderDir /= (folderName && folderName[0] ? string2wstring(folderName, CP_UTF8) : L"default"); // as toolbarButtonsConf_example.xml says

	_customIcons.clear();
	bool hasCustomIcon = false;
	for (const ToolbarIconIdUnit& icoUnit : toolbarIconIDs)
	{
		CustomIconFiles files;
		fs::path locator = iconFolderDir;
		locator /= icoUnit._id;
		locator += L".ico";
		files._normal = existingFile(locator);

		if (icoUnit._hasDisabledIcon)
		{
			fs::path locatorDisabled = iconFolderDir;
			locatorDisabled /= icoUnit._id;
			locatorDisabled += L"_disabled.ico";
			files._disabled = existingFile(locatorDisabled);
		}

		hasCustomIcon = hasCustomIcon || !files._normal.isEmpty() || !files._disabled.isEmpty();
		_customIcons.push_back(files);
	}

	if (!hasCustomIcon)
		_customIcons.clear();
}

// A file of the set takes the place of the same state of the built-in icon. As on Windows these keep their own
// colours, and a file that cannot be read leaves the built-in one.
QIcon ToolBar::withCustomIcons(const QIcon& builtIn, size_t iconIndex) const
{
	if (iconIndex >= _customIcons.size())
		return builtIn;

	const QIcon normal(_customIcons[iconIndex]._normal);
	const QIcon disabled(_customIcons[iconIndex]._disabled);
	const QList<QSize> normalSizes = normal.availableSizes();
	const QList<QSize> disabledSizes = disabled.availableSizes();
	if (normalSizes.isEmpty() && disabledSizes.isEmpty())
		return builtIn;

	const QList<QSize> builtInSizes = builtIn.availableSizes();
	QIcon icon;
	for (const QSize& size : normalSizes.isEmpty() ? builtInSizes : normalSizes)
		icon.addPixmap((normalSizes.isEmpty() ? builtIn : normal).pixmap(size));
	for (const QSize& size : disabledSizes.isEmpty() ? builtInSizes : disabledSizes)
		icon.addPixmap((disabledSizes.isEmpty() ? builtIn.pixmap(size, QIcon::Disabled) : disabled.pixmap(size)), QIcon::Disabled);
	return icon;
}

void ToolBar::initHideButtonsConf(NppXml::Document toolButtonsDocRoot, const ToolBarButtonUnit* buttonUnitArray, int arraySize)
{
	NppXml::Element toolButtons = NppXml::firstChildElement(toolButtonsDocRoot, "NotepadPlus");
	if (toolButtons)
		toolButtons = NppXml::firstChildElement(toolButtons, "ToolbarButtons");
	if (!toolButtons)
		return;

	// Standard toolbar button
	if (NppXml::Element standard = NppXml::firstChildElement(toolButtons, "Standard"))
	{
		const char* isHideAll = NppXml::attribute(standard, "hideAll");
		const bool hideAll = isHideAll && (std::strcmp(isHideAll, "yes") == 0);
		_stdButtonsVisible.assign(static_cast<size_t>(arraySize), !hideAll);

		if (!hideAll)
		{
			for (NppXml::Element button = NppXml::firstChildElement(standard, "Button"); button; button = NppXml::nextSiblingElement(button, "Button"))
			{
				const int cmdID = NppXml::intAttribute(button, "id", -1);
				const int index = NppXml::intAttribute(button, "index", -1);
				const char* isHide = NppXml::attribute(button, "hide");
				if (cmdID > -1 && index > -1 && index < arraySize && isHide && (std::strcmp(isHide, "yes") == 0) && buttonUnitArray[index]._cmdID == cmdID)
					_stdButtonsVisible[index] = false;
			}
		}
	}

	// Plugin toolbar button
	if (NppXml::Element plugin = NppXml::firstChildElement(toolButtons, "Plugin"))
	{
		const char* isHideAll = NppXml::attribute(plugin, "hideAll");
		_toolbarPluginButtonsConf._isHideAll = isHideAll && (std::strcmp(isHideAll, "yes") == 0);

		for (NppXml::Element button = NppXml::firstChildElement(plugin, "Button"); button; button = NppXml::nextSiblingElement(button, "Button"))
		{
			const int index = NppXml::intAttribute(button, "index", -1);
			const char* isHide = NppXml::attribute(button, "hide");
			if (index > -1)
			{
				if (static_cast<size_t>(index) >= _toolbarPluginButtonsConf._showPluginButtonsArray.size())
					_toolbarPluginButtonsConf._showPluginButtonsArray.resize(static_cast<size_t>(index) + 1, true);
				_toolbarPluginButtonsConf._showPluginButtonsArray[index] = !(isHide && std::strcmp(isHide, "yes") == 0);
			}
		}
	}
}

QAction* ToolBar::addCommand(int cmdID)
{
	auto* toolbar = static_cast<QToolBar*>(_hSelf);
	QAction* action = toolbar->addAction(QString());
	const HWND target = _hParent;
	QObject::connect(action, &QAction::triggered, action, [target, cmdID]()
	{
		::SendMessage(target, WM_COMMAND, MAKEWPARAM(static_cast<WORD>(cmdID), 0), 0);
	});
	action->setCheckable(false);
	_actions[cmdID] = action;
	return action;
}

bool ToolBar::init(HINSTANCE hInst, HWND hPere, toolBarStatusType type, const ToolBarButtonUnit* buttonUnitArray, int arraySize)
{
	Window::init(hInst, hPere);

	auto* toolbar = new QToolBar(hPere);
	toolbar->setObjectName(QStringLiteral("mainToolBar"));
	toolbar->setFloatable(false);
	_hSelf = toolbar;

	_buttonUnits = buttonUnitArray;
	_nbButtons = arraySize;

	for (int i = 0; i < arraySize; ++i)
	{
		if (!_stdButtonsVisible.empty() && !_stdButtonsVisible[i])
			continue;
		const int cmdID = buttonUnitArray[i]._cmdID;
		if (cmdID == 0)
			toolbar->addSeparator();
		else
			addCommand(cmdID);
	}

	if (!_dynamicButtons.empty() && !_toolbarPluginButtonsConf._isHideAll)
	{
		toolbar->addSeparator();
		for (size_t i = 0; i < _dynamicButtons.size(); ++i)
		{
			const auto& conf = _toolbarPluginButtonsConf._showPluginButtonsArray;
			if (i >= conf.size() || conf[i])
				addCommand(static_cast<int>(_dynamicButtons[i]._message));
		}
	}

	setState(type);
	return true;
}

void ToolBar::destroy()
{
	delete _hSelf;
	_hSelf = nullptr;
	_actions.clear();
}

QAction* ToolBar::findAction(int cmdID) const
{
	const auto it = _actions.find(cmdID);
	return it == _actions.end() ? nullptr : it->second;
}

void ToolBar::enable(int cmdID, bool doEnable) const
{
	if (QAction* action = findAction(cmdID))
		action->setEnabled(doEnable);
}

bool ToolBar::getCheckState(int ID2Check) const
{
	const QAction* action = findAction(ID2Check);
	return action && action->isChecked();
}

void ToolBar::setCheck(int ID2Check, bool willBeChecked) const
{
	if (QAction* action = findAction(ID2Check))
	{
		action->setCheckable(true);
		action->setChecked(willBeChecked);
	}
}

void ToolBar::setState(toolBarStatusType state)
{
	// The standard set has no place for custom icons
	_state = (state == TB_STANDARD && !_customIcons.empty()) ? TB_SMALL : state;
	state = _state;
	static_cast<QToolBar*>(_hSelf)->setIconSize((state == TB_LARGE || state == TB_LARGE2) ? QSize(32, 32) : QSize(16, 16));
	refreshIcons();
	::SendMessage(_hParent, NPPM_INTERNAL_TOOLBARICONSCHANGED, 0, 0); // for the panels, whose icons follow the set
}

void ToolBar::refreshIcons()
{
	const bool isDark = NppDarkMode::usesDarkWidgets();
	size_t iconIndex = 0;
	for (int i = 0; i < _nbButtons; ++i)
	{
		const ToolBarButtonUnit& unit = _buttonUnits[i];
		if (unit._cmdID == 0)
			continue;

		// A hidden button still counts for the icon files
		const size_t thisIcon = iconIndex++;
		QAction* action = findAction(unit._cmdID);
		if (!action)
			continue;

		switch (_state)
		{
			case TB_STANDARD:
				action->setIcon(iconFromResource(unit._stdIcon, unit._stdIcon));
				break;
			case TB_SMALL2:
			case TB_LARGE2:
				action->setIcon(withCustomIcons(isDark ? fluentIcon(unit._defaultDarkModeIcon2, unit._grayDarkModeIcon2) : fluentIcon(unit._defaultIcon2, unit._grayIcon2), thisIcon));
				break;
			default:
				action->setIcon(withCustomIcons(isDark ? fluentIcon(unit._defaultDarkModeIcon, unit._grayDarkModeIcon) : fluentIcon(unit._defaultIcon, unit._grayIcon), thisIcon));
				break;
		}
	}

	for (const DynamicButton& button : _dynamicButtons)
	{
		if (QAction* action = findAction(static_cast<int>(button._message)))
			action->setIcon(isDark && !button._iconDarkMode.isNull() ? button._iconDarkMode : button._icon);
	}
}

void ToolBar::registerDynBtn(UINT message, toolbarIcons* iconHandles, HICON absentIco)
{
	// Before the toolbar exists, as on Windows
	if (_hSelf)
		return;

	DynamicButton button;
	button._message = message;
	if (iconHandles->hToolbarIcon)
		button._icon = *iconHandles->hToolbarIcon;
	else if (iconHandles->hToolbarBmp)
		button._icon = QIcon(*iconHandles->hToolbarBmp);
	else if (absentIco)
		button._icon = *absentIco;
	_dynamicButtons.push_back(button);
}

void ToolBar::registerDynBtnDM(UINT message, toolbarIconsWithDarkMode* iconHandles)
{
	if (_hSelf)
		return;

	DynamicButton button;
	button._message = message;
	if (iconHandles->hToolbarIcon)
		button._icon = *iconHandles->hToolbarIcon;
	else if (iconHandles->hToolbarBmp)
		button._icon = QIcon(*iconHandles->hToolbarBmp);
	if (iconHandles->hToolbarIconDarkMode)
		button._iconDarkMode = *iconHandles->hToolbarIconDarkMode;
	_dynamicButtons.push_back(button);
}

void ReBar::init(HINSTANCE hInst, HWND hPere)
{
	Window::init(hInst, hPere);
	auto* holder = new QToolBar(hPere);
	holder->setMovable(false);
	holder->setFloatable(false);
	holder->toggleViewAction()->setVisible(false);
	holder->setContextMenuPolicy(Qt::PreventContextMenu);

	// Not with a style sheet: a widget with one does not follow the application's palette when dark mode changes
	holder->layout()->setContentsMargins(0, 0, 0, 0);
	holder->layout()->setSpacing(0);

	_bandArea = new QWidget(holder);
	auto* layout = new QVBoxLayout(_bandArea);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	holder->addWidget(_bandArea);

	_hSelf = holder;
	_hSelf->hide();
}

void ReBar::attachTo(QMainWindow* window, Qt::ToolBarArea area)
{
	window->addToolBar(area, static_cast<QToolBar*>(_hSelf));
}

void ReBar::destroy()
{
	delete _hSelf;
	_hSelf = nullptr;
	_bandArea = nullptr;
	_bands.clear();
}

void ReBar::addBand(int id, QWidget* band)
{
	static_cast<QVBoxLayout*>(_bandArea->layout())->addWidget(band);
	_bands[id] = band;
	_hSelf->show();
}

void ReBar::setIDVisible(int id, bool show)
{
	const auto it = _bands.find(id);
	if (it != _bands.end())
		it->second->setVisible(show);

	bool anyVisible = false;
	for (const auto& band : _bands)
		anyVisible = anyVisible || !band.second->isHidden();
	_hSelf->setVisible(anyVisible);
}

bool ReBar::getIDVisible(int id) const
{
	const auto it = _bands.find(id);
	return it != _bands.end() && !it->second->isHidden();
}

bool ReBar::hasBand(const QWidget* band) const
{
	for (const auto& entry : _bands)
	{
		if (entry.second == band)
			return true;
	}
	return false;
}
