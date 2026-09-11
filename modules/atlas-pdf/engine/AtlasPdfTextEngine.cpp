#include "AtlasPdfTextEngine.h"

#include <QPdfDocument>
#include <QPdfSelection>

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>
#include <zlib.h>

namespace {

struct PdfObject {
    int number = -1;
    QByteArray body;
};

struct PdfFontMap {
    QString name;
    QHash<int, QString> codeToUnicode;
    int maxCodeBytes = 1;
};

struct PdfTextToken {
    enum class Kind { Name, Number, String, HexString, Operator, ArrayStart, ArrayEnd };
    Kind kind = Kind::Operator;
    QString text;
    QByteArray bytes;
};

QVector<PdfTextToken> tokenizeContent(const QByteArray &content);

struct PdfPageTextProfile {
    bool hasContentText = false;
    bool hasActualText = false;
    bool hasToUnicode = false;
    bool hasType3Font = false;
    bool hasNumericGlyphNames = false;
    bool hasNamedGlyphUnicode = false;
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
        const QList<QByteArray> headerParts = pdfData.mid(lineStart, objPos - lineStart).trimmed().split(' ');
        if (headerParts.size() < 2) {
            searchFrom = objPos + 4;
            continue;
        }

        bool ok = false;
        const int objectNumber = headerParts.at(headerParts.size() - 2).toInt(&ok);
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
    return QString::fromLatin1(streamPos >= 0 ? objectBody.left(streamPos) : objectBody);
}

QByteArray decodedStream(const QByteArray &objectBody)
{
    const QString dictionary = objectDictionaryText(objectBody);
    const QByteArray streamData = objectStream(objectBody);
    if (dictionary.contains(QStringLiteral("/FlateDecode"))) {
        return inflateStream(streamData);
    }
    return streamData;
}

int indirectObjectNumber(const QString &text, const QString &key)
{
    const QRegularExpression expression(QStringLiteral(R"(%1\s+(\d+)\s+\d+\s+R)").arg(key));
    const QRegularExpressionMatch match = expression.match(text);
    return match.hasMatch() ? match.captured(1).toInt() : -1;
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

QString resourcesTextForPage(const QByteArray &pageBody, const QHash<int, QByteArray> &objectsByNumber)
{
    QString resourcesText = objectDictionaryText(pageBody);
    const int resourcesObjectNumber = indirectObjectNumber(resourcesText, QStringLiteral("/Resources"));
    if (resourcesObjectNumber > 0 && objectsByNumber.contains(resourcesObjectNumber)) {
        resourcesText = objectDictionaryText(objectsByNumber.value(resourcesObjectNumber));
    }
    return resourcesText;
}

QString glyphNameToUnicode(const QString &name)
{
    static const QHash<QString, QString> glyphs = {
        {QStringLiteral("space"), QStringLiteral(" ")},
        {QStringLiteral("exclam"), QStringLiteral("!")},
        {QStringLiteral("quotedbl"), QStringLiteral("\"")},
        {QStringLiteral("numbersign"), QStringLiteral("#")},
        {QStringLiteral("dollar"), QStringLiteral("$")},
        {QStringLiteral("percent"), QStringLiteral("%")},
        {QStringLiteral("ampersand"), QStringLiteral("&")},
        {QStringLiteral("quotesingle"), QStringLiteral("'")},
        {QStringLiteral("parenleft"), QStringLiteral("(")},
        {QStringLiteral("parenright"), QStringLiteral(")")},
        {QStringLiteral("asterisk"), QStringLiteral("*")},
        {QStringLiteral("plus"), QStringLiteral("+")},
        {QStringLiteral("comma"), QStringLiteral(",")},
        {QStringLiteral("hyphen"), QStringLiteral("-")},
        {QStringLiteral("minus"), QStringLiteral("-")},
        {QStringLiteral("period"), QStringLiteral(".")},
        {QStringLiteral("slash"), QStringLiteral("/")},
        {QStringLiteral("colon"), QStringLiteral(":")},
        {QStringLiteral("semicolon"), QStringLiteral(";")},
        {QStringLiteral("less"), QStringLiteral("<")},
        {QStringLiteral("equal"), QStringLiteral("=")},
        {QStringLiteral("greater"), QStringLiteral(">")},
        {QStringLiteral("question"), QStringLiteral("?")},
        {QStringLiteral("at"), QStringLiteral("@")},
        {QStringLiteral("bracketleft"), QStringLiteral("[")},
        {QStringLiteral("backslash"), QStringLiteral("\\")},
        {QStringLiteral("bracketright"), QStringLiteral("]")},
        {QStringLiteral("underscore"), QStringLiteral("_")},
        {QStringLiteral("braceleft"), QStringLiteral("{")},
        {QStringLiteral("bar"), QStringLiteral("|")},
        {QStringLiteral("braceright"), QStringLiteral("}")},
        {QStringLiteral("ccedilla"), QStringLiteral("ç")},
        {QStringLiteral("Ccedilla"), QStringLiteral("Ç")},
        {QStringLiteral("atilde"), QStringLiteral("ã")},
        {QStringLiteral("Atilde"), QStringLiteral("Ã")},
        {QStringLiteral("otilde"), QStringLiteral("õ")},
        {QStringLiteral("Otilde"), QStringLiteral("Õ")},
        {QStringLiteral("aacute"), QStringLiteral("á")},
        {QStringLiteral("Aacute"), QStringLiteral("Á")},
        {QStringLiteral("eacute"), QStringLiteral("é")},
        {QStringLiteral("Eacute"), QStringLiteral("É")},
        {QStringLiteral("iacute"), QStringLiteral("í")},
        {QStringLiteral("Iacute"), QStringLiteral("Í")},
        {QStringLiteral("oacute"), QStringLiteral("ó")},
        {QStringLiteral("Oacute"), QStringLiteral("Ó")},
        {QStringLiteral("uacute"), QStringLiteral("ú")},
        {QStringLiteral("Uacute"), QStringLiteral("Ú")},
        {QStringLiteral("acircumflex"), QStringLiteral("â")},
        {QStringLiteral("Acircumflex"), QStringLiteral("Â")},
        {QStringLiteral("ecircumflex"), QStringLiteral("ê")},
        {QStringLiteral("Ecircumflex"), QStringLiteral("Ê")},
        {QStringLiteral("ocircumflex"), QStringLiteral("ô")},
        {QStringLiteral("Ocircumflex"), QStringLiteral("Ô")},
        {QStringLiteral("agrave"), QStringLiteral("à")},
        {QStringLiteral("Agrave"), QStringLiteral("À")},
    };

    if (name.size() == 1 && name.at(0).isLetterOrNumber()) {
        return name;
    }

    if (name.startsWith(QLatin1String("uni")) && name.size() >= 7) {
        bool ok = false;
        const uint code = name.mid(3, 4).toUInt(&ok, 16);
        if (ok) {
            return QString(QChar(code));
        }
    }

    if (name.startsWith(QLatin1String("u")) && name.size() >= 5) {
        bool ok = false;
        const uint code = name.mid(1).toUInt(&ok, 16);
        if (ok) {
            return QString(QChar(code));
        }
    }

    return glyphs.value(name);
}

QByteArray hexToBytes(QString hex)
{
    hex.remove(QRegularExpression(QStringLiteral(R"(\s+)")));
    if (hex.size() % 2 != 0) {
        hex += QLatin1Char('0');
    }
    return QByteArray::fromHex(hex.toLatin1());
}

QString unicodeFromHex(QString hex)
{
    hex.remove(QRegularExpression(QStringLiteral(R"(\s+)")));
    QString result;
    for (int i = 0; i + 3 < hex.size(); i += 4) {
        bool ok = false;
        const uint code = hex.mid(i, 4).toUInt(&ok, 16);
        if (ok) {
            result += QChar(code);
        }
    }
    return result;
}

void applyToUnicodeCMap(PdfFontMap *font, const QByteArray &cmapData)
{
    if (!font || cmapData.isEmpty()) {
        return;
    }

    const QString cmap = QString::fromLatin1(cmapData);
    const QRegularExpression bfchar(QStringLiteral(R"(<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>)"));
    QRegularExpressionMatchIterator charIt = bfchar.globalMatch(cmap);
    while (charIt.hasNext()) {
        const QRegularExpressionMatch match = charIt.next();
        bool ok = false;
        const int code = match.captured(1).toInt(&ok, 16);
        if (ok) {
            font->codeToUnicode.insert(code, unicodeFromHex(match.captured(2)));
            font->maxCodeBytes = std::max(font->maxCodeBytes,
                                          static_cast<int>(match.captured(1).size() / 2));
        }
    }

    const QRegularExpression bfrange(
        QStringLiteral(R"(<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>)"));
    QRegularExpressionMatchIterator rangeIt = bfrange.globalMatch(cmap);
    while (rangeIt.hasNext()) {
        const QRegularExpressionMatch match = rangeIt.next();
        bool startOk = false;
        bool endOk = false;
        bool destOk = false;
        const int start = match.captured(1).toInt(&startOk, 16);
        const int end = match.captured(2).toInt(&endOk, 16);
        uint dest = match.captured(3).toUInt(&destOk, 16);
        if (!startOk || !endOk || !destOk || end < start || end - start > 512) {
            continue;
        }
        for (int code = start; code <= end; ++code) {
            font->codeToUnicode.insert(code, QString(QChar(dest++)));
        }
        font->maxCodeBytes = std::max(font->maxCodeBytes,
                                      static_cast<int>(match.captured(1).size() / 2));
    }
}

void applyDifferences(PdfFontMap *font, const QString &encodingText)
{
    if (!font) {
        return;
    }

    const QRegularExpression differencesExpression(QStringLiteral(R"(/Differences\s*\[(.*?)\])"),
                                                   QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch differences = differencesExpression.match(encodingText);
    if (!differences.hasMatch()) {
        return;
    }

    int code = -1;
    const QRegularExpression tokenExpression(QStringLiteral(R"((\d+)|/([A-Za-z0-9_.-]+))"));
    QRegularExpressionMatchIterator it = tokenExpression.globalMatch(differences.captured(1));
    while (it.hasNext()) {
        const QRegularExpressionMatch token = it.next();
        if (!token.captured(1).isEmpty()) {
            code = token.captured(1).toInt();
            continue;
        }

        const QString unicode = glyphNameToUnicode(token.captured(2));
        if (code >= 0 && !unicode.isEmpty()) {
            font->codeToUnicode.insert(code, unicode);
        }
        ++code;
    }
}

PdfFontMap buildFontMap(const QString &name,
                        const QByteArray &fontBody,
                        const QHash<int, QByteArray> &objectsByNumber)
{
    PdfFontMap font;
    font.name = name;

    for (int code = 32; code <= 126; ++code) {
        font.codeToUnicode.insert(code, QString(QChar(code)));
    }

    QString fontText = objectDictionaryText(fontBody);
    const int encodingObjectNumber = indirectObjectNumber(fontText, QStringLiteral("/Encoding"));
    if (encodingObjectNumber > 0 && objectsByNumber.contains(encodingObjectNumber)) {
        fontText += QLatin1Char('\n') + objectDictionaryText(objectsByNumber.value(encodingObjectNumber));
    }
    applyDifferences(&font, fontText);

    const int toUnicodeObjectNumber = indirectObjectNumber(fontText, QStringLiteral("/ToUnicode"));
    if (toUnicodeObjectNumber > 0 && objectsByNumber.contains(toUnicodeObjectNumber)) {
        applyToUnicodeCMap(&font, decodedStream(objectsByNumber.value(toUnicodeObjectNumber)));
    }

    font.maxCodeBytes = std::clamp(font.maxCodeBytes, 1, 4);
    return font;
}

QHash<QString, PdfFontMap> pageFonts(const QByteArray &pageBody, const QHash<int, QByteArray> &objectsByNumber)
{
    const QString resourcesText = resourcesTextForPage(pageBody, objectsByNumber);
    QString fontText = resourcesText;
    const int fontObjectNumber = indirectObjectNumber(resourcesText, QStringLiteral("/Font"));
    if (fontObjectNumber > 0 && objectsByNumber.contains(fontObjectNumber)) {
        fontText = objectDictionaryText(objectsByNumber.value(fontObjectNumber));
    }

    QHash<QString, PdfFontMap> fonts;
    const QRegularExpression fontRef(QStringLiteral(R"(/([A-Za-z0-9_.-]+)\s+(\d+)\s+\d+\s+R)"));
    QRegularExpressionMatchIterator it = fontRef.globalMatch(fontText);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1);
        const int objectNumber = match.captured(2).toInt();
        const QByteArray fontBody = objectsByNumber.value(objectNumber);
        if (fontBody.isEmpty() || !objectDictionaryText(fontBody).contains(QStringLiteral("/Font"))) {
            continue;
        }
        fonts.insert(name, buildFontMap(name, fontBody, objectsByNumber));
    }
    return fonts;
}

PdfPageTextProfile pageTextProfile(const QByteArray &pageBody, const QHash<int, QByteArray> &objectsByNumber)
{
    PdfPageTextProfile profile;

    const QString resourcesText = resourcesTextForPage(pageBody, objectsByNumber);
    QString fontText = resourcesText;
    const int fontObjectNumber = indirectObjectNumber(resourcesText, QStringLiteral("/Font"));
    if (fontObjectNumber > 0 && objectsByNumber.contains(fontObjectNumber)) {
        fontText = objectDictionaryText(objectsByNumber.value(fontObjectNumber));
    }

    const QRegularExpression fontRef(QStringLiteral(R"(/([A-Za-z0-9_.-]+)\s+(\d+)\s+\d+\s+R)"));
    QRegularExpressionMatchIterator fontIt = fontRef.globalMatch(fontText);
    while (fontIt.hasNext()) {
        const QRegularExpressionMatch match = fontIt.next();
        const QByteArray fontBody = objectsByNumber.value(match.captured(2).toInt());
        const QString dictionary = objectDictionaryText(fontBody);
        if (fontBody.isEmpty() || !dictionary.contains(QStringLiteral("/Font"))) {
            continue;
        }

        profile.hasType3Font = profile.hasType3Font || dictionary.contains(QStringLiteral("/Subtype /Type3"));
        profile.hasToUnicode = profile.hasToUnicode || dictionary.contains(QStringLiteral("/ToUnicode"));

        const int encodingObjectNumber = indirectObjectNumber(dictionary, QStringLiteral("/Encoding"));
        QString encodingText = dictionary;
        if (encodingObjectNumber > 0 && objectsByNumber.contains(encodingObjectNumber)) {
            encodingText += QLatin1Char('\n') + objectDictionaryText(objectsByNumber.value(encodingObjectNumber));
        }

        const QRegularExpression differencesExpression(QStringLiteral(R"(/Differences\s*\[(.*?)\])"),
                                                       QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch differences = differencesExpression.match(encodingText);
        if (!differences.hasMatch()) {
            continue;
        }

        const QRegularExpression glyphNameExpression(QStringLiteral(R"(/([A-Za-z0-9_.-]+))"));
        QRegularExpressionMatchIterator glyphIt = glyphNameExpression.globalMatch(differences.captured(1));
        while (glyphIt.hasNext()) {
            const QString glyphName = glyphIt.next().captured(1);
            bool numeric = false;
            glyphName.toInt(&numeric);
            if (numeric) {
                profile.hasNumericGlyphNames = true;
            } else if (glyphName != QStringLiteral(".notdef") && !glyphNameToUnicode(glyphName).isEmpty()) {
                profile.hasNamedGlyphUnicode = true;
            }
        }
    }

    QByteArray content;
    for (int contentObjectNumber : contentObjectNumbers(pageBody)) {
        content += decodedStream(objectsByNumber.value(contentObjectNumber));
        content += '\n';
    }

    profile.hasActualText = content.contains("/ActualText");
    const QVector<PdfTextToken> tokens = tokenizeContent(content);
    for (const PdfTextToken &token : tokens) {
        if (token.kind == PdfTextToken::Kind::Operator
            && (token.text == QStringLiteral("Tj") || token.text == QStringLiteral("TJ")
                || token.text == QStringLiteral("'") || token.text == QStringLiteral("\""))) {
            profile.hasContentText = true;
            break;
        }
    }

    return profile;
}

PdfPageTextProfile loadPageTextProfile(const QString &filePath, int pageIndex)
{
    PdfPageTextProfile profile;
    if (filePath.isEmpty() || pageIndex < 0) {
        return profile;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return profile;
    }

    const QByteArray pdfData = file.readAll();
    const QVector<PdfObject> objects = parseObjects(pdfData);
    const QHash<int, QByteArray> objectsByNumber = objectMap(objects);
    const QVector<int> pages = pageObjectNumbers(objects);
    if (pageIndex >= pages.size()) {
        return profile;
    }

    return pageTextProfile(objectsByNumber.value(pages.at(pageIndex)), objectsByNumber);
}

QByteArray parseLiteralString(const QByteArray &content, int *index)
{
    QByteArray result;
    int depth = 1;
    ++(*index);
    while (*index < content.size() && depth > 0) {
        char ch = content.at((*index)++);
        if (ch == '\\' && *index < content.size()) {
            const char escaped = content.at((*index)++);
            switch (escaped) {
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case '(':
            case ')':
            case '\\': result += escaped; break;
            default: result += escaped; break;
            }
            continue;
        }
        if (ch == '(') {
            ++depth;
            result += ch;
            continue;
        }
        if (ch == ')') {
            --depth;
            if (depth > 0) {
                result += ch;
            }
            continue;
        }
        result += ch;
    }
    return result;
}

QVector<PdfTextToken> tokenizeContent(const QByteArray &content)
{
    QVector<PdfTextToken> tokens;
    int i = 0;
    while (i < content.size()) {
        const char ch = content.at(i);
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ++i;
            continue;
        }
        if (ch == '%') {
            while (i < content.size() && content.at(i) != '\n' && content.at(i) != '\r') {
                ++i;
            }
            continue;
        }
        if (ch == '(') {
            tokens.push_back({PdfTextToken::Kind::String, QString(), parseLiteralString(content, &i)});
            continue;
        }
        if (ch == '<' && i + 1 < content.size() && content.at(i + 1) != '<') {
            const int start = ++i;
            while (i < content.size() && content.at(i) != '>') {
                ++i;
            }
            tokens.push_back({PdfTextToken::Kind::HexString,
                              QString(),
                              hexToBytes(QString::fromLatin1(content.mid(start, i - start)))});
            if (i < content.size()) {
                ++i;
            }
            continue;
        }
        if (ch == '[' || ch == ']') {
            tokens.push_back({ch == '[' ? PdfTextToken::Kind::ArrayStart : PdfTextToken::Kind::ArrayEnd,
                              QString(QChar::fromLatin1(ch)),
                              {}});
            ++i;
            continue;
        }
        if (ch == '/') {
            const int start = i++;
            while (i < content.size() && !std::isspace(static_cast<unsigned char>(content.at(i)))
                   && QByteArray("[]<>()/%").indexOf(content.at(i)) < 0) {
                ++i;
            }
            tokens.push_back({PdfTextToken::Kind::Name,
                              QString::fromLatin1(content.mid(start + 1, i - start - 1)),
                              {}});
            continue;
        }

        const int start = i++;
        while (i < content.size() && !std::isspace(static_cast<unsigned char>(content.at(i)))
               && QByteArray("[]<>()/%").indexOf(content.at(i)) < 0) {
            ++i;
        }
        const QString text = QString::fromLatin1(content.mid(start, i - start));
        bool isNumber = false;
        text.toDouble(&isNumber);
        tokens.push_back({isNumber ? PdfTextToken::Kind::Number : PdfTextToken::Kind::Operator, text, {}});
    }
    return tokens;
}

QString decodePdfBytes(const PdfFontMap &font, const QByteArray &bytes)
{
    QString text;
    for (int i = 0; i < bytes.size();) {
        QString mapped;
        int used = 0;
        for (int width = font.maxCodeBytes; width >= 1; --width) {
            if (i + width > bytes.size()) {
                continue;
            }
            int code = 0;
            for (int j = 0; j < width; ++j) {
                code = (code << 8) | static_cast<unsigned char>(bytes.at(i + j));
            }
            if (font.codeToUnicode.contains(code)) {
                mapped = font.codeToUnicode.value(code);
                used = width;
                break;
            }
        }
        if (used == 0) {
            mapped = QString(QChar(static_cast<uchar>(bytes.at(i))));
            used = 1;
        }
        text += mapped;
        i += used;
    }
    return text;
}

QString normalizedStructuredText(QString text)
{
    text.replace(QLatin1Char('['), QLatin1Char(' '));
    text.replace(QLatin1Char(']'), QLatin1Char(' '));
    text.replace(QRegularExpression(QStringLiteral(R"([ \t]+\n)")), QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral(R"(\n{3,})")), QStringLiteral("\n\n"));
    return text.trimmed();
}

