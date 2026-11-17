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


#include "FindReplaceDlg.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QToolTip>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "ui_FindReplaceDlg.h"
#include "ui_FindinfinderDlg.h"
#include "ui_IncrementFindDlg.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <BoostRegexSearch.h>
#include <SciLexer.h>
#include <Sci_Position.h>
#include <Scintilla.h>

#include "Buffer.h"
#include "Common.h"
#include "ContextMenu.h"
#include "Docking.h"
#include "DockingCont.h"
#include "DockingDlgInterface.h"
#include "FindReplaceDlg_rc.h"
#include "Notepad_plus_msgs.h"
#include "NppConstants.h"
#include "NppControls.h"
#include "NppDarkMode.h"
#include "NppMenu.h"
#include "NppResources.h"
#include "Parameters.h"
#include "ScintillaEditView.h"
#include "StaticDialog.h"
#include "ToolBar.h"
#include "Utf8.h"
#include "Window.h"
#include "colors.h"
#include "dpiManagerV2.h"
#include "localization.h"
#include "menuCmdID.h"
#include "resource.h"

using namespace std;

FindOption * FindReplaceDlg::_env;
FindOption FindReplaceDlg::_options;

const wstring noFoundPotentialReason = L"The given occurrence cannot be found. You may have forgotten to check \"Wrap around\" (to ON), \"Match case\" (to OFF), or \"Match whole word only\" (to OFF).";
const wstring regexEmptyFoundReason = L"The given regular expression matches empty strings.";

static void addText2Combo(const wchar_t* txt2add, HWND hCombo)
{
	auto* combo = qobject_cast<QComboBox*>(hCombo);
	if (!combo || !txt2add[0])
		return;

	const QString text = QString::fromWCharArray(txt2add);
	const int found = combo->findText(text, Qt::MatchExactly | Qt::MatchCaseSensitive);
	if (found != -1) // found
		combo->removeItem(found);

	combo->insertItem(0, text);
	combo->setCurrentIndex(0);
}

static std::wstring getTextFromCombo(HWND hCombo)
{
	const auto* combo = qobject_cast<QComboBox*>(hCombo);
	return combo ? combo->currentText().toStdWString() : std::wstring();
}

int Searching::convertExtendedToString(const wchar_t * query, wchar_t * result, int length)
{	//query may equal to result, since it always gets smaller
	int i = 0, j = 0;
	int charLeft = length;

	while (i < length)
	{	//because the backslash escape quences always reduce the size of the wstring, no overflow checks have to be made for target, assuming parameters are correct
		wchar_t current = query[i];
		--charLeft;
		if (current == '\\' && charLeft)
		{	//possible escape sequence
			++i;
			--charLeft;
			current = query[i];
			switch(current)
			{
				case 'r':
					result[j] = '\r';
					break;
				case 'n':
					result[j] = '\n';
					break;
				case '0':
					result[j] = '\0';
					break;
				case 't':
					result[j] = '\t';
					break;
				case '\\':
					result[j] = '\\';
					break;
				case 'b':
				case 'd':
				case 'o':
				case 'x':
				case 'u':
				{
					int size = 0, base = 0;
					if (current == 'b')
					{	//11111111
						size = 8, base = 2;
					}
					else if (current == 'o')
					{	//377
						size = 3, base = 8;
					}
					else if (current == 'd')
					{	//255
						size = 3, base = 10;
					}
					else if (current == 'x')
					{	//0xFF
						size = 2, base = 16;
					}
					else if (current == 'u')
					{	//0xCDCD
						size = 4, base = 16;
					}

					if (charLeft >= size)
					{
						int res = 0;
						if (Searching::readBase(query+(i+1), &res, base, size))
						{
							result[j] = static_cast<wchar_t>(res);
							i += size;
							break;
						}
					}
					//not enough chars to make parameter, use default method as fallback
					[[fallthrough]];
				}

				default:
				{	//unknown sequence, treat as regular text
					result[j] = '\\';
					++j;
					result[j] = current;
					break;
				}
			}
		}
		else
		{
			result[j] = query[i];
		}
		++i;
		++j;
	}
	result[j] = 0;
	return j;
}

bool Searching::readBase(const wchar_t * str, int * value, int base, int size)
{
	int i = 0, temp = 0;
	*value = 0;
	wchar_t max = '0' + static_cast<wchar_t>(base) - 1;

	while (i < size)
	{
		wchar_t current = str[i];
		if (current >= 'A')
		{
			current &= 0xdf;
			current -= ('A' - '0' - 10);
		}
		else if (current > '9')
			return false;

		if (current >= '0' && current <= max)
		{
			temp *= base;
			temp += (current - '0');
		}
		else
		{
			return false;
		}
		++i;
	}
	*value = temp;
	return true;
}

void Searching::displaySectionCentered(size_t posStart, size_t posEnd, ScintillaEditView * pEditView, bool isDownwards)
{
	// Make sure target lines are unfolded
	pEditView->execute(SCI_ENSUREVISIBLE, pEditView->execute(SCI_LINEFROMPOSITION, posStart));
	pEditView->execute(SCI_ENSUREVISIBLE, pEditView->execute(SCI_LINEFROMPOSITION, posEnd));

	// Jump-scroll to center, if current position is out of view
	pEditView->execute(SCI_SETVISIBLEPOLICY, CARET_JUMPS | CARET_EVEN);
	pEditView->execute(SCI_ENSUREVISIBLEENFORCEPOLICY, pEditView->execute(SCI_LINEFROMPOSITION, isDownwards ? posEnd : posStart));
	// When searching up, the beginning of the (possible multiline) result is important, when scrolling down the end
	pEditView->execute(SCI_GOTOPOS, isDownwards ? posEnd : posStart);
	pEditView->execute(SCI_SETVISIBLEPOLICY, CARET_EVEN);
	pEditView->execute(SCI_ENSUREVISIBLEENFORCEPOLICY, pEditView->execute(SCI_LINEFROMPOSITION, isDownwards ? posEnd : posStart));

	// Adjust so that we see the entire match; primarily horizontally
	pEditView->execute(SCI_SCROLLRANGE, posStart, posEnd);

	// make sure won't start/end the selection in the middle of a multibyte character,
	// or in between a CR/LF pair for Windows files
	// (needed for stale search-results where user has made doc edits after the search)
	if (posStart > 0)
	{
		posStart = pEditView->execute(SCI_POSITIONBEFORE, posStart);
		posStart = pEditView->execute(SCI_POSITIONAFTER, posStart);
	}
	if (posEnd > 0)
	{
		posEnd = pEditView->execute(SCI_POSITIONBEFORE, posEnd);
		posEnd = pEditView->execute(SCI_POSITIONAFTER, posEnd);
	}

	// Move cursor to end of result and select result
	pEditView->execute(SCI_GOTOPOS, posEnd);
	pEditView->execute(SCI_SETANCHOR, posStart);

	// Update Scintilla's knowledge about what column the caret is in, so that if user
	// does up/down arrow as first navigation after the search result is selected,
	// the caret doesn't jump to an unexpected column
	pEditView->execute(SCI_CHOOSECARETX);
}

FindReplaceDlg::FindReplaceDlg()
{
	_uniFileName = new char[(_fileNameLenMax + 3) * 2];
	_env = &_options;
}

FindReplaceDlg::~FindReplaceDlg()
{
	_tab.destroy();
	delete _pFinder;
	for (int n = static_cast<int32_t>(_findersOfFinder.size()) - 1; n >= 0; n--)
	{
		delete _findersOfFinder[n];
		_findersOfFinder.erase(_findersOfFinder.begin() + n);
	}

	delete[] _uniFileName;
}

static bool isUnexpectedInvisible(wchar_t ch)
{
	if (ch <= 0x001F) // includ EOL
	{
		return (ch != L'\t'); // Tab is not considered as unexpected "invisible" characters 
	}

	switch (ch)
	{
	case 0x007F: // Delete

	case 0x00A0: // NBSP
	case 0x00AD: // SHY
	case 0x061C: // ALM
	case 0x070F: // SAM
	case 0x1680: // OSPM
	case 0x180E: // MVS
	case 0x2000: case 0x2001: case 0x2002: case 0x2003: // Quads/Spaces
	case 0x2004: case 0x2005: case 0x2006: case 0x2007: // Spaces
	case 0x2008: case 0x2009: case 0x200A:             // Spaces
	case 0x200B: // ZWSP
	case 0x200C: // ZWNJ
	case 0x200D: // ZWJ
	case 0x200E: // LRM
	case 0x200F: // RLM
	case 0x202A: case 0x202B: case 0x202C: case 0x202D: case 0x202E: // Embed/Override
	case 0x202F: // NNBSP
	case 0x205F: // MMSP
	case 0x2060: // WJ
	case 0x2061: case 0x2062: case 0x2063: case 0x2064: // Invisible Math
	case 0x2066: case 0x2067: case 0x2068: case 0x2069: // Isolates
	case 0x206A: case 0x206B: case 0x206C: case 0x206D: // Shaping/Swapping
	case 0x206E: case 0x206F: // Digit shapes
	case 0x2028: // Line separator
	case 0x2029: // Paragraph separator
	case 0x3000: // IDSP
	case 0xFFF9: case 0xFFFA: case 0xFFFB: // Interlinear annotation
		return true;

	default:
		return false;
	}
}

static bool containsInvisibleChar(const std::wstring& text)
{
	for (wchar_t ch : text)
	{
		if (isUnexpectedInvisible(ch)) return true;
	}
	return false;
}

// Handles what the form's combo boxes need beyond QLineEdit: browsing the history with a draft of the typed text,
// removing a history entry with Delete, and warning about invisible characters in pasted text.
class FindComboFilter final : public QObject
{
public:
	FindComboFilter(QObject* parent, QComboBox* combo, FindReplaceDlg* dialog, bool isSearchText)
		: QObject(parent), _combo(combo), _dialog(dialog), _isSearchText(isSearchText) {}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::FocusIn)
		{
			_draft.clear();
		}
		else if (event->type() == QEvent::KeyPress)
		{
			auto* key = static_cast<QKeyEvent*>(event);
			if (_isSearchText && key->matches(QKeySequence::Paste))
				return handlePaste();

			const int current = currentHistoryIndex();
			if (key->key() == Qt::Key_Down && current == -1)
			{
				// down key on unselected combobox item -> store current edit text as draft
				_draft = _combo->currentText();
			}
			else if (key->key() == Qt::Key_Up)
			{
				if (current == -1)
				{
					// up key on unselected combobox item -> no change but select current edit text
					_combo->lineEdit()->selectAll();
					return true;
				}
				if (current == 0 && !_draft.isEmpty())
				{
					// up key on top selected combobox item -> restore draft to edit text
					_combo->setCurrentIndex(-1);
					_combo->setEditText(_draft);
					_combo->lineEdit()->selectAll();
					return true;
				}
			}
		}
		return QObject::eventFilter(watched, event);
	}

private:
	// The entry the edit text comes from: typing leaves the selection as it was in Qt, unlike on Win32
	int currentHistoryIndex() const
	{
		return _combo->findText(_combo->currentText(), Qt::MatchExactly | Qt::MatchCaseSensitive);
	}

	bool handlePaste()
	{
		// needed to allow CR (i.e., multiline) into combobox text
		const QMimeData* data = QGuiApplication::clipboard()->mimeData();
		if (data && data->hasFormat(QStringLiteral("MSDEVColumnSelect")))
			return true; // a column selection cannot be used as search text

		const std::wstring text = strFromClipboard();
		if (!text.empty())
		{
			_combo->lineEdit()->insert(QString::fromStdWString(text));
			_dialog->warnIfInvisibleChars(text);
		}
		return true;
	}

	QComboBox* _combo;
	FindReplaceDlg* _dialog;
	bool _isSearchText;
	QString _draft;
};

// Shift+Enter searches in the other direction, which a QDialog does not pass to the default button.
class FindDialogKeyFilter final : public QObject
{
public:
	FindDialogKeyFilter(QObject* parent, FindReplaceDlg* dialog) : QObject(parent), _dialog(dialog) {}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::KeyPress)
		{
			const auto* key = static_cast<QKeyEvent*>(event);
			const bool isEnter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
			if (isEnter && (key->modifiers() & Qt::ShiftModifier))
			{
				_dialog->onCommand(MAKEWPARAM(IDOK, BN_CLICKED), 0);
				return true;
			}
		}
		return QObject::eventFilter(watched, event);
	}

private:
	FindReplaceDlg* _dialog;
};

