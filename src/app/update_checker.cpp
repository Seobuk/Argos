/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "update_checker.h"

#include <common/mayo_version.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWidgets/QMessageBox>

namespace Mayo {

bool isRemoteNewer(const QString& remoteTag, const QString& localVersion)
{
    auto fnComponent = [](const QStringList& parts, int i) {
        return i < parts.size() ? parts.at(i).toInt() : 0;
    };

    QString remote = remoteTag;
    if (remote.startsWith('v') || remote.startsWith('V'))
        remote.remove(0, 1);

    const QStringList remoteParts = remote.split('.');
    const QStringList localParts = localVersion.split('.');
    for (int i = 0; i < 3; ++i) {
        const int r = fnComponent(remoteParts, i);
        const int l = fnComponent(localParts, i);
        if (r != l)
            return r > l;
    }

    return false;
}

void checkForUpdates(QWidget* parent, bool silentIfUpToDate)
{
    // Runnable self-checks of the version parser (compiled out in release)
    Q_ASSERT(isRemoteNewer("v1.2.0", "1.1.9"));
    Q_ASSERT(!isRemoteNewer("1.0.0", "1.0.0"));
    Q_ASSERT(!isRemoteNewer("v0.9.9", "1.0.0"));

    auto netMgr = new QNetworkAccessManager(parent);
    QNetworkRequest request(QUrl("https://api.github.com/repos/Seobuk/Argos/releases/latest"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Argos-Updater");

    QPointer<QWidget> parentGuard(parent);
    QNetworkReply* reply = netMgr->get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply, [=]() {
        reply->deleteLater();
        netMgr->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            // The in-app check goes through Qt's TLS stack, which on some Windows
            // machines can't validate GitHub's certificate (antivirus/proxy SSL
            // inspection, or a Schannel fallback with no OpenSSL). The system
            // browser uses the OS trust store and downloads fine, so on failure
            // offer to open the releases page there instead of dead-ending.
            if (!silentIfUpToDate) {
                const auto answer = QMessageBox::question(
                    parentGuard,
                    QObject::tr("업데이트 확인 실패"),
                    QObject::tr("업데이트 확인에 실패했습니다:\n%1\n\n"
                                "브라우저에서 다운로드 페이지를 여시겠습니까?")
                        .arg(reply->errorString())
                );
                if (answer == QMessageBox::Yes) {
                    QDesktopServices::openUrl(
                        QUrl("https://github.com/Seobuk/Argos/releases/latest")
                    );
                }
            }

            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString remoteTag = root.value("tag_name").toString();
        const QString htmlUrl = root.value("html_url").toString();

        // Prefer the setup .exe asset, fall back to the release page
        QString downloadUrl = htmlUrl;
        static const QRegularExpression reSetup(
            "setup.*\\.exe$", QRegularExpression::CaseInsensitiveOption
        );
        for (const QJsonValue& v : root.value("assets").toArray()) {
            const QJsonObject asset = v.toObject();
            if (reSetup.match(asset.value("name").toString()).hasMatch()) {
                downloadUrl = asset.value("browser_download_url").toString();
                break;
            }
        }

        const QString localVersion = QString::fromUtf8(strVersion);
        if (isRemoteNewer(remoteTag, localVersion)) {
            const auto answer = QMessageBox::question(
                parentGuard,
                QObject::tr("업데이트"),
                QObject::tr("새 버전 %1 이(가) 있습니다 (현재 %2). 다운로드 페이지를 여시겠습니까?")
                    .arg(remoteTag, localVersion)
            );
            if (answer == QMessageBox::Yes && !downloadUrl.isEmpty())
                QDesktopServices::openUrl(QUrl(downloadUrl));
        }
        else if (!silentIfUpToDate) {
            QMessageBox::information(
                parentGuard,
                QObject::tr("업데이트"),
                QObject::tr("최신 버전입니다 (%1).").arg(localVersion)
            );
        }
    });
}

} // namespace Mayo