QString extractStructuredText(const QByteArray &content, const QHash<QString, PdfFontMap> &fonts)
{
    const QVector<PdfTextToken> tokens = tokenizeContent(content);
    QVector<PdfTextToken> operands;
    QString currentFontName;
    QString output;

    auto appendText = [&](const QByteArray &bytes) {
        const PdfFontMap font = fonts.value(currentFontName);
        output += font.name.isEmpty() ? QString::fromLatin1(bytes) : decodePdfBytes(font, bytes);
    };

    auto appendTextOperands = [&]() {
        for (const PdfTextToken &operand : operands) {
            if (operand.kind == PdfTextToken::Kind::String || operand.kind == PdfTextToken::Kind::HexString) {
                appendText(operand.bytes);
            }
        }
    };

    for (const PdfTextToken &token : tokens) {
        if (token.kind != PdfTextToken::Kind::Operator) {
            operands.push_back(token);
            continue;
        }

        const QString op = token.text;
        if (op == QStringLiteral("Tf")) {
            for (int i = operands.size() - 1; i >= 0; --i) {
                if (operands.at(i).kind == PdfTextToken::Kind::Name) {
                    currentFontName = operands.at(i).text;
                    break;
                }
            }
        } else if (op == QStringLiteral("Tj") || op == QStringLiteral("TJ")) {
            appendTextOperands();
        } else if (op == QStringLiteral("'")) {
            output += QLatin1Char('\n');
            appendTextOperands();
        } else if (op == QStringLiteral("\"")) {
            output += QLatin1Char('\n');
            appendTextOperands();
        } else if (op == QStringLiteral("T*")) {
            output += QLatin1Char('\n');
        } else if (op == QStringLiteral("Td") || op == QStringLiteral("TD")) {
            if (operands.size() >= 2) {
                bool ok = false;
                const qreal y = operands.at(operands.size() - 1).text.toDouble(&ok);
                if (ok && std::abs(y) > 3.0) {
                    output += QLatin1Char('\n');
                } else if (!output.endsWith(QLatin1Char(' '))) {
                    output += QLatin1Char(' ');
                }
            }
        }

        operands.clear();
    }

    return normalizedStructuredText(output);
}

