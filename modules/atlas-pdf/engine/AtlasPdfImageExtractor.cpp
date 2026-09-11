#include "AtlasPdfImageExtractor.h"

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSizeF>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <utility>
#include <zlib.h>

namespace {

struct PdfObject {
    int number = -1;
    QByteArray body;
};

struct PdfMatrix {
    qreal a = 1.0;
    qreal b = 0.0;
    qreal c = 0.0;
    qreal d = 1.0;
    qreal e = 0.0;
    qreal f = 0.0;
};

struct PdfImageObject {
    QString name;
    int objectNumber = -1;
    int width = 0;
    int height = 0;
    QString filter;
    QByteArray streamData;
};

QVector<PdfObject> parseObjects(const QByteArray &pdfData)
{
    QVector<PdfObject> objects;
    qsizetype searchFrom = 0;

    while (true) {
        const qsizetype objPos = pdfData.indexOf(" obj", searchFrom);
        if (objPos < 0) {
            break;
        }

        qsizetype lineStart = pdfData.lastIndexOf('\n', objPos);
        lineStart = lineStart < 0 ? 0 : lineStart + 1;
        const QByteArray header = pdfData.mid(lineStart, objPos - lineStart).trimmed();
        const QList<QByteArray> parts = header.split(' ');
        if (parts.size() < 2) {
            searchFrom = objPos + 4;
            continue;
        }

        bool ok = false;
        const int objectNumber = parts.at(parts.size() - 2).toInt(&ok);
        if (!ok) {
            searchFrom = objPos + 4;
            continue;
        }

        const qsizetype bodyStart = objPos + 4;
        const qsizetype bodyEnd = pdfData.indexOf("endobj", bodyStart);
        if (bodyEnd < 0) {
            break;
        }

        objects.push_back({objectNumber, pdfData.mid(bodyStart, bodyEnd - bodyStart)});
        searchFrom = bodyEnd + 6;
    }

    return objects;
}

QHash<int, QByteArray> objectMap(const QVector<PdfObject> &objects)
{
    QHash<int, QByteArray> map;
    for (const PdfObject &object : objects) {
        map.insert(object.number, object.body);
    }
    return map;
}

QByteArray objectStream(const QByteArray &objectBody)
{
    const qsizetype streamPos = objectBody.indexOf("stream");
    const qsizetype endStreamPos = objectBody.indexOf("endstream", streamPos + 6);
    if (streamPos < 0 || endStreamPos < 0) {
        return {};
    }

    qsizetype dataStart = streamPos + 6;
    if (objectBody.mid(dataStart, 2) == "\r\n") {
        dataStart += 2;
    } else if (objectBody.mid(dataStart, 1) == "\n" || objectBody.mid(dataStart, 1) == "\r") {
        dataStart += 1;
    }

    qsizetype dataEnd = endStreamPos;
    while (dataEnd > dataStart && (objectBody.at(dataEnd - 1) == '\n' || objectBody.at(dataEnd - 1) == '\r')) {
        --dataEnd;
    }

    return objectBody.mid(dataStart, dataEnd - dataStart);
}

QByteArray inflateStream(const QByteArray &compressed)
{
    if (compressed.isEmpty()) {
        return {};
    }

    z_stream stream {};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());

    if (inflateInit(&stream) != Z_OK) {
        return {};
    }

    QByteArray output;
    char buffer[16384];
    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        result = inflate(&stream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END) {
            inflateEnd(&stream);
            return {};
        }
        output.append(buffer, sizeof(buffer) - stream.avail_out);
    }

    inflateEnd(&stream);
    return output;
}

QString objectDictionaryText(const QByteArray &objectBody)
{
    const qsizetype streamPos = objectBody.indexOf("stream");
    const QByteArray dictionary = streamPos >= 0 ? objectBody.left(streamPos) : objectBody;
    return QString::fromLatin1(dictionary);
}

int firstIntMatch(const QString &text, const QString &pattern, int fallback = 0)
{
    const QRegularExpression expression(pattern);
    const QRegularExpressionMatch match = expression.match(text);
    return match.hasMatch() ? match.captured(1).toInt() : fallback;
}