void FindReplaceDlg::create([[maybe_unused]] int dialogID, bool isRTL, [[maybe_unused]] bool msgDestParent, bool toShow)
{
	QDialog* dialog = createDialogWidget(isRTL);
	_ui = std::make_unique<Ui::FindReplaceDlg>();
	_ui->setupUi(dialog);

	setupControls();
	fillFindHistory();
	_currentStatus = REPLACE_DLG;
	initOptionsFromDlg();

	_tab.init(_hInst, _hSelf, false, false);
	_tab.insertAtEnd(L"Find");
	_tab.insertAtEnd(L"Replace");
	_tab.insertAtEnd(L"Find in Files");
	_tab.insertAtEnd(L"Find in Projects");
	_tab.insertAtEnd(L"Mark");
	_ui->mainLayout->insertWidget(0, _tab.getHSelf());
	QObject::connect(_tab.tabs(), &QTabBar::tabBarClicked, _hSelf, [this](int index)
	{
		if (index != -1)
			doDialog(static_cast<DIALOG_TYPE>(index));
	});

	_statusBar.init(_hInst, _hSelf, 1);
	_ui->mainLayout->addWidget(_statusBar.getHSelf());
	_statusBar.display();

	nppRegisterMessageTarget(_hSelf, [this](UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT
	{
		if (message == WM_COMMAND)
			return onCommand(wParam, lParam);
		if (message == NPPM_MODELESSDIALOG)
			return ::SendMessage(_hParent, NPPM_MODELESSDIALOG, wParam, lParam);
		return 0;
	});

	measureTabs();

	NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();

	if (nppGUI._findWindowPos.bottom - nppGUI._findWindowPos.top != 0)  // check height against 0 as a test of valid data from config
	{
		const RECT rc = getViewablePositionRect(nppGUI._findWindowPos);
		_hSelf->move(rc.left, rc.top);
		_hSelf->resize(rc.right - rc.left, _hSelf->height());
	}
	else if (toShow)
	{
		goToCenter();
	}

	if (nppGUI._findWindowLessMode)
	{
		// reverse the value of _findWindowLessMode because the value will be inversed again in IDD_RESIZE_TOGGLE_BUTTON
		nppGUI._findWindowLessMode = false;

		onCommand(IDD_RESIZE_TOGGLE_BUTTON, 0);
	}
}

void FindReplaceDlg::fillFindHistory()
{
	NppParameters& nppParams = NppParameters::getInstance();
	FindHistory & findHistory = nppParams.getFindHistory();

	fillComboHistory(IDFINDWHAT, findHistory._findHistoryFinds);
	fillComboHistory(IDREPLACEWITH, findHistory._findHistoryReplaces);
	fillComboHistory(IDD_FINDINFILES_FILTERS_COMBO, findHistory._findHistoryFilters);
	fillComboHistory(IDD_FINDINFILES_DIR_COMBO, findHistory._findHistoryPaths);

	setChecked(IDWRAP, findHistory._isWrap);
	setChecked(IDWHOLEWORD, findHistory._isMatchWord);
	setChecked(IDMATCHCASE, findHistory._isMatchCase);
	setChecked(IDC_BACKWARDDIRECTION, !findHistory._isDirectionDown);

	setChecked(IDD_FINDINFILES_INHIDDENDIR_CHECK, findHistory._isFifInHiddenFolder);
	setChecked(IDD_FINDINFILES_RECURSIVE_CHECK, findHistory._isFifRecursive);

	setChecked(IDD_FINDINFILES_PROJECT1_CHECK, findHistory._isFifProjectPanel_1);
	setChecked(IDD_FINDINFILES_PROJECT2_CHECK, findHistory._isFifProjectPanel_2);
	setChecked(IDD_FINDINFILES_PROJECT3_CHECK, findHistory._isFifProjectPanel_3);

	setChecked(IDNORMAL, findHistory._searchMode == FindHistory::normal);
	setChecked(IDEXTENDED, findHistory._searchMode == FindHistory::extended);
	setChecked(IDREGEXP, findHistory._searchMode == FindHistory::regExpr);
	setChecked(IDREDOTMATCHNL, findHistory._dotMatchesNewline);

	setChecked(IDC_MARKLINE_CHECK, findHistory._isBookmarkLine);
	setChecked(IDC_PURGE_CHECK, findHistory._isPurge);

	setChecked(IDC_2_BUTTONS_MODE, findHistory._isSearch2ButtonsMode);


	const NppGUI&	nppGUI = nppParams.getNppGUI();
	setChecked(IDC_CONFIRMREPLACEOPENDOCS, nppGUI._confirmReplaceInAllOpenDocs);
	setChecked(IDC_REPLACEANDSTOP, nppGUI._replaceStopsWithoutFindingNext);

	setChecked(IDC_HIGHLIGHT_MODE, findHistory._findDlgHighlightMatches);

	if (findHistory._searchMode == FindHistory::regExpr)
	{
		//regex doesn't allow wholeword
		setChecked(IDWHOLEWORD, false);
		enableFindDlgItem(IDWHOLEWORD, false);

		// regex upward search is disabled
		setChecked(IDC_BACKWARDDIRECTION, false);
		enableFindDlgItem(IDC_BACKWARDDIRECTION, nppParams.regexBackward4PowerUser());
		enableFindDlgItem(IDC_FINDPREV, nppParams.regexBackward4PowerUser());

		// If the search mode from history is regExp then enable the checkbox (. matches newline)
		enableFindDlgItem(IDREDOTMATCHNL);
	}

	if (!NppParameters::isWindowOpacityAvailable())
		showFindDlgItem(IDC_TRANSPARENT_GRPBOX, false);

	if (NppParameters::isWindowOpacityAvailable())
	{
		showFindDlgItem(IDC_TRANSPARENT_CHECK);
		showFindDlgItem(IDC_TRANSPARENT_GRPBOX);
		showFindDlgItem(IDC_TRANSPARENT_LOSSFOCUS_RADIO);
		showFindDlgItem(IDC_TRANSPARENT_ALWAYS_RADIO);
		showFindDlgItem(IDC_PERCENTAGE_SLIDER);

		_ui->percentageSlider->setRange(20, 200);
		_ui->percentageSlider->setValue(findHistory._transparency);

		if (findHistory._transparencyMode == FindHistory::none)
		{
			enableFindDlgItem(IDC_TRANSPARENT_LOSSFOCUS_RADIO, false);
			enableFindDlgItem(IDC_TRANSPARENT_ALWAYS_RADIO, false);
			enableFindDlgItem(IDC_PERCENTAGE_SLIDER, false);
		}
		else
		{
			setChecked(IDC_TRANSPARENT_CHECK, true);

			int id;
			if (findHistory._transparencyMode == FindHistory::onLosingFocus)
			{
				id = IDC_TRANSPARENT_LOSSFOCUS_RADIO;
			}
			else
			{
				id = IDC_TRANSPARENT_ALWAYS_RADIO;
				NppParameters::setTransparent(_hSelf, findHistory._transparency);

			}
			setChecked(id, true);
		}
	}
}

void FindReplaceDlg::fillComboHistory(int id, const vector<wstring> & strings)
{
	QComboBox* hCombo = combo(id);

	for (vector<wstring>::const_reverse_iterator i = strings.rbegin() ; i != strings.rend(); ++i)
	{
		addText2Combo(i->c_str(), hCombo);
	}

	//empty string is not added to CB items, so we need to set it manually
	if (!strings.empty() && strings.begin()->empty())
	{
		hCombo->setEditText(QString());
		return;
	}

	hCombo->setCurrentIndex(strings.empty() ? -1 : 0); // select first item
}


void FindReplaceDlg::saveFindHistory()
{
	if (! isCreated()) return;
	FindHistory& findHistory = (NppParameters::getInstance()).getFindHistory();

	saveComboHistory(IDD_FINDINFILES_DIR_COMBO, findHistory._nbMaxFindHistoryPath, findHistory._findHistoryPaths, false);
	saveComboHistory(IDD_FINDINFILES_FILTERS_COMBO, findHistory._nbMaxFindHistoryFilter, findHistory._findHistoryFilters, true);
	saveComboHistory(IDFINDWHAT,                    findHistory._nbMaxFindHistoryFind, findHistory._findHistoryFinds, false);
	saveComboHistory(IDREPLACEWITH,                 findHistory._nbMaxFindHistoryReplace, findHistory._findHistoryReplaces, true);
}

int FindReplaceDlg::saveComboHistory(int id, int maxcount, vector<wstring> & strings, bool saveEmpty)
{
	const QComboBox* hCombo = combo(id);
	const int count = std::min<int>(hCombo->count(), maxcount);

	strings.clear();

	if (saveEmpty && hCombo->currentText().isEmpty())
		strings.push_back(wstring());

	for (int i = 0 ; i < count ; ++i)
	{
		const std::wstring text = hCombo->itemText(i).toStdWString();
		if (text.length() <= FINDREPLACE_MAXLENGTH2SAVE - 1)
			strings.push_back(text);
	}
	return count;
}

void FindReplaceDlg::updateCombos()
{
	updateCombo(IDREPLACEWITH);
	updateCombo(IDFINDWHAT);
}

void FindReplaceDlg::updateCombo(int comboID)
{
	HWND hCombo = dlgItem(comboID);
	addText2Combo(getTextFromCombo(hCombo).c_str(), hCombo);
}

FoundInfo Finder::EmptyFoundInfo(0, 0, 0, L"");
SearchResultMarkingLine Finder::EmptySearchResultMarking;

bool Finder::notify(SCNotification *notification)
{
	static bool isDoubleClicked = false;

	switch (notification->nmhdr.code)
	{
		case SCN_MARGINCLICK:
			if (notification->margin == ScintillaEditView::_SC_MARGE_FOLDER)
			{
				_scintView.marginClick(notification->position, notification->modifiers);
			}
			break;

		case SCN_DOUBLECLICK:
		{
			// remove selection from the finder
			isDoubleClicked = true;

			size_t pos = notification->position;
			if (static_cast<intptr_t>(pos) == INVALID_POSITION)
				pos = _scintView.execute(SCI_GETLINEENDPOSITION, notification->line);
			_scintView.execute(SCI_SETSEL, pos, pos);

			std::pair<intptr_t, intptr_t> newPos = gotoFoundLine();
			auto lineStartAbsPos = _scintView.execute(SCI_POSITIONFROMLINE, notification->line);
			intptr_t lineEndAbsPos = _scintView.execute(SCI_GETLINEENDPOSITION, notification->line);

			intptr_t begin = newPos.first + lineStartAbsPos;
			intptr_t end = newPos.second + lineStartAbsPos;

			if (end > lineEndAbsPos)
				end = lineEndAbsPos;
			
			if (begin < end)
			{
				_scintView.execute(SCI_SETSEL, begin, end);
				_scintView.execute(SCI_SCROLLRANGE, begin, end);
			}

		}
		break;

		case SCN_PAINTED :
			if (isDoubleClicked)
			{
				(*_ppEditView)->grabFocus();
				isDoubleClicked = false;
			}
			break;

		case SCN_UPDATEUI:
			::SendMessage(_hParent, NPPM_INTERNAL_CHECKUNDOREDOSTATE, 0, 0);
			break;

	}
	return false;
}


std::pair<intptr_t, intptr_t> Finder::gotoFoundLine(size_t nOccurrence)
{
	std::pair<intptr_t, intptr_t> emptyResult(0, 0);
	auto currentPos = _scintView.execute(SCI_GETCURRENTPOS);
	auto lno = _scintView.execute(SCI_LINEFROMPOSITION, currentPos);
	auto start = _scintView.execute(SCI_POSITIONFROMLINE, lno);
	auto end = _scintView.execute(SCI_GETLINEENDPOSITION, lno);

	if (start + 2 >= end) return emptyResult; // avoid empty lines

	if (_scintView.execute(SCI_GETFOLDLEVEL, lno) & SC_FOLDLEVELHEADERFLAG)
	{
		_scintView.execute(SCI_TOGGLEFOLD, lno);
		return  emptyResult;
	}

	if (static_cast<size_t>(lno) >= _pMainFoundInfos->size())
		return std::pair<intptr_t, intptr_t>(0, 0);

	const FoundInfo& fInfo = *(_pMainFoundInfos->begin() + lno);
	const SearchResultMarkingLine& markingLine = *(_pMainMarkings->begin() + lno);

	// Switch to another document
	if (!::SendMessage(_hParent, WM_DOOPEN, 0, reinterpret_cast<LPARAM>(fInfo._fullPath.c_str()))) return emptyResult;

	(*_ppEditView)->_positionRestoreNeeded = false;

	size_t index = 0;

	if (nOccurrence > 0)
	{
		index = nOccurrence - 1;
	}
	else // nOccurrence == 0 -> not used: use current line relative pos to check if it's inside of a marked occurrence
	{
		intptr_t currentPosInLine = currentPos - start;

		for (std::pair<intptr_t, intptr_t> range : markingLine._segmentPostions)
		{
			if (range.first <= currentPosInLine && currentPosInLine <= range.second)
				break;

			++index;
		}
	}

	if (index >= fInfo._ranges.size())
		index = 0;

	Searching::displaySectionCentered(fInfo._ranges[index].first, fInfo._ranges[index].second, *_ppEditView);

	return markingLine._segmentPostions[index];
}

void Finder::deleteResult()
{
	auto currentPos = _scintView.execute(SCI_GETCURRENTPOS); // yniq - add handling deletion of multiple lines?

	auto lno = _scintView.execute(SCI_LINEFROMPOSITION, currentPos);
	auto start = _scintView.execute(SCI_POSITIONFROMLINE, lno);
	auto end = _scintView.execute(SCI_GETLINEENDPOSITION, lno);
	if (start + 2 >= end) return; // avoid empty lines

	if (_scintView.execute(SCI_GETLEXER) == SCLEX_NULL)
	{
		_scintView.setLexer(L_SEARCHRESULT, LIST_NONE); // Restore searchResult lexer in case the lexer was changed to SCLEX_NULL in GotoFoundLine()
	}

	if (_scintView.execute(SCI_GETFOLDLEVEL, lno) & SC_FOLDLEVELHEADERFLAG)  // delete a folder
	{
		auto endline = _scintView.execute(SCI_GETLASTCHILD, lno, -1) + 1;
		if ((size_t)endline > _pMainFoundInfos->size()) return;

		_pMainFoundInfos->erase(_pMainFoundInfos->begin() + lno, _pMainFoundInfos->begin() + endline); // remove found info
		_pMainMarkings->erase(_pMainMarkings->begin() + lno, _pMainMarkings->begin() + endline);

		auto end2 = _scintView.execute(SCI_POSITIONFROMLINE, endline);
		_scintView.execute(SCI_SETSEL, start, end2);
		setFinderReadOnly(false);
		_scintView.execute(SCI_CLEAR);
		setFinderReadOnly(true);
	}
	else // delete one line
	{
		if ((size_t)lno >= _pMainFoundInfos->size()) return;

		_pMainFoundInfos->erase(_pMainFoundInfos->begin() + lno); // remove found info
		_pMainMarkings->erase(_pMainMarkings->begin() + lno);

		setFinderReadOnly(false);
		_scintView.execute(SCI_LINEDELETE);
		setFinderReadOnly(true);
	}
	_markingsStruct._length = static_cast<long>(_pMainMarkings->size());

	assert(_pMainFoundInfos->size() == _pMainMarkings->size());
	assert(size_t(_scintView.execute(SCI_GETLINECOUNT)) == _pMainFoundInfos->size() + 1);
}

vector<wstring> Finder::getResultFilePaths(bool onlyInSelectedText) const
{
	size_t fromLine = 0, toLine = 0;

	if (onlyInSelectedText)
	{
		const pair<size_t, size_t> lineRange = _scintView.getSelectionLinesRange();
		fromLine = lineRange.first;
		toLine = lineRange.second;
	}
	else
	{
		toLine = _scintView.execute(SCI_GETLINECOUNT) - 1;
	}

	size_t len = _pMainFoundInfos->size();
	vector<wstring> paths;

	for (size_t line = fromLine; line <= toLine; ++line)
	{
		const int lineFoldLevel = _scintView.execute(SCI_GETFOLDLEVEL, line) & SC_FOLDLEVELNUMBERMASK;
		if (lineFoldLevel == fileHeaderLevel)
		{
			// fileHeaderLevel lines don't have path info; have to look into the NEXT line for it,
			// but only need to do something special here if we are on the LAST line of the selection
			if (line == toLine)
			{
				++line;
			}
		}

		if (line < len)
		{
			wstring& path2add = (*_pMainFoundInfos)[line]._fullPath;
			if (!path2add.empty())
			{
				// make sure that path is not already in
				if (std::find(paths.begin(), paths.end(), path2add) == paths.end())
				{
					paths.push_back(path2add);
				}
			}
		}
	}

	return paths;
}


bool Finder::canFind(const wchar_t *fileName, size_t lineNumber, size_t* indexToStartFrom) const
{
	size_t len = _pMainFoundInfos->size();
	for (size_t i = *indexToStartFrom; i < len; ++i)
	{
		if ((*_pMainFoundInfos)[i]._fullPath == fileName)
		{
			if (lineNumber == (*_pMainFoundInfos)[i]._lineNumber)
			{
				*indexToStartFrom = i;
				return true;
			}
		}
	}
	return false;
}

// Y     : current pos
// X     : current sel
// XXXXY : current sel + current pos
// 
//                occurrence 1   occur. 2         occur. 3  occur. 4              Status      auxiliaryInfo
// =========================================================================================================
// situation  1:     Y [     ]    [     ]          [     ]  [     ]            : pos_infront        -1
// situation  2:       [     ]    [     ]          [     ]  [  Y  ]            : pos_inside          4
// situation  3:       [     ]  XXY     ]          [     ]  [     ]            : pos_inside          2
// situation  4:       [     ]    [     ]          [     ]  [  XXXY            : pos_inside          4
// situation  5:       [     ]    [     ]          [XXXXXY  [     ]            : pos_inside          3
// situation  6:       [     Y    [     ]          [     ]  [     ]            : pos_between         1
// situation  7:       [     ]    [     ]          [     ]  Y     ]            : pos_between         3
// situation  8:       [     ]    [     ]          [     Y  [     ]            : pos_between         3
// situation  9:       [     ]    [     ]  Y       [     ]  [     ]            : pos_between         2
// situation 10:       [     ]    [     ]          [     ]  [     ]    Y       : pos_behind          4

Finder::CurrentPosInLineInfo Finder::getCurrentPosInLineInfo(intptr_t currentPosInLine, const SearchResultMarkingLine& markingLine) const
{
	CurrentPosInLineInfo cpili;
	size_t count = 0;
	intptr_t lastEnd = 0;
	auto selStart = _scintView.execute(SCI_GETSELECTIONSTART);
	auto selEnd = _scintView.execute(SCI_GETSELECTIONEND);
	bool hasSel = (selEnd - selStart) != 0;

	for (std::pair<intptr_t, intptr_t> range : markingLine._segmentPostions)
	{
		++count;

		if (lastEnd <= currentPosInLine && currentPosInLine < range.first)
		{
			if (count == 1)
			{
				cpili._status = pos_infront;
				break;
			}
			else
			{
				cpili._status = pos_between;
				cpili.auxiliaryInfo = count - 1;
				break;
			}
		}
		
		if (range.first <= currentPosInLine && currentPosInLine <= range.second)
		{
			if (currentPosInLine == range.first && !hasSel)
			{
				cpili._status = pos_between;
				cpili.auxiliaryInfo = count - 1; //  c1      c2
				                                 // [   ]  I[   ]      : I is recongnized with c2, so auxiliaryInfo should be c1 (c2-1)
			}
			else if (currentPosInLine == range.second && !hasSel)
			{
				cpili._status = pos_between;
				cpili.auxiliaryInfo = count;     //  c1      c2
				                                 // [   ]I  [   ]      : I is recongnized with c1, so auxiliaryInfo should be c1
			}
			else
			{
				cpili._status = pos_inside;
				cpili.auxiliaryInfo = count;
			}
			break;
		}

		if (range.second < currentPosInLine)
		{
			if (markingLine._segmentPostions.size() == count)
			{
				cpili._status = pos_behind;
				cpili.auxiliaryInfo = count;
				break;
			}
		}

		lastEnd = range.second;
	}

	return cpili;
}

void Finder::anchorWithNoHeaderLines(intptr_t& currentL, intptr_t initL, intptr_t minL, intptr_t maxL, int direction)
{
	if (currentL > maxL && direction == 0)
		currentL = minL;

	while (_scintView.execute(SCI_GETFOLDLEVEL, currentL) & SC_FOLDLEVELHEADERFLAG)
	{
		currentL += direction == -1 ? -1 : 1;

		if (currentL > maxL)
			currentL = minL;
		else if (currentL < minL)
			currentL = maxL;

		if (currentL == initL)
			break;
	}

	auto extremityAbsoltePos = _scintView.execute(direction == -1 ? SCI_GETLINEENDPOSITION : SCI_POSITIONFROMLINE, currentL);
	_scintView.execute(SCI_SETSEL, extremityAbsoltePos, extremityAbsoltePos);
}

void Finder::gotoNextFoundResult(int direction)
{
	//
	// Get currentLine & currentPosInLine from CurrentPos
	//
	auto currentPos = _scintView.execute(SCI_GETCURRENTPOS);
	intptr_t lno = _scintView.execute(SCI_LINEFROMPOSITION, currentPos);
	auto total_lines = _scintView.execute(SCI_GETLINECOUNT);
	if (total_lines <= 1) return;

	auto lineStartAbsPos = _scintView.execute(SCI_POSITIONFROMLINE, lno);
	intptr_t currentPosInLine = currentPos - lineStartAbsPos;

	auto init_lno = lno;
	auto max_lno = _scintView.execute(SCI_GETLASTCHILD, lno, searchHeaderLevel);

	if (max_lno > total_lines - 2) return;

	// get the line number of the current search (searchHeaderLevel)
	int level = _scintView.execute(SCI_GETFOLDLEVEL, lno) & SC_FOLDLEVELNUMBERMASK;
	auto min_lno = lno;
	while (level-- >= fileHeaderLevel)
	{
		min_lno = _scintView.execute(SCI_GETFOLDPARENT, min_lno);
		assert(min_lno >= 0);
	}

	if (min_lno < 0)
		min_lno = lno; // when lno is a search header line

	assert(min_lno <= max_lno);

	if (lno > max_lno && direction == 0)
		lno = min_lno;
	else if (lno < min_lno)
		lno = max_lno;

	//
	// Set anchor and make sure that achor is not on the last (empty) line or head lines
	//
	while (_scintView.execute(SCI_GETFOLDLEVEL, lno) & SC_FOLDLEVELHEADERFLAG)
	{
		lno += direction == -1 ? -1 : 1;

		if (lno > max_lno)
			lno = min_lno;
		else if (lno < min_lno)
			lno = max_lno;

		if (lno == init_lno)
			break;
	}

	if (lno != init_lno)
	{
		auto extremityAbsoltePos = _scintView.execute(direction == -1 ? SCI_GETLINEENDPOSITION : SCI_POSITIONFROMLINE, lno);
		_scintView.execute(SCI_SETSEL, extremityAbsoltePos, extremityAbsoltePos);
		currentPos = extremityAbsoltePos;
		auto start = _scintView.execute(SCI_POSITIONFROMLINE, lno);
		currentPosInLine = currentPos - start;
	}


	size_t occurrenceNumberInLine_base1 = 0;
	const SearchResultMarkingLine& markingLine = *(_pMainMarkings->begin() + lno);

	//
	// Determine currentPosInLine status among pos_infront, pose_between, pos_inside and pos_behind
	//
	CurrentPosInLineInfo cpili  = getCurrentPosInLineInfo(currentPosInLine, markingLine);

	//
	// According CurrentPosInLineInfo and direction, set position and get number of occurrence
	//
	if (direction == 0) // Next
	{
		switch (cpili._status)
		{
			case pos_infront: // 2 situation: 
			{
				if (!markingLine._segmentPostions.size())
					return;

				if (markingLine._segmentPostions[0].second < SC_SEARCHRESULT_LINEBUFFERMAXLENGTH) // 1. The occurrence is displayed in the line
				{
					occurrenceNumberInLine_base1 = 1;
				}
				else // 2. The occurrence is NOT displayed in the line
				{
					lno++;
					anchorWithNoHeaderLines(lno, init_lno, min_lno, max_lno, direction);
					occurrenceNumberInLine_base1 = 1;
				}
			}
			break;

			case pos_between:
			case pos_inside:
			{
				occurrenceNumberInLine_base1 = cpili.auxiliaryInfo + 1;
				if (occurrenceNumberInLine_base1 > markingLine._segmentPostions.size())
				{
					lno++;
					anchorWithNoHeaderLines(lno, init_lno, min_lno, max_lno, direction);
					occurrenceNumberInLine_base1 = 1;
				}
			}
			break;

			case pos_behind:
			{
				lno++;
				anchorWithNoHeaderLines(lno, init_lno, min_lno, max_lno, direction);
				occurrenceNumberInLine_base1 = 1;
			}
			break;
		}
	}
	else if (direction == -1) // Previous
	{
		switch (cpili._status)
		{
			case pos_infront:
			{
				lno--;
				if (lno < 0) return;
				anchorWithNoHeaderLines(lno, init_lno, min_lno, max_lno, direction);
				const SearchResultMarkingLine& newMarkingLine = *(_pMainMarkings->begin() + lno);
				occurrenceNumberInLine_base1 = newMarkingLine._segmentPostions.size();
			}
			break;

			case pos_between:
			{
				occurrenceNumberInLine_base1 = cpili.auxiliaryInfo;
			}
			break;

			case pos_inside:
			{
				if (cpili.auxiliaryInfo > 1)
					occurrenceNumberInLine_base1 = cpili.auxiliaryInfo - 1;
				else
				{
					lno--;
					if (lno < 0) return;
					anchorWithNoHeaderLines(lno, init_lno, min_lno, max_lno, direction);
					const SearchResultMarkingLine& newMarkingLine = *(_pMainMarkings->begin() + lno);
					occurrenceNumberInLine_base1 = newMarkingLine._segmentPostions.size();
				}
			}
			break;

			case pos_behind:
			{
				occurrenceNumberInLine_base1 = cpili.auxiliaryInfo;
			}
			break;
		}
	}
	else // invalid
	{
		return;
	}

	_scintView.execute(SCI_ENSUREVISIBLE, lno);
	std::pair<intptr_t, intptr_t> newPos = gotoFoundLine(occurrenceNumberInLine_base1);

	lineStartAbsPos = _scintView.execute(SCI_POSITIONFROMLINE, lno);
	intptr_t lineEndAbsPos = _scintView.execute(SCI_GETLINEENDPOSITION, lno);

	intptr_t begin = newPos.first + lineStartAbsPos;
	intptr_t end = newPos.second + lineStartAbsPos;

	if (end > lineEndAbsPos)
		end = lineEndAbsPos;

	if (begin < end)
	{
		_scintView.execute(SCI_SETSEL, begin, end);
		_scintView.execute(SCI_SCROLLRANGE, begin, end);
	}
}

void FindInFinderDlg::initFromOptions()
{
	HWND hFindCombo = dlgItem(IDFINDWHAT_FIFOLDER);
	addText2Combo(_options._str2Search.c_str(), hFindCombo);

	setChecked(IDC_MATCHLINENUM_CHECK_FIFOLDER, _options._isMatchLineNumber);

	setChecked(IDWHOLEWORD_FIFOLDER, _options._searchType != FindRegex && _options._isWholeWord);
	enableItem(IDWHOLEWORD_FIFOLDER, _options._searchType != FindRegex);

	setChecked(IDMATCHCASE_FIFOLDER, _options._isMatchCase);

	setChecked(IDNORMAL_FIFOLDER, _options._searchType == FindNormal);
	setChecked(IDEXTENDED_FIFOLDER, _options._searchType == FindExtended);
	setChecked(IDREGEXP_FIFOLDER, _options._searchType == FindRegex);

	setChecked(IDREDOTMATCHNL_FIFOLDER, _options._dotMatchesNewline);
	enableItem(IDREDOTMATCHNL_FIFOLDER, _options._searchType == FindRegex);
}

void FindInFinderDlg::writeOptions()
{
	HWND hFindCombo = dlgItem(IDFINDWHAT_FIFOLDER);
	_options._str2Search = getTextFromCombo(hFindCombo);
	_options._isMatchLineNumber = isCheckedOrNot(IDC_MATCHLINENUM_CHECK_FIFOLDER);
	_options._isWholeWord = isCheckedOrNot(IDWHOLEWORD_FIFOLDER);
	_options._isMatchCase = isCheckedOrNot(IDMATCHCASE_FIFOLDER);
	_options._searchType = isCheckedOrNot(IDREGEXP_FIFOLDER) ? FindRegex : isCheckedOrNot(IDEXTENDED_FIFOLDER) ? FindExtended : FindNormal;
	_options._dotMatchesNewline = isCheckedOrNot(IDREDOTMATCHNL_FIFOLDER);
}

void FindInFinderDlg::runDialog(bool isRTL)
{
	QDialog* dialog = createDialogWidget(isRTL);
	Ui::FindinfinderDlg ui;
	ui.setupUi(dialog);

	(NppParameters::getInstance()).getNativeLangSpeaker()->changeDlgLang(_hSelf, "FindInFinder");

	initFromOptions();

	const auto refreshModeOptions = [this]()
	{
		if (isCheckedOrNot(IDREGEXP_FIFOLDER))
		{
			enableItem(IDWHOLEWORD_FIFOLDER, false);
			setChecked(IDWHOLEWORD_FIFOLDER, false);
			enableItem(IDREDOTMATCHNL_FIFOLDER, true);
		}
		else
		{
			// extended or normal mode
			enableItem(IDWHOLEWORD_FIFOLDER, true);
			enableItem(IDREDOTMATCHNL_FIFOLDER, false);
		}
	};
	refreshModeOptions();

	nppConnectControlCommands(_hSelf, [this, refreshModeOptions](int id, WORD)
	{
		switch (id)
		{
			case IDCANCEL:
				endDialog(-1);
				break;

			case IDOK:
			{
				writeOptions();
				endDialog(-1);
				FindersInfo findersInfo;
				findersInfo._pSourceFinder = _pFinder2Search;
				findersInfo._findOption = _options;
				::SendMessage(_hParent, WM_FINDALL_INCURRENTFINDER, reinterpret_cast<WPARAM>(&findersInfo), 0);
				break;
			}

			case IDNORMAL_FIFOLDER:
			case IDEXTENDED_FIFOLDER:
			case IDREGEXP_FIFOLDER:
				refreshModeOptions();
				break;

			default:
				break;
		}
	});

	goToCenter();
	runModal();
}

void FindInFinderDlg::onCancel()
{
	endDialog(-1);
}


void FindReplaceDlg::setupControls()
{
	QComboBox* hFindCombo = _ui->findwhat;
	QComboBox* hReplaceCombo = _ui->replacewith;

	hFindCombo->lineEdit()->setMaxLength(FINDREPLACE_MAXLENGTH * 2 - 1);
	hReplaceCombo->lineEdit()->setMaxLength(FINDREPLACE_MAXLENGTH * 2 - 1);

	for (QComboBox* c : {hFindCombo, hReplaceCombo, _ui->findinfilesFiltersCombo, _ui->findinfilesDirCombo})
	{
		c->setCompleter(nullptr);
		c->setDuplicatesEnabled(true);
		const bool isSearchText = (c == hFindCombo || c == hReplaceCombo);
		c->lineEdit()->installEventFilter(new FindComboFilter(c, c, this, isSearchText));
	}

	_hSelf->installEventFilter(new FindDialogKeyFilter(_hSelf, this));

	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	const auto setTip = [pNativeSpeaker](QWidget* control, const char* id, const wchar_t* defaultText)
	{
		control->setToolTip(QString::fromStdWString(pNativeSpeaker->getLocalizedStrFromID(id, defaultText)));
	};
	setTip(_ui->ok, "shift-change-direction-tip", L"Use Shift+Enter to search in the opposite direction.");
	setTip(_ui->n2ButtonsMode, "two-find-buttons-tip", L"2 find buttons mode");
	setTip(_ui->confirmreplaceopendocs, "confirm-replace-in-all-open-docs-tip", L"Confirm \"Replace All in All Opened Documents\"");
	setTip(_ui->replaceandstop, "replace-and-stop-tip", L"After replacing, don't advance to the next match");
	setTip(_ui->fifFilterTipStatic, "find-in-files-filter-tip", L"Find in cpp, cxx, h, hxx && hpp:\r*.cpp *.cxx *.h *.hxx *.hpp\r\rFind in all files except exe, obj && log:\r*.* !*.exe !*.obj !*.log\r\rFind in all files but exclude folders tests, bin && bin64:\r*.* !\\tests !\\bin*\r\rFind in all files but exclude all folders log or logs recursively:\r*.* !+\\log*");
	setTip(_ui->findinfilesSetdirfromdocButton, "find-in-files-dir-from-active-doc-tip", L"Fill directory field based on active document");
	setTip(_ui->highlightMode, "find-highlight-mode-tip", L"Highlight all matches of the current search in the active document while typing");

	// updateMatchHighlightingLater(): a moment after the last change
	constexpr int updateDelay = 250; // ms
	_matchHighlightTimer = new QTimer(_hSelf);
	_matchHighlightTimer->setSingleShot(true);
	_matchHighlightTimer->setInterval(updateDelay);
	QObject::connect(_matchHighlightTimer, &QTimer::timeout, _hSelf, [this]() { updateMatchHighlighting(); });

	_ui->findprev->setText(QStringLiteral("▲"));
	_ui->findnext->setText(QStringLiteral("▼ Find Next"));
	_ui->findreplaceSwapButton->setText(QStringLiteral("⇅"));
	_ui->resizeToggleButton->setText(QStringLiteral("˄"));

	// The swap button is a split button, as on Windows, whose arrow opens the menu of its other two actions
	vector<MenuItemUnit> itemUnitArray;
	itemUnitArray.push_back(MenuItemUnit(IDC_SWAP_FIND_REPLACE, L"⇅ Swap Find with Replace"));
	itemUnitArray.push_back(MenuItemUnit(IDC_COPY_FIND2REPLACE, L"⤵ Copy from Find to Replace"));
	itemUnitArray.push_back(MenuItemUnit(IDC_COPY_REPLACE2FIND, L"⤴ Copy from Replace to Find"));

	NativeLangSpeaker* speaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	for (auto&& i : itemUnitArray)
	{
		i._itemName = speaker->getDlgLangMenuStr("Dialog", "Find", i._cmdID, i._itemName.c_str());
	}

	_swapPopupMenu.create(_hSelf, itemUnitArray);
	_ui->findreplaceSwapButton->setMenu(_swapPopupMenu.getMenuHandle());

	QObject::connect(_ui->percentageSlider, &QSlider::valueChanged, _hSelf, [this](int percent)
	{
		FindHistory & findHistory = (NppParameters::getInstance()).getFindHistory();
		findHistory._transparency = percent;
		if (isCheckedOrNot(IDC_TRANSPARENT_ALWAYS_RADIO))
			NppParameters::setTransparent(_hSelf, percent);
	});

	nppConnectControlCommands(_hSelf, [this](int id, WORD notification)
	{
		onCommand(MAKEWPARAM(static_cast<WORD>(id), notification), 0);
	});

	if (NppParameters::getInstance().getNppGUI()._monospacedFontFindDlg)
		setMonospaceFont();
}

void FindReplaceDlg::setMonospaceFont()
{
	const QFont monospace = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	_ui->findwhat->setFont(monospace);
	_ui->replacewith->setFont(monospace);
}

void FindReplaceDlg::warnIfInvisibleChars(const std::wstring& text)
{
	if (containsInvisibleChar(text))
		setStatusMessageWithInvisibleCharsWarning();
}

void FindReplaceDlg::onActivationChanged(bool isActive)
{

	bool isInSelectionAutoChange = false;

	if (isActive)
	{
		Sci_CharacterRangeFull cr = (*_ppEditView)->getSelection();
		intptr_t nbSelected = (*_ppEditView)->execute(SCI_COUNTCHARACTERS, cr.cpMin, cr.cpMax);

		bool inSelEnabled = nbSelected != 0;

		// Searching/replacing in multiple selections or column selection is not allowed
		if (((*_ppEditView)->execute(SCI_GETSELECTIONMODE) == SC_SEL_RECTANGLE) ||
			((*_ppEditView)->execute(SCI_GETSELECTIONS) > 1))
		{
			inSelEnabled = false;
		}

		enableFindDlgItem(IDC_IN_SELECTION_CHECK, inSelEnabled);

		bool inSelChecked = isCheckedOrNot(IDC_IN_SELECTION_CHECK);
		bool origInSelChecked = inSelChecked;

		const NppGUI& nppGui = (NppParameters::getInstance()).getNppGUI();
		if (nppGui._inSelectionAutocheckThreshold != 0)
		{
			// code is allowed to change checkmark status of In-selection checkbox

			inSelChecked = inSelEnabled && (nbSelected >= nppGui._inSelectionAutocheckThreshold);

			setChecked(IDC_IN_SELECTION_CHECK, inSelChecked);
		}

		// A click on an action button that activated this dialog arrives after this: see onCommand().
		if (inSelChecked != origInSelChecked)
			isInSelectionAutoChange = true;

		_options._isInSelection = inSelEnabled && inSelChecked;
	}

	if (isCheckedOrNot(IDC_TRANSPARENT_LOSSFOCUS_RADIO))
	{
		if (!isActive && isVisible())
		{
			const int percent = _ui->percentageSlider->value();
			NppParameters::setTransparent(_hSelf, percent);
		}
		else
		{
			NppParameters::removeTransparent(_hSelf);
		}
	}

	// At very first time (when find dlg is launched), search mode is Normal.
	// In that case, ". Matches newline" should be disabled as it applicable only for Regex
	if (isCheckedOrNot(IDREGEXP))
	{
		enableFindDlgItem(IDREDOTMATCHNL);
	}
	else
	{
		enableFindDlgItem(IDREDOTMATCHNL, false);
	}

	enableProjectCheckmarks();

	if (isInSelectionAutoChange)
		_inSelectionAutoChangedAt = QDateTime::currentMSecsSinceEpoch();

	return;
}

void FindReplaceDlg::onCancel()
{
	onCommand(IDCANCEL, 0);
}

static std::mutex findOps_mutex;

intptr_t FindReplaceDlg::onCommand(WPARAM wParam, [[maybe_unused]] LPARAM lParam)
{
	static const std::vector<int> inSelActionIds = { IDCCOUNTALL, IDC_FINDALL_CURRENTFILE, IDREPLACEALL, IDCMARKALL, IDC_CLEAR_ALL };
	if (_inSelectionAutoChangedAt != 0)
	{
		// The click that activated this dialog arrives right after the "In selection" checkbox changed by itself.
		// To prevent the search from running with this unintended state, it is dropped, and the user alerted.
		const bool isTooSoon = (QDateTime::currentMSecsSinceEpoch() - _inSelectionAutoChangedAt) < 500;
		_inSelectionAutoChangedAt = 0;
		if (isTooSoon && std::find(inSelActionIds.begin(), inSelActionIds.end(), LOWORD(wParam)) != inSelActionIds.end())
		{
			(NppParameters::getInstance().getNativeLangSpeaker())->messageBox(
				"FindAutoChangeOfInSelectionWarning",
				_hSelf,
				L"The \"In selection\" checkbox state has been automatically modified.\r\n"
				L"Please verify the search condition before performing the action.",
				L"Search warning",
				MB_OK | MB_APPLMODAL);
			return TRUE;
		}
	}

	bool isMacroRecording = (static_cast<MacroStatus>(::SendMessage(_hParent, NPPM_GETCURRENTMACROSTATUS,0,0)) == MacroStatus::RecordInProgress);
	NppParameters& nppParamInst = NppParameters::getInstance();
	FindHistory & findHistory = nppParamInst.getFindHistory();
	switch (LOWORD(wParam))
	{
		case IDFINDWHAT:
		case IDREPLACEWITH:
		{
			if (LOWORD(wParam) == IDFINDWHAT && (HIWORD(wParam) == CBN_EDITUPDATE || HIWORD(wParam) == CBN_SELCHANGE))
				updateMatchHighlightingLater();

			if (HIWORD(wParam) == CBN_EDITUPDATE)
			{
				QComboBox* hComboBox = combo(LOWORD(wParam));
				const int maxLength = hComboBox->lineEdit()->maxLength();
				const qsizetype length = hComboBox->lineEdit()->text().length();

				if (length >= maxLength) // should be (FINDREPLACE_MAXLENGTH * 2 - 1)
				{
					if (!_maxLenOnSearchTip.isValid()) // Create the tooltip and add the tool ONLY ONCE
					{
						NativeLangSpeaker* pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
						wstring tip = pNativeSpeaker->getLocalizedStrFromID("len-limit-exceeded-tip", L"Length limit exceeded: Your input may exceed the limit allowed and could have been truncated, and it won't be saved for the next session.");

						static wstring maxLenOnSearchTip = tip;

						bool isSuccessful = _maxLenOnSearchTip.init(hComboBox, maxLenOnSearchTip);

						if (!isSuccessful)
						{
							return FALSE;
						}
					}
					_maxLenOnSearchTip.show(ControlInfoTip::showPosition::beginning);
					
				}
				else if (length > FINDREPLACE_MAXLENGTH - 1)
				{
					if (!_maxLenOnSearchTip.isValid()) // Create the tooltip and add the tool ONLY ONCE
					{
						NativeLangSpeaker* pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
						wstring tip = pNativeSpeaker->getLocalizedStrFromID("max-len-on-search-tip", L"Your input may exceed the limit allowed and could have been truncated, and it won't be saved for the next session.");

						static wstring maxLenOnSearchTip = tip;

						bool isSuccessful = _maxLenOnSearchTip.init(hComboBox, maxLenOnSearchTip);

						if (!isSuccessful)
						{
							return FALSE;
						}
					}
					_maxLenOnSearchTip.show(ControlInfoTip::showPosition::beginning);
				}
				else if (length >= FINDREPLACE_MAXLENGTH2SAVE - 1) // FINDREPLACE_MAXLENGTH2SAVE < length < FINDREPLACE_MAXLENGTH
				{
					if (!_maxLenOnSearchTip.isValid()) // Create the tooltip and add the tool ONLY ONCE
					{
						NativeLangSpeaker* pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
						wstring tip = pNativeSpeaker->getLocalizedStrFromID("max-len-on-save-tip", L"The length of your input is very long and may not be saved for your next session.");

						static wstring maxLenOnSaveTip = tip;

						bool isSuccessful = _maxLenOnSearchTip.init(hComboBox, maxLenOnSaveTip);

						if (!isSuccessful)
						{
							return FALSE;
						}
					}
					_maxLenOnSearchTip.show(ControlInfoTip::showPosition::beginning);
				}
				else
				{
					if (_maxLenOnSearchTip.isValid())
					{
						_maxLenOnSearchTip.hide();
					}
				}

				// Remove status bar warning if it's present
				removeStatusMessageWithInvisibleCharsWarning();
			}
			else if (HIWORD(wParam) == CBN_KILLFOCUS || HIWORD(wParam) == CBN_SELCHANGE)
			{
				if (_maxLenOnSearchTip.isValid())
				{
					_maxLenOnSearchTip.hide();
				}

				if (HIWORD(wParam) == CBN_SELCHANGE)
				{
					// Remove status bar warning if it's present
					removeStatusMessageWithInvisibleCharsWarning();
				}
			}
			return TRUE;
		}
//Single actions
		case IDC_2_BUTTONS_MODE:
		{
			bool is2ButtonsMode = isCheckedOrNot(IDC_2_BUTTONS_MODE);
			findHistory._isSearch2ButtonsMode = is2ButtonsMode;

			showFindDlgItem(IDC_FINDPREV, is2ButtonsMode);
			showFindDlgItem(IDC_FINDNEXT, is2ButtonsMode);
			showFindDlgItem(IDOK, !is2ButtonsMode);
		}
		break;

		case IDCANCEL:
			(*_ppEditView)->execute(SCI_CALLTIPCANCEL);
			setStatusbarMessage(wstring(), FSNoMessage);
			display(false);
			break;

		case IDM_SEARCH_FINDNEXT:
		case IDM_SEARCH_FINDPREV:
		{
			if (HIWORD(wParam) != 1 ||
				(getCurrentStatus() != DIALOG_TYPE::FIND_DLG &&
				getCurrentStatus() != DIALOG_TYPE::REPLACE_DLG)
				|| (LOWORD(wParam) == IDM_SEARCH_FINDPREV &&
					(_options._searchType == FindRegex) &&
					!nppParamInst.regexBackward4PowerUser()))
			{
				return FALSE;
			}
			[[fallthrough]];
		}
		case IDC_FINDPREV:
		case IDC_FINDNEXT:
		case IDOK : // Find Next : only for FIND_DLG and REPLACE_DLG
		{
			setStatusbarMessage(wstring(), FSNoMessage);

			HWND hFindCombo = dlgItem(IDFINDWHAT);
			_options._str2Search = getTextFromCombo(hFindCombo);
			updateCombo(IDFINDWHAT);

			nppParamInst._isFindReplacing = true;

			bool direction_bak = _options._whichDirection;

			if (LOWORD(wParam) == IDC_FINDPREV || LOWORD(wParam) == IDM_SEARCH_FINDPREV)
			{
				_options._whichDirection = DIR_UP;
			}
			else if (LOWORD(wParam) == IDC_FINDNEXT || LOWORD(wParam) == IDM_SEARCH_FINDNEXT)
			{
				_options._whichDirection = DIR_DOWN;
			}
			else // IDOK
			{
				// if shift-key is pressed, revert search direction
				// if shift-key is not pressed, use the normal setting
				if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
				{
					_options._whichDirection = !_options._whichDirection;
				}
			}

			if ((_options._whichDirection == DIR_UP) && (_options._searchType == FindRegex) && !nppParamInst.regexBackward4PowerUser())
			{
				// this can only happen when shift-key was pressed
				// regex upward search is disabled
				// turn user action into a no-action step

				regexBackwardMsgBox();
			}
			else
			{
				if (isMacroRecording)
					saveInMacro(IDOK, FR_OP_FIND);

				FindStatus findStatus = FSFound;
				processFindNext(_options._str2Search.c_str(), _env, &findStatus);

				NativeLangSpeaker *pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
				if (findStatus == FSEndReached)
				{
					wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-end-reached", FIND_STATUS_END_REACHED_TEXT);
					setStatusbarMessage(msg, FSEndReached);
				}
				else if (findStatus == FSTopReached)
				{
					wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-top-reached", FIND_STATUS_TOP_REACHED_TEXT);
					setStatusbarMessage(msg, FSTopReached);
				}
			}

			// restore search direction which may have been overwritten because shift-key was pressed
			_options._whichDirection = direction_bak;

			nppParamInst._isFindReplacing = false;
		}
		return TRUE;
			
		case IDC_SWAP_FIND_REPLACE:
		{
			if (_swapButtonStatus != swap)
			{
				_swapButtonStatus = swap;
				_ui->findreplaceSwapButton->setText(QString::fromWCharArray(L"⇅"));
			}
			onCommand(IDD_FINDREPLACE_SWAP_BUTTON, 0);
			return TRUE;
		}

		case IDC_COPY_FIND2REPLACE:
		{
			if (_swapButtonStatus != down)
			{
				_swapButtonStatus = down;
				_ui->findreplaceSwapButton->setText(QString::fromWCharArray(L"⤵"));
			}
			onCommand(IDD_FINDREPLACE_SWAP_BUTTON, 0);
			return TRUE;
		}

		case IDC_COPY_REPLACE2FIND:
		{
			if (_swapButtonStatus != up)
			{
				_swapButtonStatus = up;
				_ui->findreplaceSwapButton->setText(QString::fromWCharArray(L"⤴"));
			}
			onCommand(IDD_FINDREPLACE_SWAP_BUTTON, 0);
			return TRUE;
		}

		case IDD_FINDREPLACE_SWAP_BUTTON:
		{
			HWND hFindWhat = dlgItem(IDFINDWHAT);
			wstring findWhatText = getTextFromCombo(hFindWhat);
			HWND hPlaceWith = dlgItem(IDREPLACEWITH);
			wstring replaceWithText = getTextFromCombo(hPlaceWith);

			
			if (_swapButtonStatus == swap)
			{
				if ((!findWhatText.empty() || !replaceWithText.empty()) && (findWhatText != replaceWithText))
				{
					setItemText(IDFINDWHAT, replaceWithText);
					setItemText(IDREPLACEWITH, findWhatText);
				}
			}
			else if (_swapButtonStatus == down)
			{
				if (!findWhatText.empty() && (findWhatText != replaceWithText))
				{
					setItemText(IDREPLACEWITH, findWhatText);
				}
			}
			else // if (_swapButtonStatus == up)
			{
				if (!replaceWithText.empty() && (findWhatText != replaceWithText))
				{
					setItemText(IDFINDWHAT, replaceWithText);
				}
			}
		}
		return TRUE;

		case IDM_SEARCH_FIND:
			if (_currentStatus == FIND_DLG)
			{
				goToCenter();
			}
			else
				enableReplaceFunc(false);
			return TRUE;

		case IDM_SEARCH_REPLACE:
			if (_currentStatus == REPLACE_DLG)
			{
				goToCenter();
			}
			else
				enableReplaceFunc(true);
			return TRUE;

		case IDM_SEARCH_FINDINFILES:
			if (_currentStatus == FINDINFILES_DLG)
			{
				goToCenter();
			}
			else
				enableFindInFilesFunc();
			return TRUE;

		case IDM_SEARCH_MARK:
			if (_currentStatus == MARK_DLG)
			{
				goToCenter();
			}
			else
				enableMarkFunc();
			return TRUE;

		case IDC_NEXT_TAB:
		case IDC_PREV_TAB:
		{
			const int selTabIdx = _tab.getNextOrPrevTabIdx(LOWORD(wParam) == IDC_NEXT_TAB);

			switch (static_cast<DIALOG_TYPE>(selTabIdx))
			{
				case DIALOG_TYPE::FIND_DLG:
				{
					enableReplaceFunc(false);
					break;
				}

				case DIALOG_TYPE::REPLACE_DLG:
				{
					enableReplaceFunc(true);
					break;
				}

				case DIALOG_TYPE::FINDINFILES_DLG:
				{
					enableFindInFilesFunc();
					break;
				}

				case DIALOG_TYPE::FINDINPROJECTS_DLG:
				{
					enableFindInProjectsFunc();
					break;
				}

				case DIALOG_TYPE::MARK_DLG:
				{
					enableMarkFunc();
					break;
				}
			}

			return TRUE;
		}

		case IDREPLACE:
		{
			std::lock_guard<std::mutex> lock(findOps_mutex);

			if (_currentStatus == REPLACE_DLG)
			{
				setStatusbarMessage(L"", FSNoMessage);
				HWND hFindCombo = dlgItem(IDFINDWHAT);
				HWND hReplaceCombo = dlgItem(IDREPLACEWITH);
				_options._str2Search = getTextFromCombo(hFindCombo);
				_options._str4Replace = getTextFromCombo(hReplaceCombo);
				updateCombos();

				nppParamInst._isFindReplacing = true;
				if (isMacroRecording) saveInMacro(wParam, FR_OP_REPLACE);
				processReplace(_options._str2Search.c_str(), _options._str4Replace.c_str());
				nppParamInst._isFindReplacing = false;
			}
		}
		return TRUE;

		case IDC_REPLACEANDSTOP:
		{
			NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();
			nppGUI._replaceStopsWithoutFindingNext = isCheckedOrNot(IDC_REPLACEANDSTOP);
		}
		return TRUE;

		case IDC_HIGHLIGHT_MODE:
		{
			findHistory._findDlgHighlightMatches = isCheckedOrNot(IDC_HIGHLIGHT_MODE);
			updateMatchHighlighting();
			return TRUE;
		}

//Process actions
		case IDC_FINDALL_OPENEDFILES :
		{
			if (_currentStatus == FIND_DLG)
			{
				setStatusbarMessage(L"", FSNoMessage);
				HWND hFindCombo = dlgItem(IDFINDWHAT);
				_options._str2Search = getTextFromCombo(hFindCombo);
				updateCombo(IDFINDWHAT);

				nppParamInst._isFindReplacing = true;
				if (isMacroRecording) saveInMacro(wParam, FR_OP_FIND + FR_OP_GLOBAL);
				findAllIn(ALL_OPEN_DOCS);
				nppParamInst._isFindReplacing = false;
			}
		}
		return TRUE;

		case IDC_FINDALL_CURRENTFILE :
		{
			setStatusbarMessage(L"", FSNoMessage);
			HWND hFindCombo = dlgItem(IDFINDWHAT);
			_options._str2Search = getTextFromCombo(hFindCombo);
			updateCombo(IDFINDWHAT);

			nppParamInst._isFindReplacing = true;
			if (isMacroRecording) saveInMacro(wParam, FR_OP_FIND + FR_OP_GLOBAL);
			findAllIn(_options._isInSelection ? CURR_DOC_SELECTION : CURRENT_DOC);
			nppParamInst._isFindReplacing = false;
		}
		return TRUE;

		case IDD_FINDINFILES_FIND_BUTTON:
		{
			setStatusbarMessage(L"", FSNoMessage);
			const std::wstring filters = getItemText(IDD_FINDINFILES_FILTERS_COMBO);
			addText2Combo(filters.c_str(), dlgItem(IDD_FINDINFILES_FILTERS_COMBO));
			_options._filters = filters;

			HWND hFindCombo = dlgItem(IDFINDWHAT);
			_options._str2Search = getTextFromCombo(hFindCombo);
			updateCombo(IDFINDWHAT);

			if (_currentStatus == FINDINFILES_DLG)
			{
				_options._directory = getItemText(IDD_FINDINFILES_DIR_COMBO);
				trim(_options._directory);
				if (!_options._directory.empty())
				{
					addText2Combo(_options._directory.c_str(), dlgItem(IDD_FINDINFILES_DIR_COMBO));
					if (_options._directory.back() != nppPathSep)
					{
						_options._directory += nppPathSepStr;
					}
					nppParamInst._isFindReplacing = true;
					if (isMacroRecording) saveInMacro(wParam, FR_OP_FIND + FR_OP_FIF);
					findAllIn(FILES_IN_DIR);
					nppParamInst._isFindReplacing = false;
				}
				else
				{
					// move input focus to "Directory:" edit control
					dlgItem(IDD_FINDINFILES_DIR_COMBO)->setFocus();
				}
			}
			else if (_currentStatus == FINDINPROJECTS_DLG)
			{
				if (_options._isProjectPanel_1 || _options._isProjectPanel_2 || _options._isProjectPanel_3)
				{
					nppParamInst._isFindReplacing = true;
					if (isMacroRecording) saveInMacro(IDD_FINDINFILES_FINDINPROJECTS, FR_OP_FIND + FR_OP_FIP);
					findAllIn(FILES_IN_PROJECTS);
					nppParamInst._isFindReplacing = false;
				}
			}
		}
		return TRUE;

		case IDD_FINDINFILES_REPLACEINFILES:
		{
			std::lock_guard<std::mutex> lock(findOps_mutex);

			setStatusbarMessage(L"", FSNoMessage);
			const std::wstring filters = getItemText(IDD_FINDINFILES_FILTERS_COMBO);
			addText2Combo(filters.c_str(), dlgItem(IDD_FINDINFILES_FILTERS_COMBO));
			_options._filters = filters;

			_options._directory = getItemText(IDD_FINDINFILES_DIR_COMBO);
			trim(_options._directory);
			if (!_options._directory.empty())
			{
				if (replaceInFilesConfirmCheck(_options._directory, _options._filters))
				{
					addText2Combo(_options._directory.c_str(), dlgItem(IDD_FINDINFILES_DIR_COMBO));
					if (_options._directory.back() != nppPathSep)
					{
						_options._directory += nppPathSepStr;
					}

					HWND hFindCombo = dlgItem(IDFINDWHAT);
					_options._str2Search = getTextFromCombo(hFindCombo);
					HWND hReplaceCombo = dlgItem(IDREPLACEWITH);
					_options._str4Replace = getTextFromCombo(hReplaceCombo);
					updateCombo(IDFINDWHAT);
					updateCombo(IDREPLACEWITH);

					nppParamInst._isFindReplacing = true;
					if (isMacroRecording) saveInMacro(wParam, FR_OP_REPLACE + FR_OP_FIF);
					::SendMessage(_hParent, WM_REPLACEINFILES, 0, 0);
					nppParamInst._isFindReplacing = false;
				}
			}
			else
			{
				// move input focus to "Directory:" edit control
				dlgItem(IDD_FINDINFILES_DIR_COMBO)->setFocus();
			}
		}
		return TRUE;

		case IDD_FINDINFILES_REPLACEINPROJECTS:
		{
			std::lock_guard<std::mutex> lock(findOps_mutex);

			setStatusbarMessage(L"", FSNoMessage);
			const std::wstring filters = getItemText(IDD_FINDINFILES_FILTERS_COMBO);
			addText2Combo(filters.c_str(), dlgItem(IDD_FINDINFILES_FILTERS_COMBO));
			_options._filters = filters;
			if (replaceInProjectsConfirmCheck())
			{
				HWND hFindCombo = dlgItem(IDFINDWHAT);
				_options._str2Search = getTextFromCombo(hFindCombo);
				HWND hReplaceCombo = dlgItem(IDREPLACEWITH);
				_options._str4Replace = getTextFromCombo(hReplaceCombo);
				updateCombo(IDFINDWHAT);
				updateCombo(IDREPLACEWITH);

				nppParamInst._isFindReplacing = true;
				if (isMacroRecording) saveInMacro(wParam, FR_OP_REPLACE + FR_OP_FIP);
				::SendMessage(_hParent, WM_REPLACEINPROJECTS, 0, 0);
				nppParamInst._isFindReplacing = false;
			}
		}
		return TRUE;

		case IDC_CONFIRMREPLACEOPENDOCS:
		{
			NppGUI& nppGui = nppParamInst.getNppGUI();
			nppGui._confirmReplaceInAllOpenDocs = isCheckedOrNot(IDC_CONFIRMREPLACEOPENDOCS);
			return TRUE;
		}

		case IDC_REPLACE_OPENEDFILES :
		{
			std::lock_guard<std::mutex> lock(findOps_mutex);

			if (_currentStatus == REPLACE_DLG)
			{
				const NppGUI& nppGui = nppParamInst.getNppGUI();
				if (!nppGui._confirmReplaceInAllOpenDocs || replaceInOpenDocsConfirmCheck())
				{
					setStatusbarMessage(L"", FSNoMessage);
					HWND hFindCombo = dlgItem(IDFINDWHAT);
					_options._str2Search = getTextFromCombo(hFindCombo);
					HWND hReplaceCombo = dlgItem(IDREPLACEWITH);
					_options._str4Replace = getTextFromCombo(hReplaceCombo);
					updateCombos();

					nppParamInst._isFindReplacing = true;
					if (isMacroRecording) saveInMacro(wParam, FR_OP_REPLACE + FR_OP_GLOBAL);
					replaceAllInOpenedDocs();
					nppParamInst._isFindReplacing = false;
				}
			}
		}
		return TRUE;

		case IDREPLACEALL :
		{
			std::lock_guard<std::mutex> lock(findOps_mutex);

			if (_currentStatus == REPLACE_DLG)
			{
				setStatusbarMessage(L"", FSNoMessage);
				if ((*_ppEditView)->getCurrentBuffer()->isReadOnly())
				{
					NativeLangSpeaker *pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
					wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replace-readonly", L"Replace: Cannot replace text. The current document is read only.");
					setStatusbarMessage(msg, FSNotFound);
					return TRUE;
				}

				HWND hFindCombo = dlgItem(IDFINDWHAT);
				_options._str2Search = getTextFromCombo(hFindCombo);
				HWND hReplaceCombo = dlgItem(IDREPLACEWITH);
				_options._str4Replace = getTextFromCombo(hReplaceCombo);
				updateCombos();

				nppParamInst._isFindReplacing = true;
				if (isMacroRecording) saveInMacro(wParam, FR_OP_REPLACE);
				(*_ppEditView)->execute(SCI_BEGINUNDOACTION);
				int nbReplaced = processAll(ProcessReplaceAll, &_options);
				(*_ppEditView)->execute(SCI_ENDUNDOACTION);
				nppParamInst._isFindReplacing = false;

				
				NativeLangSpeaker *pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
				if (nbReplaced == FIND_INVALID_REGULAR_EXPRESSION)
				{
					setStatusbarMessageWithRegExprErr(*_ppEditView);
				}
				else
				{
					wstring result;
					if (nbReplaced == 1)
					{
						result = pNativeSpeaker->getLocalizedStrFromID("find-status-replaceall-1-replaced", L"Replace All: 1 occurrence was replaced");
					}
					else
					{
						result = pNativeSpeaker->getLocalizedStrFromID("find-status-replaceall-nb-replaced", L"Replace All: $INT_REPLACE$ occurrences were replaced");
						result = stringReplace(result, L"$INT_REPLACE$", std::to_wstring(nbReplaced));
					}
					result += L" ";
					result += getScopeInfoForStatusBar(&_options);

					wstring reasonMsg;
					bool isTheMostLaxMode = _options._isWrapAround && !_options._isMatchCase && !_options._isWholeWord;
					if (nbReplaced == 0 && !isTheMostLaxMode)
					{
						reasonMsg = pNativeSpeaker->getLocalizedStrFromID("find-status-cannot-find-pebkac-maybe", noFoundPotentialReason);
					}

					setStatusbarMessage(result, FSMessage, reasonMsg);
				}
				grabFocus();
			}
		}
		return TRUE;

		case IDCCOUNTALL :
		{
			if (_currentStatus == FIND_DLG)
			{
				setStatusbarMessage(L"", FSNoMessage);
				HWND hFindCombo = dlgItem(IDFINDWHAT);
				updateCombo(IDFINDWHAT);
				_options._str2Search = getTextFromCombo(hFindCombo);

				int nbCounted = processAll(ProcessCountAll, &_options);

				NativeLangSpeaker *pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
				if (nbCounted == FIND_INVALID_REGULAR_EXPRESSION)
				{
					setStatusbarMessageWithRegExprErr(*_ppEditView);
				}
				else
				{
					wstring result;
					if (nbCounted == 1)
					{
						result = pNativeSpeaker->getLocalizedStrFromID("find-status-count-1-match", L"Count: 1 match");
					}
					else //if (nbCounted == 0 || nbCounted > 1)
					{
						result = pNativeSpeaker->getLocalizedStrFromID("find-status-count-nb-matches", L"Count: $INT_REPLACE$ matches");
						result = stringReplace(result, L"$INT_REPLACE$", commafyInt(nbCounted));
					}
					result += L" ";
					result += getScopeInfoForStatusBar(&_options);

					wstring reasonMsg;
					bool isTheMostLaxMode = _options._isWrapAround && !_options._isMatchCase && !_options._isWholeWord;
					if (nbCounted == 0 && !isTheMostLaxMode)
					{
						reasonMsg = pNativeSpeaker->getLocalizedStrFromID("find-status-cannot-find-pebkac-maybe", noFoundPotentialReason);
					}
					else if (_options._regexEmptyStringFound)
					{
						reasonMsg = pNativeSpeaker->getLocalizedStrFromID("find-status-regex-find-empty-string", regexEmptyFoundReason);
					}


					setStatusbarMessage(result, FSMessage, reasonMsg);
				}

				if (isMacroRecording)
					saveInMacro(wParam, FR_OP_FIND);
				
				grabFocus();
			}
		}
		return TRUE;

		case IDCMARKALL :
		{
			if (_currentStatus == MARK_DLG)
			{
				setStatusbarMessage(L"", FSNoMessage);
				HWND hFindCombo = dlgItem(IDFINDWHAT);
				_options._str2Search = getTextFromCombo(hFindCombo);
				updateCombo(IDFINDWHAT);

				if (isMacroRecording) saveInMacro(wParam, FR_OP_FIND);
				nppParamInst._isFindReplacing = true;
				int nbMarked = processAll(ProcessMarkAll, &_options);
				nppParamInst._isFindReplacing = false;

				
				NativeLangSpeaker *pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
				if (nbMarked == FIND_INVALID_REGULAR_EXPRESSION)
				{
					setStatusbarMessageWithRegExprErr(*_ppEditView);
				}
				else
				{
					std::wstring result;
					if (nbMarked == 1)
					{
						result = pNativeSpeaker->getLocalizedStrFromID("find-status-mark-1-match", L"Mark: 1 match");
					}
					else //if (nbMarked == 0 || nbMarked > 1)
					{
						result = pNativeSpeaker->getLocalizedStrFromID("find-status-mark-nb-matches", L"Mark: $INT_REPLACE$ matches");
						result = stringReplace(result, L"$INT_REPLACE$", std::to_wstring(nbMarked));
					}
					result += L" ";
					result += getScopeInfoForStatusBar(&_options);
					
					wstring reasonMsg;
					bool isTheMostLaxMode = _options._isWrapAround && !_options._isMatchCase && !_options._isWholeWord;
					if (nbMarked == 0 && !isTheMostLaxMode)
					{
						reasonMsg = pNativeSpeaker->getLocalizedStrFromID("find-status-cannot-find-pebkac-maybe", noFoundPotentialReason);
					}
					else if (_options._regexEmptyStringFound)
					{
						reasonMsg = pNativeSpeaker->getLocalizedStrFromID("find-status-regex-find-empty-string", regexEmptyFoundReason);
					}

					setStatusbarMessage(result, FSMessage, reasonMsg);
				}
				
				grabFocus();
			}
		}
		return TRUE;

		case IDC_CLEAR_ALL :
		{
			if (_currentStatus == MARK_DLG)
			{
				if (isMacroRecording) saveInMacro(wParam, FR_OP_FIND);
				clearMarks(_options);
			}
		}
		return TRUE;

		case IDC_COPY_MARKED_TEXT:
		{
			::SendMessage(_hParent, WM_COMMAND, IDM_SEARCH_MARKEDTOCLIP, 0);
		}
		return TRUE;

//Option actions
		case IDREDOTMATCHNL:
			findHistory._dotMatchesNewline = _options._dotMatchesNewline = isCheckedOrNot(IDREDOTMATCHNL);
			updateMatchHighlighting();
			return TRUE;

		case IDWHOLEWORD :
			findHistory._isMatchWord = _options._isWholeWord = isCheckedOrNot(IDWHOLEWORD);
			updateMatchHighlighting();
			return TRUE;

		case IDMATCHCASE :
			findHistory._isMatchCase = _options._isMatchCase = isCheckedOrNot(IDMATCHCASE);
			updateMatchHighlighting();
			return TRUE;

		case IDNORMAL:
		case IDEXTENDED:
		case IDREGEXP :
		{
			if (isCheckedOrNot(IDREGEXP))
			{
				_options._searchType = FindRegex;
				findHistory._searchMode = FindHistory::regExpr;
				enableFindDlgItem(IDREDOTMATCHNL);
			}
			else if (isCheckedOrNot(IDEXTENDED))
			{
				_options._searchType = FindExtended;
				findHistory._searchMode = FindHistory::extended;
				enableFindDlgItem(IDREDOTMATCHNL, false);
			}
			else
			{
				_options._searchType = FindNormal;
				findHistory._searchMode = FindHistory::normal;
				enableFindDlgItem(IDREDOTMATCHNL, false);
			}

			bool isRegex = (_options._searchType == FindRegex);
			if (isRegex)
			{
				//regex doesn't allow whole word
				_options._isWholeWord = false;
				setChecked(IDWHOLEWORD, _options._isWholeWord);

				//regex upward search is disabled
				if (!nppParamInst.regexBackward4PowerUser())
				{
					setChecked(IDC_BACKWARDDIRECTION, false);
					_options._whichDirection = DIR_DOWN;
				}
			}

			enableFindDlgItem(IDWHOLEWORD, !isRegex);

			// regex upward search is disabled
			bool doEnable = true;
			if (isRegex && !nppParamInst.regexBackward4PowerUser())
			{
				doEnable = false;
			}
			enableFindDlgItem(IDC_BACKWARDDIRECTION, doEnable);
			enableFindDlgItem(IDC_FINDPREV, doEnable);

			updateMatchHighlighting();
			return TRUE;
		}

		case IDWRAP :
			findHistory._isWrap = _options._isWrapAround = isCheckedOrNot(IDWRAP);
			return TRUE;

		case IDC_BACKWARDDIRECTION:
			_options._whichDirection = isCheckedOrNot(IDC_BACKWARDDIRECTION) ? DIR_UP : DIR_DOWN;
			findHistory._isDirectionDown = _options._whichDirection == DIR_DOWN;
			return TRUE;

		case IDC_PURGE_CHECK :
		{
			if (_currentStatus == MARK_DLG)
				findHistory._isPurge = _options._doPurge = isCheckedOrNot(IDC_PURGE_CHECK);
		}
		return TRUE;

		case IDC_MARKLINE_CHECK :
		{
			if (_currentStatus == MARK_DLG)
				findHistory._isBookmarkLine = _options._doMarkLine = isCheckedOrNot(IDC_MARKLINE_CHECK);
		}
		return TRUE;

		case IDC_IN_SELECTION_CHECK :
		{
			if ((_currentStatus == FIND_DLG) || (_currentStatus == REPLACE_DLG) || (_currentStatus == MARK_DLG))
			{
				_options._isInSelection = dlgItem(IDC_IN_SELECTION_CHECK)->isEnabled() &&
					isCheckedOrNot(IDC_IN_SELECTION_CHECK);
			}
		}
		return TRUE;

		case IDC_TRANSPARENT_CHECK :
		{
			bool isChecked = isCheckedOrNot(IDC_TRANSPARENT_CHECK);

			enableFindDlgItem(IDC_TRANSPARENT_LOSSFOCUS_RADIO, isChecked);
			enableFindDlgItem(IDC_TRANSPARENT_ALWAYS_RADIO, isChecked);
			enableFindDlgItem(IDC_PERCENTAGE_SLIDER, isChecked);

			if (isChecked)
			{
				setChecked(IDC_TRANSPARENT_LOSSFOCUS_RADIO, true);
				findHistory._transparencyMode = FindHistory::onLosingFocus;
			}
			else
			{
				setChecked(IDC_TRANSPARENT_LOSSFOCUS_RADIO, false);
				setChecked(IDC_TRANSPARENT_ALWAYS_RADIO, false);
				nppParamInst.removeTransparent(_hSelf);
				findHistory._transparencyMode = FindHistory::none;
			}

			return TRUE;
		}

		case IDC_TRANSPARENT_ALWAYS_RADIO :
		{
			const int percent = _ui->percentageSlider->value();
			nppParamInst.setTransparent(_hSelf, percent);
			findHistory._transparencyMode = FindHistory::persistent;
		}
		return TRUE;

		case IDC_TRANSPARENT_LOSSFOCUS_RADIO :
		{
			nppParamInst.removeTransparent(_hSelf);
			findHistory._transparencyMode = FindHistory::onLosingFocus;
		}
		return TRUE;

		//
		// Find in Files
		//
		case IDD_FINDINFILES_RECURSIVE_CHECK :
		{
			if (_currentStatus == FINDINFILES_DLG)
				findHistory._isFifRecursive = _options._isRecursive = isCheckedOrNot(IDD_FINDINFILES_RECURSIVE_CHECK);

		}
		return TRUE;

		case IDD_FINDINFILES_INHIDDENDIR_CHECK :
		{
			if (_currentStatus == FINDINFILES_DLG)
				findHistory._isFifInHiddenFolder = _options._isInHiddenDir = isCheckedOrNot(IDD_FINDINFILES_INHIDDENDIR_CHECK);
		}
		return TRUE;

		case IDD_FINDINFILES_PROJECT1_CHECK:
		case IDD_FINDINFILES_PROJECT2_CHECK:
		case IDD_FINDINFILES_PROJECT3_CHECK:
		{
			if (_currentStatus == FINDINPROJECTS_DLG)
			{
				switch (LOWORD(wParam))
				{
					case IDD_FINDINFILES_PROJECT1_CHECK:
						findHistory._isFifProjectPanel_1 = _options._isProjectPanel_1 = isCheckedOrNot(IDD_FINDINFILES_PROJECT1_CHECK);
						break;
					case IDD_FINDINFILES_PROJECT2_CHECK:
						findHistory._isFifProjectPanel_2 = _options._isProjectPanel_2 = isCheckedOrNot(IDD_FINDINFILES_PROJECT2_CHECK);
						break;
					case IDD_FINDINFILES_PROJECT3_CHECK:
						findHistory._isFifProjectPanel_3 = _options._isProjectPanel_3 = isCheckedOrNot(IDD_FINDINFILES_PROJECT3_CHECK);
						break;
				}
				bool enable = _options._isProjectPanel_1 || _options._isProjectPanel_2 || _options._isProjectPanel_3;
				enableFindDlgItem(IDD_FINDINFILES_FIND_BUTTON, enable);
				enableFindDlgItem(IDD_FINDINFILES_REPLACEINPROJECTS, enable);
			}
		}
		return TRUE;

		case IDD_FINDINFILES_SETDIRFROMDOC_BUTTON :
		{
			wstring currPath;
			const Buffer* buf = (*_ppEditView)->getCurrentBuffer();
			if (!(buf->getStatus() & (DOC_UNNAMED | DOC_DELETED)))
			{
				currPath = buf->getFullPathName();
				pathRemoveFileSpec(currPath);
			}

			if (!currPath.empty() && doesDirectoryExist(currPath.c_str()))
			{
				setFindInFilesDirFilter(currPath.c_str(), NULL);
			}
		}
		return TRUE;

		case IDD_FINDINFILES_BROWSE_BUTTON :
		{
			if (_currentStatus == FINDINFILES_DLG)
			{
				NativeLangSpeaker* pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
				const wstring title = pNativeSpeaker->getLocalizedStrFromID("find-in-files-select-folder", L"Select a folder to search from");
				folderBrowser(_hSelf, title, _ui->findinfilesDirCombo->lineEdit(), _options._directory.c_str());
			}
		}
		return TRUE;

		case IDD_RESIZE_TOGGLE_BUTTON:
		{
			bool& isLessModeOn = nppParamInst.getNppGUI()._findWindowLessMode;
				isLessModeOn = !isLessModeOn;

				hideOrShowCtrl4reduceOrNormalMode(getCurrentStatus());

				setItemText(IDD_RESIZE_TOGGLE_BUTTON, isLessModeOn ? L"˅" : L"˄");

				fitHeight();
		}
		return TRUE;

		default :
			break;
	}
	return FALSE;
}

// return value :
// true  : the text2find is found
// false : the text2find is not found

bool FindReplaceDlg::processFindNext(const wchar_t* txt2find,
	const FindOption* options, FindStatus* oFindStatus, FindNextType findNextType /* = FINDNEXTTYPE_FINDNEXT */, MatchPosition* pMatch)
{
	if (oFindStatus)
		*oFindStatus = FSFound;

	if (!txt2find || !txt2find[0])
		return false;

	const FindOption *pOptions = options?options:_env;
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	// If txt2find contains non-ANSI characters and document type is ANSI, search cannot match
	if (isSearchUnicodeCharOnAnsi(txt2find))
	{
		if (oFindStatus)
			*oFindStatus = FSNotFound;

		// Show warning for regex (could work in theory, but we disallow it to prevent unexpected behavior)
		if (pOptions->_incrementalType == NotIncremental) //incremental search doesn't trigger messages
		{
			setStatusMessageNotFound(txt2find);

			// if the dialog is not shown, pass the focus to his parent(ie. Notepad++)
			if (!_hSelf->isVisible())
			{
				(*_ppEditView)->grabFocus();
			}
			else
			{
				dlgItem(IDFINDWHAT)->setFocus();
			}
		}
		return false;

	}

	(*_ppEditView)->execute(SCI_CALLTIPCANCEL);

	int stringSizeFind = lstrlen(txt2find);
	wchar_t *pText = new wchar_t[stringSizeFind + 1];
	wcscpy_s(pText, stringSizeFind + 1, txt2find);

	if (pOptions->_searchType == FindExtended)
	{
		stringSizeFind = Searching::convertExtendedToString(txt2find, pText, stringSizeFind);
	}

	intptr_t docLength = (*_ppEditView)->execute(SCI_GETLENGTH);
	Sci_CharacterRangeFull cr = (*_ppEditView)->getSelection();


	//The search "zone" is relative to the selection, so search happens 'outside'
	intptr_t startPosition = cr.cpMax;
	intptr_t endPosition = docLength;


	if (pOptions->_whichDirection == DIR_UP)
	{
		//When searching upwards, start is the lower part, end the upper, for backwards search
		startPosition = cr.cpMax - 1;
		endPosition = 0;
	}

	if (FirstIncremental==pOptions->_incrementalType)
	{
		// the text to find is modified so use the current position
		startPosition = cr.cpMin;
		endPosition = docLength;

		if (pOptions->_whichDirection == DIR_UP)
		{
			//When searching upwards, start is the lower part, end the upper, for backwards search
			startPosition = cr.cpMax;
			endPosition = 0;
		}
	}
	else if (NextIncremental==pOptions->_incrementalType)
	{
		// text to find is not modified, so use current position +1
		startPosition = cr.cpMin + 1;
		endPosition = docLength;

		if (pOptions->_whichDirection == DIR_UP)
		{
			//When searching upwards, start is the lower part, end the upper, for backwards search
			startPosition = cr.cpMax - 1;
			endPosition = 0;
		}
	}

	int flags = Searching::buildSearchFlags(pOptions);
	switch (findNextType)
	{
		case FINDNEXTTYPE_FINDNEXT:
			flags |= SCFIND_REGEXP_EMPTYMATCH_ALL | SCFIND_REGEXP_SKIPCRLFASONE;
		break;

		case FINDNEXTTYPE_REPLACENEXT:
			flags |= SCFIND_REGEXP_EMPTYMATCH_NOTAFTERMATCH | SCFIND_REGEXP_SKIPCRLFASONE;
			break;

		case FINDNEXTTYPE_FINDNEXTFORREPLACE:
			flags |= SCFIND_REGEXP_EMPTYMATCH_ALL | SCFIND_REGEXP_EMPTYMATCH_ALLOWATSTART | SCFIND_REGEXP_SKIPCRLFASONE;
			break;
	}

	intptr_t start, end;
	intptr_t posFind;

	// Never allow a zero length match in the middle of a line end marker
	if ((*_ppEditView)->execute(SCI_GETCHARAT, startPosition - 1) == '\r'
		&& (*_ppEditView)->execute(SCI_GETCHARAT, startPosition) == '\n')
	{
		flags = (flags & ~SCFIND_REGEXP_EMPTYMATCH_MASK) | SCFIND_REGEXP_EMPTYMATCH_NONE;
	}

	(*_ppEditView)->execute(SCI_SETSEARCHFLAGS, flags);

	posFind = (*_ppEditView)->searchInTarget(pText, stringSizeFind, startPosition, endPosition);
	if (posFind == -1) //no match found in target, check if a new target should be used
	{
		if (pOptions->_isWrapAround)
		{
			//when wrapping, use the rest of the document (entire document is usable)
			if (pOptions->_whichDirection == DIR_DOWN)
			{
				startPosition = 0;
				endPosition = docLength;
				if (oFindStatus)
					*oFindStatus = FSEndReached;
			}
			else
			{
				startPosition = docLength;
				endPosition = 0;
				if (oFindStatus)
					*oFindStatus = FSTopReached;
			}

			//new target, search again
			posFind = (*_ppEditView)->searchInTarget(pText, stringSizeFind, startPosition, endPosition);
		}

		if (posFind == -1)
		{ // not found
			if (oFindStatus)
				*oFindStatus = FSNotFound;
			//failed, or failed twice with wrap
			if (pOptions->_incrementalType == NotIncremental) //incremental search doesn't trigger messages
			{
				wstring reasonMsg;
				bool isTheMostLaxMode = _options._isWrapAround && !_options._isMatchCase && !_options._isWholeWord;
				if (!isTheMostLaxMode)
				{
					reasonMsg = pNativeSpeaker->getLocalizedStrFromID("find-status-cannot-find-pebkac-maybe", noFoundPotentialReason);
				}
				setStatusMessageNotFound(txt2find, reasonMsg);

				// if the dialog is not shown, pass the focus to his parent(ie. Notepad++)
				if (!_hSelf->isVisible())
				{
					(*_ppEditView)->grabFocus();
				}
				else
				{
					dlgItem(IDFINDWHAT)->setFocus();
				}
			}
			delete [] pText;
			return false;
		}
	}
	else if (posFind == FIND_INVALID_REGULAR_EXPRESSION)
	{ // error
		setStatusbarMessageWithRegExprErr(*_ppEditView);
		delete[] pText;
		return false;
	}

	start =	posFind;
	end = (*_ppEditView)->execute(SCI_GETTARGETEND);

	setStatusbarMessage(L"", FSNoMessage);

	// to make sure the found result is visible:
	// prevent recording of absolute positioning commands issued in the process
	(*_ppEditView)->execute(SCI_STOPRECORD);
	Searching::displaySectionCentered(start, end, *_ppEditView, pOptions->_whichDirection == DIR_DOWN);
	// Show a calltip for a zero length match
	if (start == end)
	{
		const std::string msg = "^ " + pNativeSpeaker->getLocalizedStrFromID("find-regex-zero-length-match", "zero length match");
		(*_ppEditView)->showCallTip(start, msg);
	}
	if (static_cast<MacroStatus>(::SendMessage(_hParent, NPPM_GETCURRENTMACROSTATUS,0,0)) == MacroStatus::RecordInProgress)
		(*_ppEditView)->execute(SCI_STARTRECORD);

	delete [] pText;

	if (pMatch)
	{
		pMatch->start = start;
		pMatch->end = end;
	}

	return true;
}

// return value :
// true  : the text is replaced, and find the next occurrence
// false : the text2find is not found, so the text is NOT replace
//      || the text is replaced, and do NOT find the next occurrence
bool FindReplaceDlg::processReplace(const wchar_t *txt2find, const wchar_t *txt2replace, const FindOption *options)
{
	bool moreMatches;

	if (!txt2find || !txt2find[0] || !txt2replace)
		return false;

	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	if ((*_ppEditView)->getCurrentBuffer()->isReadOnly())
	{
		wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replace-readonly", L"Replace: Cannot replace text. The current document is read only.");
		setStatusbarMessage(msg, FSNotFound);
		return false;
	}

	FindOption replaceOptions = options ? *options : *_env;
	replaceOptions._incrementalType = FirstIncremental;

	// If txt2find contains non-ANSI characters and document type is ANSI, search cannot match
	if (isSearchUnicodeCharOnAnsi(txt2find))
	{
		wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replace-not-found", L"Replace: no occurrence was found");

		msg += L" ";
		msg += getScopeInfoForStatusBar(&_options);

		setStatusbarMessage(msg, FSNotFound);

		return false;
	}
	else if (isSearchUnicodeCharOnAnsi(txt2replace))
	{
		wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replace-invalid-replace-chars", L"Replace: can't replace with non-ANSI text in ANSI document");
		setStatusbarMessage(msg, FSNotFound);
		return false;
	}
	
	Sci_CharacterRangeFull currentSelection = (*_ppEditView)->getSelection();
	FindStatus status;
	moreMatches = processFindNext(txt2find, &replaceOptions, &status, FINDNEXTTYPE_FINDNEXTFORREPLACE);

	if (moreMatches)
	{
		Sci_CharacterRangeFull nextFind = (*_ppEditView)->getSelection();

		// If the next find is the same as the last, then perform the replacement
		if (nextFind.cpMin == currentSelection.cpMin && nextFind.cpMax == currentSelection.cpMax)
		{
			bool isRegExp = replaceOptions._searchType == FindRegex;

			intptr_t start = currentSelection.cpMin;
			intptr_t replacedLen = 0;
			if (isRegExp)
			{
				replacedLen = (*_ppEditView)->replaceTargetRegExMode(txt2replace);
			}
			else
			{
				if (replaceOptions._searchType == FindExtended)
				{
					int stringSizeReplace = lstrlen(txt2replace);
					wchar_t *pText2ReplaceExtended = new wchar_t[stringSizeReplace + 1];
					Searching::convertExtendedToString(txt2replace, pText2ReplaceExtended, stringSizeReplace);

					replacedLen = (*_ppEditView)->replaceTarget(pText2ReplaceExtended);

					delete[] pText2ReplaceExtended;
				}
				else
				{
					replacedLen = (*_ppEditView)->replaceTarget(txt2replace);
				}
			}
			(*_ppEditView)->execute(SCI_SETSEL, start + replacedLen, start + replacedLen);

			NppParameters& nppParam = NppParameters::getInstance();
			const NppGUI& nppGui = nppParam.getNppGUI();

			if (nppGui._replaceStopsWithoutFindingNext)
			{
				wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replaced-without-continuing", L"Replace: 1 occurrence was replaced.");
				setStatusbarMessage(msg, FSMessage);
			}
			else
			{
				// Do the next find
				moreMatches = processFindNext(txt2find, &replaceOptions, &status, FINDNEXTTYPE_REPLACENEXT);

				if (status == FSEndReached)
				{
					wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replace-end-reached", FIND_STATUS_REPLACE_END_REACHED_TEXT);
					setStatusbarMessage(msg, FSEndReached);
				}
				else if (status == FSTopReached)
				{
					wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replace-top-reached", FIND_STATUS_REPLACE_TOP_REACHED_TEXT);
					setStatusbarMessage(msg, FSTopReached);
				}
				else
				{
					wstring msg;
					if (moreMatches)
						msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replaced-next-found", L"Replace: 1 occurrence was replaced. The next occurrence found.");
					else
						msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replaced-next-not-found", L"Replace: 1 occurrence was replaced. No more occurrences were found.");

					setStatusbarMessage(msg, FSMessage);
				}
			}
		}
	}
	else
	{
		if (_statusbarTooltipMsg.empty()) // Tooltip message non-empty means there's a find problem - so we keep the message as it is and not erase it
		{
			wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replace-not-found", L"Replace: no occurrence was found");

			msg += L" ";
			msg += getScopeInfoForStatusBar(&_options);

			wstring reasonMsg;
			bool isTheMostLaxMode = _options._isWrapAround && !_options._isMatchCase && !_options._isWholeWord;
			if (!isTheMostLaxMode)
			{
				reasonMsg = pNativeSpeaker->getLocalizedStrFromID("find-status-cannot-find-pebkac-maybe", noFoundPotentialReason);
			}
			setStatusbarMessage(msg, FSNotFound, reasonMsg);
		}
	}

	return moreMatches;
}



int FindReplaceDlg::markAll(const wchar_t *txt2find, int styleID)
{
	const NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();
	FindOption markAllOpt;

	markAllOpt._isMatchCase = nppGUI._markAllCaseSensitive;
	markAllOpt._isWholeWord = nppGUI._markAllWordOnly;
	markAllOpt._str2Search = txt2find;

	int nbFound = processAll(ProcessMarkAllExt, &markAllOpt, true, NULL, styleID);
	return nbFound;
}


int FindReplaceDlg::markAllInc(FindOption *opt)
{
	int nbFound = processAll(ProcessMarkAll_IncSearch, opt,  true);
	return nbFound;
}

int FindReplaceDlg::processAll(ProcessOperation op, FindOption* opt,
	bool isEntire, const FindersInfo* pFindersInfo, int colourStyleID, std::vector<MatchPosition>* pMatches)
{
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	if (op == ProcessReplaceAll && (*_ppEditView)->getCurrentBuffer()->isReadOnly())
	{
		wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replaceall-readonly", L"Replace All: Cannot replace text. The current document is read only.");
		setStatusbarMessage(msg, FSNotFound);
		return 0;
	}

	FindOption* pOptions = opt ? opt : _env;
	const wchar_t* txt2find = pOptions->_str2Search.c_str();
	const wchar_t* txt2replace = pOptions->_str4Replace.c_str();

	// If txt2find contains non-ANSI characters and document type is ANSI, search cannot match
	if (isSearchUnicodeCharOnAnsi(txt2find))
	{
		return 0;
	}

	// If txt2replace contains non-ANSI characters and document type is ANSI, the match cannot be replaced
	if (isSearchUnicodeCharOnAnsi(txt2replace))
	{
		wstring msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replace-invalid-replace-chars", L"Replace: can't replace with non-ANSI text in ANSI document");
		setStatusbarMessage(msg, FSNotFound);
		return 0;
	}

	// Turn OFF all the notification of modification (SCN_MODIFIED) for the sake of performance
	LRESULT notifFlag = (*_ppEditView)->execute(SCI_GETMODEVENTMASK);
	(*_ppEditView)->execute(SCI_SETMODEVENTMASK, 0);

	Sci_CharacterRangeFull cr = (*_ppEditView)->getSelection();
	size_t docLength = (*_ppEditView)->execute(SCI_GETLENGTH);

	// Default :
	//        direction : down
	//        begin at : 0
	//        end at : end of doc
	size_t startPosition = 0;
	size_t endPosition = docLength;

	bool direction = pOptions->_whichDirection;

	//first try limiting scope by direction
	if (direction == DIR_UP)
	{
		startPosition = 0;
		endPosition = cr.cpMax;
	}
	else
	{
		startPosition = cr.cpMin;
		endPosition = docLength;
	}

	//then adjust scope if the full document needs to be changed
	if (op == ProcessCountAll && pOptions->_isInSelection)
	{
		startPosition = cr.cpMin;
		endPosition = cr.cpMax;
	}
	else if (pOptions->_isWrapAround || isEntire)	//entire document needs to be scanned
	{
		startPosition = 0;
		endPosition = docLength;
	}

	//then readjust scope if the selection override is active and allowed
	if ((pOptions->_isInSelection) && ((op == ProcessMarkAll) || ((op == ProcessReplaceAll || op == ProcessFindAll) && (!isEntire))))
		//if selection limiter and either mark all or replace all or find all w/o entire document override
	{
		startPosition = cr.cpMin;
		endPosition = cr.cpMax;
	}

	if ((op == ProcessMarkAllExt) && (colourStyleID != -1))
	{
		startPosition = 0;
		endPosition = docLength;
	}

	FindReplaceInfo findReplaceInfo;
	findReplaceInfo._txt2find = txt2find;
	findReplaceInfo._txt2replace = txt2replace;
	findReplaceInfo._startRange = startPosition;
	findReplaceInfo._endRange = endPosition;

	int nbProcessed = processRange(op, findReplaceInfo, pFindersInfo, pOptions, colourStyleID, nullptr, pMatches);

	// Turn ON the notifications after operations
	(*_ppEditView)->execute(SCI_SETMODEVENTMASK, notifFlag);
	if (op == ProcessReplaceAll && nbProcessed > 0) // All the notification of modification (SCN_MODIFIED) were removed during the operations, so we set modified status true here
	{
		Buffer* buf = (*_ppEditView)->getCurrentBuffer();
		buf->setModifiedStatus(true);
		::SendMessage(_hParent, NPPM_INTERNAL_DOCMODIFIEDBYREPLACEALL, reinterpret_cast<WPARAM>(buf), 0);
	}


	if (nbProcessed == FIND_INVALID_REGULAR_EXPRESSION)
		return FIND_INVALID_REGULAR_EXPRESSION;

	if (nbProcessed > 0 && op == ProcessReplaceAll && pOptions->_isInSelection)
	{
		size_t newDocLength = (*_ppEditView)->execute(SCI_GETLENGTH);
		endPosition += newDocLength - docLength;
		(*_ppEditView)->execute(SCI_SETSELECTION, endPosition, startPosition);
		(*_ppEditView)->execute(SCI_SCROLLRANGE, startPosition, endPosition);
		if (startPosition == endPosition)
		{
			const NppGUI& nppGui = (NppParameters::getInstance()).getNppGUI();
			if (nppGui._inSelectionAutocheckThreshold != 0)
			{
				setChecked(IDC_IN_SELECTION_CHECK, false);
			}
			enableFindDlgItem(IDC_IN_SELECTION_CHECK, false);
		}
	}
	return nbProcessed;
}

int FindReplaceDlg::processRange(ProcessOperation op, FindReplaceInfo& findReplaceInfo, const FindersInfo* pFindersInfo,
	FindOption* opt, int colourStyleID, ScintillaEditView* view2Process, std::vector<MatchPosition>* pMatches)
{
	int nbProcessed = 0;

	if (!isCreated() && !findReplaceInfo._txt2find)
		return 0;

	if (!_ppEditView)
		return 0;

	ScintillaEditView *pEditView = *_ppEditView;

	if (view2Process)
		pEditView = view2Process;

	if ((op == ProcessReplaceAll) && pEditView->getCurrentBuffer()->isReadOnly())
		return nbProcessed;

	if (findReplaceInfo._startRange == findReplaceInfo._endRange)
		return nbProcessed;

	const FindOption *pOptions = opt ? opt : _env;

	LRESULT stringSizeFind = 0;
	LRESULT stringSizeReplace = 0;

	wchar_t *pTextFind = NULL;
	if (!findReplaceInfo._txt2find)
	{
		HWND hFindCombo = dlgItem(IDFINDWHAT);
		wstring str2Search = getTextFromCombo(hFindCombo);
		stringSizeFind = str2Search.length();
		pTextFind = new wchar_t[stringSizeFind + 1];
		wcscpy_s(pTextFind, stringSizeFind + 1, str2Search.c_str());
	}
	else
	{
		stringSizeFind = lstrlen(findReplaceInfo._txt2find);
		pTextFind = new wchar_t[stringSizeFind + 1];
		wcscpy_s(pTextFind, stringSizeFind + 1, findReplaceInfo._txt2find);
	}

	if (!pTextFind[0])
	{
		delete [] pTextFind;
		return nbProcessed;
	}

	wchar_t *pTextReplace = NULL;
	if (op == ProcessReplaceAll)
	{
		if (!findReplaceInfo._txt2replace)
		{
			HWND hReplaceCombo = dlgItem(IDREPLACEWITH);
			wstring str2Replace = getTextFromCombo(hReplaceCombo);
			stringSizeReplace = str2Replace.length();
			pTextReplace = new wchar_t[stringSizeReplace + 1];
			wcscpy_s(pTextReplace, stringSizeReplace + 1, str2Replace.c_str());
		}
		else
		{
			stringSizeReplace = lstrlen(findReplaceInfo._txt2replace);
			pTextReplace = new wchar_t[stringSizeReplace + 1];
			wcscpy_s(pTextReplace, stringSizeReplace + 1, findReplaceInfo._txt2replace);
		}
	}

	if (pOptions->_searchType == FindExtended)
	{
		stringSizeFind = Searching::convertExtendedToString(pTextFind, pTextFind, static_cast<int32_t>(stringSizeFind));
		if (op == ProcessReplaceAll)
			stringSizeReplace = Searching::convertExtendedToString(pTextReplace, pTextReplace, static_cast<int32_t>(stringSizeReplace));
	}

	bool isRegExp = pOptions->_searchType == FindRegex;
	int flags = Searching::buildSearchFlags(pOptions) | SCFIND_REGEXP_SKIPCRLFASONE;

	// Allow empty matches, but not immediately after previous match, so Count and Mark All
	// (like Replace All and Find All already do) don't silently skip zero-length matches.
	if (op == ProcessReplaceAll || op == ProcessFindAll || op == ProcessCountAll || op == ProcessMarkAll)
		flags |= SCFIND_REGEXP_EMPTYMATCH_NOTAFTERMATCH;



	if (op == ProcessMarkAll && colourStyleID == -1)	//if marking, check if purging is needed
	{
		if (_env->_doPurge)
		{
			clearMarks(*_env);
		}
	}

	intptr_t targetStart = 0;

	//Initial range for searching
	pEditView->execute(SCI_SETSEARCHFLAGS, flags);


	bool findAllFileNameAdded = false;

	// A temporary string which is used to populate the search result window
	std::unique_ptr<std::string> text2AddUtf8(new std::string());
	size_t indexBuffer = 0;

	while (targetStart >= 0)
	{
		targetStart = pEditView->searchInTarget(pTextFind, stringSizeFind, findReplaceInfo._startRange, findReplaceInfo._endRange);

		// If we've not found anything, just break out of the loop
		if (targetStart == -1)
			break;

		if (targetStart == FIND_INVALID_REGULAR_EXPRESSION)
			return FIND_INVALID_REGULAR_EXPRESSION;

		intptr_t targetEnd = pEditView->execute(SCI_GETTARGETEND);

		if (targetEnd > findReplaceInfo._endRange)
		{
			//we found a result but outside our range, therefore do not process it
			break;
		}

		if (pMatches)
		{
			MatchPosition match;
			match.start = targetStart;
			match.end = targetEnd;
			pMatches->push_back(match);
		}

		intptr_t foundTextLen = targetEnd - targetStart;
		intptr_t replaceDelta = 0;
		bool processed = true;

		switch (op)
		{
			case ProcessFindAll:
			{
				const wchar_t *pFileName = L"";
				if (pFindersInfo && pFindersInfo->_pFileName)
					pFileName = pFindersInfo->_pFileName;

				if (!findAllFileNameAdded)	//add new filetitle in hits if we haven't already
				{
					_pFinder->addFileNameTitle(pFileName);
					findAllFileNameAdded = true;
				}

				auto totalLineNumber = pEditView->execute(SCI_GETLINECOUNT);
				auto lineNumber = pEditView->execute(SCI_LINEFROMPOSITION, targetStart);
				intptr_t lend = pEditView->execute(SCI_GETLINEENDPOSITION, lineNumber);
				intptr_t lstart = pEditView->execute(SCI_POSITIONFROMLINE, lineNumber);
				intptr_t nbChar = lend - lstart;

				// use the static buffer
				wchar_t lineBuf[SC_SEARCHRESULT_LINEBUFFERMAXLENGTH]{};

				if (nbChar > SC_SEARCHRESULT_LINEBUFFERMAXLENGTH - 3)
				{
					lend = lstart + SC_SEARCHRESULT_LINEBUFFERMAXLENGTH - 4;
					nbChar = SC_SEARCHRESULT_LINEBUFFERMAXLENGTH - 4;
				}

				intptr_t start_mark = targetStart - lstart;
				intptr_t end_mark = targetEnd - lstart;

				pEditView->getGenericText(lineBuf, SC_SEARCHRESULT_LINEBUFFERMAXLENGTH, lstart, lend, &start_mark, &end_mark, &nbChar);

				lineBuf[nbChar++] = '\r';
				lineBuf[nbChar++] = '\n';

				SearchResultMarkingLine srml;
				srml._segmentPostions.push_back(std::pair<intptr_t, intptr_t>(start_mark, end_mark));
				text2AddUtf8->append(_pFinder->foundLine(FoundInfo(targetStart, targetEnd, lineNumber + 1, pFileName), srml, lineBuf, nbChar, totalLineNumber));

				if (text2AddUtf8->length() > FINDTEMPSTRING_MAXSIZE)
				{
					_pFinder->setFinderReadOnly(false);
					_pFinder->_scintView.execute(SCI_ADDTEXT, text2AddUtf8->length(), reinterpret_cast<LPARAM>(text2AddUtf8->c_str()));
					_pFinder->setFinderReadOnly(true);
					text2AddUtf8->clear();
				}
				break;
			}

			case ProcessFindInFinder:
			{
				if (!pFindersInfo || !pFindersInfo->_pSourceFinder || !pFindersInfo->_pDestFinder)
					break;

				const wchar_t *pFileName = pFindersInfo->_pFileName ? pFindersInfo->_pFileName : L"";

				auto totalLineNumber = pEditView->execute(SCI_GETLINECOUNT);
				auto lineNumber = pEditView->execute(SCI_LINEFROMPOSITION, targetStart);
				intptr_t lend = pEditView->execute(SCI_GETLINEENDPOSITION, lineNumber);
				intptr_t lstart = pEditView->execute(SCI_POSITIONFROMLINE, lineNumber);
				intptr_t nbChar = lend - lstart;

				// use the static buffer
				wchar_t lineBuf[SC_SEARCHRESULT_LINEBUFFERMAXLENGTH]{};

				if (nbChar > SC_SEARCHRESULT_LINEBUFFERMAXLENGTH - 3)
				{
					lend = lstart + SC_SEARCHRESULT_LINEBUFFERMAXLENGTH - 4;
					nbChar = SC_SEARCHRESULT_LINEBUFFERMAXLENGTH - 4;
				}

				intptr_t start_mark = targetStart - lstart;
				intptr_t end_mark = targetEnd - lstart;

				pEditView->getGenericText(lineBuf, SC_SEARCHRESULT_LINEBUFFERMAXLENGTH, lstart, lend, &start_mark, &end_mark, &nbChar);

				lineBuf[nbChar++] = '\r';
				lineBuf[nbChar++] = '\n';

				SearchResultMarkingLine srml;
				srml._segmentPostions.push_back(std::pair<intptr_t, intptr_t>(start_mark, end_mark));

				processed = (!pOptions->_isMatchLineNumber) || (pFindersInfo->_pSourceFinder->canFind(pFileName, lineNumber + 1, &indexBuffer));
				if (processed)
				{
					if (!findAllFileNameAdded)	//add new filetitle in hits if we haven't already
					{
						pFindersInfo->_pDestFinder->addFileNameTitle(pFileName);
						findAllFileNameAdded = true;
					}
					text2AddUtf8->append(pFindersInfo->_pDestFinder->foundLine(FoundInfo(targetStart, targetEnd, lineNumber + 1, pFileName), srml, lineBuf, nbChar, totalLineNumber));

					if (text2AddUtf8->length() > FINDTEMPSTRING_MAXSIZE)
					{
						pFindersInfo->_pDestFinder->setFinderReadOnly(false);
						pFindersInfo->_pDestFinder->_scintView.execute(SCI_ADDTEXT, text2AddUtf8->length(), reinterpret_cast<LPARAM>(text2AddUtf8->c_str()));
						pFindersInfo->_pDestFinder->setFinderReadOnly(true);
						text2AddUtf8->clear();
					}
				}
				break;
			}

			case ProcessReplaceAll:
			{
				intptr_t replacedLength;
				if (isRegExp)
					replacedLength = pEditView->replaceTargetRegExMode(pTextReplace);
				else
					replacedLength = pEditView->replaceTarget(pTextReplace);

				replaceDelta = replacedLength - foundTextLen;
				break;
			}

			case ProcessMarkAll:
			{
				// Zero-length matches are possible (e.g. "^$"), but scintilla gets upset if we call
				// INDICATORFILLRANGE with a length of 0, so we protect against it here.
				// At least in version 2.27, after calling INDICATORFILLRANGE with length 0, further indicators
				// on the same line would simply not be shown.  This may have been fixed in later version of Scintilla.
				if (foundTextLen > 0)
				{
					pEditView->execute(SCI_SETINDICATORCURRENT, SCE_UNIVERSAL_FOUND_STYLE);
					pEditView->execute(SCI_INDICATORFILLRANGE,  targetStart, foundTextLen);
				}
				else if (foundTextLen == 0 && opt->_searchType == FindRegex)
				{
					opt->_regexEmptyStringFound = true;
				}

				if (_env->_doMarkLine)
				{
					auto lineNumber = pEditView->execute(SCI_LINEFROMPOSITION, targetStart);
					// For a zero-length match, "targetEnd - 1" would be the line *before* targetStart
					// (or even underflow at document start), so fall back to lineNumber in that case.
					auto lineNumberEnd = (foundTextLen > 0) ? pEditView->execute(SCI_LINEFROMPOSITION, targetEnd - 1) : lineNumber;

					for (auto i = lineNumber; i <= lineNumberEnd; ++i)
					{
						auto state = pEditView->execute(SCI_MARKERGET, i);

						if (!(state & (1 << MARK_BOOKMARK)))
							pEditView->execute(SCI_MARKERADD, i, MARK_BOOKMARK);
					}
				}

				break;
			}

			case ProcessMarkAllExt:
			{
				// See comment by ProcessMarkAll
				if (foundTextLen > 0)
				{
					pEditView->execute(SCI_SETINDICATORCURRENT,  colourStyleID);
					pEditView->execute(SCI_INDICATORFILLRANGE,  targetStart, foundTextLen);
				}
				break;
			}

			case ProcessHighLightAll:
			{
				// See comment by ProcessMarkAll
				if (foundTextLen > 0)
				{
					pEditView->execute(SCI_SETINDICATORCURRENT,  SCE_UNIVERSAL_FOUND_STYLE_SMART);
					pEditView->execute(SCI_INDICATORFILLRANGE,  targetStart, foundTextLen);
				}
				break;
			}

			case ProcessMarkAll_IncSearch:
			{
				// See comment by ProcessMarkAll
				if (foundTextLen > 0)
				{
					pEditView->execute(SCI_SETINDICATORCURRENT,  SCE_UNIVERSAL_FOUND_STYLE_INC);
					pEditView->execute(SCI_INDICATORFILLRANGE,  targetStart, foundTextLen);
				}
				break;
			}

			case ProcessCountAll:
			{
				if (foundTextLen == 0 && opt->_searchType == FindRegex)
					opt->_regexEmptyStringFound = true;

				break;
			}

			default:
			{
				delete[] pTextFind;
				if (pTextReplace)
					delete[] pTextReplace;
				return nbProcessed;
			}

		}
		if (processed) ++nbProcessed;

		// After the processing of the last string occurrence the search loop should be stopped
		// This helps to avoid the endless replacement during the EOL ("$") searching
		if (targetStart + foundTextLen == findReplaceInfo._endRange)
			break;

		findReplaceInfo._startRange = targetStart + foundTextLen + replaceDelta;		//search from result onwards
		findReplaceInfo._endRange += replaceDelta;									//adjust end of range in case of replace
	}

	delete[] pTextFind;
	if (pTextReplace)
		delete[] pTextReplace;

	if (nbProcessed > 0)
	{
		Finder *pFinder = nullptr;
		if (op == ProcessFindAll)
		{
			_pFinder->setFinderReadOnly(false);
			_pFinder->_scintView.execute(SCI_ADDTEXT, text2AddUtf8->length(), reinterpret_cast<LPARAM>(text2AddUtf8->c_str()));
			_pFinder->setFinderReadOnly(true);
			text2AddUtf8->clear();
			pFinder = _pFinder;
		}
		else if (op == ProcessFindInFinder)
		{
			if (pFindersInfo && pFindersInfo->_pDestFinder)
			{
				pFindersInfo->_pDestFinder->setFinderReadOnly(false);
				pFindersInfo->_pDestFinder->_scintView.execute(SCI_ADDTEXT, text2AddUtf8->length(), reinterpret_cast<LPARAM>(text2AddUtf8->c_str()));
				pFindersInfo->_pDestFinder->setFinderReadOnly(true);
				text2AddUtf8->clear();
				pFinder = pFindersInfo->_pDestFinder;
			}
			else
				pFinder = _pFinder;
		}

		if (pFinder != nullptr)
			pFinder->addFileHitCount(nbProcessed);
	}
	return nbProcessed;
}

void FindReplaceDlg::replaceAllInOpenedDocs()
{
	::SendMessage(_hParent, WM_REPLACEALL_INOPENEDDOC, 0, 0);
}

void FindReplaceDlg::findAllIn(InWhat op)
{
	bool justCreated = false;
	if (!_pFinder)
	{
		NppParameters& nppParam = NppParameters::getInstance();

		_pFinder = new Finder();
		_pFinder->init(_hInst, (*_ppEditView)->getHParent(), _ppEditView);
		_pFinder->setVolatiled(false);

		DockedWidgetData data{};

		_pFinder->create(&data, { IDI_FIND_RESULT_ICON, IDR_FIND_RESULT_ICO_DM, IDR_FIND_RESULT_ICO2 });
		// define the default docking behaviour
		data.uMask = DWS_DF_CONT_BOTTOM | DWS_ICONTAB | DWS_ADDINFO | DWS_USEOWNDARKMODE;

		int icoID = _pFinder->getIconIDs().at(2);
		if (nppParam.getNppGUI()._tbIconInfo._tbIconSet == TB_STANDARD)
			icoID = _pFinder->getIconIDs().at(0);
		else if (NppDarkMode::isEnabled())
			icoID = _pFinder->getIconIDs().at(1);

		data.hIconTab = nppLoadIcon(icoID);

		data.pszAddInfo = _findAllResultStr;

		data.pszModuleName = NPP_INTERNAL_FUNCTION_STR;

		// the dlgDlg should be the index of funcItem where the current function pointer is
		// in this case is DOCKABLE_DEMO_INDEX
		data.dlgID = 0;

		NativeLangSpeaker *pNativeSpeaker = nppParam.getNativeLangSpeaker();
		wstring text = pNativeSpeaker->getLocalizedStrFromID("find-result-caption", L"");

		if (!text.empty())
		{
			_findResTitle = text;
			data.pszName = _findResTitle.c_str();
		}

		::SendMessage(_hParent, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&data));

		_pFinder->_scintView.init(_hInst, _pFinder->getHSelf());

	_pFinder->connectScintilla();

		_pFinder->setFinderReadOnly(true);
		_pFinder->_scintView.execute(SCI_SETCODEPAGE, SC_CP_UTF8);
		_pFinder->_scintView.execute(SCI_USEPOPUP, SC_POPUP_NEVER);
		_pFinder->_scintView.execute(SCI_SETMODEVENTMASK, MODEVENTMASK_OFF); // Turn off the modification event
		_pFinder->_scintView.execute(SCI_SETUNDOCOLLECTION, false);	//don't store any undo information
		_pFinder->_scintView.execute(SCI_SETCARETWIDTH, 1);
		_pFinder->_scintView.showMargin(ScintillaEditView::_SC_MARGE_FOLDER, true);

		_pFinder->_scintView.execute(SCI_SETUSETABS, true);
		_pFinder->_scintView.execute(SCI_SETTABWIDTH, 4);

		const NppGUI& nppGUI = nppParam.getNppGUI();
		_pFinder->_longLinesAreWrapped = nppGUI._finderLinesAreCurrentlyWrapped;
		_pFinder->_scintView.wrap(_pFinder->_longLinesAreWrapped);
		_pFinder->_scintView.setWrapMode(LINEWRAP_INDENT);
		_pFinder->_scintView.showWrapSymbol(true);

		_pFinder->_purgeBeforeEverySearch = nppGUI._finderPurgeBeforeEverySearch;

		// allow user to start selecting as a stream block, then switch to a column block by adding Alt keypress
		_pFinder->_scintView.execute(SCI_SETMOUSESELECTIONRECTANGULARSWITCH, true);

		// overwrite some default settings
		_pFinder->_scintView.showMargin(ScintillaEditView::_SC_MARGE_SYMBOL, false);
		_pFinder->_scintView.setMakerStyle(FOLDER_STYLE_SIMPLE);


		_pFinder->_scintView.display();
		_pFinder->setFinderStyle();
		_pFinder->setFinderStyleForNpc();

		_pFinder->display(false);
		justCreated = true;
		
	}
	
	if (_pFinder->_purgeBeforeEverySearch)
	{
		_pFinder->removeAll();
	}

	if (justCreated)
	{
		// Send the address of _MarkingsStruct to the lexer
		char ptrword[sizeof(void*) * 2 + 1];
		sprintf(ptrword, "%p", static_cast<void*>(&_pFinder->_markingsStruct));
		_pFinder->_scintView.execute(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("@MarkingsStruct"), reinterpret_cast<LPARAM>(ptrword));

		//enable "Search Results Window" under Search Menu
		const HMENU mainMenu = reinterpret_cast<HMENU>(::SendMessage(_hParent, NPPM_INTERNAL_GETMENU, 0, 0));
		nppEnableMenuCommand(mainMenu, IDM_FOCUS_ON_FOUND_RESULTS, true);
		nppEnableMenuCommand(mainMenu, IDM_SEARCH_GOTONEXTFOUND, true);
		nppEnableMenuCommand(mainMenu, IDM_SEARCH_GOTOPREVFOUND, true);
	}

	bool toRTL = (*_ppEditView)->isTextDirectionRTL();
	bool isRTL = _pFinder->_scintView.isTextDirectionRTL();

	if ((toRTL && !isRTL) || (!toRTL && isRTL))
		_pFinder->_scintView.changeTextDirection(toRTL);

	int cmdid = 0;
	if (op == ALL_OPEN_DOCS)
		cmdid = WM_FINDALL_INOPENEDDOC;
	else if (op == FILES_IN_DIR)
		cmdid = WM_FINDINFILES;
	else if (op == FILES_IN_PROJECTS)
		cmdid = WM_FINDINPROJECTS;
	else if ((op == CURRENT_DOC) || (op == CURR_DOC_SELECTION))
		cmdid = WM_FINDALL_INCURRENTDOC;

	if (!cmdid) return;

	bool limitSearchScopeToSelection = op == CURR_DOC_SELECTION;
	if (::SendMessage(_hParent, cmdid, static_cast<WPARAM>(limitSearchScopeToSelection ? 1 : 0), 0))
	{
		wstring text = _pFinder->getHitsString(_findAllResult);
		wcscpy_s(_findAllResultStr, sizeof(_findAllResultStr) / sizeof(wchar_t), text.c_str());

		if (_findAllResult)
		{
			focusOnFinder();
		}
		else
		{
			// Show finder
			_pFinder->display();
			grabFocus(); // no hits
		}
	}
	else // error - search folder doesn't exist
		dlgItem(IDD_FINDINFILES_DIR_COMBO)->setFocus();
}

