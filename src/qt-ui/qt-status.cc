#include "qt-status.h"
#include "qt-common.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QElapsedTimer>

/* Global variables declared extern in qt-common.h */
int show_machine_on_start = 0;
int confirm_on_stop_emulation = 1;
int confirm_on_reset_machine = 1;
int show_status = 0;
int show_speed_history = 0;
int show_disc_activity = 1;
int show_machine_info = 1;
int show_mount_paths = 0;
int wx_window_x = 0;
int wx_window_y = 0;

/* StatusPane */

StatusPane::StatusPane(QWidget *parent) : QWidget(parent) {
        memset(machineInfoText, 0, sizeof(machineInfoText));
        memset(statusMachineText, 0, sizeof(statusMachineText));
        memset(statusDeviceText, 0, sizeof(statusDeviceText));
        memset(speedHistory, 0, sizeof(speedHistory));
        lastSpeedUpdate = 0;

        setMinimumSize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
}

StatusPane::~StatusPane() {}

void StatusPane::paintEvent(QPaintEvent *event) {
        QPainter painter(this);
        /* TODO: implement status rendering */
        painter.fillRect(rect(), Qt::black);
}

/* StatusFrame */

StatusFrame::StatusFrame(QWidget *parent) : QMainWindow(parent) {
        setWindowTitle("PCem Status");
        setAttribute(Qt::WA_DeleteOnClose, false);

        statusPane = new StatusPane(this);
        setCentralWidget(statusPane);

        statusTimer = new QTimer(this);
        connect(statusTimer, &QTimer::timeout, this, &StatusFrame::onTimerTick);
}

StatusFrame::~StatusFrame() {
        statusTimer->stop();
}

void StatusFrame::onCommand(int id) {
        /* TODO: handle status window commands */
}

void StatusFrame::onTimerTick() {
        if (statusPane)
                statusPane->update();
}

void StatusFrame::updateToolbar() {
        /* TODO: update toolbar state */
}
