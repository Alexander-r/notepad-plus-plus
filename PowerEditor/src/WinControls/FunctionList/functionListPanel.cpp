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

#include "json.hpp"
#include "functionListPanel.h"

#include <QAction>
#include <QDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QScrollBar>
#include <QShortcut>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <fstream>

#include <json.hpp>

#include "NppResources.h"
#include "ScintillaEditView.h"
#include "localization.h"

using nlohmann::json;
using namespace std;

#define INDEX_ROOT        0
#define INDEX_NODE        1
#define INDEX_LEAF        2

#define FL_PREFERENCES_INITIALSORT_ID   1

FunctionListPanel::~FunctionListPanel()
{
	for (const auto s : _posStrs)
	{
		delete s;
	}
}

QString FunctionListPanel::searchText() const
{
	return _hSearchEdit->text();
}

void FunctionListPanel::setSearchText(const std::wstring& text)
{
	_hSearchEdit->setText(QString::fromStdWString(text));
}

void FunctionListPanel::addEntry(const wchar_t *nodeName, const wchar_t *displayText, size_t pos)
{
	HTREEITEM itemParent = NULL;
	std::wstring posStr = std::to_wstring(pos);

	HTREEITEM root = _treeView.getRoot();

	if (nodeName != NULL && *nodeName != '\0')
	{
		itemParent = _treeView.searchSubItemByName(nodeName, root);
		if (!itemParent)
		{
			wstring* invalidValueStr = new wstring(posStr);
			_posStrs.push_back(invalidValueStr);
			LPARAM lParamInvalidPosStr = reinterpret_cast<LPARAM>(invalidValueStr);

			itemParent = _treeView.addItem(nodeName, root, INDEX_NODE, lParamInvalidPosStr);
		}
	}
	else
		itemParent = root;

	wstring* posString = new wstring(posStr);
	_posStrs.push_back(posString);
	LPARAM lParamPosStr = reinterpret_cast<LPARAM>(posString);

	_treeView.addItem(displayText, itemParent, INDEX_LEAF, lParamPosStr);
}

void FunctionListPanel::removeAllEntries()
{
	_treeView.removeAllItems();
}

void FunctionListPanel::addInStateArray(TreeStateNode tree2Update, const wchar_t *searchText, bool isSorted)
{
	bool found = false;
	for (size_t i = 0, len = _treeParams.size(); i < len; ++i)
	{
		if (_treeParams[i]._treeState._extraData == tree2Update._extraData)
		{
			_treeParams[i]._searchParameters._text2Find = searchText;
			_treeParams[i]._searchParameters._doSort = isSorted;
			_treeParams[i]._treeState = tree2Update;
			found = true;
		}
	}

	if (!found)
	{
		TreeParams params;
		params._treeState = tree2Update;
		params._searchParameters._text2Find = searchText;
		params._searchParameters._doSort = isSorted;
		_treeParams.push_back(params);
	}
}

TreeParams* FunctionListPanel::getFromStateArray(const wstring& fullFilePath)
{
	for (size_t i = 0, len = _treeParams.size(); i < len; ++i)
	{
		if (_treeParams[i]._treeState._extraData == fullFilePath)
			return &_treeParams[i];
	}
	return NULL;
}

