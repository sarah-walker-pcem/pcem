#include "qt-utils.h"

#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QProgressDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QImage>
#include <QSettings>
#include <QDateTime>
#include <QHash>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QMainWindow>
#include <QMetaObject>
#include <QThread>
#include <QWidget>
#include <QCursor>
#include <QAction>

#include <QLayout>

#include <cstdarg>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#undef BITMAP
#endif

#include "qt-app.h"
#include "qt-dialogbox.h"
#include "qt-common.h"
#include "qt-status.h"
extern "C" {
#include "thread.h"
extern void pclog(const char *format, ...);
extern void *ghwnd;
void wx_handle_command(void *, int, int);
}

int (*wx_keydown_func)(void *window, void *event, int keycode, int modifiers) = nullptr;
int (*wx_keyup_func)(void *window, void *event, int keycode, int modifiers) = nullptr;
void (*wx_idle_func)(void *window, void *event) = nullptr;

static QHash<QString, int> s_xrcIdMap;
static int s_nextXrcId = 10000;

/* Pre-register a range of IDs so FOO[0], FOO[1], ..., FOO[N] are sequential,
   and FOO[start]/FOO[end] bracket them. */
static void registerIdRange(const char *prefix, const int *indices, int count) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s[start]", prefix);
        int startId = s_nextXrcId;
        s_xrcIdMap.insert(QString(buf), startId);

        for (int i = 0; i < count; i++) {
                snprintf(buf, sizeof(buf), "%s[%d]", prefix, indices[i]);
                s_xrcIdMap.insert(QString(buf), startId + indices[i]);
        }

        /* Find the max index to set the end marker */
        int maxIdx = 0;
        for (int i = 0; i < count; i++)
                if (indices[i] > maxIdx)
                        maxIdx = indices[i];

        snprintf(buf, sizeof(buf), "%s[end]", prefix);
        s_xrcIdMap.insert(QString(buf), startId + maxIdx);
        s_nextXrcId = startId + maxIdx + 1;
}

static void initIdRanges() {
        static bool initialized = false;
        if (initialized)
                return;
        initialized = true;

        /* IDM_VID_RESOLUTION: 0, 1, 2 */
        { int idx[] = {0, 1, 2}; registerIdRange("IDM_VID_RESOLUTION", idx, 3); }
        /* IDM_VID_FS: 0, 1, 2, 3 */
        { int idx[] = {0, 1, 2, 3}; registerIdRange("IDM_VID_FS", idx, 4); }
        /* IDM_VID_SCALE_MODE: 0, 1 */
        { int idx[] = {0, 1}; registerIdRange("IDM_VID_SCALE_MODE", idx, 2); }
        /* IDM_VID_SCALE: 0-7 */
        { int idx[] = {0, 1, 2, 3, 4, 5, 6, 7}; registerIdRange("IDM_VID_SCALE", idx, 8); }
        /* IDM_VID_FS_MODE: 0, 1 */
        { int idx[] = {0, 1}; registerIdRange("IDM_VID_FS_MODE", idx, 2); }
        /* IDM_VID_RENDER_DRIVER: 0, 1, 2, 5, 6 */
        { int idx[] = {0, 1, 2, 5, 6}; registerIdRange("IDM_VID_RENDER_DRIVER", idx, 5); }
        /* IDM_VID_GL3_INPUT_STRETCH: 0-3 */
        { int idx[] = {0, 1, 2, 3}; registerIdRange("IDM_VID_GL3_INPUT_STRETCH", idx, 4); }
        /* IDM_VID_GL3_INPUT_SCALE: 0-7 */
        { int idx[] = {0, 1, 2, 3, 4, 5, 6, 7}; registerIdRange("IDM_VID_GL3_INPUT_SCALE", idx, 8); }
        /* IDM_VID_GL3_SHADER_REFRESH_RATE: 0, 10, 25, 30, 50, 60, 72, 85 */
        { int idx[] = {0, 10, 25, 30, 50, 60, 72, 85}; registerIdRange("IDM_VID_GL3_SHADER_REFRESH_RATE", idx, 8); }
        /* IDM_SND_BUF: 0-3 */
        { int idx[] = {0, 1, 2, 3}; registerIdRange("IDM_SND_BUF", idx, 4); }
        /* IDM_SND_GAIN: 0-9 */
        { int idx[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}; registerIdRange("IDM_SND_GAIN", idx, 10); }
        /* IDM_SCREENSHOT_FORMAT: 0-3 */
        { int idx[] = {0, 1, 2, 3}; registerIdRange("IDM_SCREENSHOT_FORMAT", idx, 4); }
}

