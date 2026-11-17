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


#include "fileBrowser.h"

#include <QAction>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QShortcut>
#include <QStandardPaths>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <functional>
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_set>

#include "Common.h"
#include "DockingDlgInterface.h"
#include "Notepad_plus_msgs.h"
#include "NppDarkMode.h"
#include "NppResources.h"
#include "Parameters.h"
#include "RunDlg.h"
#include "dockingResource.h"
#include "fileBrowser_rc.h"
#include "localization.h"
#include "menuCmdID.h"
#include "resource.h"

using namespace std;

enum ItemIdx
{
	INDEX_OPEN_ROOT,
	INDEX_CLOSE_ROOT,
	INDEX_OPEN_NODE,
	INDEX_CLOSE_NODE,
	INDEX_LEAF
};


FileBrowser::~FileBrowser()
{
	for (const auto folder : _folderUpdaters)
	{
		folder->stopWatcher();
		delete folder;
	}

	for (const auto cd : _sortingDataArray)
	{
		delete cd;
	}
}

static std::vector<std::wstring> split(const std::wstring& string2split, wchar_t sep)
{
	vector<wstring> splitedStrings;
	size_t len = string2split.length();
	size_t beginPos = 0;
	for (size_t i = 0; i < len + 1; ++i)
	{
		if (string2split[i] == sep || string2split[i] == '\0')
		{
			splitedStrings.push_back(string2split.substr(beginPos, i - beginPos));
			beginPos = i + 1;
		}
	}
	return splitedStrings;
}

static bool isRelatedRootFolder(const std::wstring& relatedRoot, const std::wstring& subFolder)
{
	if (relatedRoot.empty())
		return false;

	if (subFolder.empty())
		return false;

	size_t pos = subFolder.find(relatedRoot);
	if (pos != 0) // pos == 0 is the necessary condition, but not enough
		return false;

	vector<wstring> relatedRootArray = split(relatedRoot, nppPathSep);
	vector<wstring> subFolderArray = split(subFolder, nppPathSep);

	size_t index2Compare = relatedRootArray.size() - 1;

	return relatedRootArray[index2Compare] == subFolderArray[index2Compare];
}

void FileBrowser::create(DockedWidgetData* data, bool isRTL)
{
	DockingDlgInterface::create(data, isRTL);

	NppParameters& nppParam = NppParameters::getInstance();

	// tips text for toolbar buttons
	NativeLangSpeaker *pNativeSpeaker = nppParam.getNativeLangSpeaker();
	_expandAllFolders = pNativeSpeaker->getAttrNameStr(_expandAllFolders.c_str(), FOLDERASWORKSPACE_NODE, "ExpandAllFoldersTip");
	_collapseAllFolders = pNativeSpeaker->getAttrNameStr(_collapseAllFolders.c_str(), FOLDERASWORKSPACE_NODE, "CollapseAllFoldersTip");
	_locateCurrentFile = pNativeSpeaker->getAttrNameStr(_locateCurrentFile.c_str(), FOLDERASWORKSPACE_NODE, "LocateCurrentFileTip");

	auto* tools = new QHBoxLayout;
	tools->setContentsMargins(2, 2, 2, 2);
	tools->setSpacing(2);

	const auto addTool = [this, tools](int lightIcon, int darkIcon, const wstring& tip, auto handler)
	{
		auto* button = new QToolButton(_hSelf);
		button->setAutoRaise(true);
		button->setToolTip(QString::fromStdWString(tip));
		addThemedButton(button, lightIcon, darkIcon);
		QObject::connect(button, &QToolButton::clicked, _hSelf, handler);
		tools->addWidget(button);
	};
	tools->addStretch(1);
	addTool(IDI_FB_SELECTCURRENTFILE, IDI_FB_SELECTCURRENTFILE_DM, _locateCurrentFile, [this]() { selectCurrentEditingFile(); });
	addTool(IDI_FB_FOLDALL, IDI_FB_FOLDALL_DM, _collapseAllFolders, [this]() { _treeView.foldAll(); });
	addTool(IDI_FB_EXPANDALL, IDI_FB_EXPANDALL_DM, _expandAllFolders, [this]() { _treeView.expandAll(); });

	FileBrowser::initPopupMenus();

	std::vector<int> imgIds = _treeView.getImageIds(
		{ IDI_FB_ROOTOPEN, IDI_FB_ROOTCLOSE, IDI_PROJECT_FOLDEROPEN, IDI_PROJECT_FOLDERCLOSE, IDI_PROJECT_FILE }
		, { IDI_FB_ROOTOPEN_DM, IDI_FB_ROOTCLOSE_DM, IDI_PROJECT_FOLDEROPEN_DM, IDI_PROJECT_FOLDERCLOSE_DM, IDI_PROJECT_FILE_DM }
		, { IDI_FB_ROOTOPEN2, IDI_FB_ROOTCLOSE2, IDI_PROJECT_FOLDEROPEN2, IDI_PROJECT_FOLDERCLOSE2, IDI_PROJECT_FILE2 }
	);

	_treeView.init(_hInst, _hSelf, ID_FILEBROWSERTREEVIEW);
	_treeView.setImageList(imgIds);
	_treeView.makeLabelEditable(false);

	auto* layout = new QVBoxLayout(_hSelf);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	layout->addLayout(tools);
	layout->addWidget(_treeView.getHSelf(), 1);

	_treeView.display();

	connectTree();
}