void FunctionListPanel::sortOrUnsort()
{
	bool doSort = shouldSort();
	if (doSort)
		_pTreeView->sort(_pTreeView->getRoot(), true);
	else
	{
		const wstring text2search = searchText().toStdWString();

		if (text2search.empty()) // main view
		{
			_pTreeView->customSorting(_pTreeView->getRoot(), categorySortFunc, 0, true);
		}
		else // aux view
		{
			reload();

			if (_treeView.getRoot() == NULL)
				return;

			_treeViewSearchResult.removeAllItems();
			const wchar_t *fn = ((*_ppEditView)->getCurrentBuffer())->getFileName();

			wstring* invalidValueStr = new wstring(L"-1");
			_posStrs.push_back(invalidValueStr);
			LPARAM lParamInvalidPosStr = reinterpret_cast<LPARAM>(invalidValueStr);
			_treeViewSearchResult.addItem(fn, NULL, INDEX_ROOT, lParamInvalidPosStr);

			_treeView.searchLeafAndBuildTree(_treeViewSearchResult, text2search, INDEX_LEAF);
			_treeViewSearchResult.display(true);
			_treeViewSearchResult.expand(_treeViewSearchResult.getRoot());
			_treeView.display(false);
			_pTreeView = &_treeViewSearchResult;
		}
	}
}

int CALLBACK FunctionListPanel::categorySortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM /*lParamSort*/)
{
	wstring* posString1 = reinterpret_cast<wstring*>(lParam1);
	wstring* posString2 = reinterpret_cast<wstring*>(lParam2);
	
	size_t pos1 = _wtoi(posString1->c_str());
	size_t pos2 = _wtoi(posString2->c_str());
	if (pos1 > pos2)
		return 1;
	else 
		return -1;
}

bool FunctionListPanel::serialize(const wstring & outputFilename)
{
	Buffer* currentBuf = (*_ppEditView)->getCurrentBuffer();
	const wchar_t* fileNameLabel = currentBuf->getFileName();

	wstring fname2write;
	if (outputFilename.empty()) // if outputFilename is not given, get the current file path by adding the file extension
	{
		const wchar_t *fullFilePath = currentBuf->getFullPathName();

		// Export function list from an existing file
		bool exportFuncntionList = (NppParameters::getInstance()).doFunctionListExport();
		if (exportFuncntionList && doesFileExist(fullFilePath))
		{
			fname2write = fullFilePath;
			fname2write += L".result";
			fname2write += L".json";
		}
		else
			return false;
	}
	else
	{
		fname2write = outputFilename;
	}

	const char* rootLabel = "root";
	const char* nodesLabel = "nodes";
	const char* leavesLabel = "leaves";
	const char* nameLabel = "name";

	WcharMbcsConvertor& wmc = WcharMbcsConvertor::getInstance();
	json j;
	j[rootLabel] = wmc.wchar2char(fileNameLabel, CP_ACP);

	for (const auto & info : _foundFuncInfos)
	{
		std::string leafName = info._data.c_str();

		if (!info._data2.empty()) // node
		{
			bool isFound = false;
			std::string nodeName = info._data2.c_str();

			for (auto & i : j[nodesLabel])
			{
				if (nodeName == std::string{ i[nameLabel] })
				{
					i[leavesLabel].push_back(leafName.c_str());
					isFound = true;
					break;
				}
			}

			if (!isFound)
			{
				json aNode = { { leavesLabel, json::array() },{ nameLabel, nodeName.c_str() } };
				aNode[leavesLabel].push_back(leafName.c_str());
				j[nodesLabel].push_back(aNode);
			}
		}
		else // leaf
		{
			j[leavesLabel].push_back(leafName.c_str());
		}
	}

	std::ofstream file(wmc.wchar2char(fname2write.c_str(), CP_ACP));
	file << j;

	return true;
}

