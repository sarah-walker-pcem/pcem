#include "qt-app.h"
#include "qt-utils.h"
#include "qt-dialogbox.h"
#include "logging-internal.h"

#include <cstdlib>

#include <QCursor>
#include <QMouseEvent>

#include <QApplication>
#include <QMessageBox>
#include <QPixmap>
#include <QImage>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QToolButton>

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#endif

extern "C" {
int wx_load_config(void *);
int wx_start(void *);
int wx_stop(void *);
void wx_show(void *);
void wx_handle_command(void *, int, int);

int start_emulation(void *);
int resume_emulation();
int pause_emulation();
int stop_emulation();
}

extern int config_override;

extern "C" {
extern int rawinputkey[272];
extern int fps;
extern int mousecapture;
drive_info_t *get_machine_info(char *s, int *num_drive_info);
void qt_mouse_motion(int dx, int dy);
void qt_mouse_set_buttons(int buttons);
void mouse_wheel_update(int);
int config_open(void *);
void resetpchard();
void savenvr();
}

#ifdef _WIN32
static HWND sdl_canvas_hwnd = 0;

bool RawMouseFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) {
        MSG *msg = static_cast<MSG *>(message);

        /* Block WM_ERASEBKGND for the SDL canvas to prevent Windows from
           painting the background black on focus changes */
        if (msg->message == WM_ERASEBKGND) {
                if (sdl_canvas_hwnd && (msg->hwnd == sdl_canvas_hwnd ||
                    IsChild(sdl_canvas_hwnd, msg->hwnd))) {
                        *result = 1;
                        return true;
                }
        }

        /* Suppress cursor when captured — intercept WM_SETCURSOR so Windows
           doesn't keep resetting the cursor shape */
        if (msg->message == WM_SETCURSOR && mousecapture) {
                SetCursor(NULL);
                *result = TRUE;
                return true;
        }

        if (!mousecapture)
                return false;

        if (msg->message != WM_INPUT)
                return false;

        UINT size = 0;
        GetRawInputData((HRAWINPUT)msg->lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
        if (size == 0 || size > 512)
                return false;

        BYTE buf[512];
        if (GetRawInputData((HRAWINPUT)msg->lParam, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)
                return false;

        RAWINPUT *raw = (RAWINPUT *)buf;
        if (raw->header.dwType == RIM_TYPEMOUSE) {
                RAWMOUSE *mouse = &raw->data.mouse;

                if (!(mouse->usFlags & MOUSE_MOVE_ABSOLUTE)) {
                        int dx = mouse->lLastX;
                        int dy = mouse->lLastY;
                        if (dx || dy)
                                qt_mouse_motion(dx, dy);
                }

                static int raw_buttons = 0;
                if (mouse->usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) raw_buttons |= 1;
                if (mouse->usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) raw_buttons &= ~1;
                if (mouse->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) raw_buttons |= 2;
                if (mouse->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) raw_buttons &= ~2;
                if (mouse->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) raw_buttons |= 4;
                if (mouse->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) raw_buttons &= ~4;
                qt_mouse_set_buttons(raw_buttons);

                if (mouse->usButtonFlags & RI_MOUSE_WHEEL) {
                        short wheel = (short)mouse->usButtonData;
                        mouse_wheel_update(wheel / WHEEL_DELTA);
                }
                return true;
        }
        return false;
}
#endif

/* Map Qt keys to PC scancodes */
static int qt_key_to_scancode(int qtKey) {
        switch (qtKey) {
        case Qt::Key_Escape: return 0x01;
        case Qt::Key_1: return 0x02; case Qt::Key_2: return 0x03; case Qt::Key_3: return 0x04;
        case Qt::Key_4: return 0x05; case Qt::Key_5: return 0x06; case Qt::Key_6: return 0x07;
        case Qt::Key_7: return 0x08; case Qt::Key_8: return 0x09; case Qt::Key_9: return 0x0A;
        case Qt::Key_0: return 0x0B;
        case Qt::Key_Minus: return 0x0c; case Qt::Key_Equal: return 0x0d;
        case Qt::Key_Backspace: return 0x0e; case Qt::Key_Tab: return 0x0f;
        case Qt::Key_Q: return 0x10; case Qt::Key_W: return 0x11; case Qt::Key_E: return 0x12;
        case Qt::Key_R: return 0x13; case Qt::Key_T: return 0x14; case Qt::Key_Y: return 0x15;
        case Qt::Key_U: return 0x16; case Qt::Key_I: return 0x17; case Qt::Key_O: return 0x18;
        case Qt::Key_P: return 0x19;
        case Qt::Key_BracketLeft: return 0x1a; case Qt::Key_BracketRight: return 0x1b;
        case Qt::Key_Return: return 0x1c; case Qt::Key_Enter: return 0x9c;
        case Qt::Key_Control: return 0x1d;
        case Qt::Key_A: return 0x1e; case Qt::Key_S: return 0x1f; case Qt::Key_D: return 0x20;
        case Qt::Key_F: return 0x21; case Qt::Key_G: return 0x22; case Qt::Key_H: return 0x23;
        case Qt::Key_J: return 0x24; case Qt::Key_K: return 0x25; case Qt::Key_L: return 0x26;
        case Qt::Key_Semicolon: return 0x27; case Qt::Key_Apostrophe: return 0x28;
        case Qt::Key_QuoteLeft: return 0x29;
        case Qt::Key_Shift: return 0x2a;
        case Qt::Key_Backslash: return 0x2b;
        case Qt::Key_Z: return 0x2c; case Qt::Key_X: return 0x2d; case Qt::Key_C: return 0x2e;
        case Qt::Key_V: return 0x2f; case Qt::Key_B: return 0x30; case Qt::Key_N: return 0x31;
        case Qt::Key_M: return 0x32;
        case Qt::Key_Comma: return 0x33; case Qt::Key_Period: return 0x34; case Qt::Key_Slash: return 0x35;
        case Qt::Key_Alt: return 0x38;
        case Qt::Key_Space: return 0x39;
        case Qt::Key_CapsLock: return 0x3a;
        case Qt::Key_F1: return 0x3B; case Qt::Key_F2: return 0x3C; case Qt::Key_F3: return 0x3D;
        case Qt::Key_F4: return 0x3e; case Qt::Key_F5: return 0x3f; case Qt::Key_F6: return 0x40;
        case Qt::Key_F7: return 0x41; case Qt::Key_F8: return 0x42; case Qt::Key_F9: return 0x43;
        case Qt::Key_F10: return 0x44; case Qt::Key_F11: return 0x57; case Qt::Key_F12: return 0x58;
        case Qt::Key_NumLock: return 0x45; case Qt::Key_ScrollLock: return 0x46;
        case Qt::Key_Home: return 0xc7; case Qt::Key_Up: return 0xc8; case Qt::Key_PageUp: return 0xc9;
        case Qt::Key_Left: return 0xcb; case Qt::Key_Right: return 0xcd;
        case Qt::Key_End: return 0xcf; case Qt::Key_Down: return 0xd0; case Qt::Key_PageDown: return 0xd1;
        case Qt::Key_Insert: return 0xd2; case Qt::Key_Delete: return 0xd3;
        case Qt::Key_Print: return 0xb7;
        /* Shifted key variants — Qt reports the character, not the physical key */
        case Qt::Key_Exclam: return 0x02;      /* Shift+1 */
        case Qt::Key_At: return 0x03;           /* Shift+2 */
        case Qt::Key_NumberSign: return 0x04;   /* Shift+3 */
        case Qt::Key_Dollar: return 0x05;       /* Shift+4 */
        case Qt::Key_Percent: return 0x06;      /* Shift+5 */
        case Qt::Key_AsciiCircum: return 0x07;  /* Shift+6 */
        case Qt::Key_Ampersand: return 0x08;    /* Shift+7 */
        case Qt::Key_Asterisk: return 0x09;     /* Shift+8 */
        case Qt::Key_ParenLeft: return 0x0A;    /* Shift+9 */
        case Qt::Key_ParenRight: return 0x0B;   /* Shift+0 */
        case Qt::Key_Underscore: return 0x0c;   /* Shift+- */
        case Qt::Key_Plus: return 0x0d;         /* Shift+= */
        case Qt::Key_BraceLeft: return 0x1a;    /* Shift+[ */
        case Qt::Key_BraceRight: return 0x1b;   /* Shift+] */
        case Qt::Key_Colon: return 0x27;        /* Shift+; */
        case Qt::Key_QuoteDbl: return 0x28;     /* Shift+' */
        case Qt::Key_AsciiTilde: return 0x29;   /* Shift+` */
        case Qt::Key_Bar: return 0x2b;          /* Shift+\ */
        case Qt::Key_Less: return 0x33;         /* Shift+, */
        case Qt::Key_Greater: return 0x34;      /* Shift+. */
        case Qt::Key_Question: return 0x35;     /* Shift+/ */
        default: return -1;
        }
}

/* SDLCanvas */

SDLCanvas::SDLCanvas(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_PaintOnScreen);   /* Prevent Qt from painting over SDL surface */
        setAttribute(Qt::WA_NoSystemBackground); /* No background erase on focus/resize */
        setAttribute(Qt::WA_InputMethodEnabled, false);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setAutoFillBackground(false);
}

void SDLCanvas::keyPressEvent(QKeyEvent *event) {
        /* When captured, keyboard is handled by raw input (Win) or SDL events.
           Only use Qt keys as fallback when not captured. */
        if (mousecapture) {
                event->accept();
                return;
        }
        if (!hasFocus())
                return;
        int sc = qt_key_to_scancode(event->key());
        if (sc >= 0 && sc < 272)
                rawinputkey[sc] = 1;
        event->accept();
}

void SDLCanvas::keyReleaseEvent(QKeyEvent *event) {
        if (mousecapture) {
                event->accept();
                return;
        }
        int sc = qt_key_to_scancode(event->key());
        if (sc >= 0 && sc < 272)
                rawinputkey[sc] = 0;
        event->accept();
}

extern "C" { extern int infocus; }

void SDLCanvas::focusOutEvent(QFocusEvent *event) {
        if (!mousecapture)
                memset(rawinputkey, 0, sizeof(int) * 272);
        QWidget::focusOutEvent(event);
}

void SDLCanvas::focusInEvent(QFocusEvent *event) {
        QWidget::focusInEvent(event);
}

void SDLCanvas::mouseMoveEvent(QMouseEvent *event) {
        /* Mouse input when captured is handled by raw input (Windows)
           for smooth, jitter-free movement. Qt events are ignored. */
        event->accept();
}

void SDLCanvas::mousePressEvent(QMouseEvent *event) {
        event->accept();
}

void SDLCanvas::mouseReleaseEvent(QMouseEvent *event) {
        event->accept();
}

void SDLCanvas::paintEvent(QPaintEvent *event) {
        /* Do nothing — SDL manages this surface. Prevents Qt from
           filling the widget with a background color on repaint. */
        event->accept();
}

QPaintEngine *SDLCanvas::paintEngine() const {
        /* Return null to tell Qt this widget does not use Qt's paint system.
           This prevents Qt from ever trying to paint on this surface. */
        return nullptr;
}

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent), m_menu(nullptr), m_sdlCanvas(nullptr), m_closing(false) {
        setWindowTitle("PCem");

        m_sdlCanvas = new SDLCanvas(this);
        m_sdlCanvas->setMinimumSize(640, 480);
        setCentralWidget(m_sdlCanvas);
        m_sdlCanvas->setFocus();

#ifdef _WIN32
        m_rawMouseFilter = new RawMouseFilter();
        QCoreApplication::instance()->installNativeEventFilter(m_rawMouseFilter);
        sdl_canvas_hwnd = (HWND)m_sdlCanvas->winId();
#endif

        setupStatusBar();

        buildMenus();
}