static int getOrCreateXrcId(const QString &name) {
        initIdRanges();
        auto it = s_xrcIdMap.find(name);
        if (it != s_xrcIdMap.end())
                return it.value();
        int id = s_nextXrcId++;
        s_xrcIdMap.insert(name, id);
        return id;
}

int confirm() {
        if (emulation_state != EMULATION_STOPPED) {
                return wx_messagebox(nullptr, "This will reset PCem!\nOkay to continue?", "PCem", WX_MB_OKCANCEL) == WX_IDOK;
        }
        return 1;
}

int wx_messagebox(void *window, const char *message, const char *title, int style) {
        QMessageBox::StandardButtons buttons;
        if (style & 0x00000010) /* wxCANCEL */
                buttons = QMessageBox::Ok | QMessageBox::Cancel;
        else if (style & 0x00000002) /* wxYES */
                buttons = QMessageBox::Yes | QMessageBox::No;
        else
                buttons = QMessageBox::Ok;

        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton;
        if (style & 0x00000080) /* wxNO_DEFAULT */
                defaultButton = QMessageBox::No;

        QMessageBox msgBox(QMessageBox::Information, title ? title : "PCem", message,
                           buttons, static_cast<QWidget *>(window));
        msgBox.setDefaultButton(defaultButton);
        int result = msgBox.exec();

        if (result == QMessageBox::Ok || result == QMessageBox::Yes)
                return WX_IDOK;
        return 0;
}

void wx_simple_messagebox(const char *title, const char *format, ...) {
        char message[2048];
        va_list ap;
        va_start(ap, format);
        vsnprintf(message, 2047, format, ap);
        message[2047] = 0;
        va_end(ap);
        wx_messagebox(nullptr, message, title, WX_MB_OK);
}

int wx_textentrydialog(void *window, const char *message, const char *title, const char *value, unsigned int min_length,
                       unsigned int max_length, LONG_PARAM result) {
        while (1) {
                bool ok;
                QString text = QInputDialog::getText(static_cast<QWidget *>(window), title, message,
                                                     QLineEdit::Normal, value, &ok);
                if (ok) {
                        if ((unsigned int)text.length() >= min_length &&
                            (max_length == 0 || (unsigned int)text.length() <= max_length)) {
                                strcpy((char *)result, text.toUtf8().constData());
                                return 1;
                        }
                } else
                        return 0;
        }
        return 0;
}

void wx_setwindowtitle(void *window, char *s) {
        if (window)
                static_cast<QMainWindow *>(window)->setWindowTitle(s);
}

int wx_xrcid(const char *s) { return getOrCreateXrcId(QString(s)); }

int wx_filedialog(void *window, const char *title, const char *path, const char *extensions, const char *extension, int open,
                  char *file) {
        QString filter = extensions ? QString(extensions) : QString();
        QString selectedFile;

        if (open) {
                selectedFile = QFileDialog::getOpenFileName(static_cast<QWidget *>(window), title, path, filter);
        } else {
                selectedFile = QFileDialog::getSaveFileName(static_cast<QWidget *>(window), title, path, filter);
        }

        if (!selectedFile.isEmpty()) {
                if (!open && extension) {
                        QFileInfo fi(selectedFile);
                        if (fi.suffix().isEmpty()) {
                                if (!selectedFile.endsWith("."))
                                        selectedFile += ".";
                                selectedFile += extension;
                        }
                }
                strcpy(file, selectedFile.toUtf8().constData());
                return 0;
        }
        return 1;
}

static QAction *findActionById(QList<QAction *> actions, int id, QMenu **parentMenu) {
        for (QAction *action : actions) {
                if (action->data().toInt() == id) {
                        return action;
                }
                if (action->menu()) {
                        QAction *found = findActionById(action->menu()->actions(), id, parentMenu);
                        if (found) {
                                if (parentMenu && !*parentMenu)
                                        *parentMenu = action->menu();
                                return found;
                        }
                }
        }
        return nullptr;
}