QString firstNameMatch(const QString &text, const QString &pattern)
{
    const QRegularExpression expression(pattern);
    const QRegularExpressionMatch match = expression.match(text);
    return match.hasMatch() ? match.captured(1) : QString();
}

QByteArray decodedContentStream(const QByteArray &objectBody)
{
    const QString dictionary = objectDictionaryText(objectBody);
    const QByteArray streamData = objectStream(objectBody);
    if (dictionary.contains(QStringLiteral("/FlateDecode"))) {
        return inflateStream(streamData);
    }
    return streamData;
}

PdfMatrix multiply(const PdfMatrix &left, const PdfMatrix &right)
{
    return {
        left.a * right.a + left.c * right.b,
        left.b * right.a + left.d * right.b,
        left.a * right.c + left.c * right.d,
        left.b * right.c + left.d * right.d,
        left.a * right.e + left.c * right.f + left.e,
        left.b * right.e + left.d * right.f + left.f,
    };
}

QPointF transformPoint(const PdfMatrix &matrix, qreal x, qreal y)
{
    return {matrix.a * x + matrix.c * y + matrix.e, matrix.b * x + matrix.d * y + matrix.f};
}

QRectF imageRectFromMatrix(const PdfMatrix &matrix, qreal pageHeight)
{
    const QVector<QPointF> points = {
        transformPoint(matrix, 0.0, 0.0),
        transformPoint(matrix, 1.0, 0.0),
        transformPoint(matrix, 0.0, 1.0),
        transformPoint(matrix, 1.0, 1.0),
    };

    qreal minX = points.first().x();
    qreal maxX = minX;
    qreal minY = points.first().y();
    qreal maxY = minY;
    for (const QPointF &point : points) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    return QRectF(QPointF(minX, pageHeight - maxY), QPointF(maxX, pageHeight - minY)).normalized();
}

QVector<QString> contentTokens(const QByteArray &content)
{
    QVector<QString> tokens;
    const QString text = QString::fromLatin1(content);
    const QRegularExpression expression(QStringLiteral(R"((/[A-Za-z0-9_.-]+|-?(?:\d+\.?\d*|\.\d+)|q|Q|cm|Do))"));
    QRegularExpressionMatchIterator it = expression.globalMatch(text);
    while (it.hasNext()) {
        tokens.push_back(it.next().captured(1));
    }
    return tokens;
}

QVector<QPair<QString, PdfMatrix>> imageDraws(const QByteArray &content)
{
    QVector<QPair<QString, PdfMatrix>> draws;
    QVector<PdfMatrix> stack;
    PdfMatrix current;
    QVector<qreal> numbers;
    QString lastName;

    for (const QString &token : contentTokens(content)) {
        if (token == QStringLiteral("q")) {
            stack.push_back(current);
            numbers.clear();
            lastName.clear();
            continue;
        }

        if (token == QStringLiteral("Q")) {
            if (!stack.isEmpty()) {
                current = stack.takeLast();
            }
            numbers.clear();
            lastName.clear();
            continue;
        }

        if (token == QStringLiteral("cm")) {
            if (numbers.size() >= 6) {
                const int offset = numbers.size() - 6;
                const PdfMatrix matrix {numbers.at(offset),
                                        numbers.at(offset + 1),
                                        numbers.at(offset + 2),
                                        numbers.at(offset + 3),
                                        numbers.at(offset + 4),
                                        numbers.at(offset + 5)};
                current = multiply(current, matrix);
            }
            numbers.clear();
            lastName.clear();
            continue;
        }

        if (token == QStringLiteral("Do")) {
            if (!lastName.isEmpty()) {
                draws.push_back({lastName.mid(1), current});
            }
            numbers.clear();
            lastName.clear();
            continue;
        }

        if (token.startsWith(QLatin1Char('/'))) {
            lastName = token;
            continue;
        }

        bool ok = false;
        const qreal value = token.toDouble(&ok);
        if (ok) {
            numbers.push_back(value);
            if (numbers.size() > 12) {
                numbers.remove(0, numbers.size() - 12);
            }
        }
    }

    return draws;
}

