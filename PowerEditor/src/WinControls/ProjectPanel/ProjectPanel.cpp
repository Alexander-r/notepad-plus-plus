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


#include "ProjectPanel.h"

#include <QAbstractItemDelegate>
#include <QAction>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QShortcut>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Common.h"
#include "CustomFileDialog.h"
#include "DockingDlgInterface.h"
#include "Notepad_plus_msgs.h"
#include "NppXml.h"
#include "Parameters.h"
#include "ProjectPanel_rc.h"
#include "StaticDialog.h"
#include "dockingResource.h"
#include "localization.h"
#include "menuCmdID.h"
#include "resource.h"
#include "ui_FilerelocalizerDlg.h"

enum TvIndex : int
{
	INDEX_CLEAN_ROOT,
	INDEX_DIRTY_ROOT,
	INDEX_PROJECT,
	INDEX_OPEN_NODE,
	INDEX_CLOSED_NODE,
	INDEX_LEAF,
	INDEX_LEAF_INVALID,
};

using namespace std;

namespace
{
	// The folder of a file path, without the last separator
	wstring folderOf(const wstring& path)
	{
		const size_t found = path.rfind(nppPathSep);
		if (found == wstring::npos)
			return {};
		return found == 0 ? nppPathSepStr : path.substr(0, found);
	}
}

ProjectPanel::~ProjectPanel() = default;

void ProjectPanel::create(DockedWidgetData* data, bool isRTL)
{
	DockingDlgInterface::create(data, isRTL);

	ProjectPanel::initMenus();

	NppParameters& nppParam = NppParameters::getInstance();
	NativeLangSpeaker *pNativeSpeaker = nppParam.getNativeLangSpeaker();
	wstring workspace_entry = pNativeSpeaker->getProjectPanelLangMenuStr("Entries", 0, PM_WORKSPACEMENUENTRY);
	wstring edit_entry = pNativeSpeaker->getProjectPanelLangMenuStr("Entries", 1, PM_EDITMENUENTRY);

	// Create toolbar menu
	const auto addButton = [this](const wstring& text, int cmdID)
	{
		auto* button = new QToolButton(_hSelf);
		button->setAutoRaise(true);
		button->setText(QString::fromStdWString(text));
		QObject::connect(button, &QToolButton::clicked, _hSelf, [this, cmdID]() { popupMenuCmd(cmdID); });
		return button;
	};
	_workspaceButton = addButton(workspace_entry, IDB_PROJECT_BTN);
	_editButton = addButton(edit_entry, IDB_EDIT_BTN);

	auto* tools = new QHBoxLayout;
	tools->setContentsMargins(0, 0, 0, 0);
	tools->setSpacing(0);
	tools->addWidget(_workspaceButton);
	tools->addWidget(_editButton);
	tools->addStretch(1);

	std::vector<int> imgIds = _treeView.getImageIds(
		{ IDI_PROJECT_WORKSPACE, IDI_PROJECT_WORKSPACEDIRTY, IDI_PROJECT_PROJECT, IDI_PROJECT_FOLDEROPEN, IDI_PROJECT_FOLDERCLOSE, IDI_PROJECT_FILE, IDI_PROJECT_FILEINVALID }
		, { IDI_PROJECT_WORKSPACE_DM, IDI_PROJECT_WORKSPACEDIRTY_DM, IDI_PROJECT_PROJECT_DM, IDI_PROJECT_FOLDEROPEN_DM, IDI_PROJECT_FOLDERCLOSE_DM, IDI_PROJECT_FILE_DM, IDI_PROJECT_FILEINVALID_DM }
		, { IDI_PROJECT_WORKSPACE2, IDI_PROJECT_WORKSPACEDIRTY2, IDI_PROJECT_PROJECT2, IDI_PROJECT_FOLDEROPEN2, IDI_PROJECT_FOLDERCLOSE2, IDI_PROJECT_FILE2, IDI_PROJECT_FILEINVALID2 }
	);

	_treeView.init(_hInst, _hSelf, ID_PROJECTTREEVIEW);
	_treeView.setImageList(imgIds);
	_treeView.makeLabelEditable(true);

	_treeView.addCanNotDropInList(INDEX_LEAF);
	_treeView.addCanNotDropInList(INDEX_LEAF_INVALID);

	_treeView.addCanNotDragOutList(INDEX_CLEAN_ROOT);
	_treeView.addCanNotDragOutList(INDEX_DIRTY_ROOT);
	_treeView.addCanNotDragOutList(INDEX_PROJECT);
	_treeView.enableDragAndDrop([this]() { setWorkSpaceDirty(true); });

	auto* layout = new QVBoxLayout(_hSelf);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	layout->addLayout(tools);
	layout->addWidget(_treeView.getHSelf(), 1);

	_treeView.display();
	connectTree();

	if (!openWorkSpace(_workSpaceFilePath.c_str(), true))
		newWorkSpace();
}

