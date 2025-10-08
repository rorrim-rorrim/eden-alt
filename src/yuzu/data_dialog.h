#ifndef DATA_DIALOG_H
#define DATA_DIALOG_H

#include <QDialog>
#include <QFutureWatcher>
#include <QSortFilterProxyModel>
#include <QTableWidgetItem>
#include "frontend_common/data_manager.h"
#include <qnamespace.h>

namespace Ui {
class DataDialog;
}

class DataDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataDialog(QWidget *parent = nullptr);
    ~DataDialog();

private:
    std::unique_ptr<Ui::DataDialog> ui;
};

class DataItem : public QTableWidgetItem
{
public:
    DataItem(FrontendCommon::DataManager::DataDir data_dir, QWidget *parent);
    enum DataRole { SIZE = Qt::UserRole + 1, DATA_DIR };

    bool operator<(const QTableWidgetItem &other) const;
    void reset();
    void scan();

private:
    QWidget *m_parent;
    QFutureWatcher<u64> *m_watcher = nullptr;
    FrontendCommon::DataManager::DataDir m_dir;
};

#endif // DATA_DIALOG_H