void FileBrowser::connectTree()
{
	QTreeWidget* tree = _treeView.tree();

	QObject::connect(tree, &QTreeWidget::itemDoubleClicked, _hSelf, [this]() { openSelectFile(); });
	QObject::connect(tree, &QTreeWidget::itemExpanded, _hSelf, [this](QTreeWidgetItem* item) { onItemFolded(item, true); });
	QObject::connect(tree, &QTreeWidget::itemCollapsed, _hSelf, [this](QTreeWidgetItem* item) { onItemFolded(item, false); });
	QObject::connect(tree, &QWidget::customContextMenuRequested, _hSelf, [this](const QPoint& position) { showContextMenu(position); });

	// The item's full path as a tip, made when the mouse first reaches it
	tree->setMouseTracking(true);
	QObject::connect(tree, &QTreeWidget::itemEntered, _hSelf, [this](QTreeWidgetItem* item)
	{
		if (item->toolTip(0).isEmpty() && getNodeType(item) != browserNodeType_folder)
			item->setToolTip(0, QString::fromStdWString(getNodePath(item)));
	});

	const auto addKey = [this, tree](int key, auto handler)
	{
		auto* shortcut = new QShortcut(QKeySequence(key), tree);
		shortcut->setContext(Qt::WidgetShortcut);
		QObject::connect(shortcut, &QShortcut::activated, _hSelf, handler);
	};
	const auto onReturn = [this]()
	{
		HTREEITEM hItem = _treeView.getSelection();
		if (getNodeType(hItem) == browserNodeType_file)
			openSelectFile();
		else
			_treeView.toggleExpandCollapse(hItem);
	};
	addKey(Qt::Key_Return, onReturn);
	addKey(Qt::Key_Enter, onReturn);
	addKey(Qt::Key_Delete, [this]()
	{
		if (getNodeType(_treeView.getSelection()) == browserNodeType_root)
			popupMenuCmd(IDM_FILEBROWSER_REMOVEROOTFOLDER);
	});
}

void FileBrowser::onItemFolded(HTREEITEM item, bool isExpanded)
{
	const BrowserNodeType type = getNodeType(item);
	if (type == browserNodeType_folder)
	{
		const int image = isExpanded ? INDEX_OPEN_NODE : INDEX_CLOSE_NODE;
		_treeView.setItemImage(item, image, image);
	}
	else if (type == browserNodeType_root)
	{
		const int image = isExpanded ? INDEX_OPEN_ROOT : INDEX_CLOSE_ROOT;
		_treeView.setItemImage(item, image, image);
	}
}

void FileBrowser::onDockingNotify(const NMHDR& notification)
{
	if (notification.code == DMN_CLOSE)
		::SendMessage(_hParent, WM_COMMAND, IDM_VIEW_FILEBROWSER, 0);
	else
		DockingDlgInterface::onDockingNotify(notification);
}

void FileBrowser::initPopupMenus()
{
	NativeLangSpeaker* pNativeSpeaker = NppParameters::getInstance().getNativeLangSpeaker();

	wstring addRoot = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_ADDROOT, FB_ADDROOT);
	wstring removeAllRoot = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_REMOVEALLROOTS, FB_REMOVEALLROOTS);
	wstring removeRootFolder = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_REMOVEROOTFOLDER, FB_REMOVEROOTFOLDER);
	wstring copyPath = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_COPYPATH, FB_COPYPATH);
	wstring copyFileName = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_COPYFILENAME, FB_COPYFILENAME);
	wstring findInFile = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_FINDINFILES, FB_FINDINFILES);
	wstring explorerHere = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_EXPLORERHERE, FB_EXPLORERHERE);
	wstring cmdHere = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_CMDHERE, FB_CMDHERE);
	wstring powershellHere = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_POWERSHELLHERE, FB_POWERSHELLHERE);
	wstring openInNpp = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_OPENINNPP, FB_OPENINNPP);
	wstring shellExecute = pNativeSpeaker->getDlgLangMenuStr(FOLDERASWORKSPACE_NODE, nullptr, IDM_FILEBROWSER_SHELLEXECUTE, FB_SHELLEXECUTE);

	const bool hasPowerShell = !QStandardPaths::findExecutable(QStringLiteral("pwsh")).isEmpty();

	const auto makeMenu = [](std::unique_ptr<QMenu>& menu) { menu = std::make_unique<QMenu>(); };
	const auto addItem = [this](const std::unique_ptr<QMenu>& menu, int cmdID, const wstring& text)
	{
		QAction* action = menu->addAction(QString::fromStdWString(text));
		QObject::connect(action, &QAction::triggered, _hSelf, [this, cmdID]() { popupMenuCmd(cmdID); });
	};
	const auto addShellItems = [&](const std::unique_ptr<QMenu>& menu)
	{
		addItem(menu, IDM_FILEBROWSER_EXPLORERHERE, explorerHere);
		addItem(menu, IDM_FILEBROWSER_CMDHERE, cmdHere);
		if (hasPowerShell)
			addItem(menu, IDM_FILEBROWSER_POWERSHELLHERE, powershellHere);
	};

	makeMenu(_hGlobalMenu);
	addItem(_hGlobalMenu, IDM_FILEBROWSER_ADDROOT, addRoot);
	addItem(_hGlobalMenu, IDM_FILEBROWSER_REMOVEALLROOTS, removeAllRoot);

	makeMenu(_hRootMenu);
	addItem(_hRootMenu, IDM_FILEBROWSER_REMOVEROOTFOLDER, removeRootFolder);
	_hRootMenu->addSeparator();
	addItem(_hRootMenu, IDM_FILEBROWSER_COPYPATH, copyPath);
	addItem(_hRootMenu, IDM_FILEBROWSER_FINDINFILES, findInFile);
	_hRootMenu->addSeparator();
	addShellItems(_hRootMenu);

	makeMenu(_hFolderMenu);
	addItem(_hFolderMenu, IDM_FILEBROWSER_COPYPATH, copyPath);
	addItem(_hFolderMenu, IDM_FILEBROWSER_FINDINFILES, findInFile);
	_hFolderMenu->addSeparator();
	addShellItems(_hFolderMenu);

	makeMenu(_hFileMenu);
	addItem(_hFileMenu, IDM_FILEBROWSER_OPENINNPP, openInNpp);
	_hFileMenu->addSeparator();
	addItem(_hFileMenu, IDM_FILEBROWSER_COPYPATH, copyPath);
	addItem(_hFileMenu, IDM_FILEBROWSER_COPYFILENAME, copyFileName);
	addItem(_hFileMenu, IDM_FILEBROWSER_SHELLEXECUTE, shellExecute);
	_hFileMenu->addSeparator();
	addShellItems(_hFileMenu);
}

