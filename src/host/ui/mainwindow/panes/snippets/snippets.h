#ifndef SNIPPETS_H
#define SNIPPETS_H

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QWidget>

class QTextBrowser;

class Snippets : public QWidget
{
    Q_OBJECT

public:
    explicit Snippets(QWidget *parent = nullptr);
    void showInstruction(const QString &instruction);

private:
    QTextBrowser *detailsView;
    QHash<QString, QJsonObject> instructionDocs;
    QString displayedInstruction;
    bool hasDisplayedInstruction = false;
};

#endif // SNIPPETS_H
