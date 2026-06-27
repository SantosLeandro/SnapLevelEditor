#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include <cstdlib>
#include "MainWindow.h"
#include "StyleSheet.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyleSheet(kAppStyleSheet);

    MainWindow window;
    window.show();

    // Optional: used only for automated screenshot verification during
    // development (QT_SNAPSHOT_PATH env var). Not needed to run the app.
    if (const char *snapshotPath = std::getenv("QT_SNAPSHOT_PATH")) {
        QTimer::singleShot(300, [&window, snapshotPath]() {
            window.grab().save(QString::fromUtf8(snapshotPath));
            qApp->quit();
        });
    }

    return app.exec();
}