bool FileBrowser::selectItemFromPath(const wstring& itemPath) const
{
	if (itemPath.empty())
		return false;

	size_t itemPathLen = itemPath.size();

	for (const auto f : _folderUpdaters)
	{
		if (isRelatedRootFolder(f->_rootFolder._rootPath, itemPath))
		{
			wstring rootPath = f->_rootFolder._rootPath;
			size_t rootPathLen = rootPath.size();
			if (rootPathLen > itemPathLen) // It should never happen
				return false;

			vector<wstring> linarPathArray;
			if (rootPathLen == itemPathLen)
			{
				// Do nothing and use empty linarPathArray
			}
			else
			{
				wstring pathSuffix = itemPath.substr(rootPathLen + 1, itemPathLen - rootPathLen);
				linarPathArray = split(pathSuffix, nppPathSep);
			}
			HTREEITEM foundItem = findInTree(rootPath, nullptr, linarPathArray);

			if (foundItem)
			{
				_treeView.selectItem(foundItem);
				_treeView.grabFocus();
				return true;
			}
		}
	}
	return false;
}

bool FileBrowser::selectCurrentEditingFile() const
{
	wchar_t currentDocPath[MAX_PATH] = { '\0' };
	::SendMessage(_hParent, NPPM_GETFULLCURRENTPATH, MAX_PATH, reinterpret_cast<LPARAM>(currentDocPath));
	wstring currentDocPathStr = currentDocPath;

	return selectItemFromPath(currentDocPathStr);
}

wstring FileBrowser::getNodePath(HTREEITEM node) const
{
	if (!node) return L"";

	vector<wstring> fullPathArray;
	wstring fullPath;

	// go up until to root, then get the full path
	HTREEITEM parent = node;
	for (; parent != nullptr;)
	{
		wstring folderName = _treeView.getItemDisplayName(parent);
	
		HTREEITEM temp = _treeView.getParent(parent);
		if (temp == nullptr)
		{
			const SortingData4lParam* customData = reinterpret_cast<SortingData4lParam*>(_treeView.getItemParam(parent));
			folderName = customData->_rootPath;
		}
		parent = temp;
		fullPathArray.push_back(folderName);
	}


	for (int i = int(fullPathArray.size()) - 1; i >= 0; --i)
	{
		fullPath += fullPathArray[i];
		if (i != 0)
			fullPath += nppPathSepStr;
	}

	return fullPath;
}

wstring FileBrowser::getNodeName(HTREEITEM node) const
{
	return node ? _treeView.getItemDisplayName(node) : L"";
}

void FileBrowser::openSelectFile()
{
	// Get the selected item
	HTREEITEM selectedNode = _treeView.getSelection();
	if (!selectedNode) return;

	_selectedNodeFullPath = getNodePath(selectedNode);

	// test the path - if it's a file, open it, otherwise just fold or unfold it
	if (!doesFileExist(_selectedNodeFullPath.c_str()))
		return;

	::PostMessage(_hParent, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(_selectedNodeFullPath.c_str()));
}


BrowserNodeType FileBrowser::getNodeType(HTREEITEM hItem)
{
	// File
	if (_treeView.getItemImage(hItem) == INDEX_LEAF)
	{
		return browserNodeType_file;
	}

	// Root
	const auto* customData = reinterpret_cast<SortingData4lParam*>(_treeView.getItemParam(hItem));
	if (customData && !customData->_rootPath.empty())
	{
		return browserNodeType_root;
	}

	// Folder
	return browserNodeType_folder;
}

void FileBrowser::showContextMenu(const QPoint& position)
{
	QTreeWidget* tree = _treeView.tree();
	const QPoint global = tree->viewport()->mapToGlobal(position);

	// Detect if the given position is on an item
	HTREEITEM item = tree->itemAt(position);
	if (item == nullptr)
	{
		_hGlobalMenu->exec(global);
	}
	else
	{
		// Make item selected
		_treeView.selectItem(item);

		// get clicked item type
		BrowserNodeType nodeType = getNodeType(item);
		if (nodeType == browserNodeType_root)
			_hRootMenu->exec(global);
		else if (nodeType == browserNodeType_folder)
			_hFolderMenu->exec(global);
		else //nodeType_file
			_hFileMenu->exec(global);
	}
}

