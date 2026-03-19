#ifndef QT_DIALOGBOX_H_
#define QT_DIALOGBOX_H_

#include <QDialog>
#include <QHash>
#include <QWidget>

#include "qt-utils.h"

class PCemDialogBox : public QDialog {
        Q_OBJECT
public:
        PCemDialogBox(QWidget *parent, int (*callback)(void *window, int message, INT_PARAM param1, LONG_PARAM param2));
        virtual ~PCemDialogBox();

        bool loadUi(const char *name);
        void onInit();
        void setReady(bool ready) { m_ready = ready; }

        void registerWidget(int id, QWidget *widget);
        QWidget *findWidgetById(int id);

private:
        void autoRegisterWidgets(QWidget *root);
        void connectWidgetSignals();
        int processEvent(int message, INT_PARAM param1, LONG_PARAM param2);
        int (*m_callback)(void *window, int message, INT_PARAM param1, LONG_PARAM param2);
        bool m_commandActive;
        bool m_ready;
        QHash<int, QWidget *> m_widgets;
};

#endif /* QT_DIALOGBOX_H_ */