void FunctionListPanel::reload()
{
	//get scroll position
	const int scrollPosition = _treeView.tree()->verticalScrollBar()->value();

	// clean up
	_findLine = -1;
	_findEndLine = -1;
	TreeStateNode currentTree;
	bool isOK = _treeView.retrieveFoldingStateTo(currentTree, _treeView.getRoot());
	if (isOK)
	{
		bool isSorted =  shouldSort();
		addInStateArray(currentTree, searchText().toStdWString().c_str(), isSorted);
	}
	removeAllEntries();
	setSearchText(L"");
	setSort(false);

	_foundFuncInfos.clear();

	Buffer* currentBuf = (*_ppEditView)->getCurrentBuffer();
	const wchar_t *fn = currentBuf->getFileName();
	LangType langID = currentBuf->getLangType();
	if (langID == L_JS_EMBEDDED)
		langID = L_JAVASCRIPT;

	const wchar_t *udln = NULL;
	if (langID == L_USER)
	{
		udln = currentBuf->getUserDefineLangName();
	}

	const wchar_t* ext = ::PathFindExtension(fn);

	bool parsedOK = _funcParserMgr.parse(_foundFuncInfos, AssociationInfo(-1, langID, ext, udln));
	if (parsedOK)
	{
		wstring* invalidValueStr = new wstring(L"-1");
		_posStrs.push_back(invalidValueStr);
		LPARAM lParamInvalidPosStr = reinterpret_cast<LPARAM>(invalidValueStr);

		_treeView.addItem(fn, NULL, INDEX_ROOT, lParamInvalidPosStr);
	}

	const auto codePage = static_cast<UINT>((*_ppEditView)->execute(SCI_GETCODEPAGE));

	for (const auto& foundFuncInfo : _foundFuncInfos)
	{
		addEntry(string2wstring(foundFuncInfo._data2, codePage).c_str(), string2wstring(foundFuncInfo._data, codePage).c_str(), foundFuncInfo._pos);
	}

	HTREEITEM root = _treeView.getRoot();

	if (root)
	{
		currentBuf = (*_ppEditView)->getCurrentBuffer();
		const wchar_t *fullFilePath = currentBuf->getFullPathName();

		wstring* fullPathStr = new wstring(fullFilePath);
		_posStrs.push_back(fullPathStr);
		LPARAM lParamFullPathStr = reinterpret_cast<LPARAM>(fullPathStr);

		_treeView.setItemParam(root, lParamFullPathStr);
		TreeParams *previousParams = getFromStateArray(fullFilePath);
		if (!previousParams)
		{
			setSearchText(L"");
			setSort(NppParameters::getInstance().getNppGUI()._shouldSortFunctionList);
			sortOrUnsort();
			_treeView.expand(root);
		}
		else
		{
			setSearchText((previousParams->_searchParameters)._text2Find);

			bool isSort = (previousParams->_searchParameters)._doSort;
			setSort(isSort);
			if (isSort)
				_pTreeView->sort(_pTreeView->getRoot(), true);

			_treeView.restoreFoldingStateFrom(previousParams->_treeState, root);
		}
	}

	updateSearchEditColor();

	//set scroll position
	_treeView.tree()->verticalScrollBar()->setValue(scrollPosition);
}

void FunctionListPanel::initPreferencesMenu()
{
	NativeLangSpeaker* pNativeSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
	NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();

	wstring shouldSortFunctionListStr = pNativeSpeaker->getAttrNameStr(L"Sort functions (A to Z) by default", FL_FUNCTIONLISTROOTNODE, FL_PREFERENCE_INITIALSORT);

	_hPreferencesMenu = std::make_unique<QMenu>();
	QAction* action = _hPreferencesMenu->addAction(QString::fromStdWString(shouldSortFunctionListStr));
	action->setCheckable(true);
	action->setChecked(nppGUI._shouldSortFunctionList);
	QObject::connect(action, &QAction::toggled, _hSelf, [&nppGUI](bool isOn) { nppGUI._shouldSortFunctionList = isOn; });
}

void FunctionListPanel::markEntry()
{
	LONG lineNr = static_cast<LONG>((*_ppEditView)->getCurrentLineNumber());
	HTREEITEM root = _treeView.getRoot();
	if (_findLine != -1 && _findEndLine != -1 && lineNr >= _findLine && lineNr < _findEndLine)
		return;
	_findLine = -1;
	_findEndLine = -1;
	findMarkEntry(root, lineNr);
	if (_findLine != -1)
	{
		_treeView.selectItem(_findItem);
	}
	else
	{
		_treeView.selectItem(root);
	}

}