void FileBrowser::popupMenuCmd(int cmdID)
{
	// get selected item handle
	HTREEITEM selectedNode = _treeView.getSelection();

	switch (cmdID)
	{
		//
		// FileBrowser menu commands
		//
		case IDM_FILEBROWSER_REMOVEROOTFOLDER:
		{
			if (!selectedNode) return;

			const auto* customData = reinterpret_cast<SortingData4lParam*>(_treeView.getItemParam(selectedNode));
			if (_treeView.getParent(selectedNode) != nullptr || customData == nullptr)
				return;

			const std::wstring rootPath = customData->_rootPath;
			size_t nbFolderUpdaters = _folderUpdaters.size();
			for (size_t i = 0; i < nbFolderUpdaters; ++i)
			{
				if (_folderUpdaters[i]->_rootFolder._rootPath == rootPath)
				{
					_folderUpdaters[i]->stopWatcher();
					delete _folderUpdaters[i];
					_folderUpdaters.erase(_folderUpdaters.begin() + i);
					_treeView.removeItem(selectedNode);
					break;
				}
			}
		}
		break;
		
		case IDM_FILEBROWSER_EXPLORERHERE:
		{
			if (!selectedNode) return;
			wstring selPath = getNodePath(selectedNode);
			if (doesPathExist(selPath.c_str()))
			{
				namespace fs = std::filesystem;
				if (fs::is_directory(selPath))
				{
					// Just open the folder without selecting anything
					openWithDesktop(selPath);
				}
				else if (openInExplorerAndSelect(selPath.c_str()) != 0)
				{
					// Fallback: open parent folder
					openWithDesktop(fs::path(selPath).parent_path().wstring());
				}
			}
		}
		break;

		case IDM_FILEBROWSER_CMDHERE:
		case IDM_FILEBROWSER_POWERSHELLHERE:
		{
			if (!selectedNode) return;

			if (getNodeType(selectedNode) == browserNodeType_file)
				selectedNode = _treeView.getParent(selectedNode);

			wstring path = getNodePath(selectedNode);
			if (doesPathExist(path.c_str()))
			{
				if (cmdID == IDM_FILEBROWSER_CMDHERE)
				{
					Command cmd(findTerminal());
					cmd.run(_hParent, path.c_str());
				}
				else
				{
					Command powerShell(L"pwsh");
					powerShell.run(_hParent, path.c_str());
				}
			}
		}
		break;

		case IDM_FILEBROWSER_COPYPATH:
		{
			if (!selectedNode) return;
			wstring path = getNodePath(selectedNode);
			str2Clipboard(path, _hParent);
		}
		break;

		case IDM_FILEBROWSER_COPYFILENAME:
		{
			if (!selectedNode) return;
			wstring fileName = getNodeName(selectedNode);
			str2Clipboard(fileName, _hParent);
		}
		break;

		case IDM_FILEBROWSER_FINDINFILES:
		{
			if (!selectedNode) return;
			wstring path = getNodePath(selectedNode);
			::SendMessage(_hParent, NPPM_LAUNCHFINDINFILESDLG, reinterpret_cast<WPARAM>(path.c_str()), 0);
		}
		break;

		case IDM_FILEBROWSER_OPENINNPP:
		{
			openSelectFile();
		}
		break;

		case IDM_FILEBROWSER_REMOVEALLROOTS:
		{
			for (int i = static_cast<int>(_folderUpdaters.size()) - 1; i >= 0; --i)
			{
				_folderUpdaters[i]->stopWatcher();
				const std::unique_ptr<FolderUpdater> updater(_folderUpdaters[i]);

				HTREEITEM root =  getRootFromFullPath(_folderUpdaters[i]->_rootFolder._rootPath);
				if (root)
					_treeView.removeItem(root);

				_folderUpdaters.erase(_folderUpdaters.begin() + i);
			}
		}
		break;

		case IDM_FILEBROWSER_ADDROOT:
		{
			NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
			wstring openWorkspaceStr = pNativeSpeaker->getAttrNameStr(L"Select a folder to add in Folder as Workspace panel", FOLDERASWORKSPACE_NODE, "SelectFolderFromBrowserString");
			wstring folderPath = folderBrowser(_hParent, openWorkspaceStr);
			if (!folderPath.empty())
			{
				addRootFolder(folderPath);
			}
		}
		break;

		case IDM_FILEBROWSER_SHELLEXECUTE:
		{
			if (!selectedNode) return;
			const wstring path = getNodePath(selectedNode);
			if (doesPathExist(path.c_str()))
				openWithDesktop(path);
		}
		break;
	}
}



void FileBrowser::getDirectoryStructure(const wchar_t *dir, const std::vector<wstring> & patterns, FolderInfo & directoryStructure, bool isRecursive, bool isInHiddenDir)
{
	if (directoryStructure._parent == nullptr) // Root!
		directoryStructure.setRootPath(dir);

	const bool isSymlinkAllowed = NppParameters::getInstance().getNppGUI()._isFawSymlinkAllowed;
	const QFileInfoList entries = QDir(QString::fromWCharArray(dir)).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
	for (const QFileInfo& entry : entries)
	{
		if (entry.isSymLink() && !isSymlinkAllowed) // Ignore symbolic links to prevent infinite recursive loop, if not allowed
			continue;

		const wstring name = entry.fileName().toStdWString();
		if (entry.isDir())
		{
			if (!isInHiddenDir && entry.isHidden())
			{
				// do nothing
			}
			else if (isRecursive)
			{
				// A link back to a parent folder would recurse for ever
				const QString parentPath = QDir(QString::fromWCharArray(dir)).canonicalPath() + QLatin1Char('/');
				if (entry.isSymLink() && parentPath.startsWith(entry.canonicalFilePath() + QLatin1Char('/')))
					continue;

				FolderInfo subDirectoryStructure(name, &directoryStructure);
				getDirectoryStructure(entry.absoluteFilePath().toStdWString().c_str(), patterns, subDirectoryStructure, isRecursive, isInHiddenDir);
				directoryStructure.addSubFolder(subDirectoryStructure);
			}
		}
		else
		{
			if (matchInList(name.c_str(), patterns))
			{
				directoryStructure.addFile(name);
			}
		}
	}
}

