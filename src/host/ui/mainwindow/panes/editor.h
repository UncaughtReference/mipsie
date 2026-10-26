#ifndef EDITOR_H
#define EDITOR_H

#include <QWidget>
#include <QTextEdit>
#include <QTabWidget>
#include <QFont>
#include <QSplitter>
#include <QStandardItemModel>

class QFrame;
class QLineEdit;
class QListView;
class QModelIndex;
class QResizeEvent;
class QSortFilterProxyModel;
class QWidget;

class Editor : public QWidget
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);

    void initialize();
    bool openFile(const QString &filePath);
    bool newTab(const QString &name);
    bool saveCurrentFile();
    bool saveCurrentFileAs();
    QTextEdit *currentEditor() const;
    QString currentFilePath() const;
    QString currentFileTitle() const;
    bool goToLine(int line, QTextEdit *buffer = nullptr);
    QString currentInstruction() const;

signals:
    void currentInstructionChanged(const QString &instruction);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QTabWidget *tabs;
    QSplitter *editorSplitter;
    QListView *labelList;
    QStandardItemModel *labelModel;
    QSortFilterProxyModel *labelFilterModel;
    QLineEdit *labelSearch;
    QFrame *findReplacePopup;
    QWidget *replaceRowWidget;
    QLineEdit *findField;
    QLineEdit *replaceField;
    QFont editorFont;
    QString lastInstruction;

    bool closeTab(int index);
    QTextEdit *createEditorTab(const QString &name, const QString &filePath = QString());
    bool saveCurrentFileToPath(const QString &filePath);
    void showFindReplace(bool showReplaceField);
    void positionFindReplacePopup();
    void findNext(bool backwards = false);
    void replaceCurrentMatch();
    void replaceAllMatches();
    void refreshLabels();
    void goToLabel(const QModelIndex &index);
    void emitCurrentInstruction();
};

#endif // EDITOR_H
