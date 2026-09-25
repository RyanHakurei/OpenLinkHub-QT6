#include "api/ApiClient.h"

#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrlQuery>

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
{
    m_base = QUrl(QStringLiteral("http://127.0.0.1:27003"));
}

void ApiClient::setBaseUrl(const QUrl &url)
{
    m_base = url;
}

QUrl ApiClient::baseUrl() const
{
    return m_base;
}

QUrl ApiClient::url(const QString &path) const
{
    return m_base.resolved(QUrl(path));
}

QUrl ApiClient::staticUrl(const QString &relative) const
{
    return url(QStringLiteral("/static/") + relative);
}

bool ApiClient::isReachable() const
{
    return m_reachable;
}

void ApiClient::markReachable(bool reachable)
{
    if (m_reachable == reachable) {
        return;
    }
    m_reachable = reachable;
    Q_EMIT reachableChanged(reachable);
}

void ApiClient::get(const QString &path, const Callback &callback)
{
    send("GET", path, {}, {}, callback);
}

void ApiClient::post(const QString &path, const QJsonObject &body, const Callback &callback)
{
    send("POST", path, QJsonDocument(body).toJson(QJsonDocument::Compact), "application/json", callback);
}

void ApiClient::put(const QString &path, const QJsonObject &body, const Callback &callback)
{
    send("PUT", path, QJsonDocument(body).toJson(QJsonDocument::Compact), "application/json", callback);
}

void ApiClient::del(const QString &path, const QJsonObject &body, const Callback &callback)
{
    send("DELETE", path, QJsonDocument(body).toJson(QJsonDocument::Compact), "application/json", callback);
}

void ApiClient::putForm(const QString &path, const QUrlQuery &form, const Callback &callback)
{
    send("PUT", path, form.query(QUrl::FullyEncoded).toUtf8(), "application/x-www-form-urlencoded", callback);
}

void ApiClient::getBinary(const QString &path, const BinaryCallback &callback)
{
    QNetworkRequest request(url(path));
    request.setTransferTimeout(15000);
    request.setRawHeader("User-Agent", "OpenLinkHub-Qt/0.1");

    QNetworkReply *reply = m_nam.get(request);
    QPointer<ApiClient> guard(this);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, guard]() {
        reply->deleteLater();
        if (!guard) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            markReachable(false);
            callback({}, reply->errorString(), {});
            return;
        }
        markReachable(true);
        callback(reply->readAll(), {}, QString::fromUtf8(reply->rawHeader("Content-Type")));
    });
}

void ApiClient::postMultipart(const QString &path, QHttpMultiPart *multiPart, const Callback &callback)
{
    QNetworkRequest request(url(path));
    request.setTransferTimeout(30000);
    request.setRawHeader("User-Agent", "OpenLinkHub-Qt/0.1");

    QNetworkReply *reply = m_nam.post(request, multiPart);
    multiPart->setParent(reply);

    QPointer<ApiClient> guard(this);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, guard]() {
        reply->deleteLater();
        if (!guard) {
            return;
        }

        const QByteArray payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            markReachable(false);
            const QString error = reply->errorString();
            Q_EMIT requestFailed(error);
            callback({}, error);
            return;
        }

        markReachable(true);
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            callback(document.object(), {});
            return;
        }
        callback(QJsonObject{{QStringLiteral("message"), QString::fromUtf8(payload)}}, {});
    });
}

void ApiClient::send(const QByteArray &method, const QString &path, const QByteArray &body, const QByteArray &contentType, const Callback &callback)
{
    QNetworkRequest request(url(path));
    request.setTransferTimeout(8000);
    request.setRawHeader("User-Agent", "OpenLinkHub-Qt/0.1");
    if (!contentType.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }

    QNetworkReply *reply = m_nam.sendCustomRequest(request, method, body);
    QPointer<ApiClient> guard(this);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, guard]() {
        reply->deleteLater();
        if (!guard) {
            return;
        }

        const QByteArray payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            markReachable(false);
            const QString error = reply->errorString();
            Q_EMIT requestFailed(error);
            if (callback) {
                callback({}, error);
            }
            return;
        }

        markReachable(true);
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            const QString error = tr("Invalid JSON from OpenLinkHub");
            Q_EMIT requestFailed(error);
            if (callback) {
                callback({}, error);
            }
            return;
        }

        if (callback) {
            callback(document.object(), {});
        }
    });
}
