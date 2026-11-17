// Loads every dialog form, and checks what the code and the translation files rely on.
// Set NPP_TEST_FORMS_DIR to write a screenshot of each form there, for review.

#include <QtTest>
#include <QApplication>
#include <QDirIterator>
#include <QLabel>
#include <QLayout>
#include <QUiLoader>

#ifndef NPP_SOURCE_DIR
#error NPP_SOURCE_DIR must be defined
#endif

class UiFormsTest : public QObject
{
	Q_OBJECT

private:
	static QStringList forms()
	{
		QStringList result;
		QDirIterator it(QStringLiteral(NPP_SOURCE_DIR), {QStringLiteral("*.ui")}, QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext())
			result << it.next();
		result.sort();
		return result;
	}

	static std::unique_ptr<QWidget> load(const QString& path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
			return nullptr;
		QUiLoader loader;
		return std::unique_ptr<QWidget>(loader.load(&file));
	}

private slots:
	void formsExist()
	{
		QVERIFY(forms().size() >= 50);
	}

	void everyFormLoadsWithALayout()
	{
		for (const QString& path : forms())
		{
			const auto form = load(path);
			QVERIFY2(form != nullptr, qPrintable(path));
			if (!form->findChildren<QWidget*>().isEmpty())
				QVERIFY2(form->layout() != nullptr, qPrintable(path));
		}
	}

	void controlIdsAreUniqueWithinAForm()
	{
		for (const QString& path : forms())
		{
			const auto form = load(path);
			QHash<int, QString> seen;
			const auto children = form->findChildren<QWidget*>();
			for (const QWidget* child : children)
			{
				const QVariant id = child->property("nppId");
				if (!id.isValid() || id.toInt() <= 0)
					continue; // static text without an ID
				// The Find dialog reuses no IDs, but two forms of tab pages may share a label: report exact pairs
				const QString previous = seen.value(id.toInt());
				QVERIFY2(previous.isEmpty(), qPrintable(path + ": " + previous + " and " + child->objectName() + " share ID " + QString::number(id.toInt())));
				seen.insert(id.toInt(), child->objectName());
			}
		}
	}

	void renderForReview()
	{
		const QByteArray output = qgetenv("NPP_TEST_FORMS_DIR");
		if (output.isEmpty())
			QSKIP("set NPP_TEST_FORMS_DIR to write screenshots");
		for (const QString& path : forms())
		{
			const auto form = load(path);
			form->adjustSize();
			form->grab().save(QString::fromLocal8Bit(output) + u'/' + QFileInfo(path).baseName() + QStringLiteral(".png"));
		}
	}
};

QTEST_MAIN(UiFormsTest)
#include "uiFormsTest.moc"