MainWindow::~MainWindow() {
#ifdef _WIN32
        QCoreApplication::instance()->removeNativeEventFilter(m_rawMouseFilter);
        delete m_rawMouseFilter;
#endif
}

QAction *MainWindow::addMenuItem(QMenu *menu, const char *id, const char *label, bool checkable, bool radio) {
        QAction *action = menu->addAction(QString(label).remove('_'));
        action->setData(wx_xrcid(id));
        if (checkable || radio)
                action->setCheckable(true);
        connect(action, &QAction::triggered, this, [this, action](bool checked) {
                wx_handle_command(this, action->data().toInt(), checked);
        });
        return action;
}

QMenu *MainWindow::addSubmenu(QMenu *parent, const char *id, const char *label) {
        QMenu *sub = parent->addMenu(QString(label).remove('_'));
        if (id && id[0]) {
                /* Register this submenu's action with the XRCID so wx_getsubmenu can find it */
                sub->menuAction()->setData(wx_xrcid(id));
        }
        return sub;
}

void MainWindow::buildMenus() {
        /* Build the main menu to match the XRC structure from pc.xrc */
        m_menu = new QMenu(this); /* context/popup menu reference */

        /* System */
        QMenu *systemMenu = menuBar()->addMenu("System");
        addMenuItem(systemMenu, "IDM_FILE_HRESET", "Hard Reset");
        addMenuItem(systemMenu, "IDM_FILE_RESET_CAD", "Ctrl+Alt+Del");
        systemMenu->addSeparator();
        addMenuItem(systemMenu, "IDM_FILE_EXIT", "Shutdown");

        /* Disc */
        QMenu *discMenu = menuBar()->addMenu("Disc");
        addMenuItem(discMenu, "IDM_DISC_A", "Change drive A:...");
        addMenuItem(discMenu, "IDM_DISC_B", "Change drive B:...");
        addMenuItem(discMenu, "IDM_EJECT_A", "Eject drive A:");
        addMenuItem(discMenu, "IDM_EJECT_B", "Eject drive B:");
        discMenu->addSeparator();
        addMenuItem(discMenu, "IDM_BPB_DISABLE", "Disable BPB checking", true);
        discMenu->addSeparator();
        addMenuItem(discMenu, "IDM_DISC_CREATE", "Create blank disc image...");
        discMenu->addSeparator();
        addMenuItem(discMenu, "IDM_DISC_ZIP", "Load ZIP drive...");
        addMenuItem(discMenu, "IDM_EJECT_ZIP", "Eject ZIP drive");

        /* CD-ROM */
        QMenu *cdromMenu = menuBar()->addMenu("CD-ROM");
        cdromMenu->menuAction()->setData(wx_xrcid("IDM_CDROM"));
        addMenuItem(cdromMenu, "IDM_CDROM_IMAGE_LOAD", "Load image...");
        cdromMenu->addSeparator();
        addMenuItem(cdromMenu, "IDM_CDROM_EMPTY", "Empty", false, true);
        addMenuItem(cdromMenu, "IDM_CDROM_IMAGE", "Image...", false, true);

        /* Cassette */
        QMenu *cassetteMenu = menuBar()->addMenu("Cassette");
        cassetteMenu->menuAction()->setData(wx_xrcid("IDM_CASSETTE"));
        addMenuItem(cassetteMenu, "IDM_CASSETTE_LOAD", "Load tapefile...");
        addMenuItem(cassetteMenu, "IDM_CASSETTE_EJECT", "Eject tape");

        /* Video */
        QMenu *videoMenu = menuBar()->addMenu("Video");
        {
                QMenu *resMenu = addSubmenu(videoMenu, "", "Resolution");
                addMenuItem(resMenu, "IDM_VID_RESOLUTION[0]", "Original", false, true);
                addMenuItem(resMenu, "IDM_VID_RESOLUTION[1]", "Resizable", false, true);
                addMenuItem(resMenu, "IDM_VID_RESOLUTION[2]", "Custom", false, true);
                resMenu->addSeparator();
                addMenuItem(resMenu, "IDM_VID_RESOLUTION_CUSTOM", "Set custom resolution...");
        }
        addMenuItem(videoMenu, "IDM_VID_REMEMBER", "Remember size && position", true);
        videoMenu->addSeparator();
        addMenuItem(videoMenu, "IDM_VID_FULLSCREEN_TOGGLE", "Toggle fullscreen");
        addMenuItem(videoMenu, "IDM_VID_FULLSCREEN", "Fullscreen on input grab", true);
        videoMenu->addSeparator();
        {
                QMenu *fsMode = addSubmenu(videoMenu, "", "Fullscreen mode");
                addMenuItem(fsMode, "IDM_VID_FS_MODE[0]", "Borderless", false, true);
                addMenuItem(fsMode, "IDM_VID_FS_MODE[1]", "Exclusive", false, true);
        }
        {
                QMenu *renderMenu = addSubmenu(videoMenu, "IDM_VID_RENDER_DRIVER", "Render driver");
                addMenuItem(renderMenu, "IDM_VID_RENDER_DRIVER[0]", "Auto", false, true);
                addMenuItem(renderMenu, "IDM_VID_RENDER_DRIVER[1]", "Direct3D", false, true);
                addMenuItem(renderMenu, "IDM_VID_RENDER_DRIVER[2]", "OpenGL", false, true);
                addMenuItem(renderMenu, "IDM_VID_RENDER_DRIVER[6]", "OpenGL 3.0", false, true);
                addMenuItem(renderMenu, "IDM_VID_RENDER_DRIVER[5]", "Software", false, true);
        }
        addMenuItem(videoMenu, "IDM_VID_VSYNC", "VSync", true);
        addMenuItem(videoMenu, "IDM_VID_LOST_FOCUS_DIM", "Dim display on lost focus", true);
        addMenuItem(videoMenu, "IDM_VID_ALTERNATIVE_UPDATE_LOCK", "Alternative update-lock", true);
        videoMenu->addSeparator();
        {
                QMenu *scaleFilter = addSubmenu(videoMenu, "", "Scale filtering");
                addMenuItem(scaleFilter, "IDM_VID_SCALE_MODE[0]", "Nearest", false, true);
                addMenuItem(scaleFilter, "IDM_VID_SCALE_MODE[1]", "Linear", false, true);
        }
        {
                QMenu *fsStretch = addSubmenu(videoMenu, "", "Output stretch-mode");
                addMenuItem(fsStretch, "IDM_VID_FS[0]", "None", false, true);
                addMenuItem(fsStretch, "IDM_VID_FS[1]", "4:3", false, true);
                addMenuItem(fsStretch, "IDM_VID_FS[2]", "Square pixels", false, true);
                addMenuItem(fsStretch, "IDM_VID_FS[3]", "Integer scale", false, true);
        }
        {
                QMenu *scaleMenu = addSubmenu(videoMenu, "IDM_VID_SCALE_MENU", "Output scale");
                addMenuItem(scaleMenu, "IDM_VID_SCALE[0]", "0.5x", false, true);
                addMenuItem(scaleMenu, "IDM_VID_SCALE[1]", "1x", false, true);
                addMenuItem(scaleMenu, "IDM_VID_SCALE[2]", "1.5x", false, true);
                addMenuItem(scaleMenu, "IDM_VID_SCALE[3]", "2x", false, true);
                addMenuItem(scaleMenu, "IDM_VID_SCALE[4]", "2.5x", false, true);
                addMenuItem(scaleMenu, "IDM_VID_SCALE[5]", "3x", false, true);
                addMenuItem(scaleMenu, "IDM_VID_SCALE[6]", "3.5x", false, true);
                addMenuItem(scaleMenu, "IDM_VID_SCALE[7]", "4x", false, true);
        }
        videoMenu->addSeparator();
        {
                QMenu *gl3Menu = addSubmenu(videoMenu, "IDM_VID_GL3", "OpenGL 3.0 renderer");
                {
                        QMenu *inputStretch = addSubmenu(gl3Menu, "", "Input stretch-mode");
                        addMenuItem(inputStretch, "IDM_VID_GL3_INPUT_STRETCH[0]", "None", false, true);
                        addMenuItem(inputStretch, "IDM_VID_GL3_INPUT_STRETCH[1]", "4:3", false, true);
                        addMenuItem(inputStretch, "IDM_VID_GL3_INPUT_STRETCH[2]", "Square pixels", false, true);
                        addMenuItem(inputStretch, "IDM_VID_GL3_INPUT_STRETCH[3]", "Integer scale", false, true);
                }
                {
                        QMenu *inputScale = addSubmenu(gl3Menu, "", "Input scale");
                        for (int i = 0; i <= 7; i++) {
                                char id[64], label[16];
                                snprintf(id, sizeof(id), "IDM_VID_GL3_INPUT_SCALE[%d]", i);
                                snprintf(label, sizeof(label), "%.1fx", 0.5 + i * 0.5);
                                addMenuItem(inputScale, id, label, false, true);
                        }
                }
                {
                        QMenu *shaderRate = addSubmenu(gl3Menu, "", "Shader refresh rate");
                        addMenuItem(shaderRate, "IDM_VID_GL3_SHADER_REFRESH_RATE[0]", "Same as emulated display", false, true);
                        int rates[] = {10, 25, 30, 50, 60, 72, 85};
                        for (int i = 0; i < 7; i++) {
                                char id[64], label[16];
                                snprintf(id, sizeof(id), "IDM_VID_GL3_SHADER_REFRESH_RATE[%d]", rates[i]);
                                snprintf(label, sizeof(label), "%d hz", rates[i]);
                                addMenuItem(shaderRate, id, label, false, true);
                        }
                }
                gl3Menu->addSeparator();
                addMenuItem(gl3Menu, "IDM_VID_GL3_SHADER_MANAGER", "Shader manager");
        }

        /* Sound */
        QMenu *soundMenu = menuBar()->addMenu("Sound");
        {
                QMenu *bufMenu = addSubmenu(soundMenu, "", "Buffer length");
                const char *bufLabels[] = {"50 ms", "100 ms", "200 ms", "400 ms"};
                for (int i = 0; i < 4; i++) {
                        char id[64];
                        snprintf(id, sizeof(id), "IDM_SND_BUF[%d]", i);
                        addMenuItem(bufMenu, id, bufLabels[i], false, true);
                }
        }
        {
                QMenu *gainMenu = addSubmenu(soundMenu, "", "Output level");
                addMenuItem(gainMenu, "IDM_SND_GAIN[0]", "Normal", false, true);
                for (int i = 1; i <= 9; i++) {
                        char id[64], label[32];
                        snprintf(id, sizeof(id), "IDM_SND_GAIN[%d]", i);
                        snprintf(label, sizeof(label), "+%d dB", i * 2);
                        addMenuItem(gainMenu, id, label, false, true);
                }
        }

        /* Misc */
        QMenu *miscMenu = menuBar()->addMenu("Misc");
        {
                QMenu *ssMenu = addSubmenu(miscMenu, "", "Screenshot");
                addMenuItem(ssMenu, "IDM_SCREENSHOT", "Take screenshot");
                QMenu *fmtMenu = addSubmenu(ssMenu, "", "Format");
                addMenuItem(fmtMenu, "IDM_SCREENSHOT_FORMAT[0]", "PNG", false, true);
                addMenuItem(fmtMenu, "IDM_SCREENSHOT_FORMAT[1]", "TIFF", false, true);
                addMenuItem(fmtMenu, "IDM_SCREENSHOT_FORMAT[2]", "BMP", false, true);
                addMenuItem(fmtMenu, "IDM_SCREENSHOT_FORMAT[3]", "JPG", false, true);
                addMenuItem(ssMenu, "IDM_SCREENSHOT_FLASH", "Flash screen", true);
        }
        addMenuItem(miscMenu, "IDM_STATUS", "Machine");

        /* View (for viewers) */
        QMenu *viewMenu = menuBar()->addMenu("View");
        viewMenu->menuAction()->setData(wx_xrcid("IDM_VIEW"));

        /* Help */
        QMenu *helpMenu = menuBar()->addMenu("Help");
        {
                QAction *configAction = helpMenu->addAction("Configure...");
                connect(configAction, &QAction::triggered, this, [this]() {
                        /* Pause emulation before opening config.
                           config_dialog_proc also sets pause=1 on init, but we
                           need it paused so we can resetpchard after OK. */
                        extern volatile int pause;
                        pause = 1;
                        int result = config_open(this);
                        /* config_dialog_proc sets pause=0 on OK/Cancel, but
                           we need to stay paused for the reset if OK was pressed. */
                        if (result == 1) {
                                pause = 1;
                                savenvr();
                                resetpchard();
                        }
                        pause = 0;
                });
                helpMenu->addSeparator();
                QAction *aboutAction = helpMenu->addAction("About PCem...");
                connect(aboutAction, &QAction::triggered, this, [this]() {
                        showAboutDialog();
                });
        }
}