Finder* FindReplaceDlg::createFinder()
{
	NppParameters& nppParam = NppParameters::getInstance();

	Finder* pFinder = new Finder();
	pFinder->init(_hInst, (*_ppEditView)->getHParent(), _ppEditView);

	DockedWidgetData	data{};
	bool isRTL = _pFinder->_scintView.isTextDirectionRTL();
	pFinder->create(&data, { IDI_FIND_RESULT_ICON, IDR_FIND_RESULT_ICO_DM, IDR_FIND_RESULT_ICO2 }, isRTL);
	// define the default docking behaviour
	data.uMask = DWS_DF_CONT_BOTTOM | DWS_ICONTAB | DWS_ADDINFO | DWS_USEOWNDARKMODE;

	int icoID = pFinder->getIconIDs().at(2);
	if (nppParam.getNppGUI()._tbIconInfo._tbIconSet == TB_STANDARD)
		icoID = pFinder->getIconIDs().at(0);
	else if (NppDarkMode::isEnabled())
		icoID = pFinder->getIconIDs().at(1);

	data.hIconTab = nppLoadIcon(icoID);

	data.pszAddInfo = _findAllResultStr;

	data.pszModuleName = NPP_INTERNAL_FUNCTION_STR;

	// the dlgDlg should be the index of funcItem where the current function pointer is
	// in this case is DOCKABLE_DEMO_INDEX
	data.dlgID = 0;

	NativeLangSpeaker *pNativeSpeaker = nppParam.getNativeLangSpeaker();
	wstring text = pNativeSpeaker->getLocalizedStrFromID("find-result-caption", L"");
	if (!text.empty())
	{
		_findResTitle = text;
		data.pszName = _findResTitle.c_str();
	}

	::SendMessage(_hParent, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&data));

	pFinder->_scintView.init(_hInst, pFinder->getHSelf());
	if (isRTL)
		pFinder->_scintView.changeTextDirection(true);

	pFinder->connectScintilla();

	pFinder->setFinderReadOnly(true);
	pFinder->_scintView.execute(SCI_SETCODEPAGE, SC_CP_UTF8);
	pFinder->_scintView.execute(SCI_USEPOPUP, SC_POPUP_NEVER);
	pFinder->_scintView.execute(SCI_SETMODEVENTMASK, MODEVENTMASK_OFF); // Turn off the modification event
	pFinder->_scintView.execute(SCI_SETUNDOCOLLECTION, false);	//don't store any undo information
	pFinder->_scintView.execute(SCI_SETCARETWIDTH, 1);
	pFinder->_scintView.showMargin(ScintillaEditView::_SC_MARGE_FOLDER, true);

	pFinder->_scintView.execute(SCI_SETUSETABS, true);
	pFinder->_scintView.execute(SCI_SETTABWIDTH, 4);

	// inherit setting from current state of main finder:
	pFinder->_longLinesAreWrapped = _pFinder->_longLinesAreWrapped;
	pFinder->_scintView.wrap(pFinder->_longLinesAreWrapped);
	pFinder->_scintView.setWrapMode(LINEWRAP_INDENT);
	pFinder->_scintView.showWrapSymbol(true);

	// allow user to start selecting as a stream block, then switch to a column block by adding Alt keypress
	pFinder->_scintView.execute(SCI_SETMOUSESELECTIONRECTANGULARSWITCH, true);

	// overwrite some default settings
	pFinder->_scintView.showMargin(ScintillaEditView::_SC_MARGE_SYMBOL, false);
	pFinder->_scintView.setMakerStyle(FOLDER_STYLE_SIMPLE);

	pFinder->_scintView.display();

	pFinder->setFinderStyle();
	pFinder->setFinderStyleForNpc();

	// Send the address of _MarkingsStruct to the lexer
	char ptrword[sizeof(void*) * 2 + 1];
	sprintf(ptrword, "%p", static_cast<void*>(&pFinder->_markingsStruct));
	pFinder->_scintView.execute(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("@MarkingsStruct"), reinterpret_cast<LPARAM>(ptrword));

	_findersOfFinder.push_back(pFinder);

	// Show finder
	pFinder->display();
	pFinder->_scintView.grabFocus();

	return pFinder;
}