void FunctionListPanel::findMarkEntry(HTREEITEM htItem, LONG line)
{
	HTREEITEM cItem{};
	for (; htItem != NULL; htItem = _treeView.getNextSibling(htItem))
	{
		cItem = _treeView.getChildFrom(htItem);
		if (cItem != NULL)
		{
			findMarkEntry(cItem, line);
		}
		else
		{
			wstring *posStr = reinterpret_cast<wstring *>(_treeView.getItemParam(htItem));
			if (posStr)
			{
				int pos = _wtoi(posStr->c_str());
				if (pos != -1)
				{
					LONG sci_line = static_cast<LONG>((*_ppEditView)->execute(SCI_LINEFROMPOSITION, pos));
					if (line >= sci_line)
					{
						if (sci_line > _findLine || _findLine == -1)
						{
							_findLine = sci_line;
							_findItem = htItem;
						}
					}
					else
					{
						if (sci_line < _findEndLine)
							_findEndLine = sci_line;
					}
				}
			}
		}
	}
}

void FunctionListPanel::init(HINSTANCE hInst, HWND hPere, ScintillaEditView **ppEditView)
{
	DockingDlgInterface::init(hInst, hPere);
	_ppEditView = ppEditView;
	NppParameters& nppParams = NppParameters::getInstance();

	wstring funcListXmlPath = nppParams.getUserPath();
	pathAppend(funcListXmlPath, L"functionList");

	wstring funcListDefaultXmlPath = nppParams.getNppPath();
	pathAppend(funcListDefaultXmlPath, L"functionList");

	bool doLocalConf = nppParams.isLocal();

	if (!doLocalConf)
	{
		if (!doesDirectoryExist(funcListXmlPath.c_str()))
		{
			if (doesDirectoryExist(funcListDefaultXmlPath.c_str()))
			{
				_funcParserMgr.init(funcListXmlPath, funcListDefaultXmlPath, ppEditView);
			}
		}
		else
		{
			_funcParserMgr.init(funcListXmlPath, funcListDefaultXmlPath, ppEditView);
		}
	}
	else
	{
		if (doesDirectoryExist(funcListDefaultXmlPath.c_str()))
		{
			_funcParserMgr.init(funcListDefaultXmlPath, funcListDefaultXmlPath, ppEditView);
		}
	}
}

bool FunctionListPanel::openSelection(const TreeView & treeView)
{
	const HTREEITEM selection = treeView.getSelection();
	const int image = treeView.getItemImage(selection);
	if (!selection || image == INDEX_ROOT || image == INDEX_NODE)
	{
		return false;
	}

	wstring *posStr = reinterpret_cast<wstring *>(treeView.getItemParam(selection));
	if (!posStr)
		return false;

	int pos = _wtoi(posStr->c_str());
	if (pos == -1)
		return false;

	auto sci_line = (*_ppEditView)->execute(SCI_LINEFROMPOSITION, pos);
	(*_ppEditView)->execute(SCI_ENSUREVISIBLE, sci_line);
	(*_ppEditView)->scrollPosToCenter(pos);

	return true;
}

void FunctionListPanel::focusOnEditor() const
{
	::PostMessage(_hParent, WM_COMMAND, SCEN_SETFOCUS << 16, reinterpret_cast<LPARAM>((*_ppEditView)->getHSelf()));
}

void FunctionListPanel::onReturnKey(const TreeView& treeView)
{
	if (!openSelection(treeView))
	{
		treeView.toggleExpandCollapse(treeView.getSelection());
		return;
	}
	focusOnEditor();
}