void FileBrowser::addRootFolder(wstring rootFolderPath, std::unordered_set<std::wstring>* pExpandedPaths)
{
	if (!doesDirectoryExist(rootFolderPath.c_str()))
		return;

	// make sure there's no separator at the end
	if (rootFolderPath.length() > 1 && rootFolderPath.back() == nppPathSep)
	{
		rootFolderPath.pop_back();
	}

	for (const auto f : _folderUpdaters)
	{
		if (f->_rootFolder._rootPath == rootFolderPath)
			return;
		else
		{
			if (isRelatedRootFolder(f->_rootFolder._rootPath, rootFolderPath))
			{
				//do nothing, go down to select the dir
				wstring rootPath = f->_rootFolder._rootPath;
				wstring pathSuffix = rootFolderPath.substr(rootPath.size() + 1, rootFolderPath.size() - rootPath.size());
				vector<wstring> linarPathArray = split(pathSuffix, nppPathSep);
				
				HTREEITEM foundItem = findInTree(rootPath, nullptr, linarPathArray);
				if (foundItem)
					_treeView.selectItem(foundItem);
				return;
			}
			
			if (isRelatedRootFolder(rootFolderPath, f->_rootFolder._rootPath))
			{
				NppParameters::getInstance().getNativeLangSpeaker()->messageBox("FolderAsWorspaceSubfolderExists",
					_hParent,
					L"A sub-folder of the folder you want to add exists.\rPlease remove its root from the panel before you add folder \"$STR_REPLACE$\".",
					L"Folder as Workspace adding folder problem",
					MB_OK,
					0, // not used
					rootFolderPath.c_str());
				return;
			}
		}
	}

	std::vector<wstring> patterns2Match;
	patterns2Match.push_back(L"*.*");

	const wstring rootLabel = (rootFolderPath == nppPathSepStr) ? rootFolderPath : wstring(PathFindFileName(rootFolderPath.c_str()));

	FolderInfo directoryStructure(rootLabel, nullptr);
	getDirectoryStructure(rootFolderPath.c_str(), patterns2Match, directoryStructure, true, false);
	HTREEITEM hRootItem = createFolderItemsFromDirStruct(nullptr, directoryStructure);
	_treeView.customSorting(hRootItem, categorySortFunc, 0, true); // needed here for possible *nix like storages (Samba, WebDAV, WSL, ...)

	if (pExpandedPaths)
	{
		applyExpandState(hRootItem, pExpandedPaths);
	}
	/*
	else
	{
		//_treeView.fold(hRootItem);
		// Nothing to do, the root is already folded by default
	}
	*/

	_folderUpdaters.push_back(new FolderUpdater(directoryStructure, this));
	_folderUpdaters[_folderUpdaters.size() - 1]->startWatcher();
}

HTREEITEM FileBrowser::createFolderItemsFromDirStruct(HTREEITEM hParentItem, const FolderInfo & directoryStructure)
{
	HTREEITEM hFolderItem = nullptr;
	if (directoryStructure._parent == nullptr && hParentItem == nullptr)
	{
		SortingData4lParam* customData = new SortingData4lParam(directoryStructure._rootPath, L"", true);
		_sortingDataArray.push_back(customData);

		hFolderItem = _treeView.addItem(directoryStructure._name.c_str(), nullptr, INDEX_CLOSE_ROOT, reinterpret_cast<LPARAM>(customData));
	}
	else
	{
		SortingData4lParam* customData = new SortingData4lParam(L"", directoryStructure._name, true);
		_sortingDataArray.push_back(customData);

		hFolderItem = _treeView.addItem(directoryStructure._name.c_str(), hParentItem, INDEX_CLOSE_NODE, reinterpret_cast<LPARAM>(customData));
	}

	for (const auto& folder : directoryStructure._subFolders)
	{
		createFolderItemsFromDirStruct(hFolderItem, folder);
	}

	for (const auto& file : directoryStructure._files)
	{
		SortingData4lParam* customData = new SortingData4lParam(L"", file._name, false);
		_sortingDataArray.push_back(customData);

		_treeView.addItem(file._name.c_str(), hFolderItem, INDEX_LEAF, reinterpret_cast<LPARAM>(customData));
	}

	return hFolderItem;
}

HTREEITEM FileBrowser::getRootFromFullPath(const wstring & rootPath) const
{
	HTREEITEM node = nullptr;
	for (HTREEITEM hItemNode = _treeView.getRoot();
		hItemNode != nullptr && node == nullptr;
		hItemNode = _treeView.getNextSibling(hItemNode))
	{
		const LPARAM param = _treeView.getItemParam(hItemNode);
		if (param != 0 && rootPath == reinterpret_cast<SortingData4lParam *>(param)->_rootPath)
			node = hItemNode;
	}
	return node;
}

HTREEITEM FileBrowser::findChildNodeFromName(HTREEITEM parent, const wstring& label) const
{
	for (HTREEITEM hItemNode = _treeView.getChildFrom(parent);
		hItemNode != NULL;
		hItemNode = _treeView.getNextSibling(hItemNode))
	{
		const wstring itemText = _treeView.getItemDisplayName(hItemNode);

		if (label == itemText)
		{
			return hItemNode;
		}
	}
	return nullptr;
}

vector<wstring> FileBrowser::getRoots() const
{
	vector<wstring> roots;

	for (HTREEITEM hItemNode = _treeView.getRoot();
		hItemNode != nullptr;
		hItemNode = _treeView.getNextSibling(hItemNode))
	{
		roots.push_back((reinterpret_cast<SortingData4lParam*>(_treeView.getItemParam(hItemNode)))->_rootPath);
	}
	return roots;
}