struct PdfVisualTextChar {
    QString text;
    QRectF rect;
};

QVector<PdfVisualTextChar> extractVisualStructuredChars(const QString &filePath,
                                                        int pageIndex,
                                                        const QSizeF &pageSize)
{
    QVector<PdfVisualTextChar> chars;
    if (filePath.isEmpty() || pageIndex < 0 || pageSize.isEmpty()) {
        return chars;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return chars;
    }

    const QByteArray pdfData = file.readAll();
    const QVector<PdfObject> objects = parseObjects(pdfData);
    const QHash<int, QByteArray> objectsByNumber = objectMap(objects);
    const QVector<int> pages = pageObjectNumbers(objects);
    if (pageIndex >= pages.size()) {
        return chars;
    }

    const QByteArray pageBody = objectsByNumber.value(pages.at(pageIndex));
    const QHash<QString, PdfFontMap> fonts = pageFonts(pageBody, objectsByNumber);

    QByteArray content;
    for (int contentObjectNumber : contentObjectNumbers(pageBody)) {
        content += decodedStream(objectsByNumber.value(contentObjectNumber));
        content += '\n';
    }

    const QVector<PdfTextToken> tokens = tokenizeContent(content);
    QVector<PdfTextToken> operands;
    QString currentFontName;
    qreal fontSize = 10.0;
    qreal textX = 0.0;
    qreal textY = 0.0;
    qreal lineX = 0.0;
    qreal lineY = 0.0;

    auto tokenNumber = [](const PdfTextToken &token, qreal fallback = 0.0) {
        bool ok = false;
        const qreal value = token.text.toDouble(&ok);
        return ok ? value : fallback;
    };

    auto decodedText = [&](const QByteArray &bytes) {
        const PdfFontMap font = fonts.value(currentFontName);
        return font.name.isEmpty() ? QString::fromLatin1(bytes) : decodePdfBytes(font, bytes);
    };

    auto appendText = [&](const QString &text) {
        const qreal charHeight = std::max<qreal>(fontSize, 1.0);
        const qreal defaultWidth = std::max<qreal>(fontSize * 0.48, 1.0);
        for (const QChar character : text) {
            const qreal charWidth = character.isSpace() ? std::max<qreal>(fontSize * 0.26, 1.0) : defaultWidth;
            const QRectF charRect(textX,
                                  pageSize.height() - textY - charHeight,
                                  charWidth,
                                  charHeight * 1.18);
            if (!character.isSpace()) {
                chars.push_back({QString(character), charRect.normalized()});
            }
            textX += charWidth;
        }
    };

    auto appendTextOperands = [&]() {
        for (const PdfTextToken &operand : std::as_const(operands)) {
            if (operand.kind == PdfTextToken::Kind::String || operand.kind == PdfTextToken::Kind::HexString) {
                appendText(decodedText(operand.bytes));
            } else if (operand.kind == PdfTextToken::Kind::Number) {
                textX -= tokenNumber(operand) * fontSize / 1000.0;
            }
        }
    };

    for (const PdfTextToken &token : tokens) {
        if (token.kind != PdfTextToken::Kind::Operator) {
            operands.push_back(token);
            continue;
        }

        const QString op = token.text;
        if (op == QStringLiteral("Tf")) {
            for (int i = operands.size() - 1; i >= 0; --i) {
                if (operands.at(i).kind == PdfTextToken::Kind::Number && fontSize == 10.0) {
                    fontSize = std::max<qreal>(tokenNumber(operands.at(i), fontSize), 1.0);
                } else if (operands.at(i).kind == PdfTextToken::Kind::Name) {
                    currentFontName = operands.at(i).text;
                    if (i + 1 < operands.size() && operands.at(i + 1).kind == PdfTextToken::Kind::Number) {
                        fontSize = std::max<qreal>(tokenNumber(operands.at(i + 1), fontSize), 1.0);
                    }
                    break;
                }
            }
        } else if (op == QStringLiteral("Tm") && operands.size() >= 6) {
            textX = tokenNumber(operands.at(operands.size() - 2));
            textY = tokenNumber(operands.at(operands.size() - 1));
            lineX = textX;
            lineY = textY;
        } else if ((op == QStringLiteral("Td") || op == QStringLiteral("TD")) && operands.size() >= 2) {
            lineX += tokenNumber(operands.at(operands.size() - 2));
            lineY += tokenNumber(operands.at(operands.size() - 1));
            textX = lineX;
            textY = lineY;
        } else if (op == QStringLiteral("T*")) {
            lineY -= fontSize * 1.2;
            textX = lineX;
            textY = lineY;
        } else if (op == QStringLiteral("Tj") || op == QStringLiteral("TJ")) {
            appendTextOperands();
        } else if (op == QStringLiteral("'")) {
            lineY -= fontSize * 1.2;
            textX = lineX;
            textY = lineY;
            appendTextOperands();
        } else if (op == QStringLiteral("\"")) {
            lineY -= fontSize * 1.2;
            textX = lineX;
            textY = lineY;
            appendTextOperands();
        }

        operands.clear();
    }

    return chars;
}

