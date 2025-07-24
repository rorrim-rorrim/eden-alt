// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_frontend_util.h"

namespace QtCommon::Frontend {
QMessageBox::StandardButton ShowMessage(QMessageBox::Icon icon,
                                        const QString &title,
                                        const QString &text,
                                        QMessageBox::StandardButtons buttons,
                                        QWidget *parent)
{
#ifdef YUZU_QT_WIDGETS
    QMessageBox *box = new QMessageBox(icon, title, text, buttons, parent);
    return static_cast<QMessageBox::StandardButton>(box->exec());
#endif
}

QMessageBox::StandardButton ShowMessage(QMessageBox::Icon icon,
                                        const char *title,
                                        const char *text,
                                        QMessageBox::StandardButtons buttons,
                                        QWidget *parent)
{
    return ShowMessage(icon, qApp->tr(title), qApp->tr(text), buttons, parent);
}

} // namespace QtCommon::Frontend