wstring FileBrowser::getSelectedItemPath() const
{
	wstring itemPath;
	HTREEITEM hItemNode = _treeView.getSelection();
	if (hItemNode)
	{
		itemPath = getNodePath(hItemNode);
	}
	return itemPath;
}

bool FileBrowser::addToTree(FilesToChange & group, HTREEITEM node)
{
	if (node == nullptr) // it's a root. Search the right root with rootPath
	{
		// Search
		if ((node = getRootFromFullPath(group._rootPath)) == nullptr)
			return false;
	}

	if (group._linarWithoutLastPathElement.size() == 0)
	{
		// Items to add should exist on the disk
		group._files.erase(std::remove_if(group._files.begin(), group._files.end(), 
			[&group](const auto & file)
			{
				return !doesPathExist((group._commonPath + file).c_str());
			}),
			group._files.end());

		if (group._files.empty()) 
		{
			return false;
		}

		// Search: if not found, add
		removeNamesAlreadyInNode(node, group._files);
		if (group._files.empty()) 
		{
			return false;
		}

		// Not found, good - Action
		for (auto & file : group._files) {
			if (doesDirectoryExist((group._commonPath + file).c_str()))
			{
				SortingData4lParam* customData = new SortingData4lParam(L"", file, true);
				_sortingDataArray.push_back(customData);

				_treeView.addItem(file.c_str(), node, INDEX_CLOSE_NODE, reinterpret_cast<LPARAM>(customData));
			}
			else
			{
				SortingData4lParam* customData = new SortingData4lParam(L"", file, false);
				_sortingDataArray.push_back(customData);

				_treeView.addItem(file.c_str(), node, INDEX_LEAF, reinterpret_cast<LPARAM>(customData));
			}
		}
		_treeView.customSorting(node, categorySortFunc, 0, false);
		return true;
	}
	else
	{
		for (HTREEITEM hItemNode = _treeView.getChildFrom(node);
			hItemNode != NULL;
			hItemNode = _treeView.getNextSibling(hItemNode))
		{
			const wstring itemText = _treeView.getItemDisplayName(hItemNode);

			if (group._linarWithoutLastPathElement[0] == itemText)
			{
				// search recursively the node for an action
				group._linarWithoutLastPathElement.erase(group._linarWithoutLastPathElement.begin());
				return addToTree(group, hItemNode);
			}
		}
		return false;
	}

}

bool FileBrowser::deleteFromTree(FilesToChange & group)
{
	std::vector<HTREEITEM> foundItems = findInTree(group, nullptr);

	if (foundItems.empty() == true)
	{
		return false;
	}

	for (auto & item : foundItems)
	{
		_treeView.removeItem(item);
	}

	return true;
}

HTREEITEM FileBrowser::findInTree(const wstring& rootPath, HTREEITEM node, std::vector<wstring> linarPathArray) const
{
	if (node == nullptr) // it's a root. Search the right root with rootPath
	{
		// Search
		if ((node = getRootFromFullPath(rootPath)) == nullptr)
			return nullptr;
	}

	if (linarPathArray.empty()) // nothing to search, return node
	{
		return node;
	}
	else if (linarPathArray.size() == 1)
	{
		// Search
		return findChildNodeFromName(node, linarPathArray[0]);
	}
	else
	{
		for (HTREEITEM hItemNode = _treeView.getChildFrom(node);
			hItemNode != NULL;
			hItemNode = _treeView.getNextSibling(hItemNode))
		{
			const wstring itemText = _treeView.getItemDisplayName(hItemNode);

			if (linarPathArray[0] == itemText)
			{
				// search recursively the node for an action
				linarPathArray.erase(linarPathArray.begin());
				return findInTree(rootPath, hItemNode, linarPathArray);
			}
		}
		return nullptr;
	}
}

std::vector<HTREEITEM> FileBrowser::findInTree(FilesToChange & group, HTREEITEM node) const
{
	if (node == nullptr) // it's a root. Search the right root with rootPath
	{
		// Search
		if ((node = getRootFromFullPath(group._rootPath)) == nullptr)
		{
			return {};
		}
	}

	if (group._linarWithoutLastPathElement.empty())
	{
		// items to find should have existing corresponding files on disk (fixes vanishing or multiplying of FaW-panel items)
		group._files.erase(std::remove_if(group._files.begin(), group._files.end(),
			[&group](const auto& file)
			{
				return doesPathExist((group._commonPath + file).c_str());
			}),
			group._files.end());

		if (group._files.empty())
		{
			return {};
		}

		// Search
		return findChildNodesFromNames(node, group._files);
	}
	else
	{
		for (HTREEITEM hItemNode = _treeView.getChildFrom(node);
			hItemNode != NULL;
			hItemNode = _treeView.getNextSibling(hItemNode))
		{
			const wstring itemText = _treeView.getItemDisplayName(hItemNode);

			if (group._linarWithoutLastPathElement[0] == itemText)
			{
				// search recursively the node for an action
				group._linarWithoutLastPathElement.erase(group._linarWithoutLastPathElement.begin());
				return findInTree(group, hItemNode);
			}
		}
		return {};
	}
}

std::vector<HTREEITEM> FileBrowser::findChildNodesFromNames(HTREEITEM parent, std::vector<wstring> & labels) const
{
	std::vector<HTREEITEM> itemNodes;

	for (HTREEITEM hItemNode = _treeView.getChildFrom(parent);
		hItemNode != NULL && !labels.empty();
		hItemNode = _treeView.getNextSibling(hItemNode)
		)
	{
		const wstring itemText = _treeView.getItemDisplayName(hItemNode);

		auto it = std::find(labels.begin(), labels.end(), itemText);
		if (it != labels.end())
		{
			labels.erase(it); // remove, as it was already found
			itemNodes.push_back(hItemNode);
		}
	}
	return itemNodes;
}

