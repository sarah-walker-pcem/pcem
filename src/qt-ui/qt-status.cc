#include "qt-status.h"
#include "qt-common.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QElapsedTimer>
#include <QFontMetrics>

extern "C" {
drive_info_t *get_machine_info(char *s, int *num_drive_info);
int get_status(char *machine, char *device);
}

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
        numDrives = 0;

        setMinimumSize(DEFAULT_WINDOW_WIDTH, 200);
}

StatusPane::~StatusPane() {}

void StatusPane::refresh() {
        if (emulation_state == EMULATION_STOPPED) {
                strcpy(machineInfoText, "Emulation is not running.");
                statusMachineText[0] = 0;
                statusDeviceText[0] = 0;
                numDrives = 0;
                return;
        }

        drives = get_machine_info(machineInfoText, &numDrives);
        get_status(statusMachineText, statusDeviceText);
}

void StatusPane::paintEvent(QPaintEvent *event) {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(32, 32, 32));

        QFont font("Consolas", 9);
        painter.setFont(font);
        QFontMetrics fm(font);
        int lineHeight = fm.height() + 2;
        int y = 8;
        int x = 10;

        /* Machine info */
        if (show_machine_info && machineInfoText[0]) {
                painter.setPen(QColor(220, 220, 220));
                QStringList lines = QString(machineInfoText).split('\n');
                for (const QString &line : lines) {
                        painter.drawText(x, y + fm.ascent(), line);
                        y += lineHeight;
                }
                y += 6;
        }

        /* Drive info */
        if (show_disc_activity && numDrives > 0) {
                painter.setPen(QColor(180, 180, 180));
                painter.drawText(x, y + fm.ascent(), "Drives:");
                y += lineHeight;

                for (int i = 0; i < numDrives; i++) {
                        drive_info_t *d = &drives[i];
                        const char *typeStr;
                        switch (d->type) {
                        case DRIVE_TYPE_FDD: typeStr = "FDD"; break;
                        case DRIVE_TYPE_HDD: typeStr = "HDD"; break;
                        case DRIVE_TYPE_CDROM: typeStr = "CD"; break;
                        default: typeStr = "?"; break;
                        }

                        QColor color;
                        if (d->readflash)
                                color = QColor(0, 220, 0);
                        else if (d->enabled)
                                color = QColor(160, 160, 160);
                        else
                                color = QColor(100, 100, 100);

                        painter.setPen(color);
                        QString driveText = QString("  %1: %2").arg(QChar(d->drive_letter), typeStr);
                        if (show_mount_paths && d->enabled && d->fn[0])
                                driveText += QString(" - %1").arg(d->fn);
                        painter.drawText(x, y + fm.ascent(), driveText);
                        y += lineHeight;
                }
                y += 6;
        }

        /* Resize to fit content */
        int neededHeight = y + 8;
        if (neededHeight != minimumHeight()) {
                setMinimumHeight(neededHeight);
                if (parentWidget())
                        parentWidget()->adjustSize();
        }
}

/* StatusFrame */

StatusFrame::StatusFrame(QWidget *parent) : QMainWindow(parent) {
        setWindowTitle("PCem Status");
        setAttribute(Qt::WA_DeleteOnClose, false);
        setWindowFlags(windowFlags() | Qt::Tool);

        statusPane = new StatusPane(this);
        setCentralWidget(statusPane);

        statusTimer = new QTimer(this);
        connect(statusTimer, &QTimer::timeout, this, &StatusFrame::onTimerTick);
}

StatusFrame::~StatusFrame() {
        statusTimer->stop();
}

void StatusFrame::onCommand(int id) {
}

void StatusFrame::onTimerTick() {
        if (statusPane) {
                statusPane->refresh();
                statusPane->update();
        }
}

void StatusFrame::updateToolbar() {
}