bool FindReplaceDlg::removeFinder(Finder *finder2remove)
{
	for (vector<Finder *>::iterator i = _findersOfFinder.begin(); i != _findersOfFinder.end(); ++i)
	{
		if (*i == finder2remove)
		{
			delete finder2remove;
			_findersOfFinder.erase(i);
			return true;
		}
	}
	return false;
}

Finder* FindReplaceDlg::getFinderFrom(HWND hwnd)
{
	if (_pFinder && _pFinder->isCreated())
	{
		if (_pFinder->_scintView.getHSelf() == hwnd)
			return _pFinder;

		if (!_findersOfFinder.empty())
		{
			for (const auto& finder : _findersOfFinder)
			{
				if (finder->_scintView.getHSelf() == hwnd)
					return finder;
			}
		}
	}

	return nullptr;
}

int FindReplaceDlg::regexBackwardMsgBox()
{
	NppParameters& nppParam = NppParameters::getInstance();

	const int msgboxID = nppParam.getNativeLangSpeaker()->messageBox("FindRegexBackwardDisabled",
		(*_ppEditView)->getHParent(),
		L"By default, backward regex searching is disabled due to potentially unexpected results. " \
		L"To perform a backward search, open the Find dialog and select either normal or extended search mode instead of regular expression.\r\n" \
		L"Press the OK button to open the Find dialog or set focus on it.\r\n" \
		L"\r\n" \
		L"If you require the backward regex searching feature, consult the user manual for instructions on enabling it.",
		L"Regex backward search disabled",
		MB_OKCANCEL | MB_APPLMODAL | MB_ICONINFORMATION);

	switch (msgboxID)
	{
		case IDOK:
		{
			doDialog(FIND_DLG, nppParam.getNativeLangSpeaker()->isRTL());
			goToCenter();
			dlgItem(IDREGEXP)->setFocus();
			break;
		}

		case IDCANCEL:
		default:
			break;
	}

	return msgboxID;
}

