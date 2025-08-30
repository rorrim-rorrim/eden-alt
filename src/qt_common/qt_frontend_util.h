// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

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

// TODO(crueter) widgets-less impl, choices et al.
QMessageBox::StandardButton ShowMessage(QMessageBox::Icon icon,
                                        const QString &title,
                                        const QString &text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::NoButton,
                                        QObject *parent = nullptr);

QMessageBox::StandardButton Information(QObject *parent,
                                        const QString &title,
                                        const QString &text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::Ok);

QMessageBox::StandardButton Warning(QObject *parent,
                                    const QString &title,
                                    const QString &text,
                                    QMessageBox::StandardButtons buttons = QMessageBox::Ok);

QMessageBox::StandardButton Critical(QObject *parent,
                                     const QString &title,
                                     const QString &text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Ok);

QMessageBox::StandardButton Question(QObject *parent,
                                     const QString &title,
                                     const QString &text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Ok);

QMessageBox::StandardButton Information(QObject *parent,
                                        const char *title,
                                        const char *text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::Ok);

QMessageBox::StandardButton Warning(QObject *parent,
                                    const char *title,
                                    const char *text,
                                    QMessageBox::StandardButtons buttons = QMessageBox::Ok);

QMessageBox::StandardButton Critical(QObject *parent,
                                     const char *title,
                                     const char *text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Ok);

QMessageBox::StandardButton Question(QObject *parent,
                                     const char *title,
                                     const char *text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Ok);

} // namespace QtCommon::Frontend
#endif // QT_FRONTEND_UTIL_H
