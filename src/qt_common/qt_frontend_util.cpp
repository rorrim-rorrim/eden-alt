// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_frontend_util.h"

namespace QtCommon::Frontend {

QMessageBox::StandardButton ShowMessage(QMessageBox::Icon icon,
                                        const QString &title,
                                        const QString &text,
                                        QMessageBox::StandardButtons buttons,
                                        QObject *parent)
{
#ifdef YUZU_QT_WIDGETS
    QMessageBox *box = new QMessageBox(icon, title, text, buttons, (QWidget *) parent);
    return static_cast<QMessageBox::StandardButton>(box->exec());
#endif
    // TODO(crueter): If Qt Widgets is disabled...
    // need a way to reference icon/buttons too
}

QMessageBox::StandardButton Information(QObject *parent,
                                        const QString &title,
                                        const QString &text,
                                        QMessageBox::StandardButtons buttons)
{
    return ShowMessage(QMessageBox::Information, title, text, buttons, parent);
}

QMessageBox::StandardButton Warning(QObject *parent,
                                    const QString &title,
                                    const QString &text,
                                    QMessageBox::StandardButtons buttons)
{
    return ShowMessage(QMessageBox::Warning, title, text, buttons, parent);
}

QMessageBox::StandardButton Critical(QObject *parent,
                                     const QString &title,
                                     const QString &text,
                                     QMessageBox::StandardButtons buttons)
{
    return ShowMessage(QMessageBox::Critical, title, text, buttons, parent);
}

QMessageBox::StandardButton Question(QObject *parent,
                                     const QString &title,
                                     const QString &text,
                                     QMessageBox::StandardButtons buttons)
{
    return ShowMessage(QMessageBox::Question, title, text, buttons, parent);
}

QMessageBox::StandardButton Information(QObject *parent,
                                        const char *title,
                                        const char *text,
                                        QMessageBox::StandardButtons buttons)
{
    return ShowMessage(QMessageBox::Information, parent->tr(title), parent->tr(text), buttons, parent);
}

QMessageBox::StandardButton Warning(QObject *parent,
                                    const char *title,
                                    const char *text,
                                    QMessageBox::StandardButtons buttons)
{
    return ShowMessage(QMessageBox::Warning, parent->tr(title), parent->tr(text), buttons, parent);
}

QMessageBox::StandardButton Critical(QObject *parent,
                                     const char *title,
                                     const char *text,
                                     QMessageBox::StandardButtons buttons)
{
    return ShowMessage(QMessageBox::Critical, parent->tr(title), parent->tr(text), buttons, parent);
}

QMessageBox::StandardButton Question(QObject *parent,
                                     const char *title,
                                     const char *text,
                                     QMessageBox::StandardButtons buttons)
{
    return ShowMessage(QMessageBox::Question, parent->tr(title), parent->tr(text), buttons, parent);
}


} // namespace QtCommon::Frontend