void FunctionListPanel::connectTree(TreeView& treeView)
{
	QTreeWidget* tree = treeView.tree();
	TreeView* pTree = &treeView;

	tree->setExpandsOnDoubleClick(true);
	QObject::connect(tree, &QTreeWidget::itemDoubleClicked, _hSelf, [this, pTree]()
	{
		if (openSelection(*pTree))
			focusOnEditor();
	});

	const auto addKey = [this, tree](int key, auto handler)
	{
		auto* shortcut = new QShortcut(QKeySequence(key), tree);
		shortcut->setContext(Qt::WidgetShortcut);
		QObject::connect(shortcut, &QShortcut::activated, _hSelf, handler);
	};
	addKey(Qt::Key_Return, [this, pTree]() { onReturnKey(*pTree); });
	addKey(Qt::Key_Enter, [this, pTree]() { onReturnKey(*pTree); });
	addKey(Qt::Key_Escape, [this]()
	{
		setSearchText(L"");
		focusOnEditor();
	});
}

void FunctionListPanel::onDockingNotify(const NMHDR& notification)
{
	if (notification.code == DMN_SWITCHIN)
		reload();
	else if (notification.code == DMN_CLOSE)
		::SendMessage(_hParent, WM_COMMAND, IDM_VIEW_FUNC_LIST, 0);
	else
		DockingDlgInterface::onDockingNotify(notification);
}

void FunctionListPanel::searchFuncAndSwitchView()
{
	const wstring text2search = searchText().toStdWString();

	if (text2search.empty())
	{
		_treeViewSearchResult.display(false);
		_treeView.display(true);
		_pTreeView = &_treeView;
	}
	else
	{
		if (_treeView.getRoot() == NULL)
			return;

		_treeViewSearchResult.removeAllItems();
		const wchar_t *fn = ((*_ppEditView)->getCurrentBuffer())->getFileName();

		wstring* invalidValueStr = new wstring(L"-1");
		_posStrs.push_back(invalidValueStr);
		LPARAM lParamInvalidPosStr = reinterpret_cast<LPARAM>(invalidValueStr);
		_treeViewSearchResult.addItem(fn, NULL, INDEX_ROOT, lParamInvalidPosStr);

		_treeView.searchLeafAndBuildTree(_treeViewSearchResult, text2search, INDEX_LEAF);
		_treeViewSearchResult.display(true);
		_treeViewSearchResult.expand(_treeViewSearchResult.getRoot());
		_treeView.display(false);
		_pTreeView = &_treeViewSearchResult;
	}

	updateSearchEditColor();

	// restore selected sorting
	if (shouldSort())
		_pTreeView->sort(_pTreeView->getRoot(), true);
	else
		_pTreeView->customSorting(_pTreeView->getRoot(), categorySortFunc, 0, true);
}

// Make edit field red if not found
void FunctionListPanel::updateSearchEditColor() const
{
	bool textFound = true;
	if (!searchText().isEmpty())
	{
		// children on root found, or no root (no parser): use the default color
		const HTREEITEM searchViewRoot = _treeViewSearchResult.getRoot();
		textFound = !searchViewRoot || _treeViewSearchResult.getChildFrom(searchViewRoot);
	}

	_hSearchEdit->setStyleSheet(textFound ? QString{} : QStringLiteral("QLineEdit { background-color: #FF5050; color: white; }"));
}

bool FunctionListPanel::shouldSort()
{
	return _sortButton->isChecked();
}

void FunctionListPanel::setSort(bool isEnabled)
{
	_sortButton->setChecked(isEnabled);
}

