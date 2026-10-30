#ifndef MIPSHIGHLIGHTER_H
#define MIPSHIGHLIGHTER_H

#include <QRegularExpression>
#include <QColor>
#include <QJsonObject>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>
#include <QStringList>

enum class MipsCompletionKind
{
    Directive,
    Instruction,
    Register,
    Label,
    Constant
};

struct MipsCompletionItem
{
    QString text;
    MipsCompletionKind kind;
};

class MipsHighlighter : public QSyntaxHighlighter
{
public:
    explicit MipsHighlighter(QTextDocument *document);
    void setTheme(const QString &theme);
    void setTheme(const QString &theme, const QJsonObject &colorOverrides);
    static QStringList colorKeys();
    static QColor defaultColor(const QString &key, const QString &theme);
    static QVector<MipsCompletionItem> completionItems();
    static QStringList completionKeywords();

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightRule> rules;
    QTextCharFormat blockCommentFormat;
};

#endif // MIPSHIGHLIGHTER_H