void wx_checkmenuitem(void *menu, int id, int checked) {
        QMenuBar *menuBar = qobject_cast<QMenuBar *>(static_cast<QObject *>(menu));
        QMenu *qmenu = qobject_cast<QMenu *>(static_cast<QObject *>(menu));

        QList<QAction *> actions;
        if (menuBar)
                actions = menuBar->actions();
        else if (qmenu)
                actions = qmenu->actions();

        QMenu *parentMenu = nullptr;
        QAction *target = findActionById(actions, id, &parentMenu);
        if (!target)
                return;

        target->setChecked(checked);

        /* Radio behavior: if checking a checkable item, uncheck all other
           checkable items in the same menu (simulates QActionGroup) */
        if (checked && target->isCheckable() && parentMenu) {
                for (QAction *sibling : parentMenu->actions()) {
                        if (sibling != target && sibling->isCheckable())
                                sibling->setChecked(false);
                }
        }
}

void wx_enablemenuitem(void *menu, int id, int enable) {
        QMenuBar *menuBar = qobject_cast<QMenuBar *>(static_cast<QObject *>(menu));
        QMenu *qmenu = qobject_cast<QMenu *>(static_cast<QObject *>(menu));

        QList<QAction *> actions;
        if (menuBar)
                actions = menuBar->actions();
        else if (qmenu)
                actions = qmenu->actions();

        for (QAction *action : actions) {
                if (action->data().toInt() == id) {
                        action->setEnabled(enable);
                        return;
                }
                if (action->menu()) {
                        wx_enablemenuitem(action->menu(), id, enable);
                }
        }
}

static QMenu *findSubmenuById(QList<QAction *> actions, int id) {
        for (QAction *action : actions) {
                if (action->data().toInt() == id && action->menu())
                        return action->menu();
                if (action->menu()) {
                        QMenu *found = findSubmenuById(action->menu()->actions(), id);
                        if (found)
                                return found;
                }
        }
        return nullptr;
}

void *wx_getsubmenu(void *menu, int id) {
        QWidget *widget = static_cast<QWidget *>(menu);

        QMenuBar *menuBar = qobject_cast<QMenuBar *>(widget);
        if (menuBar)
                return findSubmenuById(menuBar->actions(), id);

        QMenu *qmenu = qobject_cast<QMenu *>(widget);
        if (qmenu)
                return findSubmenuById(qmenu->actions(), id);

        return nullptr;
}

void *wx_getnativemenu(void *menu) {
#ifdef _WIN32
        QWidget *widget = static_cast<QWidget *>(menu);
        return (void *)widget->winId();
#endif
        return nullptr;
}

void *wx_getnativewindow(void *window) {
#ifdef _WIN32
        QWidget *widget = static_cast<QWidget *>(window);
        return (void *)widget->winId();
#endif
        return nullptr;
}

#ifdef _WIN32
void wx_winsendmessage(void *window, int msg, INT_PARAM wParam, LONG_PARAM lParam) {
        MainWindow *mainWindow = static_cast<MainWindow *>(window);
        QMetaObject::invokeMethod(mainWindow, [mainWindow, msg, wParam, lParam]() {
                HWND hwnd = (HWND)mainWindow->winId();
                SendMessage(hwnd, msg, wParam, lParam);
        }, Qt::QueuedConnection);
}
#endif

void wx_appendmenu(void *sub_menu, int id, const char *title, int type) {
        QMenu *menu = static_cast<QMenu *>(sub_menu);
        QAction *action = menu->addAction(title);
        action->setData(id);
        if (type == 1) /* wxITEM_CHECK */
                action->setCheckable(true);
        else if (type == 2) /* wxITEM_RADIO */
                action->setCheckable(true);

        /* Connect to command handler */
        int actionId = id;
        QObject::connect(action, &QAction::triggered, [actionId]() {
                wx_handle_command(ghwnd, actionId, 0);
        });
}

void wx_enabletoolbaritem(void *toolbar, int id, int enable) {
        /* Toolbar removed - no-op */
}

void *wx_getmenu(void *window) {
        MainWindow *mainWindow = static_cast<MainWindow *>(window);
        return mainWindow->getQMenuBar();
}

void *wx_getmenubar(void *window) {
        MainWindow *mainWindow = static_cast<MainWindow *>(window);
        return mainWindow->getQMenuBar();
}

void *wx_gettoolbar(void *window) {
        /* Toolbar removed */
        return nullptr;
}

void *wx_getdlgitem(void *window, int id) {
        if (!window)
                return nullptr;
        PCemDialogBox *dlg = qobject_cast<PCemDialogBox *>(static_cast<QObject *>(window));
        if (dlg) {
                void *result = dlg->findWidgetById(id);
#ifndef RELEASE_BUILD
                if (!result)
                        pclog("wx_getdlgitem: widget id %d not found\n", id);
#endif
                return result;
        }

        /* Fallback: search children by property */
        QWidget *widget = static_cast<QWidget *>(window);
        for (QWidget *child : widget->findChildren<QWidget *>()) {
                if (child->property("widgetId").toInt() == id)
                        return child;
        }
        return nullptr;
}

