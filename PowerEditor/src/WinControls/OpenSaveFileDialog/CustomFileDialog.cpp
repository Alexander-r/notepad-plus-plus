// This file is part of Notepad++ project
// Copyright (C) 2021 The Notepad++ Contributors.

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


#include "CustomFileDialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLineEdit>
#include <QStringList>

#include "Parameters.h"

using std::wstring;

namespace
{
	struct Filter
	{
		wstring _text;
		wstring _exts; // patterns separated by ';', such as "*.cpp;*.h"
	};

	QString toQ(const wstring& text)
	{
		return QString::fromStdWString(text);
	}

	QString filterString(const Filter& filter)
	{
		QStringList patterns = toQ(filter._exts).split(u';', Qt::SkipEmptyParts);
		for (QString& pattern : patterns)
		{
			if (pattern == u"*.*")
				pattern = QStringLiteral("*");
		}
		return toQ(filter._text) + QStringLiteral(" (") + patterns.join(u' ') + u')';
	}

	// The extension of the filter's first pattern, without the dot. Empty for a wildcard.
	QString firstExtension(const Filter& filter)
	{
		const QString first = toQ(filter._exts).section(u';', 0, 0);
		const qsizetype dot = first.lastIndexOf(u'.');
		if (dot < 0 || first.mid(dot + 1) == u"*")
			return {};
		return first.mid(dot + 1);
	}
}

class CustomFileDialog::Impl
{
public:
	HWND _owner = nullptr;
	wstring _title;
	wstring _defExt;
	wstring _initialFileName;
	wstring _initialFolder;
	wstring _fallbackFolder;
	std::vector<Filter> _filters;
	int _filterIndex = 0;
	int _wildcardIndex = -1;

	wstring _checkboxLabel;
	bool _isCheckboxActive = true;
	bool _checkboxState = false;

	bool _hasFileTypeCheckbox = false;
	wstring _fileTypeCheckboxLabel;
	bool _fileTypeCheckboxValue = false;

	bool _isSaveAsCopy = false;
	bool _openTheCopy = false;

	std::vector<wstring> run(QFileDialog::AcceptMode mode, QFileDialog::FileMode fileMode)
	{
		QFileDialog dialog(_owner, toQ(_title));
		dialog.setAcceptMode(mode);
		dialog.setFileMode(fileMode);

		QString folder = toQ(_initialFolder);
		if (!folder.isEmpty() && QFileInfo(folder).isFile())
			folder = QFileInfo(folder).absolutePath();
		if (folder.isEmpty() || !QFileInfo(folder).isDir())
			folder = toQ(_fallbackFolder);
		if (!folder.isEmpty())
			dialog.setDirectory(folder);

		QStringList nameFilters;
		for (const Filter& filter : _filters)
			nameFilters << filterString(filter);
		if (!nameFilters.isEmpty() && fileMode != QFileDialog::Directory)
		{
			dialog.setNameFilters(nameFilters);
			const int index = (_hasFileTypeCheckbox && !_fileTypeCheckboxValue && _wildcardIndex >= 0) ? _wildcardIndex : _filterIndex;
			if (index >= 0 && index < nameFilters.size())
				dialog.selectNameFilter(nameFilters[index]);
		}

		if (!_defExt.empty())
			dialog.setDefaultSuffix(toQ(_defExt));

		if (!_initialFileName.empty())
			dialog.selectFile(toQ(_initialFileName));

		// Extra controls only exist on Qt's own dialog: desktop file dialogs cannot host them
		QCheckBox* customCheck = nullptr;
		QCheckBox* fileTypeCheck = nullptr;
		if (!_checkboxLabel.empty() || _hasFileTypeCheckbox)
		{
			dialog.setOption(QFileDialog::DontUseNativeDialog);
			auto* grid = qobject_cast<QGridLayout*>(dialog.layout());
			if (grid)
			{
				if (!_checkboxLabel.empty())
				{
					customCheck = new QCheckBox(toQ(_checkboxLabel), &dialog);
					customCheck->setEnabled(_isCheckboxActive);
					grid->addWidget(customCheck, grid->rowCount(), 0, 1, -1);
				}
				if (_hasFileTypeCheckbox)
				{
					fileTypeCheck = new QCheckBox(toQ(_fileTypeCheckboxLabel), &dialog);
					fileTypeCheck->setChecked(_fileTypeCheckboxValue);
					grid->addWidget(fileTypeCheck, grid->rowCount(), 0, 1, -1);
				}
			}
		}

		const auto extensionOfSelectedFilter = [&]()
		{
			const qsizetype index = nameFilters.indexOf(dialog.selectedNameFilter());
			return index >= 0 ? firstExtension(_filters[index]) : QString{};
		};

		if (fileTypeCheck)
		{
			// As on Windows: with the box checked the name takes the extension of the selected file type
			const auto updateSuffix = [&]()
			{
				dialog.setDefaultSuffix(fileTypeCheck->isChecked() ? extensionOfSelectedFilter() : QString{});
			};
			QObject::connect(fileTypeCheck, &QCheckBox::toggled, &dialog, updateSuffix);
			QObject::connect(&dialog, &QFileDialog::filterSelected, &dialog, updateSuffix);
			updateSuffix();
		}

		const int result = dialog.exec();

		_checkboxState = customCheck && customCheck->isChecked();
		if (fileTypeCheck)
			_fileTypeCheckboxValue = fileTypeCheck->isChecked();

		// Shift on accepting opens the copy afterwards, as on Windows
		_openTheCopy = _isSaveAsCopy && result == QDialog::Accepted && (QApplication::keyboardModifiers() & Qt::ShiftModifier);

		std::vector<wstring> names;
		if (result == QDialog::Accepted)
		{
			const QStringList selected = dialog.selectedFiles();
			for (const QString& name : selected)
				names.push_back(QDir::toNativeSeparators(name).toStdWString());
		}
		return names;
	}