void ProjectPanel::connectTree()
{
	QTreeWidget* tree = _treeView.tree();

	QObject::connect(tree, &QTreeWidget::itemDoubleClicked, _hSelf, [this]() { openSelectFile(); });
	QObject::connect(tree, &QWidget::customContextMenuRequested, _hSelf, [this](const QPoint& position) { showContextMenu(position); });

	// This runs after the tree has stored the new label
	QObject::connect(tree->itemDelegate(), &QAbstractItemDelegate::commitData, _hSelf, [this]() { onLabelEdited(); });

	const auto onFolded = [this](QTreeWidgetItem* item, int image)
	{
		if (getNodeType(item) == nodeType_folder)
			_treeView.setItemImage(item, image, image);
	};
	QObject::connect(tree, &QTreeWidget::itemExpanded, _hSelf, [onFolded](QTreeWidgetItem* item) { onFolded(item, INDEX_OPEN_NODE); });
	QObject::connect(tree, &QTreeWidget::itemCollapsed, _hSelf, [onFolded](QTreeWidgetItem* item) { onFolded(item, INDEX_CLOSED_NODE); });

	// The workspace file or the file's path as a tip
	tree->setMouseTracking(true);
	QObject::connect(tree, &QTreeWidget::itemEntered, _hSelf, [this](QTreeWidgetItem* item)
	{
		if (_treeView.getRoot() == item)
			item->setToolTip(0, QString::fromStdWString(_workSpaceFilePath));
		else if (const auto* path = reinterpret_cast<std::wstring*>(_treeView.getItemParam(item)))
			item->setToolTip(0, QString::fromStdWString(*path));
	});

	const auto addKey = [this, tree](const QKeySequence& key, auto handler)
	{
		auto* shortcut = new QShortcut(key, tree);
		shortcut->setContext(Qt::WidgetShortcut);
		QObject::connect(shortcut, &QShortcut::activated, _hSelf, handler);
	};
	addKey(QKeySequence(Qt::Key_Delete), [this]()
	{
		NodeType nType = getNodeType(_treeView.getSelection());
		if (nType == nodeType_project || nType == nodeType_folder)
			popupMenuCmd(IDM_PROJECT_DELETEFOLDER);
		else if (nType == nodeType_file)
			popupMenuCmd(IDM_PROJECT_DELETEFILE);
	});
	const auto onReturn = [this]()
	{
		HTREEITEM hItem = _treeView.getSelection();
		if (getNodeType(hItem) == nodeType_file)
			openSelectFile();
		else
			_treeView.toggleExpandCollapse(hItem);
	};
	addKey(QKeySequence(Qt::Key_Return), onReturn);
	addKey(QKeySequence(Qt::Key_Enter), onReturn);
	addKey(QKeySequence(Qt::CTRL | Qt::Key_Up), [this]() { popupMenuCmd(IDM_PROJECT_MOVEUP); });
	addKey(QKeySequence(Qt::CTRL | Qt::Key_Down), [this]() { popupMenuCmd(IDM_PROJECT_MOVEDOWN); });
	addKey(QKeySequence(Qt::Key_F2), [this]() { popupMenuCmd(IDM_PROJECT_RENAME); });
}

void ProjectPanel::onLabelEdited()
{
	HTREEITEM item = _treeView.getSelection();
	if (!item || getNodeType(item) == nodeType_root)
		return;

	// Processing for only File case
	if (auto* filePath = reinterpret_cast<std::wstring*>(_treeView.getItemParam(item)))
	{
		// The label is the last element of the path
		const size_t found = filePath->rfind(nppPathSep);
		filePath->replace(found == wstring::npos ? 0 : found + 1, wstring::npos, _treeView.getItemDisplayName(item));

		// Check the validity of modified file path
		const int image = doesFileExist(filePath->c_str()) ? INDEX_LEAF : INDEX_LEAF_INVALID;
		_treeView.setItemImage(item, image, image);
	}

	// For File, Folder and Project
	setWorkSpaceDirty(true);
}

bool ProjectPanel::checkIfNeedSave()
{
	if (_isDirty)
	{
		const wchar_t * title = _workSpaceFilePath.length() > 0 ? PathFindFileName (_workSpaceFilePath.c_str()) : _panelTitle.c_str();
		NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
		int res = pNativeSpeaker->messageBox("ProjectPanelChanged",
			_hSelf,
			L"The workspace was modified. Do you want to save it?",
			L"$STR_REPLACE$",
			MB_YESNOCANCEL | MB_ICONQUESTION,
			0,
			title);

		if (res == IDYES)
		{
			if (!saveWorkSpace())
				return false;
		}
		else if (res == IDNO)
		{
			// Don't save so do nothing here
		}
		else
		{
			// Cancelled
			return false;
		}
	}
	return true;
}