QVector<int> pageObjectNumbers(const QVector<PdfObject> &objects)
{
    QVector<int> pages;
    for (const PdfObject &object : objects) {
        const QString dictionary = objectDictionaryText(object.body);
        if (dictionary.contains(QStringLiteral("/Type /Page"))
            && !dictionary.contains(QStringLiteral("/Type /Pages"))) {
            pages.push_back(object.number);
        }
    }
    return pages;
}

qreal pageHeightFromBody(const QByteArray &pageBody)
{
    const QString dictionary = objectDictionaryText(pageBody);
    const QRegularExpression mediaBoxExpression(
        QStringLiteral(R"(/MediaBox\s*\[\s*-?[0-9.]+\s+-?[0-9.]+\s+-?[0-9.]+\s+(-?[0-9.]+)\s*\])"));
    const QRegularExpressionMatch match = mediaBoxExpression.match(dictionary);
    return match.hasMatch() ? match.captured(1).toDouble() : 842.0;
}

QSizeF pageSizeFromBody(const QByteArray &pageBody)
{
    const QString dictionary = objectDictionaryText(pageBody);
    const QRegularExpression mediaBoxExpression(
        QStringLiteral(R"(/MediaBox\s*\[\s*(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)\s*\])"));
    const QRegularExpressionMatch match = mediaBoxExpression.match(dictionary);
    if (!match.hasMatch()) {
        return QSizeF(595.0, 842.0);
    }

    const qreal left = match.captured(1).toDouble();
    const qreal bottom = match.captured(2).toDouble();
    const qreal right = match.captured(3).toDouble();
    const qreal top = match.captured(4).toDouble();
    return QSizeF(std::abs(right - left), std::abs(top - bottom));
}

int indirectObjectNumber(const QString &text, const QString &key)
{
    const QRegularExpression expression(QStringLiteral(R"(%1\s+(\d+)\s+\d+\s+R)").arg(key));
    const QRegularExpressionMatch match = expression.match(text);
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

int normalizedRotation(int rotation)
{
    rotation %= 360;
    if (rotation < 0) {
        rotation += 360;
    }
    if (rotation == 90 || rotation == 180 || rotation == 270) {
        return rotation;
    }
    return 0;
}

int directRotationDegrees(const QByteArray &objectBody)
{
    const QString dictionary = objectDictionaryText(objectBody);
    const QRegularExpression expression(QStringLiteral(R"(/Rotate\s+(-?\d+))"));
    const QRegularExpressionMatch match = expression.match(dictionary);
    return match.hasMatch() ? normalizedRotation(match.captured(1).toInt()) : -1;
}

int inheritedRotationDegrees(const QByteArray &pageBody, const QHash<int, QByteArray> &objectsByNumber)
{
    const int pageRotation = directRotationDegrees(pageBody);
    if (pageRotation >= 0) {
        return pageRotation;
    }

    QString dictionary = objectDictionaryText(pageBody);
    int parentObjectNumber = indirectObjectNumber(dictionary, QStringLiteral("/Parent"));
    QSet<int> visited;
    while (parentObjectNumber > 0 && objectsByNumber.contains(parentObjectNumber)
           && !visited.contains(parentObjectNumber)) {
        visited.insert(parentObjectNumber);
        const QByteArray parentBody = objectsByNumber.value(parentObjectNumber);
        const int parentRotation = directRotationDegrees(parentBody);
        if (parentRotation >= 0) {
            return parentRotation;
        }

        dictionary = objectDictionaryText(parentBody);
        parentObjectNumber = indirectObjectNumber(dictionary, QStringLiteral("/Parent"));
    }

    return 0;
}

QRectF rotatePageRect(const QRectF &rect, const QSizeF &unrotatedPageSize, int rotationDegrees)
{
    if (rect.isEmpty() || unrotatedPageSize.isEmpty()) {
        return rect;
    }

    const int rotation = normalizedRotation(rotationDegrees);
    if (rotation == 0) {
        return rect;
    }

    auto rotatePoint = [unrotatedPageSize, rotation](const QPointF &point) {
        switch (rotation) {
        case 90:
            return QPointF(unrotatedPageSize.height() - point.y(), point.x());
        case 180:
            return QPointF(unrotatedPageSize.width() - point.x(), unrotatedPageSize.height() - point.y());
        case 270:
            return QPointF(point.y(), unrotatedPageSize.width() - point.x());
        default:
            return point;
        }
    };

    const QVector<QPointF> rotatedPoints = {
        rotatePoint(rect.topLeft()),
        rotatePoint(rect.topRight()),
        rotatePoint(rect.bottomLeft()),
        rotatePoint(rect.bottomRight()),
    };

    qreal minX = rotatedPoints.first().x();
    qreal maxX = minX;
    qreal minY = rotatedPoints.first().y();
    qreal maxY = minY;
    for (const QPointF &point : rotatedPoints) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}

QHash<QString, int> xObjectMap(const QByteArray &pageBody, const QHash<int, QByteArray> &objectsByNumber)
{
    QString resourcesText = objectDictionaryText(pageBody);
    const int resourcesObjectNumber = indirectObjectNumber(resourcesText, QStringLiteral("/Resources"));
    if (resourcesObjectNumber > 0 && objectsByNumber.contains(resourcesObjectNumber)) {
        resourcesText = objectDictionaryText(objectsByNumber.value(resourcesObjectNumber));
    }

    const int xObjectDictionaryNumber = indirectObjectNumber(resourcesText, QStringLiteral("/XObject"));
    if (xObjectDictionaryNumber > 0 && objectsByNumber.contains(xObjectDictionaryNumber)) {
        resourcesText = objectDictionaryText(objectsByNumber.value(xObjectDictionaryNumber));
    }

    QHash<QString, int> xobjects;
    const QRegularExpression xObjectExpression(QStringLiteral(R"(/(R\d+)\s+(\d+)\s+\d+\s+R)"));
    QRegularExpressionMatchIterator it = xObjectExpression.globalMatch(resourcesText);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        xobjects.insert(match.captured(1), match.captured(2).toInt());
    }
    return xobjects;
}

