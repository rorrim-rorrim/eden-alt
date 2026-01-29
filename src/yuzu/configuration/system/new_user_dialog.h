#pragma once

#include <QDialog>
#include "common/uuid.h"

class QGraphicsScene;
namespace Ui {
class NewUserDialog;
}

struct User {
    QString username;
    Common::UUID uuid;
    QPixmap pixmap;
};

class NewUserDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(bool isDefaultAvatar READ isDefaultAvatar WRITE setIsDefaultAvatar NOTIFY
                   isDefaultAvatarChanged FINAL)

public:
    explicit NewUserDialog(QWidget *parent = nullptr);
    explicit NewUserDialog(Common::UUID uuid, QWidget *parent = nullptr);
    ~NewUserDialog();

    bool isDefaultAvatar() const;
    void setIsDefaultAvatar(bool newIsDefaultAvatar);

    static QString GetImagePath(const Common::UUID& uuid);
    static QPixmap GetIcon(const Common::UUID& uuid);
    static QPixmap DefaultAvatar();

private:
    Ui::NewUserDialog *ui;
    QGraphicsScene *m_scene;
    QPixmap m_pixmap;

    bool m_isDefaultAvatar = true;
    bool m_editing = false;

    void setup(Common::UUID uuid = Common::UUID::MakeRandom());
public slots:
    void setImage(const QPixmap &pixmap);
    void selectImage();
    void revertImage();
    void updateRevertButton();

    void generateUUID();
    void verifyUser();

    void dispatchUser();

signals:
    void isDefaultAvatarChanged(bool isDefaultAvatar);
    void userAdded(User user);
};