void FindReplaceDlg::setSearchText(const wchar_t * txt2find)
{
	QLineEdit* edit = _ui->findwhat->lineEdit();
	if (txt2find && txt2find[0])
	{
		// We got a valid search string
		edit->setText(QString::fromWCharArray(txt2find));
	}
	edit->selectAll(); // select all text - fast edit
}

void FindReplaceDlg::enableFindDlgItem(int dlgItemID, bool isEnable /* = true*/)
{
	QWidget* h = dlgItem(dlgItemID);
	if (!h) return;

	h->setEnabled(isEnable);

	// remember the real state of this control being enabled/disabled
	_controlEnableMap[dlgItemID] = isEnable;
}

void FindReplaceDlg::showFindDlgItem(int dlgItemID, bool isShow /* = true*/)
{
	QWidget* h = dlgItem(dlgItemID);
	if (!h) return;

	const bool isLessModeOn = NppParameters::getInstance().getNppGUI()._findWindowLessMode;

	h->setVisible(isShow);

	// when hiding a control to make it user-inaccessible, it can still be manipulated via a keyboard accelerator!
	// so disable it in addition to hiding it to prevent such user interaction
	// but that causes trouble when unhiding it; we don't know if it should be enabled or disabled,
	// so used the remembered state when we last enabled/disabled this control to determine it

	if (dlgItemID == IDOK)
	{
		// do not disable the standard Find-Next button (IDOK);
		// keeping it enabled allows Enter (and Shift+Enter) to work when in 2-button-find-mode
		// and IDC_FINDNEXT does not have focus
		return;
	}

	bool enable = isShow;

	if (isShow || isLessModeOn)
	{
		const auto iter = _controlEnableMap.find(dlgItemID);
		if (iter == _controlEnableMap.end())
		{
			// if control's state was never previously recorded, assume it was enabled
			enable = true;
			_controlEnableMap[dlgItemID] = true;
		}
		else
		{
			enable = iter->second;
		}
	}

	h->setEnabled(enable);
}

void FindReplaceDlg::setDefaultButton(int nID)
{
	if (auto* defaultButton = qobject_cast<QPushButton*>(dlgItem(nID)))
		defaultButton->setDefault(true);
}

void FindReplaceDlg::enableReplaceFunc(bool isEnable)
{
	_currentStatus = isEnable ? REPLACE_DLG : FIND_DLG;

	const bool isLessModeOff = !NppParameters::getInstance().getNppGUI()._findWindowLessMode;

	enableFindInFilesControls(false, false);
	enableMarkAllControls(false);
	// replace controls
	showFindDlgItem(ID_STATICTEXT_REPLACE, isEnable);
	showFindDlgItem(IDREPLACE, isEnable);
	showFindDlgItem(IDC_REPLACEANDSTOP, isEnable && isLessModeOff);
	showFindDlgItem(IDREPLACEWITH, isEnable);
	showFindDlgItem(IDD_FINDREPLACE_SWAP_BUTTON, isEnable);
	showFindDlgItem(IDREPLACEALL, isEnable);
	showFindDlgItem(IDC_REPLACE_OPENEDFILES, isEnable && isLessModeOff);
	showFindDlgItem(IDC_CONFIRMREPLACEOPENDOCS, isEnable && isLessModeOff);
	showFindDlgItem(IDC_REPLACEINSELECTION, isLessModeOff);
	showFindDlgItem(IDC_IN_SELECTION_CHECK, isLessModeOff);
	showFindDlgItem(IDC_2_BUTTONS_MODE);
	bool is2ButtonMode = isCheckedOrNot(IDC_2_BUTTONS_MODE);
	showFindDlgItem(IDOK, !is2ButtonMode);
	showFindDlgItem(IDC_FINDPREV, is2ButtonMode);
	showFindDlgItem(IDC_FINDNEXT, is2ButtonMode);

	showFindDlgItem(IDC_HIGHLIGHT_MODE, true);

	// find controls
	showFindDlgItem(IDC_FINDALL_OPENEDFILES, !isEnable && isLessModeOff);
	showFindDlgItem(IDCCOUNTALL, !isEnable);
	showFindDlgItem(IDC_FINDALL_CURRENTFILE, !isEnable && isLessModeOff);

	gotoCorrectTab();

	_hSelf->setWindowTitle(QString::fromStdWString(_tab.getCurrentTitle()));

	setDefaultButton(IDOK);

	hideOrShowCtrl4reduceOrNormalMode(_currentStatus);
}

void FindReplaceDlg::enableMarkAllControls(bool isEnable)
{
	const bool isLessModeOff = !NppParameters::getInstance().getNppGUI()._findWindowLessMode;

	showFindDlgItem(IDCMARKALL, isEnable);
	showFindDlgItem(IDC_MARKLINE_CHECK, isEnable && isLessModeOff);
	showFindDlgItem(IDC_PURGE_CHECK, isEnable && isLessModeOff);
	showFindDlgItem(IDC_CLEAR_ALL, isEnable);
	showFindDlgItem(IDC_IN_SELECTION_CHECK, isEnable && isLessModeOff);
	showFindDlgItem(IDC_COPY_MARKED_TEXT, isEnable && isLessModeOff);
}

void FindReplaceDlg::enableFindInFilesControls(bool isEnable, bool projectPanels)
{
	const bool isLessModeOff = !NppParameters::getInstance().getNppGUI()._findWindowLessMode;

	// Hide Items
	showFindDlgItem(IDC_BACKWARDDIRECTION, !isEnable && isLessModeOff);
	showFindDlgItem(IDWRAP, !isEnable && isLessModeOff);
	showFindDlgItem(IDC_HIGHLIGHT_MODE, !isEnable);
	showFindDlgItem(IDCCOUNTALL, !isEnable);
	showFindDlgItem(IDC_FINDALL_OPENEDFILES, !isEnable && isLessModeOff);
	showFindDlgItem(IDC_FINDALL_CURRENTFILE, !isEnable && isLessModeOff);

	if (isEnable)
	{
		showFindDlgItem(IDC_2_BUTTONS_MODE, false);
		showFindDlgItem(IDOK, false);
		showFindDlgItem(IDC_FINDPREV, false);
		showFindDlgItem(IDC_FINDNEXT, false);
	}
	else
	{
		showFindDlgItem(IDC_2_BUTTONS_MODE);
		bool is2ButtonMode = isCheckedOrNot(IDC_2_BUTTONS_MODE);
		showFindDlgItem(IDOK, !is2ButtonMode);
		showFindDlgItem(IDC_FINDPREV, is2ButtonMode);
		showFindDlgItem(IDC_FINDNEXT, is2ButtonMode);
	}

	showFindDlgItem(IDC_MARKLINE_CHECK, !isEnable && isLessModeOff);
	showFindDlgItem(IDC_PURGE_CHECK, !isEnable && isLessModeOff);
	showFindDlgItem(IDC_IN_SELECTION_CHECK, !isEnable && isLessModeOff);
	showFindDlgItem(IDC_CLEAR_ALL, !isEnable);
	showFindDlgItem(IDCMARKALL, !isEnable);
	showFindDlgItem(IDC_COPY_MARKED_TEXT, !isEnable && isLessModeOff);

	showFindDlgItem(IDREPLACE, !isEnable);
	showFindDlgItem(IDC_REPLACEANDSTOP, !isEnable && isLessModeOff);
	showFindDlgItem(IDC_REPLACEINSELECTION, !isEnable && isLessModeOff);
	showFindDlgItem(IDREPLACEALL, !isEnable && isLessModeOff);
	showFindDlgItem(IDC_REPLACE_OPENEDFILES, !isEnable && isLessModeOff);
	showFindDlgItem(IDC_CONFIRMREPLACEOPENDOCS, !isEnable && isLessModeOff);

	// Show Items
	if (isEnable)
	{
		showFindDlgItem(ID_STATICTEXT_REPLACE);
		showFindDlgItem(IDREPLACEWITH);
		showFindDlgItem(IDD_FINDREPLACE_SWAP_BUTTON);
	}
	showFindDlgItem(IDD_FINDINFILES_REPLACEINFILES, isEnable && (!projectPanels));
	showFindDlgItem(IDD_FINDINFILES_REPLACEINPROJECTS, isEnable && projectPanels);
	showFindDlgItem(IDD_FINDINFILES_FILTERS_STATIC, isEnable && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_FILTERS_COMBO, isEnable && isLessModeOff);
	showFindDlgItem(IDC_FIF_FILTER_TIP_STATIC, isEnable && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_DIR_STATIC, isEnable && (!projectPanels) && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_DIR_COMBO, isEnable && (!projectPanels) && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_BROWSE_BUTTON, isEnable && (!projectPanels) && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_FIND_BUTTON, isEnable);
	showFindDlgItem(IDD_FINDINFILES_RECURSIVE_CHECK, isEnable && (!projectPanels) && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_INHIDDENDIR_CHECK, isEnable && (!projectPanels) && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_PROJECT1_CHECK, isEnable && projectPanels && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_PROJECT2_CHECK, isEnable && projectPanels && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_PROJECT3_CHECK, isEnable && projectPanels && isLessModeOff);
	showFindDlgItem(IDD_FINDINFILES_SETDIRFROMDOC_BUTTON, isEnable && (!projectPanels) && isLessModeOff);
}

