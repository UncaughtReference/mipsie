#ifndef MIPSHIGHLIGHTER_H
#define MIPSHIGHLIGHTER_H

#include <QRegularExpression>
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