static QIcon makeGrayIcon(const QPixmap &pm) {
        QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < img.height(); y++) {
                QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
                for (int x = 0; x < img.width(); x++) {
                        int a = qAlpha(line[x]);
                        int gray = qGray(line[x]);
                        line[x] = qRgba(gray, gray, gray, a / 2);
                }
        }
        return QIcon(QPixmap::fromImage(img));
}

void MainWindow::setupStatusBar() {
        m_numDriveLabels = 0;
        m_statusSpeedLabel = new QLabel("Stopped", this);
        statusBar()->addPermanentWidget(m_statusSpeedLabel);

        /* Load 16x16 drive type icons */
        QPixmap fddPm(":/icons/16x16/diskette.png");
        QPixmap hddPm(":/icons/16x16/drive.png");
        QPixmap cdPm(":/icons/16x16/cd.png");
        m_iconFDD = QIcon(fddPm);
        m_iconFDDDisabled = makeGrayIcon(fddPm);
        m_iconHDD = QIcon(hddPm);
        m_iconCDROM = QIcon(cdPm);
        m_iconCDROMDisabled = makeGrayIcon(cdPm);

        for (int i = 0; i < 10; i++) {
                m_statusDriveButtons[i] = new QToolButton(this);
                m_statusDriveButtons[i]->setIconSize(QSize(16, 16));
                m_statusDriveButtons[i]->setAutoRaise(true);
                m_statusDriveButtons[i]->setContextMenuPolicy(Qt::CustomContextMenu);
                m_statusDriveButtons[i]->hide();
                statusBar()->addWidget(m_statusDriveButtons[i]);

                int idx = i;
                connect(m_statusDriveButtons[i], &QToolButton::customContextMenuRequested,
                        this, [this, idx](const QPoint &pos) {
                                onDriveContextMenu(idx, m_statusDriveButtons[idx]->mapToGlobal(pos));
                        });
                connect(m_statusDriveButtons[i], &QToolButton::clicked,
                        this, [this, idx]() {
                                onDriveContextMenu(idx, QCursor::pos());
                        });
        }
        memset(m_driveCache, 0, sizeof(m_driveCache));

        m_statusTimer = new QTimer(this);
        connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
        m_statusTimer->start(100);
}