void FindReplaceDlg::getPatterns(vector<wstring> & patternVect)
{
	cutString(_env->_filters.c_str(), patternVect);
}

void FindReplaceDlg::getAndValidatePatterns(vector<wstring> & patternVect)
{
	getPatterns(patternVect);
	if (patternVect.size() == 0)
	{
		setFindInFilesDirFilter(NULL, L"*.*");
		getPatterns(patternVect);
	}
	else if (allPatternsAreExclusion(patternVect))
	{
		patternVect.insert(patternVect.begin(), L"*.*");
	}
}

void FindReplaceDlg::saveInMacro(size_t cmd, int cmdType)
{
	int booleans = 0;
	::SendMessage(_hParent, WM_FRSAVE_INT, IDC_FRCOMMAND_INIT, 0);
	::SendMessage(_hParent, WM_FRSAVE_STR, IDFINDWHAT,  reinterpret_cast<LPARAM>(cmd == IDC_CLEAR_ALL ? "" : wstring2string(_options._str2Search, CP_UTF8).c_str()));
	booleans |= _options._isWholeWord?IDF_WHOLEWORD:0;
	booleans |= _options._isMatchCase?IDF_MATCHCASE:0;
	booleans |= _options._dotMatchesNewline?IDF_REDOTMATCHNL:0;

	::SendMessage(_hParent, WM_FRSAVE_INT, IDNORMAL, _options._searchType);
	if (cmd == IDCMARKALL)
	{
		booleans |= _options._doPurge?IDF_PURGE_CHECK:0;
		booleans |= _options._doMarkLine?IDF_MARKLINE_CHECK:0;
	}
	if (cmdType & FR_OP_REPLACE)
		::SendMessage(_hParent, WM_FRSAVE_STR, IDREPLACEWITH, reinterpret_cast<LPARAM>(wstring2string(_options._str4Replace, CP_UTF8).c_str()));
	if (cmdType & FR_OP_FIF)
	{
		::SendMessage(_hParent, WM_FRSAVE_STR, IDD_FINDINFILES_DIR_COMBO, reinterpret_cast<LPARAM>(wstring2string(_options._directory, CP_UTF8).c_str()));
		::SendMessage(_hParent, WM_FRSAVE_STR, IDD_FINDINFILES_FILTERS_COMBO, reinterpret_cast<LPARAM>(wstring2string(_options._filters, CP_UTF8).c_str()));
		booleans |= _options._isRecursive?IDF_FINDINFILES_RECURSIVE_CHECK:0;
		booleans |= _options._isInHiddenDir?IDF_FINDINFILES_INHIDDENDIR_CHECK:0;
	}
	if (cmdType & FR_OP_FIP)
	{
		::SendMessage(_hParent, WM_FRSAVE_STR, IDD_FINDINFILES_FILTERS_COMBO, reinterpret_cast<LPARAM>(wstring2string(_options._filters, CP_UTF8).c_str()));
		booleans |= _options._isProjectPanel_1?IDF_FINDINFILES_PROJECT1_CHECK:0;
		booleans |= _options._isProjectPanel_2?IDF_FINDINFILES_PROJECT2_CHECK:0;
		booleans |= _options._isProjectPanel_3?IDF_FINDINFILES_PROJECT3_CHECK:0;
	}
	else if (!(cmdType & FR_OP_GLOBAL))
	{
		booleans |= _options._isInSelection?IDF_IN_SELECTION_CHECK:0;
		booleans |= _options._isWrapAround?IDF_WRAP:0;
		booleans |= _options._whichDirection?IDF_WHICH_DIRECTION:0;
	}
	else if (cmdType == FR_OP_FIND + FR_OP_GLOBAL)
	{
		// find all in curr doc can be limited by selected text
		booleans |= _options._isInSelection ? IDF_IN_SELECTION_CHECK : 0;
	}
	if (cmd == IDC_CLEAR_ALL)
	{
		booleans = _options._doMarkLine ? IDF_MARKLINE_CHECK : 0;
		booleans |= _options._isInSelection ? IDF_IN_SELECTION_CHECK : 0;
	}
	::SendMessage(_hParent, WM_FRSAVE_INT, IDC_FRCOMMAND_BOOLEANS, booleans);
	::SendMessage(_hParent, WM_FRSAVE_INT, IDC_FRCOMMAND_EXEC, cmd);
}

void FindReplaceDlg::setStatusbarMessage(const wstring & msg, FindStatus status, const wstring& tooltipMsg)
{
	_statusbarTooltipMsg = tooltipMsg;

	QWidget* windowToFlash = isVisible() ? _hSelf : _hParent;
	if (status == FSNotFound)
	{
		if (!NppParameters::getInstance().getNppGUI()._muteSounds)
			QApplication::beep();

		QApplication::alert(windowToFlash, 300);
	}
	else if ((status == FSTopReached || status == FSEndReached) && !isVisible())
	{
		QApplication::alert(windowToFlash, 200);
	}

	if (isVisible())
	{
		_statusbarFindStatus = status;
		updateStatusBarAppearance();
		_statusBar.setOwnerDrawText((status == FSNoMessage) ? L"" : msg.c_str());
		_statusBar.getHSelf()->setToolTip(QString::fromStdWString(tooltipMsg));
	}
}

void FindReplaceDlg::setStatusbarMessageWithRegExprErr(ScintillaEditView* pEditView)
{
	if (!pEditView)
		return;

	char msg[511] {};
	pEditView->execute(SCI_GETBOOSTREGEXERRMSG, _countof(msg), reinterpret_cast<LPARAM>(msg));

	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	std::wstring result = pNativeSpeaker->getLocalizedStrFromID("find-status-invalid-re", L"Find: Invalid Regular Expression");
	string s = msg;
	
	setStatusbarMessage(result, FSNotFound, string2wstring(s, CP_UTF8));
}

void FindReplaceDlg::setStatusMessageNotFound(const std::wstring& textNotFound, const std::wstring& tooltipMsg)
{
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	wstring warningMsg = pNativeSpeaker->getLocalizedStrFromID("find-status-cannot-find", L"Find: Can't find the text \"$STR_REPLACE$\"");
	wstring newTxt2find = stringReplace(textNotFound, L"&", L"&&");

	if (newTxt2find.length() > 32) // truncate the search string to display, if the search string is too long
	{
		newTxt2find.erase(28);
		newTxt2find += L"...";
	}

	warningMsg = stringReplace(warningMsg, L"$STR_REPLACE$", newTxt2find);

	warningMsg += L" ";
	warningMsg += getScopeInfoForStatusBar(&_options);

	setStatusbarMessage(warningMsg, FSNotFound, tooltipMsg);
}

void FindReplaceDlg::setStatusMessageWithInvisibleCharsWarning()
{
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	std::wstring findWhatWarning = pNativeSpeaker->getLocalizedStrFromID("find-status-invisible-chars-findWhat", L"Invisible characters in pasted \"Find what\" or \"Replace with\" content");
	std::wstring findWhatWarningTip = pNativeSpeaker->getLocalizedStrFromID("find-status-invisible-chars-findWhat-tip", L"Warning: Invisible characters in search (or replace) field.\nThe text pasted into this field includes invisible (maybe line-ending) characters. If you proceed without deleting them, they will be included in the search (or replace) text.");

	setStatusbarMessage(findWhatWarning, FSWarning, findWhatWarningTip);
}

void FindReplaceDlg::removeStatusMessageWithInvisibleCharsWarning()
{
	if (_statusbarFindStatus == FSWarning)
		setStatusbarMessage(L"", FSMessage, L"");
}

wstring FindReplaceDlg::getScopeInfoForStatusBar(FindOption const *pFindOpt) const
{
	wstring scope;

	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	if (pFindOpt->_isInSelection)
	{
		scope += pNativeSpeaker->getLocalizedStrFromID("find-status-scope-selection", L"in selected text");
	}
	else if (pFindOpt->_isWrapAround)
	{
		scope += pNativeSpeaker->getLocalizedStrFromID("find-status-scope-all", L"in entire file");
	}
	else if (pFindOpt->_whichDirection == DIR_UP)
	{
		scope += pNativeSpeaker->getLocalizedStrFromID("find-status-scope-backward", L"from start-of-file to caret");
	}
	else
	{
		scope += pNativeSpeaker->getLocalizedStrFromID("find-status-scope-forward", L"from caret to end-of-file");
	}

	return scope;
}

void FindReplaceDlg::execSavedCommand(int cmd, uptr_t intValue, const wstring& stringValue)
{
	try {
		switch (cmd)
		{
			case IDC_FRCOMMAND_INIT:
				_env = new FindOption;
				break;
			case IDFINDWHAT:
				_env->_str2Search = stringValue;
				break;
			case IDC_FRCOMMAND_BOOLEANS:
				_env->_isWholeWord = ((intValue & IDF_WHOLEWORD) > 0);
				_env->_isMatchCase = ((intValue & IDF_MATCHCASE) > 0);
				_env->_isRecursive = ((intValue & IDF_FINDINFILES_RECURSIVE_CHECK) > 0);
				_env->_isInHiddenDir = ((intValue & IDF_FINDINFILES_INHIDDENDIR_CHECK) > 0);
				_env->_isProjectPanel_1 = ((intValue & IDF_FINDINFILES_PROJECT1_CHECK) > 0);
				_env->_isProjectPanel_2 = ((intValue & IDF_FINDINFILES_PROJECT2_CHECK) > 0);
				_env->_isProjectPanel_3 = ((intValue & IDF_FINDINFILES_PROJECT3_CHECK) > 0);
				_env->_doPurge = ((intValue & IDF_PURGE_CHECK) > 0);
				_env->_doMarkLine = ((intValue & IDF_MARKLINE_CHECK) > 0);
				_env->_isInSelection = ((intValue & IDF_IN_SELECTION_CHECK) > 0);
				_env->_isWrapAround = ((intValue & IDF_WRAP) > 0);
				_env->_whichDirection = ((intValue & IDF_WHICH_DIRECTION) > 0);
				_env->_dotMatchesNewline = ((intValue & IDF_REDOTMATCHNL) > 0);
				break;
			case IDNORMAL:
				_env->_searchType = static_cast<SearchType>(intValue);
				break;
			case IDREPLACEWITH:
				_env->_str4Replace = stringValue;
				break;
			case IDD_FINDINFILES_DIR_COMBO:
				_env->_directory = stringValue;
				break;
			case IDD_FINDINFILES_FILTERS_COMBO:
				_env->_filters = stringValue;
				break;
			case IDC_FRCOMMAND_EXEC:
			{
				NppParameters& nppParamInst = NppParameters::getInstance();
				switch (intValue)
				{
					case IDOK:
						if ((_env->_whichDirection == DIR_UP) && (_env->_searchType == FindRegex) && !nppParamInst.regexBackward4PowerUser())
						{
							// regex upward search is disabled
							// this macro step could have been recorded in an earlier version before it was not allowed, or hand-edited
							// make this a no-action macro step

							regexBackwardMsgBox();
						}
						else
						{
							nppParamInst._isFindReplacing = true;
							processFindNext(_env->_str2Search.c_str());
							nppParamInst._isFindReplacing = false;
						}
						break;

					case IDC_FINDNEXT:
						// IDC_FINDNEXT will not be recorded into new macros recorded with 7.8.5 and later
						// stay playback compatible with 7.5.5 - 7.8.4 where IDC_FINDNEXT was allowed but unneeded/undocumented
						nppParamInst._isFindReplacing = true;
						_env->_whichDirection = DIR_DOWN;
						processFindNext(_env->_str2Search.c_str());
						nppParamInst._isFindReplacing = false;
						break;

					case IDC_FINDPREV:
						// IDC_FINDPREV will not be recorded into new macros recorded with 7.8.5 and later
						// stay playback compatible with 7.5.5 - 7.8.4 where IDC_FINDPREV was allowed but unneeded/undocumented
						if (_env->_searchType == FindRegex && !nppParamInst.regexBackward4PowerUser())
						{
							// regex upward search is disabled
							// this macro step could have been recorded in an earlier version before it was not allowed, or hand-edited
							// make this a no-action macro step

							regexBackwardMsgBox();
						}
						else
						{
							_env->_whichDirection = DIR_UP;
							nppParamInst._isFindReplacing = true;
							processFindNext(_env->_str2Search.c_str());
							nppParamInst._isFindReplacing = false;
						}
						break;

					case IDREPLACE:
						if ((_env->_whichDirection == DIR_UP) && (_env->_searchType == FindRegex && !nppParamInst.regexBackward4PowerUser()))
						{
							// regex upward search is disabled
							// this macro step could have been recorded in an earlier version before it was disabled, or hand-edited
							// make this a no-action macro step

							regexBackwardMsgBox();
						}
						else
						{
							nppParamInst._isFindReplacing = true;
							processReplace(_env->_str2Search.c_str(), _env->_str4Replace.c_str(), _env);
							nppParamInst._isFindReplacing = false;
						}
						break;
					case IDC_FINDALL_OPENEDFILES:
						nppParamInst._isFindReplacing = true;
						findAllIn(ALL_OPEN_DOCS);
						nppParamInst._isFindReplacing = false;
						break;
					case IDC_FINDALL_CURRENTFILE:
						nppParamInst._isFindReplacing = true;
						findAllIn(_env->_isInSelection ? CURR_DOC_SELECTION : CURRENT_DOC);
						nppParamInst._isFindReplacing = false;
						break;
					case IDC_REPLACE_OPENEDFILES:
					{
						NppParameters& nppParam = NppParameters::getInstance();
						const NppGUI& nppGui = nppParam.getNppGUI();
						if (!nppGui._confirmReplaceInAllOpenDocs || replaceInOpenDocsConfirmCheck())
						{
							nppParamInst._isFindReplacing = true;
							replaceAllInOpenedDocs();
							nppParamInst._isFindReplacing = false;
						}
						break;
					}
					case IDD_FINDINFILES_FIND_BUTTON:
						nppParamInst._isFindReplacing = true;
						findAllIn(FILES_IN_DIR);
						nppParamInst._isFindReplacing = false;
						break;
					case IDD_FINDINFILES_FINDINPROJECTS:
						nppParamInst._isFindReplacing = true;
						findAllIn(FILES_IN_PROJECTS);
						nppParamInst._isFindReplacing = false;
						break;

					case IDD_FINDINFILES_REPLACEINFILES:
					{
						if (replaceInFilesConfirmCheck(_env->_directory, _env->_filters))
						{
							nppParamInst._isFindReplacing = true;
							::SendMessage(_hParent, WM_REPLACEINFILES, 0, 0);
							nppParamInst._isFindReplacing = false;
						}
						break;
					}

					case IDD_FINDINFILES_REPLACEINPROJECTS:
					{
						if (replaceInProjectsConfirmCheck())
						{
							nppParamInst._isFindReplacing = true;
							::SendMessage(_hParent, WM_REPLACEINPROJECTS, 0, 0);
							nppParamInst._isFindReplacing = false;
						}
						break;
					}

					case IDREPLACEALL:
					{
						nppParamInst._isFindReplacing = true;
						(*_ppEditView)->execute(SCI_BEGINUNDOACTION);
						int nbReplaced = processAll(ProcessReplaceAll, _env);
						(*_ppEditView)->execute(SCI_ENDUNDOACTION);
						nppParamInst._isFindReplacing = false;

						if (nbReplaced == FIND_INVALID_REGULAR_EXPRESSION)
						{	
							setStatusbarMessageWithRegExprErr(*_ppEditView);
						}
						else
						{
							wstring result;
							NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

							if (nbReplaced == 1)
							{
								result = pNativeSpeaker->getLocalizedStrFromID("find-status-replaceall-1-replaced", L"Replace All: 1 occurrence was replaced");
							}
							else //if (nbReplaced == 0 || nbReplaced > 1)
							{
								result = pNativeSpeaker->getLocalizedStrFromID("find-status-replaceall-nb-replaced", L"Replace All: $INT_REPLACE$ occurrences were replaced");
								result = stringReplace(result, L"$INT_REPLACE$", std::to_wstring(nbReplaced));
							}
							result += L" ";
							result += getScopeInfoForStatusBar(_env);

							setStatusbarMessage(result, FSMessage);
						}

						break;
					}

					case IDCCOUNTALL:
					{
						int nbCounted = processAll(ProcessCountAll, _env);

						if (nbCounted == FIND_INVALID_REGULAR_EXPRESSION)
						{
							setStatusbarMessageWithRegExprErr(*_ppEditView);
						}
						else
						{
							wstring result;
							NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

							if (nbCounted == 1)
							{
								result = pNativeSpeaker->getLocalizedStrFromID("find-status-count-1-match", L"Count: 1 match");
							}
							else
							{
								result = pNativeSpeaker->getLocalizedStrFromID("find-status-count-nb-matches", L"Count: $INT_REPLACE$ matches");
								result = stringReplace(result, L"$INT_REPLACE$", std::to_wstring(nbCounted));
							}
							result += L" ";
							result += getScopeInfoForStatusBar(_env);

							setStatusbarMessage(result, FSMessage);
						}
						break;
					}

					case IDCMARKALL:
					{
						nppParamInst._isFindReplacing = true;
						int nbMarked = processAll(ProcessMarkAll, _env);
						nppParamInst._isFindReplacing = false;

						if (nbMarked == FIND_INVALID_REGULAR_EXPRESSION)
						{
							setStatusbarMessageWithRegExprErr(*_ppEditView);
						}
						else
						{
							wstring result;
							NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
							if (nbMarked == 1)
							{
								result = pNativeSpeaker->getLocalizedStrFromID("find-status-mark-1-match", L"Mark: 1 match");
							}
							else
							{
								result = pNativeSpeaker->getLocalizedStrFromID("find-status-mark-nb-matches", L"Mark: $INT_REPLACE$ matches");
								result = stringReplace(result, L"$INT_REPLACE$", std::to_wstring(nbMarked));
							}
							result += L" ";
							result += getScopeInfoForStatusBar(_env);

							setStatusbarMessage(result, FSMessage);
						}

						break;
					}

					case IDC_CLEAR_ALL:
					{
						clearMarks(*_env);
						break;
					}

					default:
						throw std::runtime_error("Internal error: unknown saved command!");
				}

				delete _env;
				_env = &_options;
				break;
			}
			default:
				throw std::runtime_error("Internal error: unknown SnR command!");
		}
	}
	catch (const std::runtime_error& err)
	{
		QMessageBox::warning(nullptr, QStringLiteral("Play Macro Exception"), QString::fromLocal8Bit(err.what()));
	}
}

void FindReplaceDlg::clearMarks(const FindOption& opt)
{
	if (opt._isInSelection)
	{
		Sci_CharacterRangeFull cr = (*_ppEditView)->getSelection();

		intptr_t startPosition = cr.cpMin;
		intptr_t endPosition = cr.cpMax;

		(*_ppEditView)->execute(SCI_SETINDICATORCURRENT, SCE_UNIVERSAL_FOUND_STYLE);
		(*_ppEditView)->execute(SCI_INDICATORCLEARRANGE, startPosition, endPosition - startPosition);

		if (opt._doMarkLine)
		{
			auto lineNumber = (*_ppEditView)->execute(SCI_LINEFROMPOSITION, startPosition);
			auto lineNumberEnd = (*_ppEditView)->execute(SCI_LINEFROMPOSITION, endPosition - 1);

			for (auto i = lineNumber; i <= lineNumberEnd; ++i)
			{
				auto state = (*_ppEditView)->execute(SCI_MARKERGET, i);

				if (state & (1 << MARK_BOOKMARK))
					(*_ppEditView)->execute(SCI_MARKERDELETE, i, MARK_BOOKMARK);
			}
		}
	}
	else
	{
		(*_ppEditView)->clearIndicator(SCE_UNIVERSAL_FOUND_STYLE);
		if (opt._doMarkLine)
		{
			(*_ppEditView)->execute(SCI_MARKERDELETEALL, MARK_BOOKMARK);
		}
	}

	setStatusbarMessage(L"", FSNoMessage);
}

void FindReplaceDlg::setFindInFilesDirFilter(const wchar_t *dir, const wchar_t *filters)
{
	if (dir)
	{
		_env->_directory = dir;
		setItemText(IDD_FINDINFILES_DIR_COMBO, dir);
	}

	if (filters)
	{
		_env->_filters = filters;
		setItemText(IDD_FINDINFILES_FILTERS_COMBO, filters);
	}
}

void FindReplaceDlg::enableProjectCheckmarks()
{
	NppParameters& nppParams = NppParameters::getInstance();
	FindHistory & findHistory = nppParams.getFindHistory();
	HMENU hMenu = reinterpret_cast<HMENU>(::SendMessage(_hParent, NPPM_INTERNAL_GETMENU, 0, 0));
	if (hMenu)
	{
		int   idm [3] = {IDM_VIEW_PROJECT_PANEL_1, IDM_VIEW_PROJECT_PANEL_2, IDM_VIEW_PROJECT_PANEL_3};
		int   idd [3] = {IDD_FINDINFILES_PROJECT1_CHECK, IDD_FINDINFILES_PROJECT2_CHECK, IDD_FINDINFILES_PROJECT3_CHECK};
		bool *opt [3] = {&_options._isProjectPanel_1, &_options._isProjectPanel_2, &_options._isProjectPanel_3};
		bool *hst [3] = {&findHistory._isFifProjectPanel_1, &findHistory._isFifProjectPanel_2, &findHistory._isFifProjectPanel_3};
		bool enable = false;
		for (int i = 0; i < 3; i++)
		{
			if (nppFindMenuAction(hMenu, idm [i]))
			{
				if (nppIsMenuCommandChecked(hMenu, idm [i]))
				{
					enableFindDlgItem (idd [i], true);
					if (isCheckedOrNot(idd [i]))
						enable = true;
				}
				else
				{
					*opt [i] = 0;
					*hst [i] = 0;
					setChecked(idd [i], false);
					enableFindDlgItem (idd [i], false);
				}
			}
		}
		enableFindDlgItem (IDD_FINDINFILES_FIND_BUTTON, enable || (_currentStatus != FINDINPROJECTS_DLG));
		enableFindDlgItem (IDD_FINDINFILES_REPLACEINPROJECTS, enable);
	}
}

void FindReplaceDlg::setProjectCheckmarks(FindHistory *findHistory, int msk)
{
	_options._isProjectPanel_1 = (msk & 1) ? true : false;
	_options._isProjectPanel_2 = (msk & 2) ? true : false;
	_options._isProjectPanel_3 = (msk & 4) ? true : false;
	FindHistory *fh = findHistory;
	if (! fh)
	{
		NppParameters& nppParams = NppParameters::getInstance();
		fh = & nppParams.getFindHistory();
	}

	if (fh)
	{
		fh->_isFifProjectPanel_1 = _options._isProjectPanel_1;
		fh->_isFifProjectPanel_2 = _options._isProjectPanel_2;
		fh->_isFifProjectPanel_3 = _options._isProjectPanel_3;
	}
	setChecked(IDD_FINDINFILES_PROJECT1_CHECK, _options._isProjectPanel_1);
	setChecked(IDD_FINDINFILES_PROJECT2_CHECK, _options._isProjectPanel_2);
	setChecked(IDD_FINDINFILES_PROJECT3_CHECK, _options._isProjectPanel_3);

	if (_currentStatus == FINDINPROJECTS_DLG)
	{
		enableFindDlgItem (IDD_FINDINFILES_FIND_BUTTON, msk != 0);
		enableFindDlgItem (IDD_FINDINFILES_REPLACEINPROJECTS, msk != 0);
	}
}

void FindReplaceDlg::initOptionsFromDlg()
{
	_options._isWholeWord = isCheckedOrNot(IDWHOLEWORD);
	_options._isMatchCase = isCheckedOrNot(IDMATCHCASE);
	_options._searchType = isCheckedOrNot(IDREGEXP)?FindRegex:isCheckedOrNot(IDEXTENDED)?FindExtended:FindNormal;
	_options._isWrapAround = isCheckedOrNot(IDWRAP);
	_options._isInSelection = isCheckedOrNot(IDC_IN_SELECTION_CHECK);

	_options._dotMatchesNewline = isCheckedOrNot(IDREDOTMATCHNL);
	_options._doPurge = isCheckedOrNot(IDC_PURGE_CHECK);
	_options._doMarkLine = isCheckedOrNot(IDC_MARKLINE_CHECK);

	_options._whichDirection = isCheckedOrNot(IDC_BACKWARDDIRECTION) ? DIR_UP : DIR_DOWN;

	_options._isRecursive = isCheckedOrNot(IDD_FINDINFILES_RECURSIVE_CHECK);
	_options._isInHiddenDir = isCheckedOrNot(IDD_FINDINFILES_INHIDDENDIR_CHECK);
	_options._isProjectPanel_1 =  isCheckedOrNot(IDD_FINDINFILES_PROJECT1_CHECK);
	_options._isProjectPanel_2 =  isCheckedOrNot(IDD_FINDINFILES_PROJECT2_CHECK);
	_options._isProjectPanel_3 =  isCheckedOrNot(IDD_FINDINFILES_PROJECT3_CHECK);
}

void FindInFinderDlg::doDialog(Finder *launcher, bool isRTL)
{
	_pFinder2Search = launcher;
	runDialog(isRTL);
}

