// The Function List parsers against the samples in PowerEditor/Test/FunctionList. Each one runs the application with
// -export=functionList, which writes what the panel would show to a file next to the document, and quits.
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class FunctionListTest : public QObject
{
	Q_OBJECT

private:
	QTemporaryDir _home;

	static QByteArray normalized(QByteArray text)
	{
		if (text.startsWith("\xEF\xBB\xBF"))
			text.remove(0, 3);
		text.replace("\r\n", "\n");
		text.replace("\\r\\n", "\\n"); // inside the names, which depends on the sample's line endings
		return text.trimmed();
	}

	static QByteArray readAll(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

private slots:
	void initTestCase()
	{
		// Two more parsers, for the samples of a user defined language
		const QDir target(_home.filePath(QStringLiteral(".config/notepad++/functionList")));
		QVERIFY(target.mkpath(QStringLiteral(".")));
		const QDir source(QStringLiteral(NPP_FUNCTIONLIST_TESTFILES_DIR));
		const QStringList files = source.entryList(QDir::Files);
		for (const QString& name : files)
			QVERIFY(QFile::copy(source.filePath(name), target.filePath(name)));
	}

	void parser_data()
	{
		QTest::addColumn<QString>("sample");
		QTest::addColumn<QString>("language");
		QTest::addColumn<bool>("isCrlf");

		const QDir root(QStringLiteral(NPP_FUNCTIONLIST_DIR));
		const QStringList languages = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
		for (const QString& language : languages)
		{
			const QDir languageDir(root.filePath(language));
			if (languageDir.exists(QStringLiteral("unitTest")))
			{
				QTest::newRow(qPrintable(language)) << languageDir.path() << language << false;
				QTest::newRow(qPrintable(language + QStringLiteral(" crlf"))) << languageDir.path() << language << true;
			}

			const QStringList variants = languageDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
			for (const QString& variant : variants)
			{
				if (QDir(languageDir.filePath(variant)).exists(QStringLiteral("unitTest")))
				{
					const QString name = language + QLatin1Char('-') + variant;
					QTest::newRow(qPrintable(name)) << languageDir.filePath(variant) << language << false;
					QTest::newRow(qPrintable(name + QStringLiteral(" crlf"))) << languageDir.filePath(variant) << language << true;
				}
			}
		}
	}

	void parser()
	{
		QFETCH(QString, sample);
		QFETCH(QString, language);
		QFETCH(bool, isCrlf);

		// The result goes next to the document, so that is a copy. Upstream runs these with the CRLF of a Windows checkout.
		QTemporaryDir work;
		QVERIFY(work.isValid());
		const QString document = work.filePath(QStringLiteral("unitTest"));
		QByteArray text = readAll(QDir(sample).filePath(QStringLiteral("unitTest")));
		text.replace("\r\n", "\n");
		if (isCrlf)
			text.replace("\n", "\r\n");
		QFile copy(document);
		QVERIFY(copy.open(QIODevice::WriteOnly));
		copy.write(text);
		copy.close();

		QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
		environment.insert(QStringLiteral("HOME"), _home.path());
		environment.insert(QStringLiteral("XDG_CONFIG_HOME"), _home.filePath(QStringLiteral(".config")));
		environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));

		const QString languageArgument = language.startsWith(QStringLiteral("udl-"))
			? QStringLiteral("-udl=") + language.mid(4)
			: QStringLiteral("-l") + language;

		QProcess application;
		application.setProcessEnvironment(environment);
		application.setProcessChannelMode(QProcess::MergedChannels);
		application.start(QStringLiteral(NPP_APPLICATION), { QStringLiteral("-export=functionList"), languageArgument, document });
		QVERIFY2(application.waitForFinished(30000), qPrintable(sample));

		const QString resultPath = document + QStringLiteral(".result.json");
		QVERIFY2(QFile::exists(resultPath), qPrintable(sample));

		const QByteArray expected = normalized(readAll(QDir(sample).filePath(QStringLiteral("unitTest.expected.result"))));
		const QByteArray generated = normalized(readAll(resultPath));
		QVERIFY(!expected.isEmpty());
		if (generated != expected)
			qWarning("Generated for %s:\n%s", qPrintable(sample), generated.left(600).constData());
		QCOMPARE(generated, expected);
	}
};

QTEST_GUILESS_MAIN(FunctionListTest)
#include "functionListTest.moc"
