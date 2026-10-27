#include "bindhttp.h"

#include <sol/sol.hpp>

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QVariant>

#include <stdexcept>
#include <string>

namespace
{
QString toQString(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string toStdString(const QByteArray &value)
{
    return std::string(value.constData(), static_cast<std::size_t>(value.size()));
}

std::string toStdString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QNetworkRequest makeRequest(const std::string &urlText)
{
    const QUrl url(toQString(urlText));
    if (!url.isValid()
        || (url.scheme() != QStringLiteral("http")
            && url.scheme() != QStringLiteral("https"))) {
        throw std::runtime_error("HTTP requests require a valid http or https URL.");
    }

    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("mipsie/%1")
                          .arg(QCoreApplication::applicationVersion()));
    return request;
}

void attachResponseCallback(sol::state &lua, QNetworkAccessManager *manager,
                            QNetworkReply *reply, sol::protected_function callback,
                            const std::function<void(const QString &)> &output)
{
    QPointer<QNetworkReply> guardedReply(reply);
    QObject::connect(reply, &QNetworkReply::finished, manager,
                     [&lua, output, guardedReply,
                      callback = std::move(callback)]() mutable {
        if (!guardedReply)
            return;

        QNetworkReply *finishedReply = guardedReply.data();
        sol::table response = lua.create_table();
        const QVariant statusAttribute = finishedReply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute);
        const int status = statusAttribute.isValid() ? statusAttribute.toInt() : 0;
        const QNetworkReply::NetworkError networkError = finishedReply->error();
        response["status"] = status;
        response["body"] = toStdString(finishedReply->readAll());
        response["error"] = networkError == QNetworkReply::NoError
            ? std::string() : toStdString(finishedReply->errorString());
        response["ok"] = networkError == QNetworkReply::NoError
                          && status >= 200 && status < 300;

        sol::protected_function_result result = callback(response);
        if (!result.valid()) {
            sol::error error = result;
            output(QStringLiteral("[Lua Error] HTTP callback: %1")
                       .arg(QString::fromUtf8(error.what())));
        }
        finishedReply->deleteLater();
    });
}
}

void bindHttp(sol::state &lua, QNetworkAccessManager *networkManager,
              const std::function<void(const QString &)> &output)
{
    QPointer<QNetworkAccessManager> manager(networkManager);
    sol::table mipsie = lua["mipsie"];
    sol::table http = lua.create_table();

    http.set_function("get", [&lua, manager, output](const std::string &urlText,
                                                       sol::protected_function callback) {
        if (!manager)
            throw std::runtime_error("The HTTP service is unavailable.");
        if (!callback.valid())
            throw std::runtime_error("mipsie.http.get requires a callback function.");

        QNetworkReply *reply = manager->get(makeRequest(urlText));
        attachResponseCallback(lua, manager, reply, std::move(callback), output);
    });

    http.set_function("post", [&lua, manager, output](const std::string &urlText,
                                                        const std::string &body,
                                                        sol::protected_function callback) {
        if (!manager)
            throw std::runtime_error("The HTTP service is unavailable.");
        if (!callback.valid())
            throw std::runtime_error("mipsie.http.post requires a callback function.");

        QNetworkRequest request = makeRequest(urlText);
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json"));
        QNetworkReply *reply = manager->post(
            request, QByteArray(body.data(), static_cast<qsizetype>(body.size())));
        attachResponseCallback(lua, manager, reply, std::move(callback), output);
    });

    http.set_function("post_as", [&lua, manager, output](const std::string &urlText,
                                                          const std::string &body,
                                                          const std::string &contentType,
                                                          sol::protected_function callback) {
        if (!manager)
            throw std::runtime_error("The HTTP service is unavailable.");
        if (!callback.valid())
            throw std::runtime_error("mipsie.http.post_as requires a callback function.");

        QNetworkRequest request = makeRequest(urlText);
        request.setHeader(QNetworkRequest::ContentTypeHeader, toQString(contentType));
        QNetworkReply *reply = manager->post(
            request, QByteArray(body.data(), static_cast<qsizetype>(body.size())));
        attachResponseCallback(lua, manager, reply, std::move(callback), output);
    });

    mipsie["http"] = http;
}
