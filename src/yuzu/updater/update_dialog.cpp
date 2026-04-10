// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QRadioButton>
#include <QStandardPaths>
#include "common/logging.h"
#include "qt_common/abstract/frontend.h"
#include "qt_common/abstract/progress.h"
#include "ui_update_dialog.h"
#include "update_dialog.h"
#include <QSaveFile>
#include <qdesktopservices.h>

#include "common/httplib.h"

#ifdef YUZU_BUNDLED_OPENSSL
#include <openssl/cert.h>
#endif

#include <QDesktopServices>

#undef GetSaveFileName

UpdateDialog::UpdateDialog(const Common::Net::Release& release, QWidget* parent)
    : QDialog(parent), ui(new Ui::UpdateDialog) {
    ui->setupUi(this);

    ui->version->setText(tr("%1 is available for download.").arg(QString::fromStdString(release.title)));
    ui->url->setText(tr("<a href=\"%1\">View on Forgejo</a>").arg(QString::fromStdString(release.html_url)));

    std::string text{release.body};
    if (auto pos = text.find("# Packages"); pos != std::string::npos) {
        text = text.substr(0, pos);
    }

    ui->body->setMarkdown(QString::fromStdString(text));

    // TODO(crueter): Find a way to set default
    const auto assets = release.GetAssets();

    if (assets.empty()) {
        ui->groupBox->setHidden(true);
        connect(this, &QDialog::accepted, this, [release]() {
            QDesktopServices::openUrl(QUrl{QString::fromStdString(release.html_url)});
        });
    } else {
        u32 i = 0;
        for (const Common::Net::Asset& a : release.GetAssets()) {
            QRadioButton* r = new QRadioButton(tr(a.name.c_str()), this);
            if (i == 0) r->setChecked(true);
            ++i;

            r->setProperty("url", QString::fromStdString(a.url));
            r->setProperty("path", QString::fromStdString(a.path));
            r->setProperty("filename", QString::fromStdString(a.filename));

            ui->radioButtons->addWidget(r);
            m_buttons.append(r);
        }

        connect(this, &QDialog::accepted, this, &UpdateDialog::Download);
    }
}

UpdateDialog::~UpdateDialog() {
    delete ui;
}

void UpdateDialog::Download() {
    std::string url, path, asset_filename;
    for (QRadioButton* r : std::as_const(m_buttons)) {
        if (r->isChecked()) {
            url = r->property("url").toString().toStdString();
            path = r->property("path").toString().toStdString();
            asset_filename = r->property("filename").toString().toStdString();
            break;
        }
    }

    if (url.empty())
        return;

    const auto filename = QtCommon::Frontend::GetSaveFileName(
        tr("New Version Location"),
        qApp->applicationDirPath() % QStringLiteral("/") % QString::fromStdString(asset_filename),
        tr("All Files (*.*)"));

    if (filename.isEmpty())
        return;

    QSaveFile file(filename);
    if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        LOG_WARNING(Frontend, "Could not open file {}", filename.toStdString());
        QtCommon::Frontend::Critical(tr("Failed to save file"),
                                     tr("Could not open file %1 for writing.").arg(filename));
        return;
    }

    // TODO(crueter): Move to net.cpp
    constexpr std::size_t timeout_seconds = 15;

    std::unique_ptr<httplib::Client> client = std::make_unique<httplib::Client>(url);
    client->set_connection_timeout(timeout_seconds);
    client->set_read_timeout(timeout_seconds);
    client->set_write_timeout(timeout_seconds);

#ifdef YUZU_BUNDLED_OPENSSL
    client->load_ca_cert_store(kCert, sizeof(kCert));
#endif

    if (client == nullptr) {
        LOG_ERROR(Frontend, "Invalid URL {}{}", url, path);
        return;
    }

    auto progress =
        QtCommon::Frontend::newProgressDialog(tr("Downloading..."), tr("Cancel"), 0, 100);
    progress->show();

    QGuiApplication::processEvents();

    // Progress dialog.
    auto progress_callback = [&](size_t processed_size, size_t total_size) {
        QGuiApplication::processEvents();
        progress->setValue(static_cast<int>((processed_size * 100) / total_size));
        return !progress->wasCanceled();
    };

    // Write file in chunks.
    auto content_receiver = [&file, filename](const char* t_data, size_t data_length) -> bool {
        if (file.write(t_data, data_length) == -1) {
            LOG_WARNING(Frontend, "Could not write {} bytes to file {}", data_length,
                        filename.toStdString());
            QtCommon::Frontend::Critical(tr("Failed to save file"),
                                         tr("Could not write to file %1.").arg(filename));
            return false;
        }

        return true;
    };

    // Now send off request
    auto result = client->Get(path, content_receiver, progress_callback);
    progress->close();

    // commit to file
    if (!file.commit()) {
        LOG_WARNING(Frontend, "Could not commit to file {}", filename.toStdString());
        QtCommon::Frontend::Critical(tr("Failed to save file"),
                                     tr("Could not commit to file %1.").arg(filename));
    }

    if (!result) {
        LOG_ERROR(Frontend, "GET to {}{} returned null", url, path);
        return;
    }

    const auto& response = result.value();
    if (response.status >= 400) {
        LOG_ERROR(Frontend, "GET to {}{} returned error status code: {}", url, path,
                  response.status);
        QtCommon::Frontend::Critical(
            tr("Failed to download file"),
            tr("Could not download from %1%2\nError code: %3")
                .arg(QString::fromStdString(url), QString::fromStdString(path), QString::number(response.status)));
        return;
    }
    if (!response.headers.contains("content-type")) {
        LOG_ERROR(Frontend, "GET to {}{} returned no content", url, path);
        return;
    }

    // Download is complete. User may choose to open in the file manager.
    // TODO(crueter): Auto-extract for zip, auto-open for DMG
    // e.g. download to tmp directory?

    auto button =
        QtCommon::Frontend::Question(tr("Download Complete"),
                                     tr("Successfully downloaded %1. Would you like to open it?")
                                         .arg(QString::fromStdString(asset_filename)),
                                     QtCommon::Frontend::Yes | QtCommon::Frontend::No);

    if (button == QtCommon::Frontend::Yes) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filename));
    }
}