void FileBrowser::removeNamesAlreadyInNode(HTREEITEM parent, std::vector<wstring> & labels) const
{
	// We have to search for the labels in the child nodes of parent, and remove the ones that already exist
	for (HTREEITEM hItemNode = _treeView.getChildFrom(parent);
		hItemNode != NULL && !labels.empty();
		hItemNode = _treeView.getNextSibling(hItemNode)
		)
	{
		const wstring itemText = _treeView.getItemDisplayName(hItemNode);

		auto it = std::find(labels.begin(), labels.end(), itemText);
		if (it != labels.end())
		{
			labels.erase(it);
		}
	}
}

bool FileBrowser::renameInTree(const wstring& rootPath, HTREEITEM node, const std::vector<wstring>& linarPathArrayFrom, const wstring & renameTo)
{
	HTREEITEM foundItem = findInTree(rootPath, node, linarPathArrayFrom);
	if (foundItem == nullptr)
			return false;

	// found it, try to rename it
	if (!_treeView.renameItem(foundItem, renameTo.c_str()))
	{
		// renameItem just prevented multiplication of a FaW-panel item
		// (typical situation - renaming of a previously added short-lived "filepath.tmp" treeview item,
		// which had been created before as a part of a complex save-file-as-tmp-then-rename op...)

		std::filesystem::path path2Check = rootPath;
		for (const auto& part : linarPathArrayFrom)
		{
			path2Check /= part; // reconstruct back the split path of item being renamed
		}

		if (!doesPathExist(path2Check.wstring().c_str()))
		{
			_treeView.removeItem(foundItem); // remove non-existing orphan
			return false;
		}
	}

	_treeView.renameItem(foundItem, renameTo.c_str());
	SortingData4lParam* compareData = reinterpret_cast<SortingData4lParam*>(_treeView.getItemParam(foundItem));
	compareData->_label = renameTo;
	_treeView.customSorting(_treeView.getParent(foundItem), categorySortFunc, 0, false);

	return true;
}

std::vector<std::wstring> FileBrowser::getExpandedPathsFromFaW() const
{
	std::vector<std::wstring> expandedPaths;

	std::function<void(HTREEITEM)> collect = [&](HTREEITEM hItem)
	{
		for (; hItem != nullptr; hItem = _treeView.getNextSibling(hItem))
		{
			if (_treeView.isExpanded(hItem))
				expandedPaths.push_back(getNodePath(hItem));

			collect(_treeView.getChildFrom(hItem));
		}
	};

	collect(_treeView.getRoot());
	return expandedPaths;
}

void FileBrowser::applyExpandState(HTREEITEM rootHItem, std::unordered_set<std::wstring>* pExpandedPaths)
{
	if (!rootHItem || !pExpandedPaths || pExpandedPaths->empty()) return;

	std::wstring rootPath = getNodePath(rootHItem);
	if (rootPath.empty()) return;

	// Walk the tree and expand nodes that match the paths in pExpandedPaths.
	// pExpandedPaths is the list of expaned node under rootHItem, which is modified in place to remove paths that have been expanded,
	// so we can stop early the recursive walking if all paths are found.
	std::function<void(HTREEITEM)> walkAndExpand = [&](HTREEITEM hItem) {
		if (!hItem || pExpandedPaths->empty()) return;

		std::wstring path = getNodePath(hItem);
		auto it = pExpandedPaths->find(path);
		if (it != pExpandedPaths->end())
		{
			_treeView.expand(hItem);
			pExpandedPaths->erase(it);
		}

		if (pExpandedPaths->empty()) return;

		for (HTREEITEM hChild = _treeView.getChildFrom(hItem);
			hChild != nullptr;
			hChild = _treeView.getNextSibling(hChild))
		{
			walkAndExpand(hChild);
		}
	};

	walkAndExpand(rootHItem);
}

int FileBrowser::categorySortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM /*lParamSort*/)
{
	SortingData4lParam* item1 = reinterpret_cast<SortingData4lParam*>(lParam1);
	SortingData4lParam* item2 = reinterpret_cast<SortingData4lParam*>(lParam2);

	if (!item1 || !item2)
		return 0;

	if (item1->_isFolder && !item2->_isFolder)
		return -1;
	else if (!item1->_isFolder && item2->_isFolder)
		return 1;
	else
		return _wcsicmp(item1->_label.c_str(), item2->_label.c_str());
}

bool FolderInfo::addToStructure(wstring & fullpath, std::vector<wstring> linarPathArray)
{
	if (linarPathArray.size() == 1) // could be file or folder
	{
		fullpath += nppPathSepStr;
		fullpath += linarPathArray[0];
		if (doesDirectoryExist(fullpath.c_str()))
		{
			// search in folders, if found - no good
			for (const auto& folder : _subFolders)
			{
				if (linarPathArray[0] == folder.getName())
					return false; // Maybe already added?
			}
			_subFolders.push_back(FolderInfo(linarPathArray[0], this));
			return true;
		}
		else
		{
			// search in files, if found - no good
			for (const auto& file : _files)
			{
				if (linarPathArray[0] == file.getName())
					return false; // Maybe already added?
			}
			_files.push_back(FileInfo(linarPathArray[0]));
			return true;
		}	
	}
	else // folder
	{
		for (auto& folder : _subFolders)
		{
			if (folder.getName() == linarPathArray[0])
			{
				fullpath += nppPathSepStr;
				fullpath += linarPathArray[0];
				linarPathArray.erase(linarPathArray.begin());
				return folder.addToStructure(fullpath, linarPathArray);
			}
		}
		return false;
	}
}

