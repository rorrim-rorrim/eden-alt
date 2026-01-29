#include <algorithm>
#include "common/common_types.h"
#include "common/fs/path_util.h"
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

QPixmap NewUserDialog::DefaultAvatar() {
    QPixmap icon;

    icon.fill(Qt::black);
    icon.loadFromData(Core::Constants::ACCOUNT_BACKUP_JPEG.data(),
                      static_cast<u32>(Core::Constants::ACCOUNT_BACKUP_JPEG.size()));

    return icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QString NewUserDialog::GetImagePath(const Common::UUID& uuid) {
    const auto path =
        Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) /
        fmt::format("system/save/8000000000000010/su/avators/{}.jpg", uuid.FormattedString());
    return QString::fromStdString(Common::FS::PathToUTF8String(path));
}

QPixmap NewUserDialog::GetIcon(const Common::UUID& uuid) {
    QPixmap icon{GetImagePath(uuid)};

    if (!icon) {
        return DefaultAvatar();
    }

    return icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

NewUserDialog::NewUserDialog(Common::UUID uuid, QWidget* parent) : QDialog(parent) {
    setup(uuid);
}

NewUserDialog::NewUserDialog(QWidget* parent) : QDialog(parent) {
    setup(Common::UUID::MakeRandom());
}

void NewUserDialog::setup(Common::UUID uuid) {
    ui = new Ui::NewUserDialog;
    ui->setupUi(this);

    m_scene = new QGraphicsScene;
    ui->image->setScene(m_scene);

    // setup
    ui->uuid->setText(QString::fromStdString(uuid.RawString()).toUpper());
    verifyUser();

    setImage(GetIcon(uuid));
    updateRevertButton();

    // Validators
    QRegularExpressionValidator* username_validator =
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral(".{1,32}")), this);
    ui->username->setValidator(username_validator);

    QRegularExpressionValidator* uuid_validator = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9a-fA-F]{32}")), this);
    ui->uuid->setValidator(uuid_validator);

    // Connections
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

    setImage(image.scaled(64, 64, Qt::IgnoreAspectRatio));
    setIsDefaultAvatar(false);
}

void NewUserDialog::setImage(const QPixmap& pixmap) {
    m_pixmap = pixmap;
    m_scene->clear();
    m_scene->addPixmap(m_pixmap);
}

void NewUserDialog::revertImage() {
    setImage(DefaultAvatar());
    setIsDefaultAvatar(true);
}

void NewUserDialog::updateRevertButton() {
    if (isDefaultAvatar()) {
        ui->revert->setIcon(QIcon{});
    } else {
        QStyle* style = parentWidget()->style();
        QIcon icon(style->standardIcon(QStyle::SP_LineEditClearButton));
        ui->revert->setIcon(icon);
    }
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

// TODO: Move UUID
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