void MainWindow::updateStatusBar() {
        if (emulation_state != EMULATION_RUNNING) {
                m_statusSpeedLabel->setText(emulation_state == EMULATION_PAUSED ? "Paused" : "Stopped");
                for (int i = 0; i < 10; i++)
                        m_statusDriveButtons[i]->hide();
                return;
        }

        char machineInfo[4096];
        int numDrives = 0;
        drive_info_t *drives = get_machine_info(machineInfo, &numDrives);

        QString speedText = QString("Speed: %1%").arg(fps);
        if (mousecapture)
                speedText += " | Mouse captured (Ctrl+Alt+End to release)";
        m_statusSpeedLabel->setText(speedText);

        for (int i = 0; i < 10; i++) {
                if (i < numDrives) {
                        QToolButton *btn = m_statusDriveButtons[i];
                        drive_info_t *d = &drives[i];
                        m_driveCache[i] = *d;

                        QIcon icon;
                        switch (d->type) {
                        case DRIVE_TYPE_FDD:
                                icon = d->enabled ? m_iconFDD : m_iconFDDDisabled;
                                break;
                        case DRIVE_TYPE_HDD:
                                icon = m_iconHDD;
                                break;
                        case DRIVE_TYPE_CDROM:
                                icon = d->enabled ? m_iconCDROM : m_iconCDROMDisabled;
                                break;
                        default:
                                icon = m_iconHDD;
                                break;
                        }

                        if (d->readflash) {
                                btn->setStyleSheet("QToolButton { background-color: #00c800; }");
                        } else {
                                btn->setStyleSheet("");
                        }

                        btn->setIcon(icon);
                        btn->setToolTip(QString("%1: %2").arg(QChar(d->drive_letter),
                                d->enabled ? QString(d->fn) : "Empty"));
                        btn->show();
                } else {
                        m_statusDriveButtons[i]->hide();
                }
        }
        m_numDriveLabels = numDrives;
}