bool FolderInfo::removeFromStructure(std::vector<wstring> linarPathArray)
{
	if (linarPathArray.size() == 1) // could be file or folder
	{
		for (size_t i = 0; i < _files.size(); ++i)
		{
			if (_files[i].getName() == linarPathArray[0])
			{
				// remove this file
				_files.erase(_files.begin() + i);
				return true;
			}
		}

		for (size_t i = 0; i < _subFolders.size(); ++i)
		{
			if (_subFolders[i].getName() == linarPathArray[0])
			{
				// remove this folder
				_subFolders.erase(_subFolders.begin() + i);
				return true;
			}
		}
	}
	else // folder
	{
		for (size_t i = 0; i < _subFolders.size(); ++i)
		{
			if (_subFolders[i].getName() == linarPathArray[0])
			{
				linarPathArray.erase(linarPathArray.begin());
				return _subFolders[i].removeFromStructure(linarPathArray);
			}
		}
	}
	return false;
}

bool FolderInfo::renameInStructure(std::vector<wstring> linarPathArrayFrom, std::vector<wstring> linarPathArrayTo)
{
	if (linarPathArrayFrom.size() == 1) // could be file or folder
	{
		for (auto& file : _files)
		{
			if (file.getName() == linarPathArrayFrom[0])
			{
				// rename this file
				file.setName(linarPathArrayTo[0]);
				return true;
			}
		}

		for (auto& folder : _subFolders)
		{
			if (folder.getName() == linarPathArrayFrom[0])
			{
				// rename this folder
				folder.setName(linarPathArrayTo[0]);
				return true;
			}
		}
		return false;
	}
	else // folder
	{
		for (auto& folder : _subFolders)
		{
			if (folder.getName() == linarPathArrayFrom[0])
			{
				linarPathArrayFrom.erase(linarPathArrayFrom.begin());
				linarPathArrayTo.erase(linarPathArrayTo.begin());
				return folder.renameInStructure(linarPathArrayFrom, linarPathArrayTo);
			}
		}
		return false;
	}
}

FolderUpdater::FolderUpdater(const FolderInfo& fi, FileBrowser *pFileBrowser) : _rootFolder(fi), _pFileBrowser(pFileBrowser) {}
FolderUpdater::~FolderUpdater() = default;

void FolderUpdater::startWatcher()
{
	_watcher = std::make_unique<QFileSystemWatcher>();
	QObject::connect(_watcher.get(), &QFileSystemWatcher::directoryChanged, _watcher.get(), [this](const QString& path) { onDirectoryChanged(path); });
	watchTree(QString::fromStdWString(_rootFolder._rootPath));
}

void FolderUpdater::stopWatcher()
{
	_watcher.reset();
}

void FolderUpdater::watchTree(const QString& path)
{
	_watcher->addPath(path);

	const QFileInfoList folders = QDir(path).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot); // without the hidden ones, as in the tree
	for (const QFileInfo& folder : folders)
	{
		if (!folder.isSymLink())
			watchTree(folder.absoluteFilePath());
	}
}

// What is in the folder now against what is in the tree. A rename shows as one removed and one added.
void FolderUpdater::onDirectoryChanged(const QString& path)
{
	const QDir root(QString::fromStdWString(_rootFolder._rootPath));
	const QString relative = root.relativeFilePath(path);

	std::vector<std::wstring> linarPathArray;
	if (relative != QStringLiteral("."))
	{
		const QStringList parts = relative.split(QDir::separator(), Qt::SkipEmptyParts);
		for (const QString& part : parts)
			linarPathArray.push_back(part.toStdWString());
	}

	const HTREEITEM node = _pFileBrowser->findInTree(_rootFolder._rootPath, nullptr, linarPathArray);
	if (!node)
		return;

	std::vector<std::wstring> onDisk;
	const QFileInfoList entries = QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
	for (const QFileInfo& entry : entries)
	{
		if (entry.isDir() && entry.isHidden())
			continue;
		if (entry.isSymLink() && !NppParameters::getInstance().getNppGUI()._isFawSymlinkAllowed)
			continue;
		onDisk.push_back(entry.fileName().toStdWString());
	}

	std::vector<std::wstring> removed;
	for (HTREEITEM child = _pFileBrowser->_treeView.getChildFrom(node); child; child = _pFileBrowser->_treeView.getNextSibling(child))
	{
		const std::wstring name = _pFileBrowser->_treeView.getItemDisplayName(child);
		if (std::find(onDisk.begin(), onDisk.end(), name) == onDisk.end())
			removed.push_back(name);
	}

	const std::wstring commonPath = QDir::toNativeSeparators(path).toStdWString() + nppPathSepStr;

	if (!removed.empty())
	{
		FileBrowser::FilesToChange group;
		group._commonPath = commonPath;
		group._rootPath = _rootFolder._rootPath;
		group._linarWithoutLastPathElement = linarPathArray;
		group._files = removed;
		_pFileBrowser->deleteFromTree(group);
	}

	FileBrowser::FilesToChange added;
	added._commonPath = commonPath;
	added._rootPath = _rootFolder._rootPath;
	added._linarWithoutLastPathElement = linarPathArray;
	added._files = onDisk;
	if (_pFileBrowser->addToTree(added, nullptr))
	{
		// A new folder comes with what it already holds
		for (const std::wstring& name : added._files)
		{
			const QString folder = QDir(path).filePath(QString::fromStdWString(name));
			if (QFileInfo(folder).isDir())
			{
				watchTree(folder);
				onDirectoryChanged(folder);
			}
		}
	}
}