void wx_setdlgitemtext(void *window, int id, char *str) {
        if (!window)
                return;
        QWidget *w = static_cast<QWidget *>(wx_getdlgitem(window, id));
        if (!w)
                return;

        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(w);
        if (lineEdit) {
                lineEdit->setText(str);
                return;
        }

        QLabel *label = qobject_cast<QLabel *>(w);
        if (label) {
                label->setText(str);
                return;
        }
}

void wx_enablewindow(void *window, int enabled) {
        if (window)
                static_cast<QWidget *>(window)->setEnabled(enabled);
}

void wx_showwindow(void *window, int show) {
        if (!window)
                return;
        QWidget *widget = static_cast<QWidget *>(window);
        MainWindow *mainWindow = qobject_cast<MainWindow *>(QApplication::activeWindow());
        if (mainWindow) {
                QMetaObject::invokeMethod(mainWindow, [mainWindow, widget, show]() {
                        mainWindow->onShowWindow(widget, show);
                }, Qt::QueuedConnection);
        }
}

int wx_iswindowvisible(void *window) { return window ? static_cast<QWidget *>(window)->isVisible() : 0; }

void wx_togglewindow(void *window) { wx_showwindow(window, -1); }

void wx_callback(void *window, WX_CALLBACK callback, void *data) {
        QMetaObject::invokeMethod(qApp, [callback, data]() {
                callback(data);
        }, Qt::QueuedConnection);
}

void wx_enddialog(void *window, int ret_code) {
        if (window)
                static_cast<QDialog *>(window)->done(ret_code);
}

int wx_dlgdirlist(void *window, const char *path, int id, int static_path, int file_type) {
        QWidget *w = static_cast<QWidget *>(wx_getdlgitem(window, id));
        QListWidget *list = qobject_cast<QListWidget *>(w);
        if (!list)
                return 0;

        list->clear();
        QString pattern(path);
        QFileInfo fi(pattern);
        QDir dir = fi.absoluteDir();
        QStringList filters;
        filters << fi.fileName();
        QStringList entries = dir.entryList(filters, QDir::Files, QDir::Name);
        for (const QString &entry : entries) {
                QFileInfo entryInfo(entry);
                list->addItem(entryInfo.baseName());
        }
        return 1;
}

int wx_dlgdirselectex(void *window, LONG_PARAM path, int count, int id) {
        QWidget *w = static_cast<QWidget *>(wx_getdlgitem(window, id));
        QListWidget *list = qobject_cast<QListWidget *>(w);
        if (!list || !list->currentItem())
                return 0;

        QString selection = list->currentItem()->text() + ".";
        strcpy((char *)path, selection.toUtf8().constData());
        return 1;
}