QSizeF parsedPageSize(const QString &filePath, int pageIndex)
{
    if (filePath.isEmpty() || pageIndex < 0) {
        return QSizeF(595.0, 842.0);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QSizeF(595.0, 842.0);
    }

    const QByteArray pdfData = file.readAll();
    const QVector<PdfObject> objects = parseObjects(pdfData);
    const QHash<int, QByteArray> objectsByNumber = objectMap(objects);
    const QVector<int> pages = pageObjectNumbers(objects);
    if (pageIndex >= pages.size()) {
        return QSizeF(595.0, 842.0);
    }

    const QString pageText = objectDictionaryText(objectsByNumber.value(pages.at(pageIndex)));
    const QRegularExpression mediaBoxExpression(QStringLiteral(R"(/MediaBox\s*\[\s*([-0-9.]+)\s+([-0-9.]+)\s+([-0-9.]+)\s+([-0-9.]+)\s*\])"));
    const QRegularExpressionMatch match = mediaBoxExpression.match(pageText);
    if (!match.hasMatch()) {
        return QSizeF(595.0, 842.0);
    }

    bool x1Ok = false;
    bool y1Ok = false;
    bool x2Ok = false;
    bool y2Ok = false;
    const qreal x1 = match.captured(1).toDouble(&x1Ok);
    const qreal y1 = match.captured(2).toDouble(&y1Ok);
    const qreal x2 = match.captured(3).toDouble(&x2Ok);
    const qreal y2 = match.captured(4).toDouble(&y2Ok);
    if (!x1Ok || !y1Ok || !x2Ok || !y2Ok || x2 <= x1 || y2 <= y1) {
        return QSizeF(595.0, 842.0);
    }

    return QSizeF(x2 - x1, y2 - y1);
}