void MainWindow::onDriveContextMenu(int driveIndex, const QPoint &pos) {
        if (driveIndex < 0 || driveIndex >= m_numDriveLabels)
                return;

        drive_info_t *d = &m_driveCache[driveIndex];

        if (d->type == DRIVE_TYPE_HDD)
                return; /* No context menu for HDDs */

        QMenu menu(this);

        if (d->type == DRIVE_TYPE_FDD) {
                int driveNum = d->drive; /* 0=A, 1=B */
                const char *changeId = (driveNum == 0) ? "IDM_DISC_A" : "IDM_DISC_B";
                const char *ejectId = (driveNum == 0) ? "IDM_EJECT_A" : "IDM_EJECT_B";

                QAction *loadAct = menu.addAction(
                        m_iconFDD,
                        QString("Change drive %1:...").arg(QChar(d->drive_letter)));
                connect(loadAct, &QAction::triggered, this, [this, changeId]() {
                        wx_handle_command(this, wx_xrcid(changeId), 0);
                });

                QAction *ejectAct = menu.addAction(
                        QString("Eject drive %1:").arg(QChar(d->drive_letter)));
                ejectAct->setEnabled(d->enabled);
                connect(ejectAct, &QAction::triggered, this, [this, ejectId]() {
                        wx_handle_command(this, wx_xrcid(ejectId), 0);
                });
        } else if (d->type == DRIVE_TYPE_CDROM) {
                QAction *loadAct = menu.addAction(
                        m_iconCDROM,
                        "Load image...");
                connect(loadAct, &QAction::triggered, this, [this]() {
                        wx_handle_command(this, wx_xrcid("IDM_CDROM_IMAGE_LOAD"), 0);
                });

                QAction *emptyAct = menu.addAction("Eject");
                emptyAct->setEnabled(d->enabled);
                connect(emptyAct, &QAction::triggered, this, [this]() {
                        wx_handle_command(this, wx_xrcid("IDM_CDROM_EMPTY"), 0);
                });
        }

        menu.exec(pos);
}

