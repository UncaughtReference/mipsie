#ifndef EDITOR_H
#define EDITOR_H

#include <QWidget>
#include <QTextEdit>
#include <QTabWidget>
#include <QFont>
#include <QJsonArray>
#include <QList>
#include <QPointer>
#include <QSplitter>
#include <QStandardItemModel>
#include "editor/mipshighlighter.h"

class QFrame;
class QAction;
class QEvent;
class QLineEdit;
class QListView;
class QModelIndex;
class QResizeEvent;
class QSortFilterProxyModel;
class QShortcut;
class QWidget;

class Editor : public QWidget
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);

    void initialize();
    void setEditorPreferences(int fontSize, int tabWidth, const QString &theme,
                              const QString &fontFamily = QString());
    void setMacros(const QJsonArray &macros);
    bool openFile(const QString &filePath);
    bool newTab(const QString &name);
    bool saveCurrentFile();
    bool saveCurrentFileAs();
    bool hasUnsavedFiles() const;
    bool saveAllModifiedFiles();
    QTextEdit *currentEditor() const;
    QString currentFilePath() const;
    QString currentFileTitle() const;
    void toggleCommentOut();
    bool goToLine(int line, QTextEdit *buffer = nullptr);
    QString currentInstruction() const;

signals:
    void currentInstructionChanged(const QString &instruction);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QTabWidget *tabs;
    QSplitter *editorSplitter;
    QListView *labelList;
    QStandardItemModel *labelModel;
    QSortFilterProxyModel *labelFilterModel;
    QLineEdit *labelSearch;
    QFrame *findReplacePopup;
    QAction *closeFindReplaceAction = nullptr;
    QWidget *replaceRowWidget;
    QLineEdit *findField;
    QLineEdit *replaceField;
    QFont editorFont;
    int editorFontSize = 12;
    int editorTabWidth = 4;
    QString editorTheme = QStringLiteral("System");
    QString lastInstruction;
    QList<QPointer<MipsHighlighter>> highlighters;
    QList<QShortcut *> macroShortcuts;

    bool closeTab(int index);
    QTextEdit *createEditorTab(const QString &name,
                               const QString &filePath = QString(),
                               const QString *initialContents = nullptr);
    bool saveCurrentFileToPath(const QString &filePath);
    void showFindReplace(bool showReplaceField);
    void positionFindReplacePopup();
    void findNext(bool backwards = false);
    void replaceCurrentMatch();
    void replaceAllMatches();
    void refreshLabels();
    void goToLabel(const QModelIndex &index);
    void emitCurrentInstruction();
    void applyTextEditorPreferences(QTextEdit *textEditor);
};

#endif // EDITOR_H
