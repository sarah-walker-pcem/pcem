#ifndef QT_STATUS_H_
#define QT_STATUS_H_

#include <QMainWindow>
#include <QWidget>
#include <QTimer>
#include <QPixmap>
#include <QPaintEvent>

#define DEFAULT_WINDOW_WIDTH 350
#define DEFAULT_WINDOW_HEIGHT 25

#define SPEED_HISTORY_LENGTH 240

struct drive_info_t;

class StatusPane : public QWidget {
        Q_OBJECT
public:
        StatusPane(QWidget *parent);
        virtual ~StatusPane();

        void refresh();

protected:
        void paintEvent(QPaintEvent *event) override;

private:
        char machineInfoText[4096];
        char statusMachineText[4096];
        char statusDeviceText[4096];
        qint64 lastSpeedUpdate;
        char speedHistory[SPEED_HISTORY_LENGTH];
        drive_info_t *drives;
        int numDrives;
};

#define STATUS_WINDOW_ID 1000

class StatusFrame : public QMainWindow {
        Q_OBJECT
public:
        StatusFrame(QWidget *parent);
        virtual ~StatusFrame();

        QTimer *statusTimer;

private slots:
        void onCommand(int id);
        void onTimerTick();

private:
        void updateToolbar();
        StatusPane *statusPane;
};

#endif /* QT_STATUS_H_ */
