#include "qt-config-eventbinder.h"

#include <QListWidget>
#include <QWidget>

class QTConfigEventBinder : public QObject {
        Q_OBJECT
private:
        void (*selectedPageCallback)(void *hdlg, int selectedPage);
        QWidget *hdlg;

public:
        QTConfigEventBinder(QWidget *hdlg, void (*selectedPageCallback)(void *hdlg, int selectedPage))
                : QObject(hdlg), selectedPageCallback(selectedPageCallback), hdlg(hdlg) {
                QListWidget *control = hdlg->findChild<QListWidget *>("IDC_PAGESELECTION");
                if (control) {
                        control->setMaximumWidth(250);
                        control->setMinimumWidth(250);
                        connect(control, &QListWidget::currentRowChanged, this, &QTConfigEventBinder::onSelectionChanged);
                }
        }

        ~QTConfigEventBinder() {
                QListWidget *control = hdlg->findChild<QListWidget *>("IDC_PAGESELECTION");
                if (control) {
                        disconnect(control, &QListWidget::currentRowChanged, this, &QTConfigEventBinder::onSelectionChanged);
                }
        }

private slots:
        void onSelectionChanged(int row) {
                if (row >= 0 && selectedPageCallback != nullptr) {
                        selectedPageCallback(hdlg, row);
                }
        }
};

void *wx_config_eventbinder(void *hdlg, void (*selectedPageCallback)(void *hdlg, int selectedPage)) {
        QTConfigEventBinder *binder = new QTConfigEventBinder((QWidget *)hdlg, selectedPageCallback);
        return (void *)binder;
}

void wx_config_destroyeventbinder(void *eventBinder) { delete (QTConfigEventBinder *)eventBinder; }

#include "qt-config-eventbinder.moc"