void FunctionListPanel::create(DockedWidgetData* data, bool isRTL)
{
	DockingDlgInterface::create(data, isRTL);

	initPreferencesMenu();

	NppParameters& nppParam = NppParameters::getInstance();
	NativeLangSpeaker *pNativeSpeaker = nppParam.getNativeLangSpeaker();
	_sortTipStr = pNativeSpeaker->getAttrNameStr(_sortTipStr.c_str(), FL_FUNCTIONLISTROOTNODE, FL_SORTLOCALNODENAME);
	_reloadTipStr = pNativeSpeaker->getAttrNameStr(_reloadTipStr.c_str(), FL_FUNCTIONLISTROOTNODE, FL_RELOADLOCALNODENAME);
	_preferenceTipStr = pNativeSpeaker->getAttrNameStr(_preferenceTipStr.c_str(), FL_FUNCTIONLISTROOTNODE, FL_PREFERENCESLOCALNODENAME);

	const auto makeButton = [this](int lightIcon, int darkIcon, const wstring& tip)
	{
		auto* button = new QToolButton(_hSelf);
		button->setAutoRaise(true);
		button->setToolTip(QString::fromStdWString(tip));
		addThemedButton(button, lightIcon, darkIcon);
		return button;
	};

	_hSearchEdit = new QLineEdit(_hSelf);
	_hSearchEdit->setProperty("nppId", IDC_SEARCHFIELD_FUNCLIST);
	_hSearchEdit->setClearButtonEnabled(true);

	_sortButton = makeButton(IDI_FUNCLIST_SORTBUTTON, IDI_FUNCLIST_SORTBUTTON_DM, _sortTipStr);
	_sortButton->setCheckable(true);
	_sortButton->setProperty("nppId", IDC_SORTBUTTON_FUNCLIST);
	QToolButton* reloadButton = makeButton(IDI_FUNCLIST_RELOADBUTTON, IDI_FUNCLIST_RELOADBUTTON_DM, _reloadTipStr);
	reloadButton->setProperty("nppId", IDC_RELOADBUTTON_FUNCLIST);
	QToolButton* preferenceButton = makeButton(IDI_FUNCLIST_PREFERENCEBUTTON, IDI_FUNCLIST_PREFERENCEBUTTON_DM, _preferenceTipStr);
	preferenceButton->setProperty("nppId", IDC_PREFERENCEBUTTON_FUNCLIST);
	preferenceButton->setPopupMode(QToolButton::InstantPopup);
	preferenceButton->setMenu(_hPreferencesMenu.get());

	auto* tools = new QHBoxLayout;
	tools->setContentsMargins(2, 2, 2, 2);
	tools->setSpacing(2);
	tools->addWidget(_hSearchEdit, 1);
	tools->addWidget(_sortButton);
	tools->addWidget(reloadButton);
	tools->addWidget(preferenceButton);

	std::vector<int> imgIds = _treeView.getImageIds(
		{ IDI_FUNCLIST_ROOT, IDI_FUNCLIST_NODE, IDI_FUNCLIST_LEAF }
		, { IDI_FUNCLIST_ROOT_DM, IDI_FUNCLIST_NODE_DM, IDI_FUNCLIST_LEAF_DM }
		, { IDI_FUNCLIST_ROOT2, IDI_FUNCLIST_NODE2, IDI_FUNCLIST_LEAF2 }
	);

	_treeView.init(_hInst, _hSelf, IDC_LIST_FUNCLIST);
	_treeView.setImageList(imgIds);
	_treeViewSearchResult.init(_hInst, _hSelf, IDC_LIST_FUNCLIST_AUX);
	_treeViewSearchResult.setImageList(imgIds);
	_treeView.makeLabelEditable(false);

	auto* layout = new QVBoxLayout(_hSelf);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	layout->addLayout(tools);
	layout->addWidget(_treeView.getHSelf(), 1);
	layout->addWidget(_treeViewSearchResult.getHSelf(), 1);

	_treeView.display();

	connectTree(_treeView);
	connectTree(_treeViewSearchResult);

	QObject::connect(_hSearchEdit, &QLineEdit::textChanged, _hSelf, [this]() { searchFuncAndSwitchView(); });
	QObject::connect(_sortButton, &QToolButton::clicked, _hSelf, [this]() { sortOrUnsort(); });
	QObject::connect(reloadButton, &QToolButton::clicked, _hSelf, [this]() { reload(); });
}
