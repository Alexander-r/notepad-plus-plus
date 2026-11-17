// Checks the dialog base class.

#include <QtTest>
#include <QApplication>
#include <QDialog>

#include "StaticDialog.h"

namespace
{
	class TestDialog : public StaticDialog
	{
	public:
		int cancelCount = 0;
		int activationCount = 0;

		void create() { createDialogWidget(); }
		int runModalForTest() { return runModal(); }
		void finish(int result) { endDialog(result); }

	protected:
		void onCancel() override
		{
			++cancelCount;
			StaticDialog::onCancel();
		}
		void onActivationChanged(bool) override { ++activationCount; }
	};
}

class DialogTest : public QObject
{
	Q_OBJECT

private slots:
	void escapeCallsOnCancelAndHides()
	{
		TestDialog dlg;
		dlg.init(nullptr, nullptr);
		dlg.create();
		dlg.display(true);
		QVERIFY(dlg.isVisible());
		QTest::keyClick(dlg.getHSelf(), Qt::Key_Escape);
		QCOMPARE(dlg.cancelCount, 1);
		QVERIFY(!dlg.isVisible());
		QVERIFY(dlg.isCreated());
	}

	void closingTheWindowCallsOnCancel()
	{
		TestDialog dlg;
		dlg.init(nullptr, nullptr);
		dlg.create();
		dlg.display(true);
		dlg.getHSelf()->close();
		QCOMPARE(dlg.cancelCount, 1);
		QVERIFY(dlg.isCreated());
	}

	void modalDialogReturnsResultAndIsDestroyed()
	{
		TestDialog dlg;
		dlg.init(nullptr, nullptr);
		dlg.create();
		QTimer::singleShot(0, [&dlg]() { dlg.finish(1234); });
		QCOMPARE(dlg.runModalForTest(), 1234);
		QVERIFY(!dlg.isCreated());
	}

	void destroyIsSafeAndRepeatable()
	{
		TestDialog dlg;
		dlg.init(nullptr, nullptr);
		dlg.create();
		dlg.destroy();
		QVERIFY(!dlg.isCreated());
		dlg.destroy();
		dlg.create();
		QVERIFY(dlg.isCreated());
	}

	void messageRouting()
	{
		QWidget target;
		int received = 0;
		nppRegisterMessageTarget(&target, [&received](UINT message, WPARAM wParam, LPARAM lParam)
		{
			received = static_cast<int>(message + wParam + lParam);
			return LRESULT(42);
		});
		QCOMPARE(SendMessage(&target, 100, 2, 3), LRESULT(42));
		QCOMPARE(received, 105);

		// Posted messages arrive later
		received = 0;
		QVERIFY(PostMessage(&target, 200, 1, 1));
		QCOMPARE(received, 0);
		QCoreApplication::processEvents();
		QCOMPARE(received, 202);

		// Without a handler, the widget gets the message as an NppMessageEvent, as a plugin's panel does; one that
		// does not handle it gives 0
		nppUnregisterMessageTarget(&target);
		QCOMPARE(SendMessage(&target, 100, 2, 3), LRESULT(0));
		QVERIFY(!PostMessage(nullptr, 100));

		class EventTarget : public QWidget
		{
		public:
			int received = 0;

		protected:
			bool event(QEvent* event) override
			{
				if (event->type() != NppMessageEvent::eventType)
					return QWidget::event(event);
				auto* message = static_cast<NppMessageEvent*>(event);
				received = static_cast<int>(message->message + message->wParam + message->lParam);
				message->result = 7;
				return true;
			}
		};
		EventTarget eventTarget;
		QCOMPARE(SendMessage(&eventTarget, 100, 2, 3), LRESULT(7));
		QCOMPARE(eventTarget.received, 105);
		eventTarget.received = 0;
		QVERIFY(PostMessage(&eventTarget, 200, 1, 1));
		QCOMPARE(eventTarget.received, 0);
		QCoreApplication::processEvents();
		QCOMPARE(eventTarget.received, 202);
	}
};

QTEST_MAIN(DialogTest)
#include "dialogTest.moc"