static int about_dlgproc(void *hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam) {
        if (message == WX_COMMAND && wParam == wxID_OK) {
                wx_enddialog(hdlg, 0);
                return TRUE;
        }
        return FALSE;
}

void MainWindow::showAboutDialog() {
        PCemDialogBox dlg(this, about_dlgproc);
        if (dlg.loadUi("AboutDlg")) {
                dlg.onInit();
                dlg.setReady(true);
                dlg.adjustSize();
                dlg.exec();
        }
}

void MainWindow::start() {
        if (wx_start(this))
                showConfigSelection();
        else
                quit(false);
}

void MainWindow::showConfigSelection() {
        if (wx_load_config(this)) {
                show();
                start_emulation(this);
        } else
                quit(true);
}

QMenu *MainWindow::getMenu() { return m_menu; }

QMenuBar *MainWindow::getQMenuBar() { return menuBar(); }

void MainWindow::onCallback(WX_CALLBACK callback, void *data) { callback(data); }

void MainWindow::onStopEmulation() {
        if (emulation_state != EMULATION_STOPPED) {
                pause_emulation();
                int ret = QMessageBox::Ok;
                pclog_flush();
                if (confirm_on_stop_emulation) {
                        QDialog dlg(this);
                        dlg.setWindowTitle("PCem");
                        QVBoxLayout *layout = new QVBoxLayout(&dlg);
                        QLabel *label = new QLabel("Stop emulation?", &dlg);
                        layout->addWidget(label);
                        QCheckBox *remember = new QCheckBox("Do not ask again", &dlg);
                        layout->addWidget(remember);
                        QDialogButtonBox *buttons =
                                new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
                        layout->addWidget(buttons);
                        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

                        ret = dlg.exec() == QDialog::Accepted ? QMessageBox::Ok : QMessageBox::Cancel;
                        if (ret == QMessageBox::Ok)
                                confirm_on_stop_emulation = !remember->isChecked();
                }

                if (ret == QMessageBox::Ok) {
                        stop_emulation();
                        if (!config_override)
                                showConfigSelection();
                        else
                                quit(true);
                } else
                        resume_emulation();
        }
}

