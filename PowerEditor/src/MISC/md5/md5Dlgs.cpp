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


#include "md5Dlgs.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QFontDatabase>
#include <QPlainTextEdit>

#include <string>

#include "Common.h"
#include "CustomFileDialog.h"
#include "NppConstants.h"
#include "Parameters.h"
#include "StaticDialog.h"
#include "md5Dlgs_rc.h"
#include "resource.h"
#include "ui_HashfromfilesDlg.h"
#include "ui_HashfromtextDlg.h"

namespace
{
	// The digest in lowercase hexadecimal
	QByteArray hashOf(hashType type, QByteArrayView content)
	{
		QCryptographicHash::Algorithm algorithm = QCryptographicHash::Md5;
		switch (type)
		{
			case hash_sha1: algorithm = QCryptographicHash::Sha1; break;
			case hash_sha256: algorithm = QCryptographicHash::Sha256; break;
			case hash_sha512: algorithm = QCryptographicHash::Sha512; break;
			default: break;
		}
		return QCryptographicHash::hash(content, algorithm).toHex();
	}

	void setFixedFont(QPlainTextEdit* edit)
	{
		edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	}
}

void HashFromFilesDlg::onChooseFiles()
{
	CustomFileDialog fDlg(_hSelf);
	fDlg.setExtFilter(L"All types", L".*");

	const auto& fns = fDlg.doOpenMultiFilesDlg();
	if (fns.empty())
		return;

	std::wstring files2check, hashResultStr;
	for (const auto& it : fns)
	{
		bool bLoadingFailed = false;
		std::string content = getFileContent(it.c_str(), &bLoadingFailed);
		if (bLoadingFailed)
			return;

		files2check += it;
		files2check += L"\r\n";

		hashResultStr += QString::fromLatin1(hashOf(_ht, QByteArrayView(content.data(), content.size()))).toStdWString();
		hashResultStr += L"  ";
		hashResultStr += PathFindFileName(it.c_str());
		hashResultStr += L"\r\n";
	}

	if (!files2check.empty() && !hashResultStr.empty())
	{
		setItemText(IDC_HASH_PATH_EDIT, files2check);
		setItemText(IDC_HASH_RESULT_EDIT, hashResultStr);
	}
}

void HashFromFilesDlg::setHashType(hashType hashType2set)
{
	_ht = hashType2set;
}

void HashFromFilesDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget(isRTL);
		Ui::HashfromfilesDlg ui;
		ui.setupUi(dialog);

		setFixedFont(ui.hashPathEdit);
		setFixedFont(ui.hashResultEdit);

		QString title;
		QString buttonText;
		switch (_ht)
		{
			case hash_md5:
				title = QStringLiteral("Generate MD5 digest from files");
				buttonText = QStringLiteral("Choose files to &generate MD5...");
				break;

			case hash_sha1:
				title = QStringLiteral("Generate SHA-1 digest from files");
				buttonText = QStringLiteral("Choose files to &generate SHA-1...");
				break;

			case hash_sha256:
				title = QStringLiteral("Generate SHA-256 digest from files");
				buttonText = QStringLiteral("Choose files to &generate SHA-256...");
				break;

			case hash_sha512:
				title = QStringLiteral("Generate SHA-512 digest from files");
				buttonText = QStringLiteral("Choose files to &generate SHA-512...");
				break;

			default:
				return;
		}
		dialog->setWindowTitle(title);
		ui.hashFilebrowserButton->setText(buttonText);

		QObject::connect(ui.cancel, &QAbstractButton::clicked, dialog, [this]() { display(false); });
		QObject::connect(ui.hashFilebrowserButton, &QAbstractButton::clicked, dialog, [this]() { onChooseFiles(); });
		QObject::connect(ui.hashToclipboardButton, &QAbstractButton::clicked, dialog, [this]()
		{
			const std::wstring result = getItemText(IDC_HASH_RESULT_EDIT);
			if (!result.empty())
				str2Clipboard(result, _hSelf);
		});
	}

	// Adjust the position in the center
	goToCenter();
	display();
}

void HashFromTextDlg::generateHash()
{
	if (_ht != hash_md5 && _ht != hash_sha1 && _ht != hash_sha256 && _ht != hash_sha512)
		return;

	// it's important to get text from UNICODE then convert it to UTF8
	// So we get the result of UTF8 text (tested with Chinese).
	const QByteArray text = QString::fromStdWString(getItemText(IDC_HASH_TEXT_EDIT)).toUtf8();
	setItemText(IDC_HASH_RESULT_FOMTEXT_EDIT, text.isEmpty() ? std::wstring{} : QString::fromLatin1(hashOf(_ht, text)).toStdWString());
}

void HashFromTextDlg::generateHashPerLine()
{
	const QString text = QString::fromStdWString(getItemText(IDC_HASH_TEXT_EDIT));
	if (text.isEmpty())
	{
		setItemText(IDC_HASH_RESULT_FOMTEXT_EDIT, L"");
		return;
	}

	QStringList lines = text.split(u'\n');
	if (lines.last().isEmpty()) // text ending with an EOL has no further line
		lines.removeLast();

	QString result;
	for (QString& line : lines)
	{
		// the line can still hold the '\r' of a Windows EOL
		if (line.endsWith(u'\r'))
			line.chop(1);

		if (!line.isEmpty())
			result += QString::fromLatin1(hashOf(_ht, line.toUtf8()));
		result += QStringLiteral("\r\n");
	}

	setItemText(IDC_HASH_RESULT_FOMTEXT_EDIT, result.toStdWString());
}

void HashFromTextDlg::setHashType(hashType hashType2set)
{
	_ht = hashType2set;
}

void HashFromTextDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget(isRTL);
		Ui::HashfromtextDlg ui;
		ui.setupUi(dialog);

		setFixedFont(ui.hashTextEdit);
		setFixedFont(ui.hashResultFomtextEdit);

		QString title;
		switch (_ht)
		{
			case hash_md5: title = QStringLiteral("Generate MD5 digest"); break;
			case hash_sha1: title = QStringLiteral("Generate SHA-1 digest"); break;
			case hash_sha256: title = QStringLiteral("Generate SHA-256 digest"); break;
			case hash_sha512: title = QStringLiteral("Generate SHA-512 digest"); break;
			default: break;
		}
		dialog->setWindowTitle(title);

		const auto regenerate = [this]()
		{
			if (isCheckedOrNot(IDC_HASH_EACHLINE_CHECK))
				generateHashPerLine();
			else
				generateHash();
		};
		QObject::connect(ui.hashTextEdit, &QPlainTextEdit::textChanged, dialog, regenerate);
		QObject::connect(ui.hashEachlineCheck, &QAbstractButton::clicked, dialog, regenerate);
		QObject::connect(ui.cancel, &QAbstractButton::clicked, dialog, [this]() { display(false); });
		QObject::connect(ui.hashFromtextToclipboardButton, &QAbstractButton::clicked, dialog, [this]()
		{
			const std::wstring result = getItemText(IDC_HASH_RESULT_FOMTEXT_EDIT);
			if (!result.empty())
				str2Clipboard(result, _hSelf);
		});
	}

	// Adjust the position in the center
	goToCenter();
	display();
}