int wx_sendmessage(void *window, int type, LONG_PARAM param1, LONG_PARAM param2) {
        if (!window)
                return 0;
        QWidget *w = static_cast<QWidget *>(window);

        switch (type) {
        case WX_CB_ADDSTRING: {
                QComboBox *cb = qobject_cast<QComboBox *>(w);
                if (cb) {
                        bool wasBlocked = cb->blockSignals(true);
                        cb->addItem((char *)param2);
                        cb->blockSignals(wasBlocked);
                }
                break;
        }
        case WX_CB_SETCURSEL: {
                QComboBox *cb = qobject_cast<QComboBox *>(w);
                if (cb && param1 >= 0 && param1 < cb->count()) {
                        bool wasBlocked = cb->blockSignals(true);
                        cb->setCurrentIndex(param1);
                        cb->blockSignals(wasBlocked);
                }
                break;
        }
        case WX_CB_GETCURSEL: {
                QComboBox *cb = qobject_cast<QComboBox *>(w);
                if (cb)
                        return cb->currentIndex();
                return -1;
        }
        case WX_CB_GETLBTEXT: {
                QComboBox *cb = qobject_cast<QComboBox *>(w);
                if (cb)
                        strcpy((char *)param2, cb->itemText(param1).toUtf8().constData());
                break;
        }
        case WX_CB_RESETCONTENT: {
                QComboBox *cb = qobject_cast<QComboBox *>(w);
                if (cb) {
                        bool wasBlocked = cb->blockSignals(true);
                        cb->setCurrentText("");
                        cb->clear();
                        cb->blockSignals(wasBlocked);
                }
                break;
        }
        case WX_BM_SETCHECK: {
                QCheckBox *cb = qobject_cast<QCheckBox *>(w);
                if (cb)
                        cb->setChecked(param1);
                break;
        }
        case WX_BM_GETCHECK: {
                QCheckBox *cb = qobject_cast<QCheckBox *>(w);
                if (cb)
                        return cb->isChecked();
                return 0;
        }
        case WX_WM_SETTEXT: {
                QLineEdit *lineEdit = qobject_cast<QLineEdit *>(w);
                if (lineEdit) {
                        lineEdit->setText((char *)param2);
                } else {
                        QLabel *label = qobject_cast<QLabel *>(w);
                        if (label)
                                label->setText((char *)param2);
                }
                break;
        }
        case WX_WM_GETTEXT: {
                QLineEdit *lineEdit = qobject_cast<QLineEdit *>(w);
                if (lineEdit)
                        strcpy((char *)param2, lineEdit->text().toUtf8().constData());
                else {
                        QLabel *label = qobject_cast<QLabel *>(w);
                        if (label)
                                strcpy((char *)param2, label->text().toUtf8().constData());
                }
                break;
        }
        case WX_UDM_SETPOS: {
                QSpinBox *sb = qobject_cast<QSpinBox *>(w);
                if (sb)
                        sb->setValue(param2);
                break;
        }
        case WX_UDM_GETPOS: {
                QSpinBox *sb = qobject_cast<QSpinBox *>(w);
                if (sb)
                        return sb->value();
                return 0;
        }
        case WX_UDM_SETRANGE: {
                QSpinBox *sb = qobject_cast<QSpinBox *>(w);
                if (sb) {
                        int min = (param2 >> 16) & 0xffff;
                        int max = param2 & 0xffff;
                        sb->setRange(min, max);
                }
                break;
        }
        case WX_UDM_SETINCR: {
                QSpinBox *sb = qobject_cast<QSpinBox *>(w);
                if (sb)
                        sb->setSingleStep(param2);
                break;
        }
        case WX_LB_GETCOUNT: {
                QListWidget *lb = qobject_cast<QListWidget *>(w);
                if (lb)
                        return lb->count();
                return 0;
        }
        case WX_LB_GETCURSEL: {
                QListWidget *lb = qobject_cast<QListWidget *>(w);
                if (lb)
                        return lb->currentRow();
                return -1;
        }
        case WX_LB_SETCURSEL: {
                QListWidget *lb = qobject_cast<QListWidget *>(w);
                if (lb)
                        lb->setCurrentRow(param1);
                break;
        }
        case WX_LB_GETTEXT: {
                QListWidget *lb = qobject_cast<QListWidget *>(w);
                if (lb && lb->item(param1))
                        strcpy((char *)param2, lb->item(param1)->text().toUtf8().constData());
                break;
        }
        case WX_LB_INSERTSTRING: {
                QListWidget *lb = qobject_cast<QListWidget *>(w);
                if (lb)
                        lb->insertItem(param1, (char *)param2);
                break;
        }
        case WX_LB_DELETESTRING: {
                QListWidget *lb = qobject_cast<QListWidget *>(w);
                if (lb) {
                        QListWidgetItem *item = lb->takeItem(param1);
                        delete item;
                }
                break;
        }
        case WX_LB_RESETCONTENT: {
                QListWidget *lb = qobject_cast<QListWidget *>(w);
                if (lb)
                        lb->clear();
                break;
        }
        case WX_CHB_SETPAGETEXT: {
                QTabWidget *tw = qobject_cast<QTabWidget *>(w);
                if (tw) {
                        tw->setTabText(param1, (char *)param2);
                        break;
                }
                QStackedWidget *sw = qobject_cast<QStackedWidget *>(w);
                if (sw) {
                        /* Update companion combo box */
                        QComboBox *combo = sw->parentWidget()
                                ? sw->parentWidget()->findChild<QComboBox *>(sw->objectName() + "_COMBO")
                                : nullptr;
                        if (combo && param1 < combo->count())
                                combo->setItemText(param1, (char *)param2);
                }
                break;
        }
        case WX_CHB_ADDPAGE: {
                QWidget *page = static_cast<QWidget *>((void *)param1);
                if (!page)
                        break;
                QTabWidget *tw = qobject_cast<QTabWidget *>(w);
                if (tw) {
                        bool wasBlocked = tw->blockSignals(true);
                        page->show();
                        tw->addTab(page, (char *)param2);
                        tw->blockSignals(wasBlocked);
                        break;
                }
                QStackedWidget *sw = qobject_cast<QStackedWidget *>(w);
                if (sw) {
                        page->show();
                        sw->addWidget(page);
                        QComboBox *combo = sw->parentWidget()
                                ? sw->parentWidget()->findChild<QComboBox *>(sw->objectName() + "_COMBO")
                                : nullptr;
                        if (combo) {
                                bool wasBlocked = combo->blockSignals(true);
                                combo->addItem((char *)param2);
                                combo->blockSignals(wasBlocked);
                        }
                }
                break;
        }
        case WX_CHB_REMOVEPAGE: {
                QTabWidget *tw = qobject_cast<QTabWidget *>(w);
                if (tw && param1 < tw->count()) {
                        bool wasBlocked = tw->blockSignals(true);
                        QWidget *page = tw->widget(param1);
                        tw->removeTab(param1);
                        if (page)
                                page->hide();
                        tw->blockSignals(wasBlocked);
                        break;
                }
                QStackedWidget *sw = qobject_cast<QStackedWidget *>(w);
                if (sw && param1 < sw->count()) {
                        QWidget *page = sw->widget(param1);
                        sw->removeWidget(page);
                        if (page)
                                page->hide();
                        QComboBox *combo = sw->parentWidget()
                                ? sw->parentWidget()->findChild<QComboBox *>(sw->objectName() + "_COMBO")
                                : nullptr;
                        if (combo && param1 < combo->count()) {
                                bool wasBlocked = combo->blockSignals(true);
                                combo->removeItem(param1);
                                combo->blockSignals(wasBlocked);
                        }
                }
                break;
        }
        case WX_CHB_GETPAGECOUNT: {
                QTabWidget *tw = qobject_cast<QTabWidget *>(w);
                if (tw)
                        return tw->count();
                QStackedWidget *sw = qobject_cast<QStackedWidget *>(w);
                if (sw)
                        return sw->count();
                return 0;
        }
        case WX_REPARENT: {
                /* No-op - the C code uses this before CHB_ADDPAGE to move
                   panels into a tab widget. Qt's addTab handles parenting. */
                break;
        }
        case WX_WM_ENABLE: {
                w->setEnabled((bool)param2);
                break;
        }
        case WX_WM_SHOW: {
                w->setVisible((bool)param2);
                break;
        }
        case WX_WM_LAYOUT: {
                if (w->layout())
                        w->layout()->activate();
                break;
        }
        case WX_SB_SETCURSEL: {
                QStackedWidget *sw = qobject_cast<QStackedWidget *>(w);
                if (sw && param1 >= 0 && param1 < sw->count())
                        sw->setCurrentIndex(param1);
                break;
        }
        }

        return 0;
}