QString visualTextFromChars(QVector<PdfVisualTextChar> chars, const QRectF &selectedRect)
{
    if (chars.isEmpty() || selectedRect.isEmpty()) {
        return {};
    }

    QVector<PdfVisualTextChar> selectedChars;
    for (const PdfVisualTextChar &ch : std::as_const(chars)) {
        const QRectF overlap = ch.rect.intersected(selectedRect);
        if (overlap.isEmpty()) {
            continue;
        }

        const qreal overlapArea = overlap.width() * overlap.height();
        const qreal charArea = ch.rect.width() * ch.rect.height();
        if (charArea > 0.0 && (overlapArea / charArea >= 0.18 || selectedRect.contains(ch.rect.center()))) {
            selectedChars.push_back(ch);
        }
    }

    if (selectedChars.isEmpty()) {
        return {};
    }

    std::sort(selectedChars.begin(), selectedChars.end(), [](const PdfVisualTextChar &left, const PdfVisualTextChar &right) {
        const qreal lineTolerance = std::max<qreal>(2.0, std::min(left.rect.height(), right.rect.height()) * 0.65);
        if (std::abs(left.rect.center().y() - right.rect.center().y()) > lineTolerance) {
            return left.rect.center().y() < right.rect.center().y();
        }
        return left.rect.left() < right.rect.left();
    });

    struct VisualLine {
        QVector<PdfVisualTextChar> chars;
        qreal centerY = 0.0;
        qreal height = 0.0;
    };

    QVector<VisualLine> lines;
    for (const PdfVisualTextChar &ch : std::as_const(selectedChars)) {
        bool added = false;
        for (VisualLine &line : lines) {
            const qreal tolerance = std::max<qreal>(2.0, std::max(line.height, ch.rect.height()) * 0.70);
            if (std::abs(ch.rect.center().y() - line.centerY) <= tolerance) {
                line.chars.push_back(ch);
                const qsizetype count = line.chars.size();
                line.centerY = ((line.centerY * (count - 1)) + ch.rect.center().y()) / count;
                line.height = std::max(line.height, ch.rect.height());
                added = true;
                break;
            }
        }

        if (!added) {
            lines.push_back({QVector<PdfVisualTextChar>{ch}, ch.rect.center().y(), ch.rect.height()});
        }
    }

    std::sort(lines.begin(), lines.end(), [](const VisualLine &left, const VisualLine &right) {
        return left.centerY < right.centerY;
    });

    qreal totalLineHeight = 0.0;
    for (const VisualLine &line : std::as_const(lines)) {
        totalLineHeight += std::max<qreal>(line.height, 1.0);
    }
    const qreal averageLineHeight = totalLineHeight / std::max<qsizetype>(1, lines.size());
    const int expectedLines = std::max(1, qCeil(selectedRect.height() / std::max<qreal>(averageLineHeight * 1.45, 4.0)));
    if (lines.size() > expectedLines + 2) {
        return {};
    }

    QStringList outputLines;
    for (VisualLine &line : lines) {
        std::sort(line.chars.begin(), line.chars.end(), [](const PdfVisualTextChar &left, const PdfVisualTextChar &right) {
            return left.rect.left() < right.rect.left();
        });

        qreal totalWidth = 0.0;
        for (const PdfVisualTextChar &ch : std::as_const(line.chars)) {
            totalWidth += ch.rect.width();
        }
        const qreal averageWidth = totalWidth / std::max<qsizetype>(1, line.chars.size());
        const qreal spaceGap = std::max<qreal>(1.2, std::min(averageWidth * 0.55, line.height * 0.24));

        QString lineText;
        QRectF previousRect;
        for (const PdfVisualTextChar &ch : std::as_const(line.chars)) {
            if (!lineText.isEmpty()) {
                const qreal gap = ch.rect.left() - previousRect.right();
                if (gap > spaceGap) {
                    lineText += QLatin1Char(' ');
                }
            }
            lineText += ch.text;
            previousRect = ch.rect;
        }

        if (!lineText.trimmed().isEmpty()) {
            outputLines.push_back(lineText.trimmed());
        }
    }

    return outputLines.join(QLatin1Char('\n')).trimmed();
}