void MainWindow::onStopEmulationNow() {
        stop_emulation();
        pclog_flush();
        if (!config_override)
                showConfigSelection();
        else
                quit(true);
}

void MainWindow::onShowWindow(QWidget *window, int show) {
        bool shown = window->isVisible();
        if (show < 0)
                window->setVisible(!shown);
        else
                window->setVisible(show > 0);
        if (!shown)
                window->update();
}

void MainWindow::onPopupMenu(QMenu *menu, int *x, int *y) {
        if (x && y)
                menu->popup(QPoint(*x, *y));
        else
                menu->popup(QCursor::pos());
}

void MainWindow::onExit(int value) {
        if (m_closing)
                return;
        m_closing = true;
        PCemExitThread *exitThread = new PCemExitThread(this);
        connect(exitThread, &PCemExitThread::exitComplete, this, &MainWindow::onExitComplete);
        connect(exitThread, &QThread::finished, exitThread, &QObject::deleteLater);
        exitThread->start();
}

void MainWindow::onExitComplete(int result) {
        if (result) {
                pclog_end();
                close();
                QApplication::quit();
        } else
                m_closing = false;
}

void MainWindow::closeEvent(QCloseEvent *event) {
        if (m_closing) {
                event->accept();
                return;
        }
        /* Intercept close and route through quit logic */
        event->ignore();
        quit(emulation_state != EMULATION_STOPPED);
}

void MainWindow::quit(bool stopEmulator) {
        if (m_closing)
                return;
        m_closing = true;
        if (stopEmulator && emulation_state != EMULATION_STOPPED) {
                if (!wx_stop(this)) {
                        m_closing = false;
                        return;
                }
        }
        pclog_end();
        QApplication::quit();
        /* Force exit if the event loop isn't running yet (e.g. window never shown) */
        if (!isVisible())
                _Exit(0);
}

PCemExitThread::PCemExitThread(MainWindow *mainWindow) : mainWindow(mainWindow) {}

void PCemExitThread::run() { emit exitComplete(wx_stop(mainWindow)); }