QVector<int> contentObjectNumbers(const QByteArray &pageBody)
{
    QVector<int> contents;
    const QString dictionary = objectDictionaryText(pageBody);
    const QRegularExpression arrayExpression(QStringLiteral(R"(/Contents\s*\[(.*?)\])"),
                                             QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch arrayMatch = arrayExpression.match(dictionary);
    if (arrayMatch.hasMatch()) {
        const QRegularExpression refExpression(QStringLiteral(R"((\d+)\s+\d+\s+R)"));
        QRegularExpressionMatchIterator it = refExpression.globalMatch(arrayMatch.captured(1));
        while (it.hasNext()) {
            contents.push_back(it.next().captured(1).toInt());
        }
        return contents;
    }

    const int contentObjectNumber = indirectObjectNumber(dictionary, QStringLiteral("/Contents"));
    if (contentObjectNumber > 0) {
        contents.push_back(contentObjectNumber);
    }
    return contents;
}

PdfImageObject imageObject(const QString &name, int objectNumber, const QByteArray &body)
{
    const QString dictionary = objectDictionaryText(body);
    PdfImageObject image;
    image.name = name;
    image.objectNumber = objectNumber;
    if (!dictionary.contains(QStringLiteral("/Subtype /Image"))) {
        return image;
    }

    image.width = firstIntMatch(dictionary, QStringLiteral(R"(/Width\s+(\d+))"));
    image.height = firstIntMatch(dictionary, QStringLiteral(R"(/Height\s+(\d+))"));
    image.filter = firstNameMatch(dictionary, QStringLiteral(R"(/Filter\s*/([A-Za-z0-9]+))"));
    image.streamData = objectStream(body);
    return image;
}

QImage decodeImage(const PdfImageObject &image)
{
    if (image.filter == QStringLiteral("DCTDecode")) {
        return QImage::fromData(image.streamData, "JPG");
    }

    if (image.filter == QStringLiteral("FlateDecode") && image.width > 0 && image.height > 0) {
        const QByteArray pixels = inflateStream(image.streamData);
        if (pixels.size() >= image.width * image.height * 3) {
            QImage decoded(reinterpret_cast<const uchar *>(pixels.constData()),
                           image.width,
                           image.height,
                           image.width * 3,
                           QImage::Format_RGB888);
            return decoded.copy();
        }
    }

    return {};
}

} // namespace