QHash<QChar, QChar> substitutionMap(const QString &rawPageText, const QString &structuredPageText)
{
    QHash<QChar, QHash<QChar, int>> votes;
    const QString raw = rawPageText.simplified();
    const QString structured = structuredPageText.simplified();
    const int count = std::min(raw.size(), structured.size());
    for (int i = 0; i < count; ++i) {
        const QChar from = raw.at(i);
        const QChar to = structured.at(i);
        if (from.isSpace() || to.isSpace()) {
            continue;
        }
        votes[from][to] += 1;
    }

    QHash<QChar, QChar> map;
    for (auto it = votes.cbegin(); it != votes.cend(); ++it) {
        QChar best;
        int bestCount = 0;
        for (auto vote = it.value().cbegin(); vote != it.value().cend(); ++vote) {
            if (vote.value() > bestCount) {
                best = vote.key();
                bestCount = vote.value();
            }
        }
        if (!best.isNull() && bestCount >= 1) {
            map.insert(it.key(), best);
        }
    }
    return map;
}

} // namespace

AtlasPdfTextEngine::PageTextMap AtlasPdfTextEngine::mapPage(QPdfDocument *document,
                                                            int pageIndex) const
{
    PageTextMap pageMap;
    pageMap.pageNumber = pageIndex + 1;

    if (!document || document->status() != QPdfDocument::Status::Ready || pageIndex < 0
        || pageIndex >= document->pageCount()) {
        pageMap.kind = PageTextKind::Error;
        return pageMap;
    }

    const PdfPageTextProfile profile = loadPageTextProfile(m_filePath, pageIndex);
    const QPdfSelection selection = document->getAllText(pageIndex);
    if (!selection.isValid()) {
        pageMap.kind = profile.hasContentText ? PageTextKind::BrokenUnicodeMap : PageTextKind::NoText;
        return pageMap;
    }

    pageMap.text = selection.text();
    pageMap.boundingRectangle = selection.boundingRectangle();
    pageMap.bounds = selection.bounds();

    if (pageMap.text.trimmed().isEmpty()) {
        pageMap.kind = profile.hasContentText ? PageTextKind::BrokenUnicodeMap : PageTextKind::NoText;
        return pageMap;
    }

    if (looksCorruptText(pageMap.text)) {
        if (profile.hasActualText) {
            pageMap.kind = PageTextKind::ActualText;
        } else if (profile.hasType3Font && profile.hasNumericGlyphNames && !profile.hasToUnicode) {
            pageMap.kind = PageTextKind::Type3GlyphShape;
        } else if (profile.hasNamedGlyphUnicode) {
            pageMap.kind = PageTextKind::EncodedGlyphNames;
        } else {
            pageMap.kind = PageTextKind::BrokenUnicodeMap;
        }
        return pageMap;
    }

    pageMap.kind = PageTextKind::NativeText;
    return pageMap;
}

AtlasPdfTextEngine::DocumentTextMap AtlasPdfTextEngine::mapDocument(QPdfDocument *document) const
{
    DocumentTextMap documentMap;
    if (!document || document->status() != QPdfDocument::Status::Ready) {
        return documentMap;
    }

    documentMap.pages.reserve(document->pageCount());
    for (int pageIndex = 0; pageIndex < document->pageCount(); ++pageIndex) {
        PageTextMap pageMap = mapPage(document, pageIndex);
        switch (pageMap.kind) {
        case PageTextKind::NativeText:
            ++documentMap.nativeTextPages;
            break;
        case PageTextKind::ActualText:
            ++documentMap.actualTextPages;
            break;
        case PageTextKind::EncodedGlyphNames:
            ++documentMap.encodedGlyphPages;
            break;
        case PageTextKind::Type3GlyphShape:
            ++documentMap.type3GlyphPages;
            break;
        case PageTextKind::BrokenUnicodeMap:
            ++documentMap.brokenUnicodePages;
            break;
        case PageTextKind::NoText:
            ++documentMap.emptyPages;
            break;
        case PageTextKind::Error:
            ++documentMap.errorPages;
            break;
        }

        documentMap.pages.push_back(std::move(pageMap));
    }

    return documentMap;
}