int wx_dialogbox(void *window, const char *name,
                 int (*callback)(void *window, int message, INT_PARAM param1, LONG_PARAM param2)) {
        pclog("wx_dialogbox: loading '%s'...\n", name);
        PCemDialogBox dlg(static_cast<QWidget *>(window), callback);
        if (!dlg.loadUi(name)) {
                pclog("wx_dialogbox: loadUi FAILED for '%s'\n", name);
                return 0;
        }
        pclog("wx_dialogbox: calling onInit for '%s'...\n", name);
        dlg.onInit();
        pclog("wx_dialogbox: onInit done, calling exec for '%s'...\n", name);
        dlg.adjustSize();
        dlg.setReady(true);
        int ret = dlg.exec();
        pclog("wx_dialogbox: exec returned %d for '%s'\n", ret, name);
        return ret;
}

void wx_exit(void *window, int value) {
        MainWindow *mainWindow = static_cast<MainWindow *>(window);
        QMetaObject::invokeMethod(mainWindow, [mainWindow, value]() {
                mainWindow->onExit(value);
        }, Qt::QueuedConnection);
}

void wx_stop_emulation(void *window) {
        MainWindow *mainWindow = static_cast<MainWindow *>(window);
        QMetaObject::invokeMethod(mainWindow, [mainWindow]() {
                mainWindow->onStopEmulation();
        }, Qt::QueuedConnection);
}

void wx_stop_emulation_now(void *window) {
        MainWindow *mainWindow = static_cast<MainWindow *>(window);
        QMetaObject::invokeMethod(mainWindow, [mainWindow]() {
                mainWindow->onStopEmulationNow();
        }, Qt::QueuedConnection);
}

