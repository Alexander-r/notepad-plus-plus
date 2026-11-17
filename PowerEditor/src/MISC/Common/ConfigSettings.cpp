// This file is part of Notepad++ project
// Copyright (c) 2021 adzm / Adam D. Walling

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

#include "ConfigSettings.h"

#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

namespace
{
	constexpr char rootTag[] = "NotepadPlus";
	constexpr char namedConfigTag[] = "GUIConfig";
	const QString textKey = QStringLiteral("_text");
	const QString emptyKey = QStringLiteral("_empty");

	bool isNamedConfig(const NppXml::Element& element)
	{
		return std::strcmp(element.name(), namedConfigTag) == 0 && element.attribute("name");
	}

	void writeElement(QSettings& settings, const NppXml::Element& element, bool omitNameAttribute)
	{
		bool hasContent = false;

		for (const auto& attribute : element.attributes())
		{
			if (omitNameAttribute && std::strcmp(attribute.name(), "name") == 0)
				continue;
			settings.setValue(QString::fromUtf8(attribute.name()), QString::fromUtf8(attribute.value()));
			hasContent = true;
		}

		const char* text = element.child_value();
		if (text && text[0])
		{
			settings.setValue(textKey, QString::fromUtf8(text));
			hasContent = true;
		}

		// Group the children by tag, keeping their order
		std::map<std::string, std::vector<NppXml::Element>> childrenByTag;
		for (const auto& child : element.children())
		{
			if (child.type() != pugi::node_element)
				continue;
			hasContent = true;

			if (isNamedConfig(child))
			{
				settings.beginGroup(QString::fromUtf8(child.attribute("name").value()));
				writeElement(settings, child, true);
				settings.endGroup();
			}
			else
			{
				childrenByTag[child.name()].push_back(child);
			}
		}

		for (const auto& [tag, children] : childrenByTag)
		{
			settings.beginGroup(QString::fromStdString(tag));
			if (children.size() == 1)
			{
				writeElement(settings, children.front(), false);
			}
			else
			{
				for (size_t i = 0; i < children.size(); ++i)
				{
					settings.beginGroup(QString::number(i + 1));
					writeElement(settings, children[i], false);
					settings.endGroup();
				}
			}
			settings.endGroup();
		}

		if (!hasContent)
			settings.setValue(emptyKey, true);
	}

	// QSettings reads an unquoted value containing a comma as a list, as a hand-edited file may have.
	QString readString(const QSettings& settings, const QString& key)
	{
		const QVariant value = settings.value(key);
		return value.typeId() == QMetaType::QStringList ? value.toStringList().join(QStringLiteral(", ")) : value.toString();
	}

	// Whether all subgroups are numbers: then they are repeated elements of one tag
	bool isNumberedList(const QStringList& groups)
	{
		if (groups.isEmpty())
			return false;
		return std::all_of(groups.begin(), groups.end(), [](const QString& g)
		{
			bool isNumber = false;
			g.toInt(&isNumber);
			return isNumber;
		});
	}

	void readElement(QSettings& settings, NppXml::Element element, bool childrenAreNamedConfigs);

	void readChildGroup(QSettings& settings, NppXml::Element parent, const QString& group, bool isNamedConfigGroup)
	{
		settings.beginGroup(group);
		if (isNamedConfigGroup)
		{
			NppXml::Element child = parent.append_child(namedConfigTag);
			child.append_attribute("name").set_value(group.toUtf8().constData());
			readElement(settings, child, false);
		}
		else
		{
			const QByteArray tag = group.toUtf8();
			QStringList subgroups = settings.childGroups();
			if (isNumberedList(subgroups) && settings.childKeys().isEmpty())
			{
				std::sort(subgroups.begin(), subgroups.end(), [](const QString& a, const QString& b) { return a.toInt() < b.toInt(); });
				for (const QString& index : subgroups)
				{
					settings.beginGroup(index);
					readElement(settings, parent.append_child(tag.constData()), false);
					settings.endGroup();
				}
			}
			else
			{
				readElement(settings, parent.append_child(tag.constData()), group == QStringLiteral("GUIConfigs"));
			}
		}
		settings.endGroup();
	}

	void readElement(QSettings& settings, NppXml::Element element, bool childrenAreNamedConfigs)
	{
		const QStringList keys = settings.childKeys();
		for (const QString& key : keys)
		{
			if (key == emptyKey)
				continue;
			if (key == textKey)
				element.append_child(pugi::node_pcdata).set_value(readString(settings, key).toUtf8().constData());
			else
				element.append_attribute(key.toUtf8().constData()).set_value(readString(settings, key).toUtf8().constData());
		}

		const QStringList groups = settings.childGroups();
		for (const QString& group : groups)
			readChildGroup(settings, element, group, childrenAreNamedConfigs);
	}
}

bool loadConfigFromSettings(NppXml::Document doc, const std::wstring& iniPath)
{
	const QString path = QString::fromStdWString(iniPath);
	if (!QFileInfo::exists(path))
		return false;

	QSettings settings(path, QSettings::IniFormat);
	if (settings.status() != QSettings::NoError)
		return false;

	doc->reset();
	NppXml::createNewDeclaration(doc);
	readElement(settings, doc->append_child(rootTag), false);
	return true;
}

bool saveConfigToSettings(NppXml::Document doc, const std::wstring& iniPath)
{
	const NppXml::Element root = NppXml::firstChildElement(doc, rootTag);
	if (!root)
		return false;

	QSettings settings(QString::fromStdWString(iniPath), QSettings::IniFormat);
	settings.clear();
	writeElement(settings, root, false);
	settings.sync();
	return settings.status() == QSettings::NoError;
}
