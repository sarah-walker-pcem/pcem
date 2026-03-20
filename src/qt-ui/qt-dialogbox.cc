#include "qt-dialogbox.h"
#include "qt-utils.h"

#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QFile>
#include <QUiLoader>
#include <QMessageBox>
#include <QRegularExpression>

PCemDialogBox::PCemDialogBox(QWidget *parent,
                             int (*callback)(void *window, int message, INT_PARAM param1, LONG_PARAM param2))
        : QDialog(parent), m_callback(callback), m_commandActive(false), m_ready(false) {
}

PCemDialogBox::~PCemDialogBox() {}

bool PCemDialogBox::loadUi(const char *name) {
        /* Load .ui file from embedded Qt resources */
        QString filePath = QString(":/ui/%1.ui").arg(name);

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
                QMessageBox::warning(this, "PCem", QString("Could not open UI resource: %1").arg(filePath));
                return false;
        }

        QUiLoader loader;
        QWidget *formWidget = loader.load(&file, this);
        file.close();

        if (!formWidget) {
                QMessageBox::warning(this, "PCem",
                        QString("Failed to load UI: %1\n%2").arg(filePath, loader.errorString()));
                return false;
        }

        /* Set dialog properties from the loaded widget */
        setWindowTitle(formWidget->windowTitle());
        if (formWidget->minimumSize().width() > 0)
                setMinimumSize(formWidget->minimumSize());

        /* Embed the loaded form into this dialog */
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(formWidget);
        formWidget->show();

        /* Auto-register all named child widgets */
        autoRegisterWidgets(formWidget);

        return true;
}

void PCemDialogBox::autoRegisterWidgets(QWidget *root) {
        QList<QWidget *> children = root->findChildren<QWidget *>();
        for (QWidget *child : children) {
                QString name = child->objectName();
                if (name.isEmpty() || name.startsWith("qt_"))
                        continue;

                int id;
                /* Map special wxWidgets IDs */
                if (name == "wxID_OK")
                        id = wxID_OK;
                else if (name == "wxID_CANCEL")
                        id = wxID_CANCEL;
                else
                        id = wx_xrcid(name.toUtf8().constData());

                registerWidget(id, child);

                /* Also register with bracket notation for indexed widgets.
                   The .ui files use underscores (IDC_FOO_0) but the C code
                   uses brackets (IDC_FOO[0]). Register both forms. */
                QRegularExpression re("^(.+)_(\\d+)$");
                QRegularExpressionMatch match = re.match(name);
                if (match.hasMatch()) {
                        QString bracketName = match.captured(1) + "[" + match.captured(2) + "]";
                        int bracketId = wx_xrcid(bracketName.toUtf8().constData());
                        if (bracketId != id)
                                registerWidget(bracketId, child);
                }
        }

        /* Connect any QComboBox + QStackedWidget pairs in container widgets.
           Convention: QStackedWidget named "FOO" pairs with QComboBox named "FOO_COMBO". */
        QList<QStackedWidget *> stacks = root->findChildren<QStackedWidget *>();
        for (QStackedWidget *sw : stacks) {
                QString comboName = sw->objectName() + "_COMBO";
                QComboBox *combo = root->findChild<QComboBox *>(comboName);
                if (combo) {
                        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                                sw, &QStackedWidget::setCurrentIndex);
                }
        }
}

void PCemDialogBox::onInit() {
        if (m_callback) {
                /* Block all child widget signals during INITDIALOG to prevent
                   premature WX_COMMAND callbacks while populating widgets */
                QList<QWidget *> children = findChildren<QWidget *>();
                for (QWidget *child : children)
                        child->blockSignals(true);

                m_callback(this, WX_INITDIALOG, 0, 0);

                /* Re-scan for new widgets that may have been dynamically added
                   during INITDIALOG (e.g. pages added to stacked widgets) */
                children = findChildren<QWidget *>();
                for (QWidget *child : children)
                        child->blockSignals(false);

                connectWidgetSignals();
                m_ready = true;
        }
}

void PCemDialogBox::registerWidget(int id, QWidget *widget) {
        m_widgets.insert(id, widget);
        widget->setProperty("widgetId", id);
}

void PCemDialogBox::connectWidgetSignals() {
        for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it) {
                int id = it.key();
                QWidget *widget = it.value();

                if (auto *btn = qobject_cast<QPushButton *>(widget)) {
                        connect(btn, &QPushButton::clicked, this, [this, id]() {
                                processEvent(WX_COMMAND, id, 0);
                        });
                } else if (auto *cb = qobject_cast<QComboBox *>(widget)) {
                        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, id](int) {
                                processEvent(WX_COMMAND, id, 0);
                        });
                } else if (auto *rb = qobject_cast<QRadioButton *>(widget)) {
                        connect(rb, &QRadioButton::clicked, this, [this, id]() {
                                processEvent(WX_COMMAND, id, 0);
                        });
                } else if (auto *le = qobject_cast<QLineEdit *>(widget)) {
                        connect(le, &QLineEdit::textChanged, this, [this, id](const QString &) {
                                processEvent(WX_COMMAND, id, 0);
                        });
                } else if (auto *lw = qobject_cast<QListWidget *>(widget)) {
                        connect(lw, &QListWidget::itemDoubleClicked, this, [this, id](QListWidgetItem *) {
                                processEvent(WX_COMMAND, id, WX_LBN_DBLCLK);
                        });
                } else if (auto *tw = qobject_cast<QTabWidget *>(widget)) {
                        connect(tw, &QTabWidget::currentChanged, this, [this, id](int index) {
                                processEvent(WX_COMMAND, id, index);
                        });
                }
        }
}

QWidget *PCemDialogBox::findWidgetById(int id) {
        auto it = m_widgets.find(id);
        if (it != m_widgets.end())
                return it.value();
        return nullptr;
}

int PCemDialogBox::processEvent(int message, INT_PARAM param1, LONG_PARAM param2) {
        if (!m_ready || m_commandActive)
                return 0;
        int result = 0;
        m_commandActive = true;
        result = m_callback(this, message, param1, param2);
        m_commandActive = false;
        return result;
}