	wstring runSingle(QFileDialog::AcceptMode mode, QFileDialog::FileMode fileMode)
	{
		const std::vector<wstring> names = run(mode, fileMode);
		return names.empty() ? wstring{} : names.front();
	}
};

CustomFileDialog::CustomFileDialog(HWND hwnd) : _impl{ std::make_unique<Impl>() }
{
	_impl->_owner = hwnd;

	NppParameters& params = NppParameters::getInstance();
	NppGUI& nppGUI = params.getNppGUI();
	const wchar_t* workDir = nppGUI._openSaveDir == dir_last ? nppGUI._lastUsedDir.c_str() : params.getWorkingDir();
	if (workDir)
		_impl->_fallbackFolder = workDir;
}

CustomFileDialog::~CustomFileDialog() = default;

void CustomFileDialog::setTitle(const wchar_t* title)
{
	_impl->_title = title ? title : L"";
}

void CustomFileDialog::setExtFilter(const wchar_t *extText, const wchar_t *exts)
{
	// Add an asterisk before each dot in file patterns
	wstring newExts{ exts ? exts : L"" };
	for (size_t pos = 0; pos < newExts.size(); ++pos)
	{
		pos = newExts.find(L'.', pos);
		if (pos == wstring::npos)
			break;
		if (pos == 0 || newExts[pos - 1] != L'*')
		{
			newExts.insert(pos, 1, L'*');
			++pos;
		}
	}

	if (newExts.find(L"*.*") == 0)
		_impl->_wildcardIndex = static_cast<int>(_impl->_filters.size());

	_impl->_filters.push_back({ extText, newExts });
}

void CustomFileDialog::setExtFilter(const wchar_t *extText, std::initializer_list<const wchar_t*> extList)
{
	wstring exts;
	for (auto&& x : extList)
	{
		exts += x;
		exts += L';';
	}
	if (!exts.empty())
		exts.pop_back();	// remove the last ';'
	setExtFilter(extText, exts.c_str());
}

void CustomFileDialog::setDefExt(const wchar_t* ext)
{
	_impl->_defExt = ext ? ext : L"";
}

void CustomFileDialog::setDefFileName(const wchar_t* fn)
{
	_impl->_initialFileName = fn ? fn : L"";
}

void CustomFileDialog::setFolder(const wchar_t* folder)
{
	_impl->_initialFolder = folder ? folder : L"";
}

void CustomFileDialog::setCheckbox(const wchar_t* text, bool isActive)
{
	_impl->_checkboxLabel = text ? text : L"";
	_impl->_isCheckboxActive = isActive;
}

void CustomFileDialog::setExtIndex(int extTypeIndex)
{
	_impl->_filterIndex = extTypeIndex;
}

void CustomFileDialog::setSaveAsCopy(bool isSavingAsCopy)
{
	_impl->_isSaveAsCopy = isSavingAsCopy;
}

bool CustomFileDialog::getOpenTheCopyAfterSaveAsCopy()
{
	return _impl->_openTheCopy;
}

bool CustomFileDialog::getCheckboxState() const
{
	return _impl->_checkboxState;
}

bool CustomFileDialog::isReadOnly() const
{
	return false; // Qt's dialogs have no read-only option
}

void CustomFileDialog::enableFileTypeCheckbox(const wstring& text, bool value)
{
	if (!text.empty())
	{
		_impl->_fileTypeCheckboxLabel = text;
		_impl->_hasFileTypeCheckbox = true;
		_impl->_fileTypeCheckboxValue = value;
	}
}

bool CustomFileDialog::getFileTypeCheckboxValue() const
{
	return _impl->_fileTypeCheckboxValue;
}

wstring CustomFileDialog::doSaveDlg()
{
	return _impl->runSingle(QFileDialog::AcceptSave, QFileDialog::AnyFile);
}

wstring CustomFileDialog::doOpenSingleFileDlg()
{
	return _impl->runSingle(QFileDialog::AcceptOpen, QFileDialog::ExistingFile);
}

std::vector<wstring> CustomFileDialog::doOpenMultiFilesDlg()
{
	return _impl->run(QFileDialog::AcceptOpen, QFileDialog::ExistingFiles);
}

wstring CustomFileDialog::pickFolder()
{
	return _impl->runSingle(QFileDialog::AcceptOpen, QFileDialog::Directory);
}
