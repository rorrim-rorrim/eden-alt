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
    ~NewUserDialog();

    bool isDefaultAvatar() const;
    void setIsDefaultAvatar(bool newIsDefaultAvatar);

private:
    Ui::NewUserDialog *ui;
    QGraphicsScene *m_scene;
    QPixmap m_pixmap;

    QPixmap defaultAvatar();

    bool m_isDefaultAvatar = true;

public slots:
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