AtlasPdfImageExtractor::AtlasPdfImageExtractor(QString filePath)
    : m_filePath(std::move(filePath))
{
}

QVector<AtlasPdfImageExtractor::PdfImage> AtlasPdfImageExtractor::pageImages(int pageIndex) const
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QByteArray pdfData = file.readAll();
    const QVector<PdfObject> objects = parseObjects(pdfData);
    const QHash<int, QByteArray> objectsByNumber = objectMap(objects);
    const QVector<int> pages = pageObjectNumbers(objects);
    if (pageIndex < 0 || pageIndex >= pages.size()) {
        return {};
    }

    const QByteArray pageBody = objectsByNumber.value(pages.at(pageIndex));
    const QSizeF unrotatedPageSize = pageSizeFromBody(pageBody);
    const qreal pageHeight = pageHeightFromBody(pageBody);
    const int rotationDegrees = inheritedRotationDegrees(pageBody, objectsByNumber);
    const QHash<QString, int> xobjects = xObjectMap(pageBody, objectsByNumber);

    QHash<QString, PdfImageObject> images;
    for (auto it = xobjects.cbegin(); it != xobjects.cend(); ++it) {
        const PdfImageObject image = imageObject(it.key(), it.value(), objectsByNumber.value(it.value()));
        if (!image.streamData.isEmpty()) {
            images.insert(it.key(), image);
        }
    }

    QByteArray content;
    for (int contentObjectNumber : contentObjectNumbers(pageBody)) {
        content += decodedContentStream(objectsByNumber.value(contentObjectNumber));
        content += '\n';
    }

    QVector<PdfImage> pageImages;
    for (const auto &draw : imageDraws(content)) {
        const PdfImageObject image = images.value(draw.first);
        if (image.streamData.isEmpty()) {
            continue;
        }

        const QImage decoded = decodeImage(image);
        if (decoded.isNull()) {
            continue;
        }

        const QRectF unrotatedRect = imageRectFromMatrix(draw.second, pageHeight);
        pageImages.push_back({pageIndex,
                              draw.first,
                              rotatePageRect(unrotatedRect, unrotatedPageSize, rotationDegrees),
                              decoded});
    }

    return pageImages;
}

bool AtlasPdfImageExtractor::hasAnyPageImage(int pageCount) const
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray pdfData = file.readAll();
    const QVector<PdfObject> objects = parseObjects(pdfData);
    const QHash<int, QByteArray> objectsByNumber = objectMap(objects);
    const QVector<int> pages = pageObjectNumbers(objects);
    const int pagesToCheck = pageCount > 0 ? std::min(pageCount, static_cast<int>(pages.size()))
                                           : static_cast<int>(pages.size());

    for (int pageIndex = 0; pageIndex < pagesToCheck; ++pageIndex) {
        const QByteArray pageBody = objectsByNumber.value(pages.at(pageIndex));
        const QHash<QString, int> xobjects = xObjectMap(pageBody, objectsByNumber);
        if (xobjects.isEmpty()) {
            continue;
        }

        QByteArray content;
        for (int contentObjectNumber : contentObjectNumbers(pageBody)) {
            content += decodedContentStream(objectsByNumber.value(contentObjectNumber));
            content += '\n';
        }

        for (const auto &draw : imageDraws(content)) {
            const int objectNumber = xobjects.value(draw.first, -1);
            if (objectNumber < 0) {
                continue;
            }

            if (objectDictionaryText(objectsByNumber.value(objectNumber)).contains(QStringLiteral("/Subtype /Image"))) {
                return true;
            }
        }
    }

    return false;
}

int AtlasPdfImageExtractor::pageRotationDegrees(int pageIndex) const
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }

    const QByteArray pdfData = file.readAll();
    const QVector<PdfObject> objects = parseObjects(pdfData);
    const QHash<int, QByteArray> objectsByNumber = objectMap(objects);
    const QVector<int> pages = pageObjectNumbers(objects);
    if (pageIndex < 0 || pageIndex >= pages.size()) {
        return 0;
    }

    return inheritedRotationDegrees(objectsByNumber.value(pages.at(pageIndex)), objectsByNumber);
}