void ProjectPanel::initMenus()
{
	const auto addItem = [this](const std::unique_ptr<QMenu>& menu, int cmdID, const wstring& text)
	{
		QAction* action = menu->addAction(QString::fromStdWString(text));
		QObject::connect(action, &QAction::triggered, _hSelf, [this, cmdID]() { popupMenuCmd(cmdID); });
	};

	_hWorkSpaceMenu = std::make_unique<QMenu>();
	
	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	wstring new_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_NEWWS, PM_NEWWORKSPACE);
	wstring open_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_OPENWS, PM_OPENWORKSPACE);
	wstring reload_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_RELOADWS, PM_RELOADWORKSPACE);
	wstring save_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_SAVEWS, PM_SAVEWORKSPACE);
	wstring saveas_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_SAVEASWS, PM_SAVEASWORKSPACE);
	wstring saveacopyas_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_SAVEACOPYASWS, PM_SAVEACOPYASWORKSPACE);
	wstring newproject_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_NEWPROJECT, PM_NEWPROJECTWORKSPACE);
	wstring findinprojects_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_FINDINPROJECTSWS, PM_FINDINFILESWORKSPACE);

	addItem(_hWorkSpaceMenu, IDM_PROJECT_NEWWS, new_workspace);
	addItem(_hWorkSpaceMenu, IDM_PROJECT_OPENWS, open_workspace);
	addItem(_hWorkSpaceMenu, IDM_PROJECT_RELOADWS, reload_workspace);
	addItem(_hWorkSpaceMenu, IDM_PROJECT_SAVEWS, save_workspace);
	addItem(_hWorkSpaceMenu, IDM_PROJECT_SAVEASWS, saveas_workspace);
	addItem(_hWorkSpaceMenu, IDM_PROJECT_SAVEACOPYASWS, saveacopyas_workspace);
	_hWorkSpaceMenu->addSeparator();
	addItem(_hWorkSpaceMenu, IDM_PROJECT_NEWPROJECT, newproject_workspace);
	_hWorkSpaceMenu->addSeparator();
	addItem(_hWorkSpaceMenu, IDM_PROJECT_FINDINPROJECTSWS, findinprojects_workspace);

	wstring edit_moveup = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_MOVEUP, PM_MOVEUPENTRY);
	wstring edit_movedown = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_MOVEDOWN, PM_MOVEDOWNENTRY);
	wstring edit_rename = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_RENAME, PM_EDITRENAME);
	wstring edit_addfolder = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_NEWFOLDER, PM_EDITNEWFOLDER);
	wstring edit_addfiles = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_ADDFILES, PM_EDITADDFILES);
	wstring edit_addfilesRecursive = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_ADDFILESRECUSIVELY, PM_EDITADDFILESRECUSIVELY);
	wstring edit_remove = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_DELETEFOLDER, PM_EDITREMOVE);

	_hProjectMenu = std::make_unique<QMenu>();
	addItem(_hProjectMenu, IDM_PROJECT_MOVEUP, edit_moveup);
	addItem(_hProjectMenu, IDM_PROJECT_MOVEDOWN, edit_movedown);
	_hProjectMenu->addSeparator();
	addItem(_hProjectMenu, IDM_PROJECT_RENAME, edit_rename);
	addItem(_hProjectMenu, IDM_PROJECT_NEWFOLDER, edit_addfolder);
	addItem(_hProjectMenu, IDM_PROJECT_ADDFILES, edit_addfiles);
	addItem(_hProjectMenu, IDM_PROJECT_ADDFILESRECUSIVELY, edit_addfilesRecursive);
	addItem(_hProjectMenu, IDM_PROJECT_DELETEFOLDER, edit_remove);

	edit_moveup = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_MOVEUP, PM_MOVEUPENTRY);
	edit_movedown = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_MOVEDOWN, PM_MOVEDOWNENTRY);
	edit_rename = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_RENAME, PM_EDITRENAME);
	edit_addfolder = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_NEWFOLDER, PM_EDITNEWFOLDER);
	edit_addfiles = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_ADDFILES, PM_EDITADDFILES);
	edit_addfilesRecursive = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_ADDFILESRECUSIVELY, PM_EDITADDFILESRECUSIVELY);
	edit_remove = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_DELETEFOLDER, PM_EDITREMOVE);

	_hFolderMenu = std::make_unique<QMenu>();
	addItem(_hFolderMenu, IDM_PROJECT_MOVEUP, edit_moveup);
	addItem(_hFolderMenu, IDM_PROJECT_MOVEDOWN, edit_movedown);
	_hFolderMenu->addSeparator();
	addItem(_hFolderMenu, IDM_PROJECT_RENAME, edit_rename);
	addItem(_hFolderMenu, IDM_PROJECT_NEWFOLDER, edit_addfolder);
	addItem(_hFolderMenu, IDM_PROJECT_ADDFILES, edit_addfiles);
	addItem(_hFolderMenu, IDM_PROJECT_ADDFILESRECUSIVELY, edit_addfilesRecursive);
	addItem(_hFolderMenu, IDM_PROJECT_DELETEFOLDER, edit_remove);

	edit_moveup = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_MOVEUP, PM_MOVEUPENTRY);
	edit_movedown = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_MOVEDOWN, PM_MOVEDOWNENTRY);
	edit_rename = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_RENAME, PM_EDITRENAME);
	edit_remove = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_DELETEFILE, PM_EDITREMOVE);
	wstring edit_modifyfile = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_MODIFYFILEPATH, PM_EDITMODIFYFILE);

	_hFileMenu = std::make_unique<QMenu>();
	addItem(_hFileMenu, IDM_PROJECT_MOVEUP, edit_moveup);
	addItem(_hFileMenu, IDM_PROJECT_MOVEDOWN, edit_movedown);
	_hFileMenu->addSeparator();
	addItem(_hFileMenu, IDM_PROJECT_RENAME, edit_rename);
	addItem(_hFileMenu, IDM_PROJECT_DELETEFILE, edit_remove);
	addItem(_hFileMenu, IDM_PROJECT_MODIFYFILEPATH, edit_modifyfile);
}

