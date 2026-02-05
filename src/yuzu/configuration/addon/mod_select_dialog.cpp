#include <filesystem>
#include <qnamespace.h>
#include "mod_select_dialog.h"
#include "ui_mod_select_dialog.h"

ModSelectDialog::ModSelectDialog(const QStringList& mods, QWidget* parent)
    : QDialog(parent), ui(new Ui::ModSelectDialog) {
    ui->setupUi(this);

    item_model = new QStandardItemModel(ui->treeView);
    ui->treeView->setModel(item_model);

   // We must register all custom types with the Qt Automoc system so that we are able to use it
   // with signals/slots. In this case, QList falls under the umbrella of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    for (const auto& mod : mods) {
        const auto basename = QString::fromStdString(std::filesystem::path(mod.toStdString()).filename());

        auto* const first_item = new QStandardItem;
        first_item->setText(basename);
        first_item->setData(mod);

        first_item->setCheckable(true);
        first_item->setCheckState(Qt::Checked);

        item_model->appendRow(first_item);
        item_model->layoutChanged();
    }

    connect(this, &QDialog::accepted, this, [this]() {
        QStringList selected_mods;

        for (qsizetype i = 0; i < item_model->rowCount(); ++i) {
            auto *const item = item_model->item(i);
            if (item->checkState() == Qt::Checked)
                selected_mods << item->data().toString();
        }

        emit modsSelected(selected_mods);
    });
}

ModSelectDialog::~ModSelectDialog() {
    delete ui;
}
