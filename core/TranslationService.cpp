#include "TranslationService.h"

#include <QEventLoop>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>
#include <QUrl>

namespace {
QStringList translationEndpoints()
{
    QStringList endpoints;
    const QString configuredEndpoint = qEnvironmentVariable("LIBRETRANSLATE_URL");
    if (!configuredEndpoint.trimmed().isEmpty()) {
        endpoints << configuredEndpoint.trimmed();
    }

    endpoints << QStringLiteral("https://translate.argosopentech.com/translate")
              << QStringLiteral("https://translate.libregalaxy.org/translate")
              << QStringLiteral("https://translate.fedilab.app/translate")
              << QStringLiteral("https://translate.cutie.dating/translate");

    return endpoints;
}

QString detectEndpointFor(const QString &translateEndpoint)
{
    if (translateEndpoint.endsWith(QStringLiteral("/translate"))) {
        QString endpoint = translateEndpoint;
        endpoint.chop(QStringLiteral("/translate").size());
        return endpoint + QStringLiteral("/detect");
    }

    return translateEndpoint + QStringLiteral("/detect");
}
}

QString TranslationService::detectLanguage(const QString &text)
{
    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty()) {
        return QString();
    }

    QJsonObject body;
    body.insert(QStringLiteral("q"), trimmedText);

    const QString apiKey = qEnvironmentVariable("LIBRETRANSLATE_API_KEY");
    if (!apiKey.trimmed().isEmpty()) {
        body.insert(QStringLiteral("api_key"), apiKey);
    }

    QNetworkAccessManager manager;

    for (const QString &translateEndpoint : translationEndpoints()) {
        const QString endpoint = detectEndpointFor(translateEndpoint);
        QUrl url(endpoint);
        if (!url.isValid()) {
            continue;
        }

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json; charset=utf-8"));

        QNetworkReply *reply = manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);

        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

        timeout.start(10000);
        loop.exec();

        if (!timeout.isActive()) {
            reply->abort();
            reply->deleteLater();
            continue;
        }

        timeout.stop();

        const QByteArray responseData = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        reply->deleteLater();

        if (networkError != QNetworkReply::NoError) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseData, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            continue;
        }

        QString language;
        if (document.isArray()) {
            const QJsonArray detections = document.array();
            if (!detections.isEmpty()) {
                language = detections.first().toObject().value(QStringLiteral("language")).toString();
            }
        } else if (document.isObject()) {
            language = document.object().value(QStringLiteral("language")).toString();
        }

        if (!language.trimmed().isEmpty()) {
            return language;
        }
    }

    return QString();
}

QString TranslationService::translateText(const QString &text,
                                          const QString &sourceLanguage,
                                          const QString &targetLanguage)
{
    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty()) {
        return QCoreApplication::translate("TranslationService", "Falha: não há texto para traduzir.");
    }

    if (sourceLanguage == targetLanguage) {
        return text;
    }

    QJsonObject body;
    body.insert(QStringLiteral("q"), text);
    body.insert(QStringLiteral("source"), sourceLanguage);
    body.insert(QStringLiteral("target"), targetLanguage);
    body.insert(QStringLiteral("format"), QStringLiteral("text"));

    const QString apiKey = qEnvironmentVariable("LIBRETRANSLATE_API_KEY");
    if (!apiKey.trimmed().isEmpty()) {
        body.insert(QStringLiteral("api_key"), apiKey);
    }

    QNetworkAccessManager manager;
    QStringList errors;

    for (const QString &endpoint : translationEndpoints()) {
        QUrl url(endpoint);
        if (!url.isValid()) {
            errors << QCoreApplication::translate("TranslationService", "%1: URL inválida").arg(endpoint);
            continue;
        }

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json; charset=utf-8"));

        QNetworkReply *reply = manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);

        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

        timeout.start(20000);
        loop.exec();

        if (!timeout.isActive()) {
            reply->abort();
            reply->deleteLater();
            errors << QCoreApplication::translate("TranslationService", "%1: tempo limite atingido").arg(endpoint);
            continue;
        }

        timeout.stop();

        const QByteArray responseData = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        reply->deleteLater();

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseData, &parseError);

        if (networkError != QNetworkReply::NoError) {
            QString apiMessage;
            if (parseError.error == QJsonParseError::NoError) {
                const QJsonValue errorValue = document.object().value(QStringLiteral("error"));
                apiMessage = errorValue.isObject()
                                 ? errorValue.toObject().value(QStringLiteral("message")).toString()
                                 : errorValue.toString();
            }

            errors << endpoint + QStringLiteral(": ")
                          + (!apiMessage.isEmpty() ? apiMessage : networkErrorText);
            continue;
        }

        if (parseError.error != QJsonParseError::NoError) {
            errors << QCoreApplication::translate("TranslationService",
                                                  "%1: resposta inválida - %2")
                          .arg(endpoint, parseError.errorString());
            continue;
        }

        const QJsonObject response = document.object();
        const QJsonValue errorValue = response.value(QStringLiteral("error"));
        if (!errorValue.isUndefined()) {
            const QString apiMessage = errorValue.isObject()
                                           ? errorValue.toObject().value(QStringLiteral("message")).toString()
                                           : errorValue.toString();
            errors << endpoint + QStringLiteral(": ") + apiMessage;
            continue;
        }

        const QString translatedText = response.value(QStringLiteral("translatedText")).toString();
        if (translatedText.trimmed().isEmpty()) {
            errors << QCoreApplication::translate("TranslationService", "%1: tradução vazia").arg(endpoint);
            continue;
        }

        return translatedText;
    }

    if (errors.isEmpty()) {
        return QCoreApplication::translate("TranslationService",
                                           "Erro ao traduzir: nenhum tradutor disponível.");
    }

    return QCoreApplication::translate("TranslationService",
                                       "Erro ao traduzir: nenhum tradutor gratuito respondeu. %1")
        .arg(errors.join(QStringLiteral(" | ")));
}