bool ProjectPanel::openWorkSpace(const wchar_t* projectFileName, bool force)
{
	if (!projectFileName)
		return false;

	if ((!force) && (_workSpaceFilePath.length() > 0))
	{ // Return if it is better to keep the current workspace tree
		wstring newWorkspace = projectFileName;
		if (newWorkspace == _workSpaceFilePath)
			return true;
		if (!saveWorkspaceRequest())
			return true;
	}

	if (!doesFileExist(projectFileName))
		return false;

	NppXml::NewDocument xmlDocProject{};
	const bool loadOkay = NppXml::loadFile(&xmlDocProject, projectFileName);
	if (!loadOkay)
		return false;

	NppXml::Element root = NppXml::firstChildElement(&xmlDocProject, "NotepadPlus");
	if (!root)
		return false;

	NppXml::Element childNode = NppXml::firstChildElement(root, "Project");
	if (!childNode)
		return false;

	_treeView.removeAllItems();
	_workSpaceFilePath = projectFileName;

	HTREEITEM rootItem = _treeView.addItem(PathFindFileName(projectFileName), nullptr, INDEX_CLEAN_ROOT);
	rootItem->setFlags(rootItem->flags() & ~Qt::ItemIsEditable);

	for (;
		childNode;
		childNode = NppXml::nextSiblingElement(childNode, "Project"))
	{
		const char* itemName = NppXml::attribute(childNode, "name");
		HTREEITEM projectItem = _treeView.addItem(string2wstring(itemName).c_str(), rootItem, INDEX_PROJECT);
		buildTreeFrom(childNode, projectItem);
	}
	setWorkSpaceDirty(false);
	_treeView.expand(rootItem);

	return true;
}

void ProjectPanel::newWorkSpace()
{
	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	wstring workspace = pNativeSpeaker->getAttrNameStr(PM_WORKSPACEROOTNAME, "ProjectManager", "WorkspaceRootName");
	HTREEITEM rootItem = _treeView.addItem(workspace.c_str(), nullptr, INDEX_CLEAN_ROOT);
	rootItem->setFlags(rootItem->flags() & ~Qt::ItemIsEditable);
	setWorkSpaceDirty(false);
	_workSpaceFilePath = L"";
}

bool ProjectPanel::saveWorkSpace()
{
	if (_workSpaceFilePath == L"")
	{
		return saveWorkSpaceAs(false);
	}
	else
	{
		if (!writeWorkSpace())
			return false;

		setWorkSpaceDirty(false);
		return true;
	} 
}

bool ProjectPanel::writeWorkSpace(const wchar_t* projectFileName, bool doUpdateGUI)
{
	//write <NotepadPlus>: use the default file name if new file name is not given
	const wchar_t* fn2write = projectFileName ? projectFileName : _workSpaceFilePath.c_str();
	NppXml::NewDocument projDoc{};
	NppXml::Element root = NppXml::createChildElement(projDoc, "NotepadPlus");

	//for each project, write <Project>
	HTREEITEM tvRoot = _treeView.getRoot();
	if (!tvRoot)
		return false;

	for (HTREEITEM tvProj = _treeView.getChildFrom(tvRoot);
		tvProj != nullptr;
		tvProj = _treeView.getNextSibling(tvProj))
	{
		NppXml::Element projRoot = NppXml::createChildElement(root, "Project");
		NppXml::setAttribute(projRoot, "name", wstring2string(_treeView.getItemDisplayName(tvProj)));
		buildProjectXml(projRoot, tvProj, fn2write);
	}

	if (!NppXml::saveFileProject(&projDoc, fn2write))
	{
		const wchar_t* title = _workSpaceFilePath.length() > 0 ? PathFindFileName (_workSpaceFilePath.c_str()) : _panelTitle.c_str();
		NativeLangSpeaker *pNativeSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
		pNativeSpeaker->messageBox("ProjectPanelSaveError",
		_hSelf,
		L"An error occurred while writing your workspace file.\nYour workspace has not been saved.",
		L"$STR_REPLACE$",
		MB_OK | MB_ICONERROR,
		0,
		title);

		return false;
	}
	if (doUpdateGUI)
	{
		_treeView.renameItem(tvRoot, PathFindFileName(fn2write));
	}

	return true;
}

void ProjectPanel::buildProjectXml(NppXml::Element& root, HTREEITEM hItem, const wchar_t* fn2write)
{
	for (HTREEITEM hItemNode = _treeView.getChildFrom(hItem);
		hItemNode != nullptr;
		hItemNode = _treeView.getNextSibling(hItemNode))
	{
		const LPARAM param = _treeView.getItemParam(hItemNode);
		if (param)
		{
			const auto* const fn = reinterpret_cast<std::wstring*>(param);
			std::string newFn = wstring2string(getRelativePath(*fn, fn2write));
			NppXml::Element fileLeaf = NppXml::createChildElement(root, "File");
			NppXml::setAttribute(fileLeaf, "name", newFn);
		}
		else
		{
			NppXml::Element folderNode = NppXml::createChildElement(root, "Folder");
			NppXml::setAttribute(folderNode, "name", wstring2string(_treeView.getItemDisplayName(hItemNode)));
			buildProjectXml(folderNode, hItemNode, fn2write);
		}
	}
}

