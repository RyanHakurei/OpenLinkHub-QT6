#include "app/MainWindow.h"
#include "openlinkhub-qt-version.h"

#include <KAboutData>
#include <KCrash>
#include <KLocalizedString>
#include <KIconTheme>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

int main(int argc, char **argv)
{
    // Do not call KStyleManager::initStyle() — that can replace Kvantum with Breeze.
    KIconTheme::initTheme();

    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("openlinkhub-qt");

    KAboutData about(QStringLiteral("openlinkhub-qt"),
                     i18n("OpenLinkHub"),
                     QStringLiteral(OPENLINKHUB_QT_VERSION_STRING),
                     i18n("Companion KDE frontend for the OpenLinkHub daemon"),
                     KAboutLicense::GPL_V3,
                     i18n("© 2026 Ryan"),
                     i18n("This is an independent companion to OpenLinkHub, the device daemon by Nikola. "
                          "It is not affiliated with, endorsed by, or part of the official OpenLinkHub project."),
                     QStringLiteral("https://github.com/RyanHakurei/OpenLinkHub-QT6"),
                     QStringLiteral("https://github.com/RyanHakurei/OpenLinkHub-QT6/issues"));
    about.addAuthor(i18n("Ryan"),
                    i18n("Submit an issue on GitHub"),
                    {},
                    QStringLiteral("https://github.com/RyanHakurei/OpenLinkHub-QT6/issues"));
    about.addCredit(i18n("Nikola"),
                    i18n("Author of OpenLinkHub"),
                    {},
                    QStringLiteral("https://github.com/jurkovic-nikola/OpenLinkHub"));
    about.setDesktopFileName(QStringLiteral("pw.freyja.OpenLinkHub"));
    KAboutData::setApplicationData(about);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("openlinkhub-qt")));

    KCrash::initialize();

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    parser.process(app);
    about.processCommandLine(&parser);

    MainWindow window;
    window.show();
    return app.exec();
}
