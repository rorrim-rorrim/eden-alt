// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_FRONTEND_UTIL_H
#define QT_FRONTEND_UTIL_H

#include <QGuiApplication>
#include <QMessageBox>
#include "qt_common/qt_common.h"

#ifdef YUZU_QT_WIDGETS
#include <QFileDialog>
#include <QWidget>
#endif

/**
 * manages common functionality e.g. message boxes and such for Qt/QML
 */
namespace QtCommon::Frontend {

Q_NAMESPACE

#ifdef YUZU_QT_WIDGETS
using Options = QFileDialog::Options;
using Option = QFileDialog::Option;
#else
enum Option {
    ShowDirsOnly = 0x00000001,
    DontResolveSymlinks = 0x00000002,
    DontConfirmOverwrite = 0x00000004,
    DontUseNativeDialog = 0x00000008,
    ReadOnly = 0x00000010,
    HideNameFilterDetails = 0x00000020,
    DontUseCustomDirectoryIcons = 0x00000040
};
Q_ENUM_NS(Option)
Q_DECLARE_FLAGS(Options, Option)
Q_FLAG_NS(Options)
#endif

// TODO(crueter) widgets-less impl, choices et al.
QMessageBox::StandardButton ShowMessage(QMessageBox::Icon icon,
                                        const QString &title,
                                        const QString &text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::NoButton,
                                        QObject *parent = nullptr);

#define UTIL_OVERRIDES(level) \
    inline QMessageBox::StandardButton level(QObject *parent, \
                                             const QString &title, \
                                             const QString &text, \
                                             QMessageBox::StandardButtons buttons = QMessageBox::Ok) \
    { \
        return ShowMessage(QMessageBox::level, title, text, buttons, parent); \
    } \
    inline QMessageBox::StandardButton level(QObject *parent, \
                                             const char *title, \
                                             const char *text, \
                                             QMessageBox::StandardButtons buttons \
                                             = QMessageBox::Ok) \
    { \
        return ShowMessage(QMessageBox::level, tr(title), tr(text), buttons, parent); \
    } \
    inline QMessageBox::StandardButton level(const char *title, \
                                             const char *text, \
                                             QMessageBox::StandardButtons buttons \
                                             = QMessageBox::Ok) \
    { \
        return ShowMessage(QMessageBox::level, tr(title), tr(text), buttons, rootObject); \
    } \
    inline QMessageBox::StandardButton level(const QString title, \
                                             const QString &text, \
                                             QMessageBox::StandardButtons buttons \
                                             = QMessageBox::Ok) \
    { \
        return ShowMessage(QMessageBox::level, title, text, buttons, rootObject); \
    }

UTIL_OVERRIDES(Information)
UTIL_OVERRIDES(Warning)
UTIL_OVERRIDES(Critical)
UTIL_OVERRIDES(Question)

const QString GetOpenFileName(const QString &title,
                              const QString &dir,
                              const QString &filter,
                              QString *selectedFilter = nullptr,
                              Options options = Options());

} // namespace QtCommon::Frontend
#endif // QT_FRONTEND_UTIL_H
