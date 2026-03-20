#ifndef QT_APP_H_
#define QT_APP_H_

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QWidget>
#include <QTimer>
#include <QThread>
#include <QStatusBar>
#include <QLabel>
#include <QPixmap>
#include <QToolButton>
#include <QPaintEngine>

#include "qt-utils.h"

extern "C" {
#include "qt-common.h"
}

#ifdef _WIN32
#include <QAbstractNativeEventFilter>

class RawMouseFilter : public QAbstractNativeEventFilter {
public:
        bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
};
#endif

class MainWindow;

/* SDL canvas widget that captures keyboard input */
class SDLCanvas : public QWidget {
        Q_OBJECT
public:
        SDLCanvas(QWidget *parent = nullptr);

protected:
        void keyPressEvent(QKeyEvent *event) override;
        void keyReleaseEvent(QKeyEvent *event) override;
        void focusInEvent(QFocusEvent *event) override;
        void focusOutEvent(QFocusEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void paintEvent(QPaintEvent *event) override;
        QPaintEngine *paintEngine() const override;
};

class PCemExitThread : public QThread {
        Q_OBJECT
public:
        PCemExitThread(MainWindow *mainWindow);

signals:
        void exitComplete(int result);

protected:
        void run() override;

private:
        MainWindow *mainWindow;
};

class MainWindow : public QMainWindow {
        Q_OBJECT
public:
        MainWindow(QWidget *parent = nullptr);
        virtual ~MainWindow();

        void start();
        QMenu *getMenu();
        QMenuBar *getQMenuBar();

        QWidget *sdlCanvas() const { return m_sdlCanvas; }

public slots:
        void onCallback(WX_CALLBACK callback, void *data);
        void onExit(int value);
        void onExitComplete(int result);
        void onStopEmulation();
        void onStopEmulationNow();
        void onShowWindow(QWidget *window, int show);
        void onPopupMenu(QMenu *menu, int *x, int *y);

protected:
        void closeEvent(QCloseEvent *event) override;

private:
        void showConfigSelection();
        void buildMenus();
        void quit(bool stopEmulator = true);

        QAction *addMenuItem(QMenu *menu, const char *id, const char *label, bool checkable = false, bool radio = false);
        QMenu *addSubmenu(QMenu *parent, const char *id, const char *label);

        void setupStatusBar();
        void updateStatusBar();
        void onDriveContextMenu(int driveIndex, const QPoint &pos);
        void showAboutDialog();

        QMenu *m_menu;
        SDLCanvas *m_sdlCanvas;
        bool m_closing;

        QTimer *m_statusTimer;
        QToolButton *m_statusDriveButtons[10];
        drive_info_t m_driveCache[10];
        QLabel *m_statusSpeedLabel;
        int m_numDriveLabels;

        QIcon m_iconFDD;
        QIcon m_iconFDDDisabled;
        QIcon m_iconHDD;
        QIcon m_iconCDROM;
        QIcon m_iconCDROMDisabled;

#ifdef _WIN32
        RawMouseFilter *m_rawMouseFilter;
#endif
};

#endif /* QT_APP_H_ */