bool ProjectPanel::enumWorkSpaceFiles(HTREEITEM tvFrom, const std::vector<wstring> & patterns, std::vector<wstring> & fileNames)
{
	HTREEITEM tvRoot = tvFrom ? tvFrom : _treeView.getRoot();
	if (!tvRoot) return false;

	for (HTREEITEM tvProj = _treeView.getChildFrom(tvRoot);
		tvProj != nullptr;
		tvProj = _treeView.getNextSibling(tvProj))
	{
		const LPARAM param = _treeView.getItemParam(tvProj);
		if (param)
		{
			if (matchInList(_treeView.getItemDisplayName(tvProj).c_str(), patterns))
			{
				auto* const fn = reinterpret_cast<std::wstring*>(param);
				fileNames.push_back(*fn);
			}
		}
		else
		{
			if (!enumWorkSpaceFiles (tvProj, patterns, fileNames)) return false;
		}
	}
	return true;
}

wstring ProjectPanel::getRelativePath(const wstring& filePath, const wchar_t *workSpaceFileName)
{
	// Only what is really inside the workspace's folder is relative. The whole of the folder's name has to match.
	wstring folder = folderOf(workSpaceFileName);
	if (folder.empty())
		return filePath;
	if (folder.back() != nppPathSep)
		folder += nppPathSep;
	if (filePath.compare(0, folder.length(), folder) != 0)
		return filePath;
	return filePath.substr(folder.length());
}

bool ProjectPanel::buildTreeFrom(const NppXml::Element& projectRoot, HTREEITEM hParentItem)
{
	for (NppXml::Element childNode = NppXml::firstChildElement(projectRoot);
		childNode;
		childNode = NppXml::nextSibling(childNode))
	{
		const char* nodeName = NppXml::getName(childNode);
		if (std::strcmp(nodeName, "Folder") == 0)
		{
			const char* itemName = NppXml::attribute(childNode, "name");
			HTREEITEM addedItem = _treeView.addItem(string2wstring(itemName).c_str(), hParentItem, INDEX_CLOSED_NODE);
			if (NppXml::firstChild(childNode))
			{
				bool isOK = buildTreeFrom(childNode, addedItem);
				if (!isOK)
					return false;
			}
		}
		else if (std::strcmp(nodeName, "File") == 0)
		{
			std::wstring strValue = string2wstring(NppXml::attribute(childNode, "name"));

			// A workspace written on Windows, and not a Windows absolute path
			if (strValue.find(L'/') == wstring::npos && (strValue.length() < 2 || strValue[1] != L':'))
				std::replace(strValue.begin(), strValue.end(), L'\\', nppPathSep);

			const std::wstring fullPath = getAbsoluteFilePath(strValue.c_str());

			const wchar_t* strValueLabel = PathFindFileName(strValue.c_str());
			int iImage = doesFileExist(fullPath.c_str()) ? INDEX_LEAF : INDEX_LEAF_INVALID;

			_fullPathStrs.push_back(std::make_unique<std::wstring>(fullPath));
			auto lParamFullPathStr = reinterpret_cast<LPARAM>(_fullPathStrs.back().get());

			_treeView.addItem(strValueLabel, hParentItem, iImage, lParamFullPathStr);
		}
	}
	return true;
}

wstring ProjectPanel::getAbsoluteFilePath(const wchar_t * relativePath)
{
	if (QFileInfo(QString::fromWCharArray(relativePath)).isAbsolute())
		return relativePath;

	wstring absolutePath = folderOf(_workSpaceFilePath);
	pathAppend(absolutePath, relativePath);
	return absolutePath;
}

void ProjectPanel::openSelectFile()
{
	HTREEITEM item = _treeView.getSelection();
	if (!item)
		return;

	const auto* const fn = reinterpret_cast<std::wstring*>(_treeView.getItemParam(item));
	if (getNodeType(item) == nodeType_file && fn)
	{
		int image = INDEX_LEAF_INVALID;
		if (doesFileExist(fn->c_str()))
		{
			::PostMessage(_hParent, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(fn->c_str()));
			image = INDEX_LEAF;
		}
		_treeView.setItemImage(item, image, image);
	}
}

void ProjectPanel::onDockingNotify(const NMHDR& notification)
{
	if (notification.code == DMN_CLOSE)
		::SendMessage(_hParent, WM_COMMAND, IDM_VIEW_PROJECT_PANEL_1 + _panelID, 0);
	else
		DockingDlgInterface::onDockingNotify(notification);
}

void ProjectPanel::setWorkSpaceDirty(bool isDirty)
{
	_isDirty = isDirty;
	int iImg = _isDirty?INDEX_DIRTY_ROOT:INDEX_CLEAN_ROOT;
	_treeView.setItemImage(_treeView.getRoot(), iImg, iImg);
}

NodeType ProjectPanel::getNodeType(HTREEITEM hItem)
{
	const int image = _treeView.getItemImage(hItem);

	// Root
	if (image == INDEX_CLEAN_ROOT || image == INDEX_DIRTY_ROOT)
	{
		return nodeType_root;
	}
	// Project
	else if (image == INDEX_PROJECT)
	{
		return nodeType_project;
	}
	// Folder
	else if (!_treeView.getItemParam(hItem))
	{
		return nodeType_folder;
	}
	// File
	else
	{
		return nodeType_file;
	}
}

