#include "mipshighlighter.h"

#include <QColor>
#include <QApplication>
#include <QSet>
#include <QTextDocument>

namespace
{
QTextCharFormat makeFormat(const QColor &color, bool bold = false, bool italic = false)
{
    QTextCharFormat format;
    format.setForeground(color);
    if (bold)
        format.setFontWeight(QFont::Bold);
    if (italic)
        format.setFontItalic(true);
    return format;
}

QString wordPattern(const QString &words)
{
    // MIPS mnemonics can contain dots (for example, add.s and cvt.s.w).
    // Word boundaries treat '.' as punctuation, which would match only the
    // prefix before the dot. Keep the full mnemonic as one token instead.
    return QStringLiteral("(?<![A-Za-z0-9_.$])(?:%1)(?![A-Za-z0-9_.$])").arg(words);
}

QString directiveWordPattern(const QString &words)
{
    // Directives are commonly written with a leading dot, such as .orga
    // and .asciiz. Include that dot in the match while requiring an exact
    // directive token.
    return QStringLiteral("(?<![A-Za-z0-9_.$])\\.?(?:%1)(?![A-Za-z0-9_.$])").arg(words);
}

const QString &directivePattern()
{
    static const QString words = QStringLiteral(
        "n64|rsp|psx|ps2|psp|open|create|close|org|orga|headersize|base|"
        "endian|include|incbin|import|importobj|loadelf|align|aligna|fill|skip|"
        "definelabel|ifdef|ifndef|if|elseif|elseifdef|elseifndef|else|endif|"
        "macro|endmacro|func|endfunc|function|endfunction|area|endarea|region|endregion|"
        "notice|warning|error|erroronwarning|print|msg|resetdelay|fixloaddelay|"
        "db|dh|dw|dd|dq|byte|half|word|ascii|asciiz|"
        "loadtable|string|stringn|equ|expfunc|"
        "check|assert|protect|unprotect|copy|"
        "arm|thumb|gba|nds|3ds|saturn|32x");
    return words;
}

const QString &instructionPattern()
{
    static const QString words = QStringLiteral(
        "add|addu|addi|addiu|sub|subu|dadd|daddu|daddi|daddiu|"
        "mult|multu|div|divu|dmult|dmultu|ddiv|ddivu|"
        "and|andi|or|ori|xor|xori|nor|"
        "sll|sllv|srl|srlv|sra|srav|dsll|dsll32|dsllv|dsrl|dsrl32|dsrlv|dsra|dsra32|dsrav|"
        "slt|sltu|slti|sltiu|"
        "beq|bne|blez|bgtz|bltz|bgez|beql|bnel|blezl|bgtzl|bltzl|bgezl|"
        "j|jal|jr|jalr|"
        "lb|lbu|lh|lhu|lw|lwl|lwr|lui|ld|ldl|ldr|"
        "sb|sh|sw|swl|swr|sd|sdl|sdr|ll|sc|lwc1|swc1|ldc1|sdc1|"
        "lwc0|lwc2|lwc3|swc0|swc2|swc3|ldc0|ldc2|ldc3|sdc0|sdc2|sdc3|"
        "cache|pref|"
        "mfc0|mtc0|cfc0|ctc0|mfc1|mtc1|cfc1|ctc1|mfc2|mtc2|cfc2|ctc2|"
        "mfhi|mflo|mthi|mtlo|"
        "tlbr|tlbwi|tlbwr|tlbp|eret|rfe|syscall|break|sync|wait|"
        "tge|tgeu|tgei|tgeiu|tlt|tltu|tlti|tltui|teq|teqi|tne|tnei|"
        "movf|movt|movz|movn|"
        "nop|move|li|la|clear|not|neg|negu|"
        "b|bal|beqz|bnez|bge|bgeu|bgt|bgtu|ble|bleu|blt|bltu|"
        "abs|rol|ror|rem|remu|mul|"
        "l\\.d|s\\.d|l\\.s|s\\.s|"
        "add\\.s|sub\\.s|mul\\.s|div\\.s|sqrt\\.s|abs\\.s|mov\\.s|neg\\.s|"
        "add\\.d|sub\\.d|mul\\.d|div\\.d|sqrt\\.d|abs\\.d|mov\\.d|neg\\.d|"
        "round\\.w\\.s|round\\.w\\.d|trunc\\.w\\.s|trunc\\.w\\.d|"
        "ceil\\.w\\.s|ceil\\.w\\.d|floor\\.w\\.s|floor\\.w\\.d|"
        "cvt\\.s\\.d|cvt\\.s\\.w|cvt\\.d\\.s|cvt\\.d\\.w|cvt\\.w\\.s|cvt\\.w\\.d|"
        "c\\.f\\.s|c\\.un\\.s|c\\.eq\\.s|c\\.ueq\\.s|c\\.olt\\.s|c\\.ult\\.s|"
        "c\\.ole\\.s|c\\.ule\\.s|c\\.sf\\.s|c\\.ngle\\.s|c\\.seq\\.s|"
        "c\\.ngl\\.s|c\\.lt\\.s|c\\.nge\\.s|c\\.nlt\\.s|c\\.le\\.s|c\\.ngt\\.s|"
        "c\\.f\\.d|c\\.un\\.d|c\\.eq\\.d|c\\.ueq\\.d|c\\.olt\\.d|c\\.ult\\.d|"
        "c\\.ole\\.d|c\\.ule\\.d|c\\.sf\\.d|c\\.ngle\\.d|c\\.seq\\.d|"
        "c\\.ngl\\.d|c\\.lt\\.d|c\\.nge\\.d|c\\.nlt\\.d|c\\.le\\.d|c\\.ngt\\.d|"
        "bc0f|bc0t|bc0fl|bc0tl|bc1f|bc1t|bc1fl|bc1tl|bc2f|bc2t|bc2fl|bc2tl");
    return words;
}

const QString &registerPattern()
{
    static const QString words = QStringLiteral(
        "zero|at|v[01]|a[0-3]|t[0-9]|s[0-7]|s8|fp|k[01]|gp|sp|ra|"
        "r(?:[0-9]|[12][0-9]|3[01])|f(?:[0-9]|[12][0-9]|3[01])|"
        "hi|lo|pc|fcr[0-9]+|fpr[0-9]+|c[0-3]_[0-9]+");
    return words;
}
}

