#include "qt-utils.h"

#include <QDir>
#include <QFileInfo>

int wx_dir_exists(char *path) {
        return QDir(path).exists();
}

void wx_get_home_directory(char *path) {
        QString home = QDir::homePath();
        if (!home.endsWith(QDir::separator()))
                home += QDir::separator();
#ifdef _WIN32
        home.remove('"');
#endif
        strcpy(path, home.toUtf8().constData());
}

int wx_create_directory(char *path) { return QDir().mkpath(path); }