QString AtlasPdfTextEngine::pageText(QPdfDocument *document, int pageIndex) const
{
    return mapPage(document, pageIndex).text;
}

QString AtlasPdfTextEngine::bestEffortPageText(QPdfDocument *document, int pageIndex) const
{
    const PageTextMap pageMap = mapPage(document, pageIndex);
    if (!pageMap.text.trimmed().isEmpty() && !looksCorruptText(pageMap.text)) {
        return pageMap.text;
    }

    const QString structuredText = structuredPageText(pageIndex);
    if (!structuredText.trimmed().isEmpty()) {
        return structuredText;
    }

    return {};
}

void AtlasPdfTextEngine::setPdfFilePath(const QString &filePath)
{
    m_filePath = filePath;
}

QString AtlasPdfTextEngine::decodedSelectionText(QPdfDocument *document,
                                                 int pageIndex,
                                                 const QString &selectionText) const
{
    if (selectionText.trimmed().isEmpty()) {
        return selectionText;
    }

    const PageTextMap pageMap = mapPage(document, pageIndex);
    if (pageMap.kind == PageTextKind::NativeText) {
        return selectionText;
    }

    if (pageMap.kind == PageTextKind::Type3GlyphShape) {
        return {};
    }

    const QString structuredText = structuredPageText(pageIndex);
    if (structuredText.trimmed().isEmpty() || looksCorruptText(structuredText)) {
        return {};
    }

    const QString rawPageText = document && document->status() == QPdfDocument::Status::Ready
                                    ? document->getAllText(pageIndex).text()
                                    : QString();
    const QString decodedText = decodeByPageMap(rawPageText, structuredText, selectionText);
    return decodedText != selectionText && !looksCorruptText(decodedText) ? decodedText : QString();
}

QString AtlasPdfTextEngine::visualTextInRect(QPdfDocument *document,
                                             int pageIndex,
                                             const QRectF &selectionRect)
{
    return visualTextInRect(QString(), document, pageIndex, selectionRect);
}

QString AtlasPdfTextEngine::visualTextInRect(const QString &filePath,
                                             QPdfDocument *document,
                                             int pageIndex,
                                             const QRectF &selectionRect)
{
    if (!document || document->status() != QPdfDocument::Status::Ready || pageIndex < 0
        || pageIndex >= document->pageCount() || selectionRect.isEmpty()) {
        return {};
    }

    const QRectF pageRect(QPointF(0.0, 0.0), document->pagePointSize(pageIndex));
    const QRectF selectedRect = selectionRect.normalized().intersected(pageRect);
    if (selectedRect.isEmpty()) {
        return {};
    }

    const QPdfSelection pageSelection = document->getAllText(pageIndex);
    if (!pageSelection.isValid() || pageSelection.text().isEmpty()) {
        return visualTextFromChars(extractVisualStructuredChars(filePath, pageIndex, pageRect.size()), selectedRect);
    }

    struct VisualChar {
        QString text;
        QRectF rect;
        int index = -1;
    };

    QVector<VisualChar> chars;
    const QString pageText = pageSelection.text();
    chars.reserve(std::min<qsizetype>(pageText.size(), 4096));

    for (int index = 0; index < pageText.size(); ++index) {
        if (pageText.at(index).isSpace()) {
            continue;
        }

        const QPdfSelection characterSelection = document->getSelectionAtIndex(pageIndex, index, 1);
        if (!characterSelection.isValid() || characterSelection.bounds().isEmpty()) {
            continue;
        }

        QRectF charRect;
        bool hasCharRect = false;
        for (const QPolygonF &bound : characterSelection.bounds()) {
            const QRectF boundRect = bound.boundingRect().normalized();
            charRect = hasCharRect ? charRect.united(boundRect) : boundRect;
            hasCharRect = true;
        }

        if (!hasCharRect) {
            continue;
        }

        charRect = charRect.intersected(pageRect);
        if (charRect.width() < 0.1 || charRect.height() < 0.1) {
            continue;
        }

        const QRectF overlap = charRect.intersected(selectedRect);
        if (overlap.isEmpty()) {
            continue;
        }

        const qreal overlapArea = overlap.width() * overlap.height();
        const qreal charArea = charRect.width() * charRect.height();
        if (charArea <= 0.0 || (overlapArea / charArea < 0.18 && !selectedRect.contains(charRect.center()))) {
            continue;
        }

        chars.push_back({pageText.mid(index, 1), charRect, index});
    }

    if (chars.isEmpty()) {
        return visualTextFromChars(extractVisualStructuredChars(filePath, pageIndex, pageRect.size()), selectedRect);
    }

    std::sort(chars.begin(), chars.end(), [](const VisualChar &left, const VisualChar &right) {
        const qreal leftCenter = left.rect.center().y();
        const qreal rightCenter = right.rect.center().y();
        const qreal lineTolerance = std::max<qreal>(2.0, std::min(left.rect.height(), right.rect.height()) * 0.65);
        if (std::abs(leftCenter - rightCenter) > lineTolerance) {
            return leftCenter < rightCenter;
        }
        return left.rect.left() < right.rect.left();
    });

    struct VisualLine {
        QVector<VisualChar> chars;
        qreal centerY = 0.0;
        qreal height = 0.0;
    };

    QVector<VisualLine> lines;
    for (const VisualChar &ch : std::as_const(chars)) {
        bool added = false;
        for (VisualLine &line : lines) {
            const qreal tolerance = std::max<qreal>(2.0, std::max(line.height, ch.rect.height()) * 0.70);
            if (std::abs(ch.rect.center().y() - line.centerY) <= tolerance) {
                line.chars.push_back(ch);
                const qsizetype count = line.chars.size();
                line.centerY = ((line.centerY * (count - 1)) + ch.rect.center().y()) / count;
                line.height = std::max(line.height, ch.rect.height());
                added = true;
                break;
            }
        }

        if (!added) {
            lines.push_back({QVector<VisualChar>{ch}, ch.rect.center().y(), ch.rect.height()});
        }
    }

    std::sort(lines.begin(), lines.end(), [](const VisualLine &left, const VisualLine &right) {
        return left.centerY < right.centerY;
    });

    QStringList outputLines;
    for (VisualLine &line : lines) {
        std::sort(line.chars.begin(), line.chars.end(), [](const VisualChar &left, const VisualChar &right) {
            return left.rect.left() < right.rect.left();
        });

        qreal totalWidth = 0.0;
        for (const VisualChar &ch : std::as_const(line.chars)) {
            totalWidth += ch.rect.width();
        }
        const qreal averageWidth = totalWidth / std::max<qsizetype>(1, line.chars.size());
        const qreal spaceGap = std::max<qreal>(1.2, std::min(averageWidth * 0.55, line.height * 0.24));

        QString lineText;
        QRectF previousRect;
        int previousIndex = -1;
        for (const VisualChar &ch : std::as_const(line.chars)) {
            if (!lineText.isEmpty()) {
                const qreal gap = ch.rect.left() - previousRect.right();
                const QString between = previousIndex >= 0 && ch.index > previousIndex
                                            ? pageText.mid(previousIndex + 1, ch.index - previousIndex - 1)
                                            : QString();
                if (between.contains(QRegularExpression(QStringLiteral(R"(\s)"))) || gap > spaceGap) {
                    lineText += QLatin1Char(' ');
                }
            }
            lineText += ch.text;
            previousRect = ch.rect;
            previousIndex = ch.index;
        }

        if (!lineText.trimmed().isEmpty()) {
            outputLines.push_back(lineText.trimmed());
        }
    }

    return outputLines.join(QLatin1Char('\n')).trimmed();
}