void ProjectPanel::showContextMenu(const QPoint& position)
{
	// Detect if the given position is on an item
	QTreeWidget* tree = _treeView.tree();
	HTREEITEM item = tree->itemAt(position);
	if (item != nullptr)
	{
		// Make item selected
		_treeView.selectItem(item);
		getMenuHandler(item)->exec(tree->viewport()->mapToGlobal(position));
	}
}

QMenu* ProjectPanel::getMenuHandler(HTREEITEM selectedItem)
{
	// get clicked item type
	NodeType nodeType = getNodeType(selectedItem);

	if (nodeType == nodeType_root)
		return _hWorkSpaceMenu.get();
	else if (nodeType == nodeType_project)
		return _hProjectMenu.get();
	else if (nodeType == nodeType_folder)
		return _hFolderMenu.get();
	else //nodeType_file
		return _hFileMenu.get();
}

QPoint ProjectPanel::getMenuDisplayPoint(const QToolButton* button) const
{
	return button->mapToGlobal(button->rect().bottomLeft());
}

HTREEITEM ProjectPanel::addFolder(HTREEITEM hTreeItem, const wchar_t *folderName, bool toEditLabel)
{
	HTREEITEM addedItem = _treeView.addItem(folderName, hTreeItem, INDEX_CLOSED_NODE);

	_treeView.expand(hTreeItem);
	if (toEditLabel)
		_treeView.editLabel(addedItem);
	if (getNodeType(hTreeItem) == nodeType_folder)
		_treeView.setItemImage(hTreeItem, INDEX_OPEN_NODE, INDEX_OPEN_NODE);

	return addedItem;
}

bool ProjectPanel::saveWorkspaceRequest()
{ // returns true for continue and false for break
	if (_isDirty)
	{
		NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
		int res = pNativeSpeaker->messageBox("ProjectPanelOpenDoSaveDirtyWsOrNot",
					_hSelf,
					L"The current workspace was modified. Do you want to save the current project?",
					L"Open Workspace",
					MB_YESNOCANCEL | MB_ICONQUESTION | MB_APPLMODAL);
				
		if (res == IDYES)
		{
			if (!saveWorkSpace())
				return false;
		}
		else if (res == IDNO)
		{
			// Don't save so do nothing here
		}
		else if (res == IDCANCEL) 
		{
			// User cancels action "New Workspace" so we interrupt here
			return false;
		}
	}
	return true;
}