// The window keeps one size for all the tabs, as far as the tallest one needs it, so that they do not jump. Only the
// width can be changed by the user. The button column gets the width of the widest tab for the same reason.
void FindReplaceDlg::measureTabs()
{
	NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();
	const bool wasLessMode = nppGUI._findWindowLessMode;
	const DIALOG_TYPE current = _currentStatus;
	nppGUI._findWindowLessMode = false;

	if (_buttonsWidth)
		_buttonsWidth->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed); // a measure from before a translation

	QSize largest;
	QSize buttons;
	const auto measure = [&]()
	{
		_ui->mainLayout->activate(); // the nested layouts learn about the hidden controls only from this
		largest = largest.expandedTo(_ui->mainLayout->totalSizeHint());
		buttons = buttons.expandedTo(_ui->buttonsLayout->sizeHint());
	};

	enableReplaceFunc(false);
	measure();
	enableReplaceFunc(true);
	measure();
	enableFindInFilesFunc();
	measure();
	enableFindInProjectsFunc();
	measure();
	enableMarkFunc();
	measure();

	nppGUI._findWindowLessMode = wasLessMode;
	if (!_buttonsWidth)
	{
		_buttonsWidth = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		_ui->buttonsLayout->addSpacerItem(_buttonsWidth);
	}
	_buttonsWidth->changeSize(buttons.width(), 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
	_tabsHeight = largest.height();

	// Narrower, the layout would squeeze the button column on the tabs that need more room on the left
	_hSelf->setMinimumWidth(largest.width());

	showControlsOf(current);
	fitHeight();
}

void FindReplaceDlg::showControlsOf(DIALOG_TYPE type)
{
	if (type == FINDINFILES_DLG)
		enableFindInFilesFunc();
	else if (type == FINDINPROJECTS_DLG)
		enableFindInProjectsFunc();
	else if (type == MARK_DLG)
		enableMarkFunc();
	else
		enableReplaceFunc(type == REPLACE_DLG);
}

void FindReplaceDlg::fitHeight()
{
	const bool isLessMode = NppParameters::getInstance().getNppGUI()._findWindowLessMode;
	_ui->mainLayout->activate();
	_hSelf->setFixedHeight(isLessMode ? _ui->mainLayout->totalSizeHint().height() : std::max(_tabsHeight, _ui->mainLayout->totalSizeHint().height()));
}

void FindReplaceDlg::doDialog(DIALOG_TYPE whichType, bool isRTL, bool toShow)
{
	if (!isCreated())
	{
		_isRTL = isRTL;
		//const auto dpiContext = DPIManagerV2::setThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		create(IDD_FIND_REPLACE_DLG, isRTL, true, toShow);
		//DPIManagerV2::setThreadDpiAwarenessContext(dpiContext);

		const HMENU mainMenu = reinterpret_cast<HMENU>(::SendMessage(_hParent, NPPM_INTERNAL_GETMENU, 0, 0));
		nppEnableMenuCommand(mainMenu, IDM_SEARCH_FINDNEXT, true);
		nppEnableMenuCommand(mainMenu, IDM_SEARCH_FINDPREV, true);
	}

	showControlsOf(whichType);
	fitHeight();
	StaticDialog::displayEnhanced(toShow);
	(toShow ? dlgItem(IDFINDWHAT) : (*_ppEditView)->getHSelf())->setFocus();

	updateMatchHighlighting(); // shown, or another tab
}

// "Highlight all matches while the dialog is open": while the dialog is shown on the Find or Replace tab,
// all the matches of "Find what" are highlighted, with the options of the dialog.
// Like Smart Highlighting, only the visible lines and one screen below are searched: the highlighting follows
// the view when it scrolls, so its cost does not depend on the document size.
void FindReplaceDlg::updateMatchHighlighting()
{
	clearMatchHighlighting();

	const FindHistory& findHistory = NppParameters::getInstance().getFindHistory();
	if (!findHistory._findDlgHighlightMatches || !isVisible())
		return;

	if (_currentStatus != FIND_DLG && _currentStatus != REPLACE_DLG)
		return;

	ScintillaEditView* pEditView = *_ppEditView;
	if (!pEditView->getCurrentBuffer()->allowSmartHilite()) // large file restriction
		return;

	wstring str2Search = getTextFromCombo(dlgItem(IDFINDWHAT));
	if (str2Search.empty())
		return;

	FindOption options = *_env;
	options._str2Search = str2Search;
	options._isInSelection = false;
	options._incrementalType = NotIncremental;

	const intptr_t firstVisibleLine = pEditView->execute(SCI_GETFIRSTVISIBLELINE);
	const intptr_t nbLinesOnScreen = pEditView->execute(SCI_LINESONSCREEN);
	const intptr_t firstLine = pEditView->execute(SCI_DOCLINEFROMVISIBLE, firstVisibleLine);
	const intptr_t lastLine = pEditView->execute(SCI_DOCLINEFROMVISIBLE, firstVisibleLine + 2 * nbLinesOnScreen + 1);
	const intptr_t endPos = pEditView->execute(SCI_POSITIONFROMLINE, lastLine + 1);

	FindReplaceInfo frInfo;
	frInfo._txt2find = options._str2Search.c_str();
	frInfo._startRange = pEditView->execute(SCI_POSITIONFROMLINE, firstLine);
	frInfo._endRange = (endPos < 0) ? static_cast<intptr_t>(pEditView->getCurrentDocLen()) : endPos;

	// keep the target of the other search functions, like SmartHighlighter does
	const auto originalStartPos = pEditView->execute(SCI_GETTARGETSTART);
	const auto originalEndPos = pEditView->execute(SCI_GETTARGETEND);
	processRange(ProcessMarkAllExt, frInfo, nullptr, &options, FINDDLG_HIGHLIGHT_INDIC, pEditView);
	pEditView->execute(SCI_SETTARGETRANGE, originalStartPos, originalEndPos);

	_pMatchHighlightView = pEditView;
}

// While typing in "Find what" or after an edit: updateMatchHighlighting() a moment after the last change
void FindReplaceDlg::updateMatchHighlightingLater() const
{
	if (_matchHighlightTimer)
		_matchHighlightTimer->start();
}

void FindReplaceDlg::clearMatchHighlighting()
{
	if (_matchHighlightTimer)
		_matchHighlightTimer->stop();

	// the current view, and the view highlighted last time if the other view has become the current one
	(*_ppEditView)->clearIndicator(FINDDLG_HIGHLIGHT_INDIC);

	if (_pMatchHighlightView && _pMatchHighlightView != *_ppEditView)
		_pMatchHighlightView->clearIndicator(FINDDLG_HIGHLIGHT_INDIC);

	_pMatchHighlightView = nullptr;
}

// Where Windows sends WM_SHOWWINDOW: before the dialog is hidden, however it is closed, and when it is shown again with its owner
void FindReplaceDlg::onVisibilityChanged(bool isShown)
{
	if (isShown)
		updateMatchHighlightingLater();
	else
		clearMatchHighlighting();
}

void FindReplaceDlg::hideOrShowCtrl4reduceOrNormalMode(DIALOG_TYPE dlgT)
{
	const bool hide = NppParameters::getInstance().getNppGUI()._findWindowLessMode;
	auto showCtrls = [&](const auto& ctrlsID)
	{
		for (int id : ctrlsID)
			showItem(id, !hide);
	};

	auto showEnableCtrls = [&](const auto& ctrlsID)
	{
		for (int id : ctrlsID)
		{
			showItem(id, !hide);
			enableItem(id, !hide);
		}
	};

	showEnableCtrls(_reduce2hide_match);
	showEnableCtrls(_reduce2hide_mode);
	if (NppParameters::isWindowOpacityAvailable())
		showEnableCtrls(_reduce2hide_transparency);

	const bool isRegex = isCheckedOrNot(IDREGEXP);
	const bool isBackwardAllowed = (!isRegex || NppParameters::getInstance().regexBackward4PowerUser()) && !hide;

	enableFindDlgItem(IDREDOTMATCHNL, isRegex && !hide);
	enableFindDlgItem(IDWHOLEWORD, !isRegex && !hide);

	switch (dlgT)
	{
		case FIND_DLG:
		{
			showCtrls(_reduce2hide_find);
			enableFindDlgItem(IDC_BACKWARDDIRECTION, isBackwardAllowed);
			break;
		}

		case REPLACE_DLG:
		{
			showCtrls(_reduce2hide_findReplace);
			enableFindDlgItem(IDC_BACKWARDDIRECTION, isBackwardAllowed);
			break;
		}

		case FINDINFILES_DLG:
		{
			showCtrls(_reduce2hide_fif);
			break;
		}

		case FINDINPROJECTS_DLG:
		{
			showCtrls(_reduce2hide_fip);
			break;
		}

		case MARK_DLG:
		{
			showCtrls(_reduce2hide_mark);
			enableFindDlgItem(IDC_BACKWARDDIRECTION, isBackwardAllowed);
			break;
		}
	}
}

void FindReplaceDlg::enableFindInFilesFunc()
{
	enableFindInFilesControls(true, false);
	_currentStatus = FINDINFILES_DLG;
	gotoCorrectTab();
	_hSelf->setWindowTitle(QString::fromStdWString(_tab.getCurrentTitle()));
	setDefaultButton(IDD_FINDINFILES_FIND_BUTTON);
	enableFindDlgItem (IDD_FINDINFILES_FIND_BUTTON, true);
	hideOrShowCtrl4reduceOrNormalMode(_currentStatus);
}

void FindReplaceDlg::enableFindInProjectsFunc()
{
	enableFindInFilesControls(true, true);
	_currentStatus = FINDINPROJECTS_DLG;
	gotoCorrectTab();
	_hSelf->setWindowTitle(QString::fromStdWString(_tab.getCurrentTitle()));
	setDefaultButton(IDD_FINDINFILES_FIND_BUTTON);
	bool enable = _options._isProjectPanel_1 || _options._isProjectPanel_2 || _options._isProjectPanel_3;
	enableFindDlgItem (IDD_FINDINFILES_FIND_BUTTON, enable);
	enableFindDlgItem (IDD_FINDINFILES_REPLACEINPROJECTS, enable);
	hideOrShowCtrl4reduceOrNormalMode(_currentStatus);
}

void FindReplaceDlg::enableMarkFunc()
{
	enableFindInFilesControls(false, false);
	enableMarkAllControls(true);
	showFindDlgItem(IDC_HIGHLIGHT_MODE, false);

	// Replace controls to hide
	showFindDlgItem(ID_STATICTEXT_REPLACE, false);
	showFindDlgItem(IDREPLACE, false);
	showFindDlgItem(IDC_REPLACEANDSTOP, false);
	showFindDlgItem(IDREPLACEWITH, false);
	showFindDlgItem(IDD_FINDREPLACE_SWAP_BUTTON, false);
	showFindDlgItem(IDREPLACEALL, false);
	showFindDlgItem(IDC_REPLACE_OPENEDFILES, false);
	showFindDlgItem(IDC_CONFIRMREPLACEOPENDOCS, false);

	// find controls to hide
	showFindDlgItem(IDC_FINDALL_OPENEDFILES, false);
	showFindDlgItem(IDCCOUNTALL, false);
	showFindDlgItem(IDC_FINDALL_CURRENTFILE, false);
	showFindDlgItem(IDOK, false);
	showFindDlgItem(IDC_2_BUTTONS_MODE, false);
	showFindDlgItem(IDC_FINDPREV, false);
	showFindDlgItem(IDC_FINDNEXT, false);

	_currentStatus = MARK_DLG;
	gotoCorrectTab();

	_hSelf->setWindowTitle(QString::fromStdWString(_tab.getCurrentTitle()));
	setDefaultButton(IDCMARKALL);

	hideOrShowCtrl4reduceOrNormalMode(_currentStatus);
}

void FindReplaceDlg::updateStatusBarAppearance()
{
	NppParameters& nppParamInst = NppParameters::getInstance();

	int colourIndex = -1;
	switch (_statusbarFindStatus)
	{
		case FSNotFound: colourIndex = 0; break;
		case FSMessage: colourIndex = 1; break;
		case FSTopReached:
		case FSEndReached: colourIndex = 2; break;
		case FSWarning: colourIndex = 3; break;
		default: break;
	}

	QString style;
	if (colourIndex != -1)
	{
		COLORREF fgColor = nppParamInst.getFindDlgStatusMsgColor(colourIndex);
		if (NppDarkMode::isEnabled())
		{
			HLSColour hls(fgColor);
			fgColor = hls.toRGB4DarkMod();
		}
		style = QStringLiteral("color: rgb(%1, %2, %3);").arg(GetRValue(fgColor)).arg(GetGValue(fgColor)).arg(GetBValue(fgColor));
	}
	_statusBar.getHSelf()->setStyleSheet(style);
}

bool FindReplaceDlg::replaceInFilesConfirmCheck(const wstring& directory, const wstring& fileTypes)
{
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	wstring title = pNativeSpeaker->getLocalizedStrFromID("replace-in-files-confirm-title", L"Are you sure?");

	wstring msg = pNativeSpeaker->getLocalizedStrFromID("replace-in-files-confirm-directory", L"Are you sure you want to replace all occurrences in:");
	msg += L"\r\r";
	msg += directory;

	msg += L"\r\r";

	wstring msg2 = pNativeSpeaker->getLocalizedStrFromID("replace-in-files-confirm-filetype", L"For file type:");
	msg2 += L"\r\r";
	msg2 += fileTypes[0] ? fileTypes : L"*.*";

	msg += msg2;

	const bool confirmed = NppDarkMode::darkMessageBoxW(nullptr, msg.c_str(), title.c_str(), MB_OKCANCEL | MB_DEFBUTTON2 | MB_TASKMODAL) == IDOK;
	return confirmed;
}

bool FindReplaceDlg::replaceInProjectsConfirmCheck()
{
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	wstring title = pNativeSpeaker->getLocalizedStrFromID("replace-in-projects-confirm-title", L"Are you sure?");

	wstring msg = pNativeSpeaker->getLocalizedStrFromID("replace-in-projects-confirm-message", L"Do you want to replace all occurrences in all documents in the selected Project Panel(s)?");

	const bool confirmed = NppDarkMode::darkMessageBoxW(nullptr, msg.c_str(), title.c_str(), MB_OKCANCEL | MB_DEFBUTTON2 | MB_TASKMODAL) == IDOK;
	return confirmed;
}

bool FindReplaceDlg::replaceInOpenDocsConfirmCheck()
{
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	wstring title = pNativeSpeaker->getLocalizedStrFromID("replace-in-open-docs-confirm-title", L"Are you sure?");
	wstring msg = pNativeSpeaker->getLocalizedStrFromID("replace-in-open-docs-confirm-message", L"Are you sure you want to replace all occurrences in all open documents?");

	const bool confirmed = NppDarkMode::darkMessageBoxW(nullptr, msg.c_str(), title.c_str(), MB_OKCANCEL | MB_DEFBUTTON2 | MB_TASKMODAL) == IDOK;
	return confirmed;
}

bool FindReplaceDlg::isSearchUnicodeCharOnAnsi(const wchar_t* text) const
{
	if ((*_ppEditView)->execute(SCI_GETCODEPAGE) == CP_ACP)
	{
		// An ANSI document holds single bytes: anything beyond them cannot be found in it
		return std::any_of(text, text + std::wcslen(text), [](wchar_t c) { return static_cast<std::make_unsigned_t<wchar_t>>(c) > 0xFF; });
	}
	return false;
}

// Expand selection (if needed) and set the selected text in Find What field.
// Return empty string if nothing to set in find field.
// Otherwise return string in which the selected text was copied.
wstring FindReplaceDlg::setSearchText()
{
	const NppGUI& nppGui = NppParameters::getInstance().getNppGUI();
	Sci_Position selStrCharNum = 0;
	auto selStr = (*_ppEditView)->getSelectedTextToWChar(true, &selStrCharNum);

	if (!selStr.empty() && selStrCharNum <= nppGui._fillFindWhatThreshold)
	{
		setSearchText(selStr.c_str());
		return selStr;
	}
	return L"";
}

// Set the selected text in Find What field, according the Search settings.
// Return empty string if nothing to set in find field.
// Otherwise return string in which the selected text was copied.
wstring FindReplaceDlg::setSearchTextWithSettings()
{
	const NppGUI& nppGui = NppParameters::getInstance().getNppGUI();
	if (nppGui._fillFindFieldWithSelected)
	{
		Sci_Position selStrCharNum = 0;
		auto selStr = (*_ppEditView)->getSelectedTextToWChar(nppGui._fillFindFieldSelectCaret, &selStrCharNum);

		if (!selStr.empty() && selStrCharNum <= nppGui._fillFindWhatThreshold)
		{
			setSearchText(selStr.c_str());
			return selStr;
		}
	}
	return L"";
}

wstring Finder::getHitsString(int count) const
{
	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	wstring text = pNativeSpeaker->getLocalizedStrFromID("find-result-hits", L"");

	if (text.empty())
	{
		if (count == 1)
		{
			text = L"(1 hit)";
		}
		else
		{
			text = L"(";
			text += std::to_wstring(count);
			text += L" hits)";
		}
	}
	else
	{
		text = stringReplace(text, L"$INT_REPLACE$", std::to_wstring(count));
	}

	return text;
}

void Finder::addSearchLine(const wchar_t *searchName)
{
	wstring oneLineSearchName = searchName;
	oneLineSearchName.erase(std::remove(oneLineSearchName.begin(), oneLineSearchName.end(), L'\r'), oneLineSearchName.end());
	oneLineSearchName.erase(std::remove(oneLineSearchName.begin(), oneLineSearchName.end(), L'\n'), oneLineSearchName.end());

	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	wstring str = pNativeSpeaker->getLocalizedStrFromID("find-result-title", L"Search");
	str += L" \"";
	str += oneLineSearchName;
	str += L"\" \r\n";

	setFinderReadOnly(false);
	_scintView.addGenericText(str.c_str());
	setFinderReadOnly(true);
	_lastSearchHeaderPos = _scintView.execute(SCI_GETCURRENTPOS) - 2;

	_pMainFoundInfos->push_back(EmptyFoundInfo);
	_pMainMarkings->push_back(EmptySearchResultMarking);
	_previousLineNumber = -1;
}

void Finder::addFileNameTitle(const wchar_t * fileName)
{
	wstring str = L"  ";
	str += fileName;
	str += L"\r\n";

	setFinderReadOnly(false);
	_scintView.addGenericText(str.c_str());
	setFinderReadOnly(true);
	_lastFileHeaderPos = _scintView.execute(SCI_GETCURRENTPOS) - 2;

	_pMainFoundInfos->push_back(EmptyFoundInfo);
	_pMainMarkings->push_back(EmptySearchResultMarking);
	_previousLineNumber = -1;
}

void Finder::addFileHitCount(int count)
{
	wstring text = L" ";
	text += getHitsString(count);
	setFinderReadOnly(false);
	_scintView.insertGenericTextFrom(_lastFileHeaderPos, text.c_str());
	setFinderReadOnly(true);
	++_nbFoundFiles;
}

void Finder::addSearchResultInfo(int count, int countSearched, bool searchedEntireNotSelection, const FindOption* pFindOpt)
{
	wstring nbResStr = commafyInt(count);
	wstring nbFoundFilesStr = commafyInt(_nbFoundFiles);
	wstring nbSearchedFilesStr = commafyInt(countSearched);

	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	wstring text = pNativeSpeaker->getLocalizedStrFromID(searchedEntireNotSelection ? "find-result-title-info" : "find-result-title-info-selections", L"");

	if (text.empty())
	{
		// create default output: "(1 hit in 1 file of 1 searched)"
		//  variation             "(1 hit in 2 files of 2 searched)"
		//  variation             "(2 hits in 1 file of 3 searched)"
		//  variation             "(0 hits in 0 files of 1 searched)"
		// selection variations:  "(1 hit in 1 selection of 1 searched)"     " (0 hits in 0 selections of 1 searched)"
		// line filter variation: "(1 hit in 1 file of 1 searched) - Line Filter Mode: only display the filtered results"

		wstring hitsIn = count == 1 ? L"hit" : L"hits";

		wstring fileOrSelection = searchedEntireNotSelection ? L"file" : L"selection";
		if (_nbFoundFiles != 1)
		{
			fileOrSelection += L"s";
		}

		text = L"(" + nbResStr + L" " + hitsIn + L" in " + nbFoundFilesStr + L" " +
			fileOrSelection + L" of " + nbSearchedFilesStr + L" searched" + L")";
	}
	else
	{
		text = stringReplace(text, L"$INT_REPLACE1$", nbResStr);
		text = stringReplace(text, L"$INT_REPLACE2$", nbFoundFilesStr);
		text = stringReplace(text, L"$INT_REPLACE3$", nbSearchedFilesStr);
	}

	wstring searchModeText;
	if (pFindOpt->_searchType == FindExtended)
	{
		searchModeText += pNativeSpeaker->getLocalizedStrFromID("find-result-title-info-options-searchmode-extended", L"Extended");
	}
	else if (pFindOpt->_searchType == FindRegex)
	{
		searchModeText += pNativeSpeaker->getLocalizedStrFromID("find-result-title-info-options-searchmode-regexp", L"RegEx");
		if (pFindOpt->_dotMatchesNewline) searchModeText += L".";
	}
	else
	{
		searchModeText += pNativeSpeaker->getLocalizedStrFromID("find-result-title-info-options-searchmode-normal", L"Normal");
	}

	wstring searchOptionsText;
	if (pFindOpt->_isMatchCase)
	{
		searchOptionsText += pNativeSpeaker->getLocalizedStrFromID("find-result-title-info-options-case", L"Case");
	}
	if (pFindOpt->_isWholeWord)
	{
		if (!searchOptionsText.empty()) searchOptionsText += L"/";
		searchOptionsText += pNativeSpeaker->getLocalizedStrFromID("find-result-title-info-options-word", L"Word");
	}

	if (!searchOptionsText.empty()) searchModeText += L": ";
	
	text += L" [" + searchModeText + searchOptionsText + L"]";

	if (pFindOpt->_isMatchLineNumber)
	{
		wstring lineFilterModeInfo = pNativeSpeaker->getLocalizedStrFromID("find-result-title-info-extra", L" - Line Filter Mode: only display the filtered results");
		text += lineFilterModeInfo;
	}

	setFinderReadOnly(false);
	_scintView.insertGenericTextFrom(_lastSearchHeaderPos, text.c_str());
	setFinderReadOnly(true);
}

string Finder::foundLine(FoundInfo fi, SearchResultMarkingLine miLine, const wchar_t* foundline, size_t foundLineLen, size_t totalLineNumber)
{
	bool isRepeatedLine = false;

	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	if (nppGUI._finderShowOnlyOneEntryPerFoundLine)
	{
		if (_previousLineNumber == -1)
		{
			_previousLineNumber = fi._lineNumber;
		}
		else if (_previousLineNumber == static_cast<intptr_t>(fi._lineNumber))
		{
			isRepeatedLine = true;
		}
		else // previousLine != fi._lineNumber
		{
			_previousLineNumber = fi._lineNumber;
		}
	}

	wstring headerStr = L"\t";
	headerStr += _prefixLineStr;
	headerStr += L" ";

	size_t totalLineNumberDigit = static_cast<size_t>(nbDigitsFromNbLines(totalLineNumber) + 1);
	size_t currentLineNumberDigit = static_cast<size_t>(nbDigitsFromNbLines(fi._lineNumber) + 1);

	wstring lineNumberStr = L"";
	lineNumberStr.append(totalLineNumberDigit - currentLineNumberDigit, ' ');
	lineNumberStr.append(std::to_wstring(fi._lineNumber));
	headerStr += lineNumberStr;
	headerStr += L": ";

	miLine._segmentPostions[0].first += headerStr.length();
	miLine._segmentPostions[0].second += headerStr.length();

	headerStr += wstring(foundline, foundLineLen);
	
	WcharMbcsConvertor& wmc = WcharMbcsConvertor::getInstance();
	int text2AddUtf8Len = 0;
	const char* text2AddUtf8 = wmc.wchar2char(headerStr.c_str(), SC_CP_UTF8, &miLine._segmentPostions[0].first, &miLine._segmentPostions[0].second, static_cast<int>(headerStr.length()), &text2AddUtf8Len); // certainly utf8 here
	

	// if current line is the repeated line of previous one, and settings make per found line show ONCE in the result even there are several found occurences in the same line, for:
	if ((isRepeatedLine && 
		((text2AddUtf8Len < SC_SEARCHRESULT_LINEBUFFERMAXLENGTH) ||                                                                              // 1. All displayed whole lines, in which it contains all the occurrences.
		((text2AddUtf8Len >= SC_SEARCHRESULT_LINEBUFFERMAXLENGTH && miLine._segmentPostions[0].second < SC_SEARCHRESULT_LINEBUFFERMAXLENGTH))))) // 2. or the cut lines but the segments are displayed. For the segments displayed beyond displayed (non-cut part), go to default mode.
	{
		// Add start and end markers into the previous line's info for colourizing 
		_pMainMarkings->back()._segmentPostions.push_back(std::pair<intptr_t, intptr_t>(miLine._segmentPostions[0].first, miLine._segmentPostions[0].second));
		_pMainFoundInfos->back()._ranges.push_back(fi._ranges.back());

		return "";
	}
	else // default mode: allow same found line has several entries in search result if the searched occurrence is matched several times in the same line
	{
		_pMainFoundInfos->push_back(fi);


		if (text2AddUtf8Len >= SC_SEARCHRESULT_LINEBUFFERMAXLENGTH)
		{
			const char* endOfLongLine = " ...\r\n"; // perfectly Utf8-encoded already
			size_t lenEndOfLongLine = strlen(endOfLongLine);
			size_t cut = SC_SEARCHRESULT_LINEBUFFERMAXLENGTH - lenEndOfLongLine - 1;

			while ((cut > 0) && (!Utf8::isValid(&text2AddUtf8[cut], (int)(text2AddUtf8Len - cut))))
				cut--;

			memcpy(static_cast<void*>(const_cast<char*>(&text2AddUtf8[cut])), endOfLongLine, lenEndOfLongLine + 1);
			text2AddUtf8Len = static_cast<int>(cut + lenEndOfLongLine);
		}

		_pMainMarkings->push_back(miLine);

		return string(text2AddUtf8, text2AddUtf8Len);
	}
}

void Finder::removeAll()
{
	_pMainFoundInfos->clear();
	_pMainMarkings->clear();
	setFinderReadOnly(false);
	_scintView.execute(SCI_CLEARALL);
	setFinderReadOnly(true);
}

void Finder::openAll()
{
	for (auto&& path : getResultFilePaths(true))
	{
		::SendMessage(_hParent, WM_DOOPEN, 0, reinterpret_cast<LPARAM>(path.c_str()));
	}
}

void Finder::copyPathnames()
{
	wstring toClipboard;
	for (auto&& path : getResultFilePaths(true))
	{
		toClipboard += path + L"\r\n";
	}
	if (!toClipboard.empty())
	{
		if (!str2Clipboard(toClipboard, _hSelf))
		{
			assert(false);
			NppDarkMode::darkMessageBoxW(nullptr, L"Error placing pathnames into clipboard.", L"Notepad++", MB_ICONINFORMATION);
		}
	}
}

void Finder::wrapLongLinesToggle()
{
	_longLinesAreWrapped = !_longLinesAreWrapped;
	_scintView.wrap(_longLinesAreWrapped);

	if (!_canBeVolatiled)
	{
		// only remember this setting from the original finder
		NppParameters& nppParam = NppParameters::getInstance();
		NppGUI& nppGUI = nppParam.getNppGUI();
		nppGUI._finderLinesAreCurrentlyWrapped = _longLinesAreWrapped;
	}
}

void Finder::purgeToggle()
{
	_purgeBeforeEverySearch = !_purgeBeforeEverySearch;

	if (!_canBeVolatiled)
	{
		// only remember this setting from the original finder
		NppParameters& nppParam = NppParameters::getInstance();
		NppGUI& nppGUI = nppParam.getNppGUI();
		nppGUI._finderPurgeBeforeEverySearch = _purgeBeforeEverySearch;
	}
}

bool Finder::isLineActualSearchResult(const wstring& s)
{
	// actual-search-result lines are the only type that start with a tab character
	// sample: "\tLine 123: xxxxxxHITxxxxxx"
	return (s.find(L"\t") == 0);
}

wstring & Finder::prepareStringForClipboard(wstring & s) const
{
	// Input: a string like "\tLine 3: search result".
	// Output: "search result"
	s = stringReplace(s, L"\r", L"");
	s = stringReplace(s, L"\n", L"");
	const auto firstColon = s.find(L':');
	if (firstColon == std::string::npos)
	{
		// Should never happen.
		assert(false);
		return s;
	}
	else
	{
		// Plus 2 in order to deal with ": ".
		s = s.substr(2 + firstColon);
		return s;
	}
}

