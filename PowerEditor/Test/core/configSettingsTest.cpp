// The settings must survive the trip through config.ini exactly, because the original code reads them.

#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>

#include <sstream>
#include <string>

#include "ConfigSettings.h"

namespace
{
	// Children sorted by name and attributes as they are, because config.ini has no order between different tags
	std::string canonical(const NppXml::Element& element)
	{
		std::vector<std::string> attributes;
		for (const auto& a : element.attributes())
			attributes.push_back(std::string(a.name()) + "=" + a.value());
		std::sort(attributes.begin(), attributes.end());

		std::vector<std::string> children;
		std::map<std::string, std::vector<std::string>> byTag;
		for (const auto& c : element.children())
		{
			if (c.type() == pugi::node_element)
				byTag[c.name()].push_back(canonical(c));
		}

		std::ostringstream out;
		out << "<" << element.name();
		for (const auto& a : attributes)
			out << " " << a;
		out << ">" << element.child_value();
		for (auto& [tag, list] : byTag)
		{
			// GUIConfig is looked up by name, so its order is free. Other repeated tags keep their order.
			if (tag == "GUIConfig")
				std::sort(list.begin(), list.end());
			for (const auto& c : list)
				out << c;
		}
		out << "</" << element.name() << ">";
		return out.str();
	}

	std::string canonicalOf(pugi::xml_document& doc)
	{
		return canonical(doc.child("NotepadPlus"));
	}
}

class ConfigSettingsTest : public QObject
{
	Q_OBJECT

private slots:
	void roundTrip()
	{
		std::string many;
		for (int i = 1; i <= 12; ++i) // 12 sorts before 2 as text, and the order must still hold
			many += "<File filename=\"/home/u/file" + std::to_string(i) + ".txt\"/>";

		const std::string xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" ?><NotepadPlus>"
			"<FindHistory nbMaxFindHistoryPath=\"10\" transparency=\"150\" isSearch2ButtonsMode=\"no\">"
			"<Path name=\"/home/u/\xD0\xBF\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82\"/><Path name=\"C:\\Program Files\\x\"/>"
			"<Filter name=\"*.cpp *.h, !*.obj; x\"/><Find name=\"a=b &amp; &quot;c&quot; [x]\"/><Find name=\"@odd\"/><Find name=\" leading\"/><Replace name=\"\"/></FindHistory>"
			"<History nbMaxFile=\"10\" inSubMenu=\"no\">" + many + "</History>"
			"<ProjectPanels><ProjectPanel id=\"0\" workSpaceFile=\"\"/><ProjectPanel id=\"1\" workSpaceFile=\"/w.xml\"/></ProjectPanels>"
			"<GUIConfigs>"
			"<GUIConfig name=\"ToolBar\" visible=\"yes\" fluentColor=\"0\">standard</GUIConfig>"
			"<GUIConfig name=\"TabBar\" dragAndDrop=\"yes\"/>"
			"<GUIConfig name=\"Auto-detection\">yes</GUIConfig>"
			"<GUIConfig name=\"ScintillaViewsSplitter\">vertical</GUIConfig>"
			"<GUIConfig name=\"DockingManager\" leftWidth=\"200\"><PluginDlg pluginName=\"a.so\" id=\"1\"/><PluginDlg pluginName=\"b.so\" id=\"2\"/><ActiveTabs cont=\"0\" activeTab=\"-1\"/></GUIConfig>"
			"<GUIConfig name=\"Empty\"/>"
			"<GUIConfig name=\"SmartHighLight\" matchCase=\"no\">multiline\nvalue</GUIConfig>"
			"</GUIConfigs></NotepadPlus>";

		pugi::xml_document original;
		QVERIFY(original.load_string(xml.c_str()));

		QTemporaryDir dir;
		const std::wstring path = (dir.path() + QStringLiteral("/config.ini")).toStdWString();
		QVERIFY(saveConfigToSettings(&original, path));

		pugi::xml_document loaded;
		QVERIFY(loadConfigFromSettings(&loaded, path));
		QCOMPARE(canonicalOf(loaded), canonicalOf(original));

		// Saved again from what was loaded, the file must be stable
		QVERIFY(saveConfigToSettings(&loaded, path));
		pugi::xml_document loadedAgain;
		QVERIFY(loadConfigFromSettings(&loadedAgain, path));
		QCOMPARE(canonicalOf(loadedAgain), canonicalOf(original));
	}

	void iniIsReadableAndUsesNames()
	{
		pugi::xml_document doc;
		QVERIFY(doc.load_string("<NotepadPlus><GUIConfigs><GUIConfig name=\"TabBar\" dragAndDrop=\"yes\"/></GUIConfigs></NotepadPlus>"));
		QTemporaryDir dir;
		const QString path = dir.path() + QStringLiteral("/config.ini");
		QVERIFY(saveConfigToSettings(&doc, path.toStdWString()));

		QSettings settings(path, QSettings::IniFormat);
		QCOMPARE(settings.value(QStringLiteral("GUIConfigs/TabBar/dragAndDrop")).toString(), QStringLiteral("yes"));
	}

	void handEditedValueWithCommaIsOneString()
	{
		QTemporaryDir dir;
		const QString path = dir.path() + QStringLiteral("/config.ini");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write("[GUIConfigs]\nTabBar\\list=a, b\n");
		file.close();

		pugi::xml_document doc;
		QVERIFY(loadConfigFromSettings(&doc, path.toStdWString()));
		const auto tabBar = doc.child("NotepadPlus").child("GUIConfigs").find_child_by_attribute("GUIConfig", "name", "TabBar");
		QVERIFY(tabBar);
		QCOMPARE(std::string(tabBar.attribute("list").value()), std::string("a, b"));
	}

	void missingFileIsReported()
	{
		pugi::xml_document doc;
		QVERIFY(!loadConfigFromSettings(&doc, L"/nonexistent-dir/config.ini"));
	}
};

QTEST_MAIN(ConfigSettingsTest)
#include "configSettingsTest.moc"
