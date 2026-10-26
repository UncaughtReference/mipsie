#include "snippets.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QTextBrowser>
#include <QVBoxLayout>

Snippets::Snippets(QWidget *parent)
    : QWidget(parent),
      detailsView(new QTextBrowser(this))
{
    detailsView->setObjectName(QStringLiteral("InstructionSnippetView"));
    detailsView->setOpenExternalLinks(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(detailsView);

    QFile dataFile(QStringLiteral(":/snippets/instructions.json"));
    if (dataFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(dataFile.readAll());
        const QJsonArray entries = document.object().value(QStringLiteral("entries")).toArray();
        for (const QJsonValue &value : entries) {
            const QJsonObject entry = value.toObject();
            instructionDocs.insert(entry.value(QStringLiteral("name")).toString().toCaseFolded(), entry);
        }
    }

    showInstruction(QString());
}

void Snippets::showInstruction(const QString &instruction)
{
    const QString key = instruction.trimmed().toCaseFolded();
    if (hasDisplayedInstruction && key == displayedInstruction)
        return;
    displayedInstruction = key;
    hasDisplayedInstruction = true;

    if (key.isEmpty()) {
        detailsView->setHtml(tr("<h2>Instruction snippets</h2><p>Place the cursor on a MIPS instruction in the Editor to see its reference here.</p>"));
        return;
    }

    if (!instructionDocs.contains(key)) {
        detailsView->setHtml(QStringLiteral("<h2>%1</h2><p>No entry for this instruction is available in the selected reference.</p>")
                                 .arg(instruction.toHtmlEscaped()));
        return;
    }

    const QJsonObject entry = instructionDocs.value(key);
    const QString name = entry.value(QStringLiteral("name")).toString().toHtmlEscaped();
    const QString longName = entry.value(QStringLiteral("longName")).toString().toHtmlEscaped();
    const QString summary = entry.value(QStringLiteral("summary")).toString().toHtmlEscaped();
    QString html = QStringLiteral("<h2>%1</h2>").arg(name);
    if (!longName.isEmpty() && longName.compare(name, Qt::CaseInsensitive) != 0)
        html += QStringLiteral("<p><b>%1</b></p>").arg(longName);
    if (!summary.isEmpty())
        html += QStringLiteral("<p>%1</p>").arg(summary);

    const QJsonArray forms = entry.value(QStringLiteral("forms")).toArray();
    if (!forms.isEmpty()) {
        html += QStringLiteral("<h3>Forms</h3>");
        for (const QJsonValue &form : forms)
            html += QStringLiteral("<pre><code>%1</code></pre>").arg(form.toString().toHtmlEscaped());
    }

    const QJsonArray operands = entry.value(QStringLiteral("operands")).toArray();
    if (!operands.isEmpty()) {
        html += QStringLiteral("<h3>Operands</h3><dl>");
        for (const QJsonValue &value : operands) {
            const QJsonObject operand = value.toObject();
            html += QStringLiteral("<dt><code>%1</code></dt><dd>%2</dd>")
                .arg(operand.value(QStringLiteral("operand")).toString().toHtmlEscaped(),
                     operand.value(QStringLiteral("description")).toString().toHtmlEscaped());
        }
        html += QStringLiteral("</dl>");
    }

    const QString source = entry.value(QStringLiteral("source")).toString().toHtmlEscaped();
    html += QStringLiteral("<p><a href=\"%1\">Instruction reference</a></p>").arg(source);
    detailsView->setHtml(html);
}