int wx_yield() {
        QApplication::processEvents();
        return 1;
}

/* Timer */

class PCemTimer : public QTimer {
public:
        PCemTimer(void (*fn)()) : m_fn(fn) {
                connect(this, &QTimer::timeout, this, [this]() { m_fn(); });
        }

private:
        void (*m_fn)();
};

void *wx_createtimer(void (*fn)()) { return new PCemTimer(fn); }

void wx_starttimer(void *timer, int milliseconds, int once) {
        QTimer *t = static_cast<QTimer *>(timer);
        t->setSingleShot(once);
        t->start(milliseconds);
}

void wx_stoptimer(void *timer) { static_cast<QTimer *>(timer)->stop(); }

void wx_destroytimer(void *timer) {
        wx_stoptimer(timer);
        delete static_cast<QTimer *>(timer);
}

void wx_popupmenu(void *window, void *menu, int *x, int *y) {
        MainWindow *mainWindow = static_cast<MainWindow *>(window);
        QMenu *qmenu = static_cast<QMenu *>(menu);
        QMetaObject::invokeMethod(mainWindow, [mainWindow, qmenu, x, y]() {
                mainWindow->onPopupMenu(qmenu, x, y);
        }, Qt::QueuedConnection);
}

void *wx_create_status_frame(void *window) {
        StatusFrame *frame = new StatusFrame(static_cast<QWidget *>(window));
        return frame;
}

void wx_destroy_status_frame(void *window) {
        StatusFrame *frame = static_cast<StatusFrame *>(window);
        delete frame;
}

void wx_setwindowposition(void *window, int x, int y) { static_cast<QWidget *>(window)->move(x, y); }

void wx_setwindowsize(void *window, int width, int height) { static_cast<QWidget *>(window)->resize(width, height); }

static StatusFrame *statusFrameInstance = nullptr;

void wx_show_status(void *window) {
        QWidget *parent = static_cast<QWidget *>(window);
        if (!statusFrameInstance) {
                statusFrameInstance = new StatusFrame(parent);
        }
        statusFrameInstance->show();
        statusFrameInstance->raise();
        statusFrameInstance->statusTimer->start(100);
}

void wx_close_status(void *window) {
        if (statusFrameInstance) {
                statusFrameInstance->statusTimer->stop();
                statusFrameInstance->close();
                delete statusFrameInstance;
                statusFrameInstance = nullptr;
        }
}

int wx_setup(char *path) {
        QDir dir(path);
        if (!dir.exists()) {
                if (!dir.mkpath("."))
                        return 0;

                QStringList subdirs = {"configs", "roms", "nvr", "screenshots", "logs"};
                for (const QString &sub : subdirs) {
                        if (!dir.exists(sub) && !dir.mkdir(sub))
                                return 0;
                }
        }
        return 1;
}

int wx_file_exists(char *path) { return QFileInfo::exists(path); }

int wx_dir_exists(char *path) { return QDir(path).exists(); }