QString AtlasPdfTextEngine::visualTextInRect(const QString &filePath,
                                             int pageIndex,
                                             const QSizeF &pageSize,
                                             const QRectF &selectionRect)
{
    const QRectF pageRect(QPointF(0.0, 0.0), pageSize.isEmpty() ? parsedPageSize(filePath, pageIndex) : pageSize);
    const QRectF selectedRect = selectionRect.normalized().intersected(pageRect);
    if (selectedRect.isEmpty()) {
        return {};
    }

    return visualTextFromChars(extractVisualStructuredChars(filePath, pageIndex, pageRect.size()), selectedRect);
}

QString AtlasPdfTextEngine::kindLabel(PageTextKind kind)
{
    switch (kind) {
    case PageTextKind::NativeText:
        return QStringLiteral("Texto nativo");
    case PageTextKind::ActualText:
        return QStringLiteral("Texto por ActualText");
    case PageTextKind::EncodedGlyphNames:
        return QStringLiteral("Texto por nomes de glifos");
    case PageTextKind::Type3GlyphShape:
        return QStringLiteral("Fonte Type3 sem mapa Unicode");
    case PageTextKind::BrokenUnicodeMap:
        return QStringLiteral("Mapa Unicode quebrado");
    case PageTextKind::NoText:
        return QStringLiteral("Sem texto");
    case PageTextKind::Error:
        return QStringLiteral("Erro");
    }

    return QStringLiteral("Desconhecido");
}

QString AtlasPdfTextEngine::structuredPageText(int pageIndex) const
{
    if (m_filePath.isEmpty() || pageIndex < 0) {
        return {};
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QByteArray pdfData = file.readAll();
    const QVector<PdfObject> objects = parseObjects(pdfData);
    const QHash<int, QByteArray> objectsByNumber = objectMap(objects);
    const QVector<int> pages = pageObjectNumbers(objects);
    if (pageIndex >= pages.size()) {
        return {};
    }

    const QByteArray pageBody = objectsByNumber.value(pages.at(pageIndex));
    const QHash<QString, PdfFontMap> fonts = pageFonts(pageBody, objectsByNumber);

    QByteArray content;
    for (int contentObjectNumber : contentObjectNumbers(pageBody)) {
        content += decodedStream(objectsByNumber.value(contentObjectNumber));
        content += '\n';
    }

    return extractStructuredText(content, fonts);
}

QString AtlasPdfTextEngine::decodeByPageMap(const QString &rawPageText,
                                            const QString &structuredPageText,
                                            const QString &selectionText) const
{
    if (rawPageText.trimmed().isEmpty() || structuredPageText.trimmed().isEmpty()) {
        return selectionText;
    }

    const QHash<QChar, QChar> map = substitutionMap(rawPageText, structuredPageText);
    if (map.isEmpty()) {
        return selectionText;
    }

    QString decoded;
    decoded.reserve(selectionText.size());
    bool changed = false;
    for (const QChar character : selectionText) {
        const auto mapped = map.constFind(character);
        if (mapped != map.cend()) {
            decoded += mapped.value();
            changed = true;
        } else if (character == QLatin1Char('[') || character == QLatin1Char(']') || character.unicode() < 32) {
            decoded += QLatin1Char(' ');
            changed = true;
        } else {
            decoded += character;
        }
    }

    return changed ? decoded.simplified() : selectionText;
}

bool AtlasPdfTextEngine::looksCorruptText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.size() < 20) {
        return false;
    }

    if (trimmed.contains(QStringLiteral("(cid:"), Qt::CaseInsensitive)) {
        return true;
    }

    int suspicious = 0;
    int meaningful = 0;
    for (const QChar character : trimmed) {
        if (character.isLetterOrNumber() || character.isSpace()) {
            ++meaningful;
            continue;
        }

        const ushort unicode = character.unicode();
        const bool acceptedPunctuation = QStringLiteral(".,;:!?()[]{}<>/\\-_'\"@#$%&*+=|").contains(character);
        const bool replacement = unicode == 0xFFFD;
        const bool privateUse = unicode >= 0xE000 && unicode <= 0xF8FF;
        const bool control = character.isNull() || (unicode < 32 && !character.isSpace());
        if (!acceptedPunctuation || replacement || privateUse || control) {
            ++suspicious;
        }
    }

    const int counted = meaningful + suspicious;
    if (counted == 0) {
        return false;
    }

    return suspicious > 12 && suspicious > counted * 0.20;
}