void Finder::copy()
{
	const pair<size_t, size_t> lineRange = _scintView.getSelectionLinesRange();
	size_t fromLine = lineRange.first;
	size_t toLine = lineRange.second;

	if (_scintView.execute(SCI_GETSELECTIONEMPTY) || fromLine == toLine)
	{
		const int selectedLineFoldLevel = _scintView.execute(SCI_GETFOLDLEVEL, fromLine) & SC_FOLDLEVELNUMBERMASK;
		if (selectedLineFoldLevel != resultLevel)
		{
			// caret on Search "..." header line
			// or
			// caret is on a line with a pathname
			
			// locate the final resultLevel line under its parent grouping
			toLine = _scintView.execute(SCI_GETLASTCHILD, fromLine, selectedLineFoldLevel);
			const int toLineFoldLevel = _scintView.execute(SCI_GETFOLDLEVEL, toLine) & SC_FOLDLEVELNUMBERMASK;
			if (toLineFoldLevel != resultLevel)
			{
				return;  // the search had 0 hits, so no resultLevel lines, nothing to copy
			}
		}
	}

	std::vector<wstring> lines;
	wstring previousResultLineStr(L"");
	for (size_t line = fromLine; line <= toLine; ++line)
	{
		wstring lineStr = _scintView.getLine(line);
		if (isLineActualSearchResult(lineStr))
		{
			if (lineStr != previousResultLineStr)
			{
				previousResultLineStr = lineStr;
				lines.push_back(prepareStringForClipboard(lineStr));
			}
		}
	}
	wstring toClipboard;
	stringJoin(lines, L"\r\n", toClipboard);
	toClipboard += L"\r\n";
	if (!str2Clipboard(toClipboard, _hSelf))
	{
		assert(false);
		NppDarkMode::darkMessageBoxW(nullptr, L"Error placing text in clipboard.", L"Notepad++", MB_ICONINFORMATION);
	}
}

void Finder::beginNewFilesSearch()
{
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	_prefixLineStr = pNativeSpeaker->getLocalizedStrFromID("find-result-line-prefix", L"Line");

	// Use SCI_SETSEL(0, 0) instead of SCI_SETCURRENTPOS(0) to workaround
	// an eventual regression or a change of behaviour in Scintilla 4.4.6
	// ref: https://github.com/notepad-plus-plus/notepad-plus-plus/issues/9595#issuecomment-789824579
	//
	_scintView.execute(SCI_SETSEL, 0, 0);
	//_scintView.execute(SCI_SETCURRENTPOS, 0);

	_pMainFoundInfos = _pMainFoundInfos == &_foundInfos1 ? &_foundInfos2 : &_foundInfos1;
	_pMainMarkings = _pMainMarkings == &_markings1 ? &_markings2 : &_markings1;
	_nbFoundFiles = 0;

	// fold all old searches (1st level only)
	_scintView.foldLevel(searchHeaderLevel - SC_FOLDLEVELBASE, fold_collapse);
}

void Finder::finishFilesSearch(int count, int searchedCount, bool searchedEntireNotSelection, const FindOption* pFindOpt)
{
	std::vector<FoundInfo>* _pOldFoundInfos;
	std::vector<SearchResultMarkingLine>* _pOldMarkings;
	_pOldFoundInfos = _pMainFoundInfos == &_foundInfos1 ? &_foundInfos2 : &_foundInfos1;
	_pOldMarkings = _pMainMarkings == &_markings1 ? &_markings2 : &_markings1;

	_pOldFoundInfos->insert(_pOldFoundInfos->begin(), _pMainFoundInfos->begin(), _pMainFoundInfos->end());
	_pOldMarkings->insert(_pOldMarkings->begin(), _pMainMarkings->begin(), _pMainMarkings->end());
	_pMainFoundInfos->clear();
	_pMainMarkings->clear();
	_pMainFoundInfos = _pOldFoundInfos;
	_pMainMarkings = _pOldMarkings;

	_markingsStruct._length = static_cast<long>(_pMainMarkings->size());
	if (_pMainMarkings->size() > 0)
		_markingsStruct._markings = &((*_pMainMarkings)[0]);

	addSearchResultInfo(count, searchedCount, searchedEntireNotSelection, pFindOpt);
	_scintView.execute(SCI_SETSEL, 0, 0);

	//SCI_SETILEXER resets the lexer property @MarkingsStruct and then no data could be exchanged with the searchResult lexer
	char ptrword[sizeof(void*) * 2 + 1];
	sprintf(ptrword, "%p", static_cast<void*>(&_markingsStruct));
	_scintView.execute(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("@MarkingsStruct"), reinterpret_cast<LPARAM>(ptrword));

	//previous code: _scintView.execute(SCI_SETILEXER, 0, reinterpret_cast<LPARAM>(CreateLexer("searchResult")));
	_scintView.execute(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("fold"), reinterpret_cast<LPARAM>("1"));

	_previousLineNumber = -1;
}


void Finder::setFinderStyle()
{
	// Set global styles for the finder
	_scintView.performGlobalStyles();

	// Set current line background color for the finder
	const wchar_t * lexerName = g_langNameInfoArray[L_SEARCHRESULT]._langName;
	LexerStyler *pStyler = (NppParameters::getInstance().getLStylerArray()).getLexerStylerByName(lexerName);
	if (pStyler)
	{
		const Style * pStyle = pStyler->findByID(SCE_SEARCHRESULT_CURRENT_LINE);
		if (pStyle)
		{
			// The caret starts on the first line, the search's title: the dark theme has its colour on the current line
			const Style* pHeader = pStyler->findByID(SCE_SEARCHRESULT_SEARCH_HEADER);
			_scintView.setElementColour(SC_ELEMENT_CARET_LINE_BACK, pHeader ? NppDarkMode::readableOn(pStyle->_bgColor, pHeader->_fgColor) : pStyle->_bgColor);
			_scintView.execute(SCI_SETCARETLINEFRAME, 0);
			_scintView.execute(SCI_SETCARETLINEVISIBLEALWAYS, true);
		}
	}
	_scintView.setSearchResultLexer();

	// Override foreground & background colour by default foreground & background coulour
	StyleArray & stylers = NppParameters::getInstance().getMiscStylerArray();
	Style * pStyleDefault = stylers.findByID(STYLE_DEFAULT);
	if (pStyleDefault)
	{
		_scintView.setStyle(*pStyleDefault);

		GlobalOverride & go = NppParameters::getInstance().getGlobalOverrideStyle();
		if (go.isEnable())
		{
			const Style * pStyleGlobalOverride = stylers.findByName(L"Global override");
			if (pStyleGlobalOverride)
			{
				if (go.enableFg)
				{
					pStyleDefault->_fgColor = pStyleGlobalOverride->_fgColor;
				}
				if (go.enableBg)
				{
					pStyleDefault->_bgColor = pStyleGlobalOverride->_bgColor;
				}
			}
		}

		_scintView.execute(SCI_STYLESETFORE, SCE_SEARCHRESULT_DEFAULT, pStyleDefault->_fgColor);
		_scintView.execute(SCI_STYLESETBACK, SCE_SEARCHRESULT_DEFAULT, pStyleDefault->_bgColor);
	}

	_scintView.execute(SCI_COLOURISE, 0, -1);

	// finder fold style follows user preference but use box when user selects none
	const ScintillaViewParams& svp = NppParameters::getInstance().getSVP();
	_scintView.setMakerStyle(svp._folderStyle == FOLDER_STYLE_NONE ? FOLDER_STYLE_BOX : svp._folderStyle);
}

void Finder::setFinderStyleForNpc(bool onlyColor)
{
	NppParameters& nppParam = NppParameters::getInstance();
	const bool isShown = nppParam.getSVP()._npcShow;
	if (!onlyColor)
	{
		_scintView.showNpc(isShown, true);
	}
	else if (isShown)
	{
		_scintView.setNpcAndCcUniEOL();
	}
}

void Finder::connectScintilla()
{
	auto* layout = new QVBoxLayout(_hSelf);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_scintView.getHSelf());

	// Scintilla doesn't notify all key presses
	_scintView.setKeyPressFilter([this](QKeyEvent* event)
	{
		switch (event->key())
		{
			case Qt::Key_Return:
			case Qt::Key_Enter:
			{
				std::pair<intptr_t, intptr_t> newPos = gotoFoundLine();

				auto currentPos = _scintView.execute(SCI_GETCURRENTPOS);
				intptr_t lno = _scintView.execute(SCI_LINEFROMPOSITION, currentPos);
				intptr_t lineStartAbsPos = _scintView.execute(SCI_POSITIONFROMLINE, lno);
				intptr_t lineEndAbsPos = _scintView.execute(SCI_GETLINEENDPOSITION, lno);

				intptr_t begin = newPos.first + lineStartAbsPos;
				intptr_t end = newPos.second + lineStartAbsPos;

				if (end > lineEndAbsPos)
					end = lineEndAbsPos;

				_scintView.execute(SCI_SETSEL, begin, end);
				_scintView.execute(SCI_SCROLLRANGE, begin, end);
				return true;
			}

			case Qt::Key_Escape:
				display(false);
				return true;

			case Qt::Key_Delete:
				deleteResult();
				return true;

			default:
				return false;
		}
	});

	_scintView.getHSelf()->setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(_scintView.getHSelf(), &QWidget::customContextMenuRequested, _hSelf, [this]() { showContextMenu(); });
}

void Finder::showContextMenu()
{
	const QPoint cursor = QCursor::pos();
	POINT p;
	p.x = cursor.x();
	p.y = cursor.y();
	ContextMenu scintillaContextmenu;
	vector<MenuItemUnit> tmp;

	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	wstring findInFinder = pNativeSpeaker->getLocalizedStrFromID("finder-find-in-finder", L"Find in these search results...");
	wstring closeThis = pNativeSpeaker->getLocalizedStrFromID("finder-close-this", L"Close these search results");
	wstring collapseAll = pNativeSpeaker->getLocalizedStrFromID("finder-collapse-all", L"Fold all");
	wstring uncollapseAll = pNativeSpeaker->getLocalizedStrFromID("finder-uncollapse-all", L"Unfold all");
	wstring copyLines = pNativeSpeaker->getLocalizedStrFromID("finder-copy", L"Copy Selected Line(s)");
	wstring copyVerbatim = pNativeSpeaker->getNativeLangMenuString(IDM_EDIT_COPY, L"Copy", true);
	copyVerbatim += L"\tCtrl+C";
	wstring copyPaths = pNativeSpeaker->getLocalizedStrFromID("finder-copy-selected-paths", L"Copy Selected Pathname(s)");
	wstring selectAll = pNativeSpeaker->getNativeLangMenuString(IDM_EDIT_SELECTALL, L"Select all", true);
	selectAll += L"\tCtrl+A";
	wstring clearAll = pNativeSpeaker->getLocalizedStrFromID("finder-clear-all", L"Clear all");
	wstring purgeForEverySearch = pNativeSpeaker->getLocalizedStrFromID("finder-purge-for-every-search", L"Purge for every search");
	wstring openSelectedPath = pNativeSpeaker->getLocalizedStrFromID("finder-open-selected-paths", L"Open Selected Pathname(s)");
	wstring wrapLongLines = pNativeSpeaker->getLocalizedStrFromID("finder-wrap-long-lines", L"Word wrap long lines");

	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_FINDINFINDERDLG, findInFinder));
	if (_canBeVolatiled)
		tmp.push_back(MenuItemUnit(NPPM_INTERNAL_REMOVEFINDER, closeThis));
	tmp.push_back(MenuItemUnit(0, L"Separator"));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDERCOLLAPSE, collapseAll));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDERUNCOLLAPSE, uncollapseAll));
	tmp.push_back(MenuItemUnit(0, L"Separator"));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDERCOPYVERBATIM, copyVerbatim));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDERCOPY, copyLines));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDERCOPYPATHS, copyPaths));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDERSELECTALL, selectAll));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDERCLEARALL, clearAll));
	tmp.push_back(MenuItemUnit(0, L"Separator"));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDEROPENALL, openSelectedPath));
	// configuration items go at the bottom:
	tmp.push_back(MenuItemUnit(0, L"Separator"));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDERWRAP, wrapLongLines));
	tmp.push_back(MenuItemUnit(NPPM_INTERNAL_SCINTILLAFINDERPURGE, purgeForEverySearch));

	scintillaContextmenu.create(_hSelf, tmp);

	bool hasSomeSelectedText = _scintView.getSelectedTextCount() > 0;
	scintillaContextmenu.enableItem(NPPM_INTERNAL_SCINTILLAFINDERCOPYVERBATIM, hasSomeSelectedText);

	scintillaContextmenu.enableItem(NPPM_INTERNAL_SCINTILLAFINDERCLEARALL, !_canBeVolatiled);

	scintillaContextmenu.enableItem(NPPM_INTERNAL_SCINTILLAFINDERPURGE, !_canBeVolatiled);
	scintillaContextmenu.checkItem(NPPM_INTERNAL_SCINTILLAFINDERPURGE, _purgeBeforeEverySearch && !_canBeVolatiled);

	scintillaContextmenu.checkItem(NPPM_INTERNAL_SCINTILLAFINDERWRAP, _longLinesAreWrapped);

	scintillaContextmenu.display(p);
}

LRESULT Finder::onPanelMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_NOTIFY)
	{
		const auto* header = reinterpret_cast<const NMHDR*>(lParam);
		if (header->hwndFrom == _hParent)
			onDockingNotify(*header);
		else
			notify(reinterpret_cast<SCNotification*>(lParam));
		return 0;
	}

	if (message != WM_COMMAND)
		return 0;

	if (HIWORD(wParam) == SCEN_SETFOCUS)
	{
		::SendMessage(_hParent, NPPM_INTERNAL_CHECKUNDOREDOSTATE, 0, 0);
		return TRUE;
	}

	switch (wParam)
	{
		case NPPM_INTERNAL_FINDINFINDERDLG:
		{
			::SendMessage(_hParent, NPPM_INTERNAL_FINDINFINDERDLG, reinterpret_cast<WPARAM>(this), 0);
			return TRUE;
		}

		case NPPM_INTERNAL_REMOVEFINDER:
		{
			if (_canBeVolatiled)
			{
				::SendMessage(_hParent, NPPM_DMMHIDE, 0, reinterpret_cast<LPARAM>(_hSelf));
				setClosed(true);
			}
			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDERCOLLAPSE :
		{
			_scintView.foldAll(fold_collapse);
			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDERUNCOLLAPSE :
		{
			_scintView.foldAll(fold_expand);
			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDERCOPY :
		{
			copy();
			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDERCOPYVERBATIM:
		{
			_scintView.execute(SCI_COPY);

			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDERCOPYPATHS:
		{
			copyPathnames();
			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDERSELECTALL :
		{
			_scintView.execute(SCI_SELECTALL);
			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDERCLEARALL:
		{
			removeAll();
			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDEROPENALL:
		{
			openAll();
			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDERWRAP:
		{
			wrapLongLinesToggle();
			return TRUE;
		}

		case NPPM_INTERNAL_SCINTILLAFINDERPURGE:
		{
			purgeToggle();
			return TRUE;
		}

		default :
			return FALSE;
	}
}

FindIncrementDlg::FindIncrementDlg() = default;
FindIncrementDlg::~FindIncrementDlg() = default;

void FindIncrementDlg::init(HINSTANCE hInst, HWND hPere, FindReplaceDlg *pFRDlg, bool isRTL)
{
	Window::init(hInst, hPere);
	if (!pFRDlg)
		throw std::runtime_error("FindIncrementDlg::init : Parameter pFRDlg is null");
	_pFRDlg = pFRDlg;
	_isRTL = isRTL;

	QDialog* dialog = createDialogWidget(isRTL);
	dialog->setWindowFlags(Qt::Widget);
	_ui = std::make_unique<Ui::IncrementFindDlg>();
	_ui->setupUi(dialog);

	const FindHistory& findHistory = NppParameters::getInstance().getFindHistory();
	setChecked(IDC_INCFINDMATCHCASE, findHistory._isIncrementalMatchCase);
	setChecked(IDC_INCFINDHILITEALL, findHistory._isIncrementalHighlightAll);
	setChecked(IDC_INCFINDCOUNT, findHistory._isIncrementalCount);
	setItemText(IDC_GOUPDOWN_TOGGLE_BUTTON, findHistory._incrementalSearchBarAtTop ? L"˅" : L"˄");

	nppConnectControlCommands(_hSelf, [this](int id, WORD notification)
	{
		onCommand(MAKEWPARAM(static_cast<WORD>(id), notification));
	});
	QObject::connect(_ui->incfindtext, &QLineEdit::returnPressed, _hSelf, [this]() { onCommand(IDOK); });

	nppRegisterMessageTarget(_hSelf, [this](UINT message, WPARAM wParam, LPARAM) -> LRESULT
	{
		return (message == WM_COMMAND) ? onCommand(wParam) : 0;
	});
}

void FindIncrementDlg::destroy()
{
	_pRebar = nullptr;
}

void FindIncrementDlg::display(bool toShow) const
{
	if (!_pRebar)
	{
		Window::display(toShow);
		return;
	}

	if (toShow)
	{
		// select the whole find editor text
		_ui->incfindtext->setFocus();
		_ui->incfindtext->selectAll();
	}
	_pRebar->setIDVisible(REBAR_BAR_SEARCH, toShow);
}

void FindIncrementDlg::onCancel()
{
	onCommand(IDCANCEL);
}

intptr_t FindIncrementDlg::onCommand(WPARAM wParam)
{
	bool updateSearch = false;
	bool forward = true;
	bool advance = false;
	bool updateHiLight = false;
	bool updateCase = false;

	switch (LOWORD(wParam))
	{
		case IDC_GOUPDOWN_TOGGLE_BUTTON:
		{
			FindHistory& findHistory = NppParameters::getInstance().getFindHistory();
			findHistory._incrementalSearchBarAtTop = !findHistory._incrementalSearchBarAtTop;
			setItemText(IDC_GOUPDOWN_TOGGLE_BUTTON, findHistory._incrementalSearchBarAtTop ? L"˅" : L"˄");
			::SendMessage(_hParent, NPPM_INTERNAL_INCSEARCHBARPOSITION, 0, 0);
			return TRUE;
		}

		case IDCANCEL :
			(*(_pFRDlg->_ppEditView))->clearIndicator(SCE_UNIVERSAL_FOUND_STYLE_INC);
			(*(_pFRDlg->_ppEditView))->grabFocus();
			display(false);
			_matches.clear();
			return TRUE;

		case IDM_SEARCH_FINDINCREMENT:	// Accel table: Start incremental search
			// if focus is on a some other control, return it to the edit field
			if (QApplication::focusWidget() != dlgItem(IDC_INCFINDTEXT))
				{
					dlgItem(IDC_INCFINDTEXT)->setFocus();
					return TRUE;
				}
			// otherwise, repeat the search
			[[fallthrough]];
		case IDM_SEARCH_FINDPREV:		// Accel table: find prev
		case IDM_SEARCH_FINDNEXT:		// Accel table: find next
		case IDC_INCFINDPREVOK:
		case IDC_INCFINDNXTOK:
		case IDOK:						// Enter key in the edit field
			updateSearch = true;
			advance = true;
			forward = (LOWORD(wParam) == IDC_INCFINDNXTOK) ||
				(LOWORD(wParam) == IDM_SEARCH_FINDNEXT) ||
				(LOWORD(wParam) == IDM_SEARCH_FINDINCREMENT) ||
				((LOWORD(wParam) == IDOK) && !(QGuiApplication::keyboardModifiers() & Qt::ShiftModifier));
			break;

		case IDC_INCFINDMATCHCASE:
			NppParameters::getInstance().getFindHistory()._isIncrementalMatchCase = isCheckedOrNot(IDC_INCFINDMATCHCASE);
			updateSearch = true;
			updateCase = true;
			updateHiLight = true;
			_matches.clear();
			break;

		case IDC_INCFINDHILITEALL:
			NppParameters::getInstance().getFindHistory()._isIncrementalHighlightAll = isCheckedOrNot(IDC_INCFINDHILITEALL);
			updateHiLight = true;
			break;

		case IDC_INCFINDCOUNT:
			NppParameters::getInstance().getFindHistory()._isIncrementalCount = isCheckedOrNot(IDC_INCFINDCOUNT);
			_matches.clear();
			break;

		case IDC_INCFINDTEXT:
			if (HIWORD(wParam) == EN_CHANGE)
			{
				updateSearch = true;
				updateHiLight = isCheckedOrNot(IDC_INCFINDHILITEALL);
				updateCase = isCheckedOrNot(IDC_INCFINDMATCHCASE);
				_matches.clear();
				break;
			}
			// treat other edit notifications as unhandled
			[[fallthrough]];
		default:
			return FALSE;
	}
	FindOption fo;
	fo._isWholeWord = false;
	fo._incrementalType = advance ? NextIncremental : FirstIncremental;
	fo._whichDirection = forward ? DIR_DOWN : DIR_UP;
	fo._isMatchCase = isCheckedOrNot(IDC_INCFINDMATCHCASE);

	wstring str2Search = getItemText(IDC_INCFINDTEXT);
	if (updateSearch)
	{
		FindStatus findStatus = FSFound;
		MatchPosition match;

		bool isFound = _pFRDlg->processFindNext(str2Search.c_str(), &fo, &findStatus, FINDNEXTTYPE_FINDNEXT, &match);

		fo._str2Search = str2Search;
		intptr_t nth = 0;

		if (isCheckedOrNot(IDC_INCFINDCOUNT))
		{
			if (!_matches.size())
				_pFRDlg->processAll(ProcessCountAll, &fo, false, nullptr, -1, &_matches);

			if (_matches.size() && (match.start > -1) && (match.end > -1))
			{
				size_t totalMatches = _matches.size();
				for (size_t i = 0; i < totalMatches; ++i)
				{
					if ((match.start == _matches[i].start) && (match.end == _matches[i].end))
					{
						nth = i + 1;
						break;
					}
				}
			}
		}

		setFindStatus(findStatus, _matches.size(), static_cast<int>(nth));

		// If case-sensitivity changed (to Match=yes), there may have been a matched selection that
		// now does not match; so if Not Found, clear selection and put caret at beginning of what was
		// selected (no change, if there was no selection)
		if (updateCase && !isFound)
		{
			Sci_CharacterRangeFull range = (*(_pFRDlg->_ppEditView))->getSelection();
			(*(_pFRDlg->_ppEditView))->execute(SCI_SETSEL, static_cast<WPARAM>(-1), range.cpMin);
		}
	}

	if (updateHiLight)
	{
		bool highlight = !str2Search.empty() && isCheckedOrNot(IDC_INCFINDHILITEALL);
		markSelectedTextInc(highlight, &fo);
	}
	return TRUE;
}

void FindIncrementDlg::markSelectedTextInc(bool enable, FindOption *opt)
{
	(*(_pFRDlg->_ppEditView))->clearIndicator(SCE_UNIVERSAL_FOUND_STYLE_INC);

	if (!enable)
		return;

	//Get selection
	Sci_CharacterRangeFull range = (*(_pFRDlg->_ppEditView))->getSelection();

	//If nothing selected, don't mark anything
	if (range.cpMin == range.cpMax)
		return;

	auto text2Find = (*(_pFRDlg->_ppEditView))->getSelectedTextToWChar(false);	//do not expand selection (false)
	if (text2Find.empty())
		return;

	opt->_str2Search = text2Find;
	_pFRDlg->markAllInc(opt);
}

void FindIncrementDlg::setFindStatus(FindStatus iStatus, size_t nbCounted, int nth)
{
	wstring statusStr2Display;

	static const wchar_t* const strFSNotFound = L"Phrase not found";
	static const wchar_t* const strFSTopReached = L"Reached top of page, continued from bottom";
	static const wchar_t* const strFSEndReached = L"Reached end of page, continued from top";

	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	if (nbCounted > 0)
		statusStr2Display = commafyInt(nth) + L"/" + commafyInt(nbCounted);

	switch (iStatus)
	{
		case FindStatus::FSNotFound:
			statusStr2Display = pNativeSpeaker->getLocalizedStrFromID("IncrementalFind-FSNotFound", strFSNotFound);
			break;

		case FindStatus::FSTopReached:
		{
			auto topReached = pNativeSpeaker->getLocalizedStrFromID("IncrementalFind-FSTopReached", strFSTopReached);

			if (nbCounted > 0)
				statusStr2Display += (L" - " + topReached);
			else
				statusStr2Display = topReached;

			break;
		}

		case FindStatus::FSEndReached:
		{
			auto endReached = pNativeSpeaker->getLocalizedStrFromID("IncrementalFind-FSEndReached", strFSEndReached);

			if (nbCounted > 0)
				statusStr2Display += (L" - " + endReached);
			else
				statusStr2Display = endReached;

			break;
		}

		case FindStatus::FSFound:
			break;

		case FindStatus::FSResetCount:
			statusStr2Display = L"";
			break;

		default:
			return; // out of range
	}

	_findStatus = iStatus;

	// the edit field turns red when the text is not found
	QString style;
	if (iStatus == FSNotFound)
	{
		const COLORREF background = NppDarkMode::isEnabled() ? NppDarkMode::getErrorBackgroundColor() : BCKGRD_COLOR;
		const COLORREF text = NppDarkMode::isEnabled() ? NppDarkMode::getTextColor() : TXT_COLOR;
		style = QStringLiteral("background-color: rgb(%1, %2, %3); color: rgb(%4, %5, %6);")
			.arg(GetRValue(background)).arg(GetGValue(background)).arg(GetBValue(background))
			.arg(GetRValue(text)).arg(GetGValue(text)).arg(GetBValue(text));
	}
	_ui->incfindtext->setStyleSheet(style);

	setItemText(IDC_INCFINDSTATUS, statusStr2Display);
}

void FindIncrementDlg::addToRebar(ReBar * rebar)
{
	if (_pRebar)
		return;

	_pRebar = rebar;
	_pRebar->addBand(REBAR_BAR_SEARCH, _hSelf);
	_pRebar->setIDVisible(REBAR_BAR_SEARCH, false);
}


Progress::Progress(HINSTANCE) {}

Progress::~Progress()
{
	close();
}

HWND Progress::open(HWND hCallerWnd, const wchar_t* header)
{
	if (_dialog)
		return _dialog;

	if (!hCallerWnd)
		return nullptr;

	_dialog = new QProgressDialog(hCallerWnd);
	_dialog->setWindowTitle(QString::fromWCharArray(header ? header : L"Operation progress..."));
	_dialog->setWindowModality(Qt::WindowModal);
	_dialog->setMinimumDuration(0);
	_dialog->setRange(0, 100);
	_dialog->setValue(0);
	_dialog->setMinimumWidth(500);
	_dialog->show();
	QCoreApplication::processEvents();
	return _dialog;
}

void Progress::close()
{
	delete _dialog;
	_dialog = nullptr;
}

bool Progress::isCancelled() const
{
	// The search runs on this thread: this is where the Cancel button gets its chance to respond
	QCoreApplication::processEvents();
	return _dialog && _dialog->wasCanceled();
}

void Progress::setPercent(unsigned percent, const wchar_t* fileName, int nbHitsSoFar) const
{
	if (_dialog)
	{
		_dialog->setValue(static_cast<int>(percent));
		setInfo(fileName, nbHitsSoFar);
	}
}

void Progress::setInfo(const wchar_t* info, int nbHitsSoFar) const
{
	if (_dialog)
	{
		QString text = QString::fromWCharArray(info);
		if (nbHitsSoFar != -1)
			text += QStringLiteral("\n") + QString::number(nbHitsSoFar);
		_dialog->setLabelText(text);
		QCoreApplication::processEvents();
	}
}
