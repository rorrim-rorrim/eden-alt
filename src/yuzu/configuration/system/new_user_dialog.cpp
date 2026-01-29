#include <algorithm>
#include "common/common_types.h"
#include "common/uuid.h"
#include "core/constants.h"
#include "new_user_dialog.h"
#include "ui_new_user_dialog.h"

#include <QFileDialog>
#include <QStyle>
#include <fmt/format.h>
#include <qnamespace.h>
#include <qregularexpression.h>
#include <qvalidator.h>

// TODO: edit user. Will need a "revert UUID" button, auto move UUID stuffs?
NewUserDialog::NewUserDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NewUserDialog) {
    ui->setupUi(this);

    // TODO: Only show revert when not default image
    QStyle* style = parent->style();
    QIcon icon(style->standardIcon(QStyle::SP_LineEditClearButton));
    ui->revert->setIcon(icon);

    m_scene = new QGraphicsScene;
    ui->image->setScene(m_scene);

    // setup
    generateUUID();
    verifyUser();

    revertImage();
    updateRevertButton();

    // Validators
    QRegularExpressionValidator* username_validator = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral(".{1,32}")), this);
    ui->username->setValidator(username_validator);

    QRegularExpressionValidator* uuid_validator = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9a-fA-F]{32}")), this);
    ui->uuid->setValidator(uuid_validator);

    connect(ui->uuid, &QLineEdit::textEdited, this, &::NewUserDialog::verifyUser);
    connect(ui->username, &QLineEdit::textEdited, this, &::NewUserDialog::verifyUser);

    connect(ui->revert, &QAbstractButton::clicked, this, &NewUserDialog::revertImage);
    connect(ui->selectImage, &QAbstractButton::clicked, this, &NewUserDialog::selectImage);
    connect(ui->generate, &QAbstractButton::clicked, this, &NewUserDialog::generateUUID);

    connect(this, &NewUserDialog::isDefaultAvatarChanged, this, &NewUserDialog::updateRevertButton);
    connect(this, &QDialog::accepted, this, &NewUserDialog::dispatchUser);
}

NewUserDialog::~NewUserDialog() {
    delete ui;
}

QPixmap NewUserDialog::defaultAvatar() {
    QPixmap icon;

    icon.fill(Qt::black);
    icon.loadFromData(Core::Constants::ACCOUNT_BACKUP_JPEG.data(),
                      static_cast<u32>(Core::Constants::ACCOUNT_BACKUP_JPEG.size()));

    return icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

bool NewUserDialog::isDefaultAvatar() const {
    return m_isDefaultAvatar;
}

void NewUserDialog::setIsDefaultAvatar(bool newIsDefaultAvatar) {
    if (m_isDefaultAvatar == newIsDefaultAvatar)
        return;
    m_isDefaultAvatar = newIsDefaultAvatar;
    emit isDefaultAvatarChanged(m_isDefaultAvatar);
}

// TODO: setAvatar
void NewUserDialog::selectImage() {
    const auto file = QFileDialog::getOpenFileName(this, tr("Select User Image"), QString(),
                                                   tr("Image Formats (*.jpg *.jpeg *.png *.bmp)"));
    if (file.isEmpty()) {
        return;
    }

    // Profile image must be 256x256
    QPixmap image(file);
    if (image.width() != 256 || image.height() != 256) {
        image = image.scaled(256, 256, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    m_pixmap = image.scaled(64, 64, Qt::IgnoreAspectRatio);

    m_scene->clear();
    m_scene->addPixmap(m_pixmap);
    setIsDefaultAvatar(false);
}

void NewUserDialog::revertImage() {
    m_pixmap = defaultAvatar();

    m_scene->clear();
    m_scene->addPixmap(m_pixmap);
    setIsDefaultAvatar(true);
}

void NewUserDialog::updateRevertButton() {
    ui->revert->setVisible(!isDefaultAvatar());
}

void NewUserDialog::generateUUID() {
    Common::UUID uuid = Common::UUID::MakeRandom();
    ui->uuid->setText(QString::fromStdString(uuid.RawString()).toUpper());
}

void NewUserDialog::verifyUser() {
    const QPixmap checked = QIcon::fromTheme(QStringLiteral("checked")).pixmap(16);
    const QPixmap failed = QIcon::fromTheme(QStringLiteral("failed")).pixmap(16);

    const bool uuid_good = ui->uuid->hasAcceptableInput();
    const bool username_good = ui->username->hasAcceptableInput();

    auto ok_button = ui->buttonBox->button(QDialogButtonBox::Ok);
    ok_button->setEnabled(username_good && uuid_good);

    if (uuid_good) {
        ui->uuidVerified->setPixmap(checked);
        ui->uuidVerified->setToolTip(tr("All Good", "Tooltip"));
    } else {
        ui->uuidVerified->setPixmap(failed);
        ui->uuidVerified->setToolTip(tr("Must be 32 hex characters (0-9, a-f)", "Tooltip"));
    }

    if (username_good) {
        ui->usernameVerified->setPixmap(checked);
        ui->usernameVerified->setToolTip(tr("All Good", "Tooltip"));
    } else {
        ui->usernameVerified->setPixmap(failed);
        ui->usernameVerified->setToolTip(tr("Must be between 1 and 32 characters", "Tooltip"));
    }

}

void NewUserDialog::dispatchUser() {
    QByteArray bytes = QByteArray::fromHex(ui->uuid->text().toLatin1());

    // convert to 16 u8's
    std::array<u8, 16> uuid_arr;
    std::copy_n(reinterpret_cast<const u8*>(bytes.constData()), 16, uuid_arr.begin());
    std::ranges::reverse(uuid_arr);

    Common::UUID uuid(uuid_arr);

    User user{
        .username = ui->username->text(),
        .uuid = uuid,
        .pixmap = m_pixmap,
    };

    emit userAdded(user);
}