void ProjectPanel::popupMenuCmd(int cmdID)
{
	// get selected item handle
	HTREEITEM hTreeItem = _treeView.getSelection();
	if (!hTreeItem)
		return;

	switch (cmdID)
	{
		//
		// Toolbar menu buttons
		//
		case IDB_PROJECT_BTN:
		{
			_hWorkSpaceMenu->exec(getMenuDisplayPoint(_workspaceButton));
		}
		break;

		case IDB_EDIT_BTN:
		{
			QMenu* menu = nullptr;
			NodeType nodeType = getNodeType(hTreeItem);
			if (nodeType == nodeType_project)
				menu = _hProjectMenu.get();
			else if (nodeType == nodeType_folder)
				menu = _hFolderMenu.get();
			else if (nodeType == nodeType_file)
				menu = _hFileMenu.get();
			if (menu)
				menu->exec(getMenuDisplayPoint(_editButton));
		}
		break;

		//
		// Toolbar menu commands
		//
		case IDM_PROJECT_NEWPROJECT :
		{
			HTREEITEM root = _treeView.getRoot();

			NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
			wstring newProjectLabel = pNativeSpeaker->getAttrNameStr(PM_NEWPROJECTNAME, "ProjectManager", "NewProjectName");
			HTREEITEM addedItem = _treeView.addItem(newProjectLabel.c_str(),  root, INDEX_PROJECT);
			setWorkSpaceDirty(true);
			_treeView.expand(hTreeItem);
			_treeView.editLabel(addedItem);
		}
		break;

		case IDM_PROJECT_NEWWS :
		{
			if (_isDirty)
			{
				NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
				int res = pNativeSpeaker->messageBox("ProjectPanelNewDoSaveDirtyWsOrNot",
					_hSelf,
					L"The current workspace was modified. Do you want to save the current project?",
					L"New Workspace",
					MB_YESNOCANCEL | MB_ICONQUESTION | MB_APPLMODAL);
				if (res == IDYES)
				{
					if (!saveWorkSpace())
						return;
				}
				else if (res == IDNO)
				{
					// Don't save so do nothing here
				}
				else if (res == IDCANCEL) 
				{
					// User cancels action "New Workspace" so we interrupt here
					return;
				}
			}
			_treeView.removeAllItems();
			newWorkSpace();
		}
		break;

		case IDM_PROJECT_RENAME :
			if (getNodeType(hTreeItem) != nodeType_root)
				_treeView.editLabel(hTreeItem);
		break;

		case IDM_PROJECT_NEWFOLDER :
		{
			NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
			wstring newFolderLabel = pNativeSpeaker->getAttrNameStr(PM_NEWFOLDERNAME, "ProjectManager", "NewFolderName");
			addFolder(hTreeItem, newFolderLabel.c_str());
			setWorkSpaceDirty(true);
		}
		break;

		case IDM_PROJECT_MOVEDOWN :
		{
			if (_treeView.moveDown(hTreeItem))
				setWorkSpaceDirty(true);
		}
		break;

		case IDM_PROJECT_MOVEUP :
		{
			if (_treeView.moveUp(hTreeItem))
				setWorkSpaceDirty(true);
		}
		break;

		case IDM_PROJECT_ADDFILES :
		{
			addFiles(hTreeItem);
			if (getNodeType(hTreeItem) == nodeType_folder)
				_treeView.setItemImage(hTreeItem, INDEX_OPEN_NODE, INDEX_OPEN_NODE);
		}
		break;

		case IDM_PROJECT_ADDFILESRECUSIVELY :
		{
			addFilesFromDirectory(hTreeItem);
			if (getNodeType(hTreeItem) == nodeType_folder)
				_treeView.setItemImage(hTreeItem, INDEX_OPEN_NODE, INDEX_OPEN_NODE);
		}
		break;

		case IDM_PROJECT_OPENWS:
		{
			if (!saveWorkspaceRequest())
				break;

			CustomFileDialog fDlg(_hSelf);
			setFileExtFilter(fDlg);
			const wstring fn = fDlg.doOpenSingleFileDlg();
			if (!fn.empty())
			{
				if (!openWorkSpace(fn.c_str(), true))
				{
					NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
					pNativeSpeaker->messageBox("ProjectPanelOpenFailed",
						_hSelf,
						L"The workspace could not be opened.\rIt seems the file to open is not a valid project file.",
						L"Open Workspace",
						MB_OK);
					return;
				}
			}
		}
		break;

		case IDM_PROJECT_RELOADWS:
		{
			NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
			if (_isDirty)
			{
				int res = pNativeSpeaker->messageBox("ProjectPanelReloadDirty",
					_hSelf,
					L"The current workspace was modified. Reloading will discard all modifications.\rDo you want to continue?",
					L"Reload Workspace",
					MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL);

				if (res == IDNO)
				{
					return;
				}
			}

			if (doesFileExist(_workSpaceFilePath.c_str()))
			{
				openWorkSpace(_workSpaceFilePath.c_str(), true);
			}
			else
			{
				pNativeSpeaker->messageBox("ProjectPanelReloadError",
					_hSelf,
					L"Cannot find the file to reload.",
					L"Reload Workspace",
					MB_OK);
			}
		}
		break;

		case IDM_PROJECT_SAVEWS:
			saveWorkSpace();
		break;

		case IDM_PROJECT_SAVEACOPYASWS:
		case IDM_PROJECT_SAVEASWS:
		{
			saveWorkSpaceAs(cmdID == IDM_PROJECT_SAVEACOPYASWS);
		}
		break;

		case IDM_PROJECT_FINDINPROJECTSWS:
		{
			::SendMessage(_hParent, NPPM_INTERNAL_FINDINPROJECTS, static_cast<WPARAM>(1) << _panelID, 0);
		}
		break;

		case IDM_PROJECT_DELETEFOLDER :
		{
			HTREEITEM parent = _treeView.getParent(hTreeItem);

			if (_treeView.getChildFrom(hTreeItem) != nullptr)
			{
				NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
				int res = pNativeSpeaker->messageBox("ProjectPanelRemoveFolderFromProject",
					_hSelf,
					L"All the sub-items will be removed.\rAre you sure you want to remove this folder from the project?",
					L"Remove folder from project",
					MB_YESNO);
				if (res == IDYES)
				{
					_treeView.removeItem(hTreeItem);
					setWorkSpaceDirty(true);
				}
			}
			else
			{
				_treeView.removeItem(hTreeItem);
				setWorkSpaceDirty(true);
			}

			if (getNodeType(parent) == nodeType_folder)
				_treeView.setItemImage(parent, INDEX_CLOSED_NODE, INDEX_CLOSED_NODE);
		}
		break;

		case IDM_PROJECT_DELETEFILE :
		{
			HTREEITEM parent = _treeView.getParent(hTreeItem);
			
			NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
			int res = pNativeSpeaker->messageBox("ProjectPanelRemoveFileFromProject",
				_hSelf,
				L"Are you sure you want to remove this file from the project?",
				L"Remove file from project",
				MB_YESNO);
			if (res == IDYES)
			{
				_treeView.removeItem(hTreeItem);
				setWorkSpaceDirty(true);
				if (getNodeType(parent) == nodeType_folder)
					_treeView.setItemImage(parent, INDEX_CLOSED_NODE, INDEX_CLOSED_NODE);
			}
		}
		break;

		case IDM_PROJECT_MODIFYFILEPATH :
		{
			FileRelocalizerDlg fileRelocalizerDlg;
			fileRelocalizerDlg.init(_hInst, _hParent);

			auto* const fn = reinterpret_cast<std::wstring*>(_treeView.getItemParam(hTreeItem));
			if (!fn)
				return;

			if (fileRelocalizerDlg.doDialog(fn->c_str()) == 0)
			{
				std::wstring newValue = fileRelocalizerDlg.getFullFilePath();
				if (*fn == newValue)
					return;

				*fn = newValue;
				_treeView.renameItem(hTreeItem, PathFindFileName(fn->c_str()));
				const int iImage = doesFileExist(fn->c_str()) ? INDEX_LEAF : INDEX_LEAF_INVALID;
				_treeView.setItemImage(hTreeItem, iImage, iImage);
				setWorkSpaceDirty(true);
			}
		}
		break;
	}
}

