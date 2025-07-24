#ifndef QT_FRONTEND_UTIL_H
#define QT_FRONTEND_UTIL_H

#include <QGuiApplication>
#include <QMessageBox>

#ifdef YUZU_QT_WIDGETS
#include <QWidget>
#endif

/**
 * manages common functionality e.g. message boxes and such for Qt/QML
 */
namespace QtCommon::Frontend {
Q_NAMESPACE

QMessageBox::StandardButton ShowMessage(QMessageBox::Icon icon,
                                        const QString &title,
                                        const QString &text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::NoButton,
                                        QWidget *parent = nullptr);

QMessageBox::StandardButton ShowMessage(QMessageBox::Icon icon,
                                        const char *title,
                                        const char *text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::NoButton,
                                        QWidget *parent = nullptr);
} // namespace QtCommon::Frontend
#endif // QT_FRONTEND_UTIL_H
