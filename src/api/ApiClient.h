#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <functional>

class QHttpMultiPart;
class QUrlQuery;

class ApiClient : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const QJsonObject &json, const QString &error)>;
    using BinaryCallback = std::function<void(const QByteArray &data, const QString &error, const QString &contentType)>;

    explicit ApiClient(QObject *parent = nullptr);

    void setBaseUrl(const QUrl &url);
    QUrl baseUrl() const;

    QUrl url(const QString &path) const;
    QUrl staticUrl(const QString &relative) const;

    void get(const QString &path, const Callback &callback);
    void post(const QString &path, const QJsonObject &body, const Callback &callback);
    void put(const QString &path, const QJsonObject &body, const Callback &callback);
    void del(const QString &path, const QJsonObject &body, const Callback &callback);
    void putForm(const QString &path, const QUrlQuery &form, const Callback &callback);
    void getBinary(const QString &path, const BinaryCallback &callback);
    void postMultipart(const QString &path, QHttpMultiPart *multiPart, const Callback &callback);

    bool isReachable() const;

Q_SIGNALS:
    void reachableChanged(bool reachable);
    void requestFailed(const QString &message);

private:
    void send(const QByteArray &method, const QString &path, const QByteArray &body, const QByteArray &contentType, const Callback &callback);
    void markReachable(bool reachable);

    QNetworkAccessManager m_nam;
    QUrl m_base;
    bool m_reachable = false;
};
