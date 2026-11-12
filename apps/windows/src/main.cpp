#include "ui/icons.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"

#include <QApplication>
#include <QMessageBox>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MxU"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("mxu.app"));
    QCoreApplication::setApplicationName(QStringLiteral("MxU Slides"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setWindowIcon(mxu::ui::appIcon());

    mxu::ui::applyTheme(application);

    mxu::ui::MainWindow window;
    window.show();

    QString bundlePath;
    const QStringList arguments = application.arguments();
    for (int index = 1; index < arguments.size(); ++index) {
        const QString &argument = arguments.at(index);
        if (argument.endsWith(QStringLiteral(".mxuslides"), Qt::CaseInsensitive)) {
            bundlePath = argument;
            break;
        }
    }
    if (!bundlePath.isEmpty()) {
        QTimer::singleShot(0, &window, [&window, bundlePath] {
            window.openBundlePath(bundlePath);
        });
    }

    if (arguments.contains(QStringLiteral("--smoke-test"))) {
        QTimer::singleShot(1600, &application, &QCoreApplication::quit);
    }

    return application.exec();
}
