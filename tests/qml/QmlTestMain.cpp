#include "UiTypography.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QtQuickTest/quicktest.h>

class QmlTestSetup final : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        QGuiApplication::setFont(tweakopedia::ui::UiTypography::applicationFont());
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tweakopedia_qml, QmlTestSetup)

#include "QmlTestMain.moc"