bool ProjectPanel::saveWorkSpaceAs(bool saveCopyAs)
{
	CustomFileDialog fDlg(_hSelf);
	setFileExtFilter(fDlg);
	fDlg.setExtIndex(0);		// 0 index for "custom extension" type if any else for "All types *.*"
	fDlg.setFolder(folderOf(_workSpaceFilePath).c_str());

	const wstring fn = fDlg.doSaveDlg();
	if (fn.empty())
		return false;

	if (!writeWorkSpace(fn.c_str(), !saveCopyAs))
		return false;

	if (!saveCopyAs)
	{
		_workSpaceFilePath = fn;
		setWorkSpaceDirty(false);
	}
	return true;
}

void ProjectPanel::setFileExtFilter(CustomFileDialog & fDlg)
{
	const wchar_t *ext = NppParameters::getInstance().getNppGUI()._definedWorkspaceExt.c_str();
	if (*ext != '\0')
	{
		wstring workspaceExt = L"";

		if (*ext != '.')
			workspaceExt += L".";
		workspaceExt += ext;
		fDlg.setExtFilter(L"Workspace file", workspaceExt.c_str());
		fDlg.setDefExt(ext);
	}
	fDlg.setExtFilter(L"All types", L".*");
}

void ProjectPanel::addFiles(HTREEITEM hTreeItem)
{
	CustomFileDialog fDlg(_hSelf);
	fDlg.setExtFilter(L"All types", L".*");

	const auto& fns = fDlg.doOpenMultiFilesDlg();
	if (!fns.empty())
	{
		size_t sz = fns.size();
		for (size_t i = 0 ; i < sz ; ++i)
		{
			const wchar_t *strValueLabel = PathFindFileName(fns.at(i).c_str());

			_fullPathStrs.push_back(std::make_unique<std::wstring>(fns.at(i)));
			auto lParamPathFileStr = reinterpret_cast<LPARAM>(_fullPathStrs.back().get());

			_treeView.addItem(strValueLabel, hTreeItem, INDEX_LEAF, lParamPathFileStr);
		}
		_treeView.expand(hTreeItem);
		setWorkSpaceDirty(true);
	}
}

void ProjectPanel::recursiveAddFilesFrom(const wchar_t *folderPath, HTREEITEM hTreeItem)
{
	const QDir folder(QString::fromWCharArray(folderPath));

	// Hidden directories are ignored
	const QFileInfoList subFolders = folder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
	for (const QFileInfo& subFolder : subFolders)
	{
		HTREEITEM addedItem = addFolder(hTreeItem, subFolder.fileName().toStdWString().c_str(), false);
		recursiveAddFilesFrom(subFolder.absoluteFilePath().toStdWString().c_str(), addedItem);
	}

	const QFileInfoList files = folder.entryInfoList(QDir::Files | QDir::Hidden | QDir::System);
	for (const QFileInfo& file : files)
	{
		_fullPathStrs.push_back(std::make_unique<std::wstring>(file.absoluteFilePath().toStdWString()));
		auto lParamPathFileStr = reinterpret_cast<LPARAM>(_fullPathStrs.back().get());
		_treeView.addItem(file.fileName().toStdWString().c_str(), hTreeItem, INDEX_LEAF, lParamPathFileStr);
	}
}

void ProjectPanel::addFilesFromDirectory(HTREEITEM hTreeItem)
{
	if (_selDirOfFilesFromDirDlg == L"" && _workSpaceFilePath != L"")
	{
		_selDirOfFilesFromDirDlg = folderOf(_workSpaceFilePath);
	}
	wstring dirPath;
	if (_selDirOfFilesFromDirDlg != L"")
		dirPath = getFolderName(_hSelf, _selDirOfFilesFromDirDlg.c_str());
	else
		dirPath = getFolderName(_hSelf);

	if (dirPath != L"")
	{
		recursiveAddFilesFrom(dirPath.c_str(), hTreeItem);
		_treeView.expand(hTreeItem);
		setWorkSpaceDirty(true);
		_selDirOfFilesFromDirDlg = dirPath;
	}
}

int FileRelocalizerDlg::doDialog(const wchar_t *fn, bool isRTL)
{
	_fullFilePath = fn;

	QDialog* dialog = createDialogWidget(isRTL);
	Ui::FilerelocalizerDlg ui;
	ui.setupUi(dialog);

	ui.editFilefullpathname->setText(QString::fromStdWString(_fullFilePath));

	QObject::connect(ui.ok, &QAbstractButton::clicked, dialog, [this, line = ui.editFilefullpathname]()
	{
		_fullFilePath = line->text().toStdWString();
		endDialog(0);
	});
	QObject::connect(ui.cancel, &QAbstractButton::clicked, dialog, [this]() { endDialog(-1); });

	goToCenter();
	return runModal();
}