MipsHighlighter::MipsHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
    setTheme(qApp ? qApp->property("mipsieTheme").toString() : QString());
}

void MipsHighlighter::setTheme(const QString &theme)
{
    const bool light = theme == QStringLiteral("Light");
    const QTextCharFormat directiveFormat = makeFormat(QColor(light ? "#0000ff" : "#569cd6"), true);
    const QTextCharFormat instructionFormat = makeFormat(QColor(light ? "#795e26" : "#dcdcaa"), true);
    const QTextCharFormat registerFormat = makeFormat(QColor(light ? "#001080" : "#9cdcfe"));
    const QTextCharFormat numberFormat = makeFormat(QColor(light ? "#098658" : "#b5cea8"));
    const QTextCharFormat stringFormat = makeFormat(QColor(light ? "#a31515" : "#ce9178"));
    const QTextCharFormat labelFormat = makeFormat(QColor(light ? "#267f99" : "#4ec9b0"));
    const QTextCharFormat symbolFormat = makeFormat(QColor(light ? "#af00db" : "#c586c0"));
    const QTextCharFormat commentFormat = makeFormat(
        QColor(light ? "#008000" : "#6a9955"), false, true);

    rules.clear();

    rules.append({QRegularExpression(QStringLiteral(R"(\b[A-Za-z_.$][A-Za-z0-9_.$]*\b)")), symbolFormat});

    rules.append({QRegularExpression(directiveWordPattern(directivePattern()), QRegularExpression::CaseInsensitiveOption), directiveFormat});
    rules.append({QRegularExpression(wordPattern(instructionPattern()), QRegularExpression::CaseInsensitiveOption), instructionFormat});
    // Accept armips register names with or without '$', but only as complete
    // tokens. In particular, don't color aliases embedded in symbols like "wrap".
    rules.append({QRegularExpression(
        QStringLiteral(R"((?<![A-Za-z0-9_.$@])\$?(?:%1)(?![A-Za-z0-9_.$@]))").arg(registerPattern()),
        QRegularExpression::CaseInsensitiveOption), registerFormat});

    rules.append({QRegularExpression(QStringLiteral(
        R"((?:0[xX][0-9a-fA-F]+|0[oO][0-7]+|0[bB][01]+|\b[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?|\b[0-9a-fA-F]+[hH]|\b[0-7]+[oO]|\b[01]+[bB])\b)")), numberFormat});
    rules.append({QRegularExpression(QStringLiteral(R"("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')")), stringFormat});
    rules.append({QRegularExpression(QStringLiteral(R"(\b[A-Za-z_.$][A-Za-z0-9_.$]*\s*:)")), labelFormat});
    // armips accepts both semicolon and // line comments, as well as /* */ blocks.
    rules.append({QRegularExpression(QStringLiteral(R"(;.*$|//.*$)")), commentFormat});
    blockCommentFormat = commentFormat;
    rehighlight();
}

QVector<MipsCompletionItem> MipsHighlighter::completionItems()
{
    QVector<MipsCompletionItem> items;
    QSet<QString> seen;
    const auto appendUnique = [&items, &seen](const QString &keyword, MipsCompletionKind kind) {
        const QString folded = keyword.toCaseFolded();
        if (!keyword.isEmpty() && !seen.contains(folded)) {
            seen.insert(folded);
            items.append({keyword, kind});
        }
    };

    const QStringList directives = directivePattern().split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (const QString &directive : directives) {
        appendUnique(directive, MipsCompletionKind::Directive);
        appendUnique(QLatin1Char('.') + directive, MipsCompletionKind::Directive);
    }

    QStringList instructions = instructionPattern().split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (QString instruction : instructions) {
        instruction.replace(QStringLiteral("\\."), QStringLiteral("."));
        appendUnique(instruction, MipsCompletionKind::Instruction);
    }

    QStringList registers = {
        QStringLiteral("zero"), QStringLiteral("at"), QStringLiteral("v0"), QStringLiteral("v1"),
        QStringLiteral("a0"), QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3"),
        QStringLiteral("fp"), QStringLiteral("gp"), QStringLiteral("sp"), QStringLiteral("ra"),
        QStringLiteral("k0"), QStringLiteral("k1"), QStringLiteral("s8"),
        QStringLiteral("hi"), QStringLiteral("lo"), QStringLiteral("pc")
    };
    for (int i = 0; i <= 9; ++i)
        registers.append(QStringLiteral("t%1").arg(i));
    for (int i = 0; i <= 7; ++i)
        registers.append(QStringLiteral("s%1").arg(i));
    for (int i = 0; i <= 31; ++i) {
        registers.append(QStringLiteral("r%1").arg(i));
        registers.append(QStringLiteral("f%1").arg(i));
        registers.append(QStringLiteral("fcr%1").arg(i));
        registers.append(QStringLiteral("fpr%1").arg(i));
        for (int coprocessor = 0; coprocessor <= 3; ++coprocessor)
            registers.append(QStringLiteral("c%1_%2").arg(coprocessor).arg(i));
    }
    for (const QString &reg : registers) {
        appendUnique(reg, MipsCompletionKind::Register);
        appendUnique(QLatin1Char('$') + reg, MipsCompletionKind::Register);
    }

    return items;
}

QStringList MipsHighlighter::completionKeywords()
{
    QStringList keywords;
    const QVector<MipsCompletionItem> items = completionItems();
    for (const MipsCompletionItem &item : items)
        keywords.append(item.text);
    return keywords;
}

void MipsHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightRule &rule : rules) {
        QRegularExpressionMatchIterator matches = rule.pattern.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    setCurrentBlockState(0);
    int start = 0;
    if (previousBlockState() != 1)
        start = text.indexOf(QStringLiteral("/*"));

    while (start >= 0) {
        const int end = text.indexOf(QStringLiteral("*/"), start + 2);
        const int length = end < 0 ? text.length() - start : end - start + 2;
        setFormat(start, length, blockCommentFormat);
        if (end < 0) {
            setCurrentBlockState(1);
            break;
        }
        start = text.indexOf(QStringLiteral("/*"), end + 2);
    }
    if (previousBlockState() == 1 && text.indexOf(QStringLiteral("*/")) < 0)
        setCurrentBlockState(1);
}
