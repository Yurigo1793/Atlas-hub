#pragma once

#include <QList>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

class QPdfDocument;

class AtlasPdfTextEngine
{
public:
    enum class PageTextKind {
        NativeText,
        ActualText,
        EncodedGlyphNames,
        Type3GlyphShape,
        BrokenUnicodeMap,
        NoText,
        Error
    };

    struct PageTextMap {
        int pageNumber = 0;
        PageTextKind kind = PageTextKind::NoText;
        QString text;
        QRectF boundingRectangle;
        QList<QPolygonF> bounds;
    };

    struct DocumentTextMap {
        QVector<PageTextMap> pages;
        int nativeTextPages = 0;
        int actualTextPages = 0;
        int encodedGlyphPages = 0;
        int type3GlyphPages = 0;
        int brokenUnicodePages = 0;
        int emptyPages = 0;
        int errorPages = 0;
    };

    PageTextMap mapPage(QPdfDocument *document, int pageIndex) const;
    DocumentTextMap mapDocument(QPdfDocument *document) const;
    QString pageText(QPdfDocument *document, int pageIndex) const;
    QString bestEffortPageText(QPdfDocument *document, int pageIndex) const;
    void setPdfFilePath(const QString &filePath);
    QString decodedSelectionText(QPdfDocument *document, int pageIndex, const QString &selectionText) const;

    static QString visualTextInRect(QPdfDocument *document, int pageIndex, const QRectF &selectionRect);
    static QString visualTextInRect(const QString &filePath,
                                    QPdfDocument *document,
                                    int pageIndex,
                                    const QRectF &selectionRect);
    static QString visualTextInRect(const QString &filePath,
                                    int pageIndex,
                                    const QSizeF &pageSize,
                                    const QRectF &selectionRect);
    static QString kindLabel(PageTextKind kind);
    static bool looksCorruptText(const QString &text);

private:
    QString structuredPageText(int pageIndex) const;
    QString decodeByPageMap(const QString &rawPageText,
                            const QString &structuredPageText,
                            const QString &selectionText) const;

    QString m_filePath;
};