int wx_copy_file(char *from, char *to, int overwrite) {
        if (overwrite && QFile::exists(to))
                QFile::remove(to);
        return QFile::copy(from, to);
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

void wx_date_format(char *s, const char *format) {
        QString fmt(format);
        /* Convert C strftime format to Qt format */
        fmt.replace("%Y", "yyyy");
        fmt.replace("%m", "MM");
        fmt.replace("%d", "dd");
        fmt.replace("%H", "HH");
        fmt.replace("%M", "mm");
        fmt.replace("%S", "ss");
        QString res = QDateTime::currentDateTime().toString(fmt);
        strcpy(s, res.toUtf8().constData());
}

int wx_image_save(const char *path, const char *name, const char *format, unsigned char *rgba, int width, int height, int alpha) {
        QImage image(width, height, alpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888);

        if (alpha) {
                for (int y = 0; y < height; ++y) {
                        for (int x = 0; x < width; ++x) {
                                int srcIdx = (y * width + x) * 4;
                                image.setPixelColor(x, y, QColor(rgba[srcIdx], rgba[srcIdx + 1], rgba[srcIdx + 2], rgba[srcIdx + 3]));
                        }
                }
        } else {
                for (int y = 0; y < height; ++y) {
                        memcpy(image.scanLine(y), rgba + y * width * 3, width * 3);
                }
        }

        QString ext;
        if (!strcmp(format, IMAGE_TIFF))
                ext = "tif";
        else if (!strcmp(format, IMAGE_BMP))
                ext = "bmp";
        else if (!strcmp(format, IMAGE_JPG))
                ext = "jpg";
        else
                ext = "png";

        QString fullPath = QString(path);
        if (!fullPath.endsWith("/"))
                fullPath += "/";
        fullPath += QString(name) + "." + ext;

        return image.save(fullPath);
}

void *wx_image_load(const char *path) {
        QImage *image = new QImage(path);
        if (!image->isNull())
                return image;
        delete image;
        return nullptr;
}

void wx_image_free(void *image) { delete static_cast<QImage *>(image); }

void wx_image_rescale(void *image, int width, int height) {
        QImage *img = static_cast<QImage *>(image);
        *img = img->scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void wx_image_get_size(void *image, int *width, int *height) {
        QImage *img = static_cast<QImage *>(image);
        *width = img->width();
        *height = img->height();
}

unsigned char *wx_image_get_data(void *image) { return static_cast<QImage *>(image)->bits(); }

unsigned char *wx_image_get_alpha(void *image) {
        /* QImage doesn't have a separate alpha channel like wxImage.
           Return nullptr and handle alpha differently where needed. */
        return nullptr;
}

/* Config file handling using QSettings INI format */

void *wx_config_load(const char *path) {
        if (!QFileInfo::exists(path))
                return nullptr;
        QSettings *settings = new QSettings(path, QSettings::IniFormat);
        if (settings->status() == QSettings::NoError)
                return settings;
        delete settings;
        return nullptr;
}

int wx_config_get_string(void *config, const char *name, char *dst, int size, const char *defVal) {
        QSettings *s = static_cast<QSettings *>(config);
        QVariant val = s->value(name);
        if (val.isValid()) {
                strncpy(dst, val.toString().toUtf8().constData(), size - 1);
                dst[size - 1] = 0;
                return 1;
        }
        if (defVal) {
                strncpy(dst, defVal, size - 1);
                dst[size - 1] = 0;
        }
        return 0;
}

int wx_config_get_int(void *config, const char *name, int *dst, int defVal) {
        QSettings *s = static_cast<QSettings *>(config);
        QVariant val = s->value(name);
        if (val.isValid()) {
                *dst = val.toInt();
                return 1;
        }
        *dst = defVal;
        return 0;
}

int wx_config_get_float(void *config, const char *name, float *dst, float defVal) {
        QSettings *s = static_cast<QSettings *>(config);
        QVariant val = s->value(name);
        if (val.isValid()) {
                *dst = val.toFloat();
                return 1;
        }
        *dst = defVal;
        return 0;
}

int wx_config_get_bool(void *config, const char *name, int *dst, int defVal) {
        QSettings *s = static_cast<QSettings *>(config);
        QVariant val = s->value(name);
        if (val.isValid()) {
                QString str = val.toString().toLower();
                if (str == "true" || str == "1") {
                        *dst = 1;
                        return 1;
                } else if (str == "false" || str == "0") {
                        *dst = 0;
                        return 1;
                }
                *dst = val.toBool();
                return 1;
        }
        *dst = defVal;
        return 0;
}

int wx_config_has_entry(void *config, const char *name) {
        QSettings *s = static_cast<QSettings *>(config);
        return s->contains(name);
}

void wx_config_free(void *config) { delete static_cast<QSettings *>(config); }

typedef struct progress_data_t {
        WX_CALLBACK callback;
        void *data;
        int active;
        int result;
} progress_data_t;

static void progress_callback(void *data) {
        progress_data_t *d = (progress_data_t *)data;
        d->result = d->callback(d->data);
        d->active = 0;
}

int wx_progressdialog(void *window, const char *title, const char *message, WX_CALLBACK callback, void *data, int range,
                      volatile int *pos) {
        struct progress_data_t pdata;
        pdata.callback = callback;
        pdata.data = data;
        pdata.active = 1;
        pdata.result = 0;

        thread_t *t = thread_create(progress_callback, &pdata);

        QProgressDialog dlg(message, QString(), 0, range, static_cast<QWidget *>(window));
        dlg.setWindowTitle(title);
        dlg.setWindowModality(Qt::ApplicationModal);
        dlg.setMinimumDuration(0);

        while (pdata.active) {
                dlg.setValue(*pos);
                QApplication::processEvents();
                QThread::msleep(50);
        }
        thread_kill(t);

        return pdata.result;
}
