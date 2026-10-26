#ifndef MIPSCOMPLETER_H
#define MIPSCOMPLETER_H

#include "mipshighlighter.h"

#include <QObject>
#include <QStringList>
#include <QVector>

class QCompleter;
class QEvent;
class QStandardItemModel;
class QTextEdit;

class MipsCompleter : public QObject
{
public:
    explicit MipsCompleter(QTextEdit *editor);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QTextEdit *editor;
    QCompleter *completer;
    QStandardItemModel *model;
    QStringList dynamicKeys;
    bool pendingTextUpdate = false;
    bool userSelectedSuggestion = false;

    QString currentPrefix(int *startPosition = nullptr) const;
    QVector<MipsCompletionItem> dynamicCompletionCandidates() const;
    void rebuildModel();
    void refreshDynamicCandidates();
    void updatePopup(bool force = false);
    void insertCompletion(const QString &completion);
};

#endif // MIPSCOMPLETER_H
