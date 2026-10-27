#include "editor.h"

#include "mipscompleter.h"
#include "mipshighlighter.h"

#include <QBoxLayout>
#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QFrame>
#include <QFileInfo>
#include <QFile>
#include <QFileDialog>
#include <QFontMetricsF>
#include <QIcon>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QtAlgorithms>
#include <QJsonObject>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QSaveFile>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStandardItem>
#include <QStyle>
#include <QTextBlock>
#include <QTabBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
class CodeTextEdit final : public QTextEdit
{
public:
    using QTextEdit::QTextEdit;

protected:
    void insertFromMimeData(const QMimeData *source) override
    {
        if (!source || !source->hasText())
            return;

        QString text = source->text();
        text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        insertPlainText(text);
    }
};

class LineNumberArea final : public QWidget
{
public:
    explicit LineNumberArea(QTextEdit *editor, QWidget *parent)
        : QWidget(parent), editor(editor)
    {
        setFont(editor->font());
        editor->viewport()->installEventFilter(this);
        connect(editor->document(), &QTextDocument::blockCountChanged,
                this, [this] { updateWidth(); });
        connect(editor->document(), &QTextDocument::contentsChanged,
                this, [this] {
            updateWidth();
            update();
        });
        connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, [this] { update(); });
        connect(editor, &QTextEdit::cursorPositionChanged,
                this, [this] { update(); });
        updateWidth();
    }

    void setTheme(const QString &theme)
    {
        darkTheme = theme != QStringLiteral("Light");
        update();
    }

    void setEditorFont(const QFont &font)
    {
        setFont(font);
        updateWidth();
        update();
    }

    void updateWidth()
    {
        if (!editor)
            return;
        const int digits = QString::number(qMax(1, editor->document()->blockCount())).size();
        const int width = fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 12;
        setFixedWidth(width);
        update();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (editor && watched == editor->viewport()
            && (event->type() == QEvent::Resize || event->type() == QEvent::Paint))
            update();
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *) override
    {
        if (!editor)
            return;

        QPainter painter(this);
        const QColor background = darkTheme ? QColor(QStringLiteral("#202124"))
                                            : QColor(QStringLiteral("#f3f3f3"));
        const QColor normalNumber = darkTheme ? QColor(QStringLiteral("#858585"))
                                              : QColor(QStringLiteral("#858585"));
        const QColor currentNumber = darkTheme ? QColor(QStringLiteral("#e0e0e0"))
                                               : QColor(QStringLiteral("#202124"));
        painter.fillRect(rect(), background);

        QTextBlock block = editor->cursorForPosition(QPoint(0, 0)).block();
        const int currentBlockNumber = editor->textCursor().blockNumber();
        const int lineHeight = fontMetrics().height();
        while (block.isValid()) {
            const int top = editor->cursorRect(QTextCursor(block)).top();
            if (top >= height())
                break;
            painter.setPen(block.blockNumber() == currentBlockNumber
                               ? currentNumber : normalNumber);
            painter.drawText(0, top, width() - 5, lineHeight,
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(block.blockNumber() + 1));
            block = block.next();
        }
    }

private:
    QTextEdit *editor;
    bool darkTheme = true;
};

QTextEdit *textEditorForTab(QWidget *tab)
{
    if (auto *textEditor = qobject_cast<QTextEdit *>(tab))
        return textEditor;
    return tab ? tab->findChild<QTextEdit *>() : nullptr;
}
}

Editor::Editor(QWidget *parent) :
    QWidget(parent),
    tabs(new QTabWidget()),
    editorSplitter(new QSplitter(Qt::Horizontal, this)),
    labelList(new QListView(this)),
    labelModel(new QStandardItemModel(this)),
    labelFilterModel(new QSortFilterProxyModel(this)),
    labelSearch(new QLineEdit(this)),
    findReplacePopup(new QFrame(this)),
    replaceRowWidget(new QWidget(findReplacePopup)),
    findField(new QLineEdit(findReplacePopup)),
    replaceField(new QLineEdit(findReplacePopup)) {}

void Editor::initialize()
{
    editorFont = font();
    editorFont.setPointSize(editorFontSize);

    tabs->setTabsClosable(true);
    connect(tabs, &QTabWidget::tabCloseRequested,
            this, [this](int index) { closeTab(index); });
    connect(tabs, &QTabWidget::currentChanged, this, [this] {
        refreshLabels();
        emitCurrentInstruction();
    });

    auto *closeTabAction = new QAction(this);
    closeTabAction->setText(tr("Close Tab"));
    closeTabAction->setObjectName(QStringLiteral("closeTabAction"));
    closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    closeTabAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(closeTabAction);
    connect(closeTabAction, &QAction::triggered, this, [this] {
        closeTab(tabs->currentIndex());
    });

    auto *previousTabAction = new QAction(this);
    previousTabAction->setText(tr("Previous Tab"));
    previousTabAction->setObjectName(QStringLiteral("previousTabAction"));
    previousTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    previousTabAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(previousTabAction);
    connect(previousTabAction, &QAction::triggered, this, [this] {
        const int count = tabs->count();
        if (count > 1)
            tabs->setCurrentIndex((tabs->currentIndex() + count - 1) % count);
    });

    auto *nextTabAction = new QAction(this);
    nextTabAction->setText(tr("Next Tab"));
    nextTabAction->setObjectName(QStringLiteral("nextTabAction"));
    nextTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    nextTabAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(nextTabAction);
    connect(nextTabAction, &QAction::triggered, this, [this] {
        const int count = tabs->count();
        if (count > 1)
            tabs->setCurrentIndex((tabs->currentIndex() + 1) % count);
    });

    auto *newTabAction = new QAction(this);
    newTabAction->setText(tr("New Tab"));
    newTabAction->setObjectName(QStringLiteral("newTabAction"));
    newTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    newTabAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(newTabAction);
    connect(newTabAction, &QAction::triggered, this, [this] {
        newTab(tr("new"));
        if (QTextEdit *editor = currentEditor())
            editor->setFocus();
    });

    newTab("new");

    labelSearch->setPlaceholderText(tr("Search labels"));
    labelSearch->installEventFilter(this);
    labelFilterModel->setSourceModel(labelModel);
    labelFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    labelFilterModel->setFilterKeyColumn(0);
    labelList->setModel(labelFilterModel);
    labelList->setObjectName(QStringLiteral("LabelListView"));
    labelList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    labelList->setUniformItemSizes(true);
    auto *labelPanel = new QWidget(this);
    auto *labelLayout = new QVBoxLayout(labelPanel);
    labelLayout->setContentsMargins(4, 4, 4, 4);
    labelLayout->addWidget(new QLabel(tr("Labels"), labelPanel));
    labelLayout->addWidget(labelSearch);
    labelLayout->addWidget(labelList, 1);
    editorSplitter->addWidget(labelPanel);
    editorSplitter->addWidget(tabs);
    editorSplitter->setStretchFactor(0, 0);
    editorSplitter->setStretchFactor(1, 1);
    editorSplitter->setSizes({180, 820});
    connect(labelList, &QListView::activated, this, &Editor::goToLabel);
    connect(labelSearch, &QLineEdit::textChanged, labelFilterModel,
            &QSortFilterProxyModel::setFilterFixedString);

    QVBoxLayout* editorWidgetLayout = new QVBoxLayout();
    editorWidgetLayout->setContentsMargins(0, 0, 0, 0);
    editorWidgetLayout->addWidget(editorSplitter);

    this->setLayout(editorWidgetLayout);

    findReplacePopup->setObjectName(QStringLiteral("findReplacePopup"));
    findReplacePopup->setFrameShape(QFrame::StyledPanel);
    findReplacePopup->setAutoFillBackground(true);
    closeFindReplaceAction = new QAction(this);
    closeFindReplaceAction->setObjectName(QStringLiteral("closeFindReplaceAction"));
    closeFindReplaceAction->setShortcut(QKeySequence(Qt::Key_Escape));
    closeFindReplaceAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    closeFindReplaceAction->setEnabled(false);
    addAction(closeFindReplaceAction);
    connect(closeFindReplaceAction, &QAction::triggered, this, [this] {
        findReplacePopup->hide();
        closeFindReplaceAction->setEnabled(false);
        if (QTextEdit *editor = currentEditor())
            editor->setFocus(Qt::ShortcutFocusReason);
    });
    findField->setPlaceholderText(tr("Find"));
    replaceField->setPlaceholderText(tr("Replace with"));
    auto *findNextButton = new QPushButton(tr("Next"), findReplacePopup);
    auto *findPreviousButton = new QPushButton(tr("Previous"), findReplacePopup);
    auto *replaceButton = new QPushButton(tr("Replace"), findReplacePopup);
    auto *replaceAllButton = new QPushButton(tr("Replace All"), findReplacePopup);
    auto *closeFindButton = new QToolButton(findReplacePopup);
    closeFindButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeFindButton->setToolTip(tr("Close"));
    closeFindButton->setAutoRaise(true);
    closeFindButton->setFixedSize(28, 28);

    auto *findRow = new QHBoxLayout;
    findRow->addWidget(findField, 1);
    findRow->addWidget(findPreviousButton);
    findRow->addWidget(findNextButton);
    findRow->addWidget(closeFindButton);
    auto *replaceRow = new QHBoxLayout(replaceRowWidget);
    replaceRow->setContentsMargins(0, 0, 0, 0);
    replaceRow->addWidget(replaceField, 1);
    replaceRow->addWidget(replaceButton);
    replaceRow->addWidget(replaceAllButton);
    auto *popupLayout = new QVBoxLayout(findReplacePopup);
    popupLayout->setContentsMargins(8, 8, 8, 8);
    popupLayout->addLayout(findRow);
    popupLayout->addWidget(replaceRowWidget);
    findReplacePopup->adjustSize();
    findReplacePopup->hide();

    connect(findNextButton, &QPushButton::clicked, this, [this] { findNext(); });
    connect(findPreviousButton, &QPushButton::clicked, this, [this] { findNext(true); });
    connect(replaceButton, &QPushButton::clicked, this, &Editor::replaceCurrentMatch);
    connect(replaceAllButton, &QPushButton::clicked, this, &Editor::replaceAllMatches);
    connect(closeFindButton, &QToolButton::clicked,
            closeFindReplaceAction, &QAction::trigger);
    connect(findField, &QLineEdit::returnPressed, this, [this] { findNext(); });
    connect(replaceField, &QLineEdit::returnPressed, this, &Editor::replaceCurrentMatch);

    auto *findAction = new QAction(tr("Find"), this);
    findAction->setObjectName(QStringLiteral("findAction"));
    findAction->setShortcut(QKeySequence::Find);
    findAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(findAction);
    connect(findAction, &QAction::triggered, this, [this] { showFindReplace(false); });

    auto *replaceAction = new QAction(tr("Find and Replace"), this);
    replaceAction->setObjectName(QStringLiteral("replaceAction"));
    replaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    replaceAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(replaceAction);
    connect(replaceAction, &QAction::triggered, this, [this] { showFindReplace(true); });

    auto *searchLabelsAction = new QAction(tr("Search Labels"), this);
    searchLabelsAction->setObjectName(QStringLiteral("searchLabelsAction"));
    searchLabelsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    searchLabelsAction->setShortcutContext(Qt::WindowShortcut);
    addAction(searchLabelsAction);
    connect(searchLabelsAction, &QAction::triggered, this, [this] {
        labelSearch->setFocus();
        labelSearch->selectAll();
    });

    refreshLabels();
}

void Editor::setEditorPreferences(int fontSize, int tabWidth, const QString &theme,
                                  const QString &fontFamily)
{
    editorFontSize = qBound(8, fontSize, 40);
    editorTabWidth = qBound(1, tabWidth, 16);
    editorTheme = theme;
    editorFont = font();
    if (!fontFamily.isEmpty())
        editorFont.setFamily(fontFamily);
    editorFont.setPointSize(editorFontSize);

    for (auto it = highlighters.begin(); it != highlighters.end();) {
        if (it->isNull()) {
            it = highlighters.erase(it);
            continue;
        }
        it->data()->setTheme(editorTheme);
        ++it;
    }

    for (int i = 0; i < tabs->count(); ++i) {
        if (auto *textEditor = textEditorForTab(tabs->widget(i)))
            applyTextEditorPreferences(textEditor);
    }
}

void Editor::setMacros(const QJsonArray &macros)
{
    qDeleteAll(macroShortcuts);
    macroShortcuts.clear();

    for (const QJsonValue &value : macros) {
        const QJsonObject macro = value.toObject();
        const QKeySequence shortcut = QKeySequence::fromString(
            macro.value(QStringLiteral("shortcut")).toString(),
            QKeySequence::PortableText);
        const QString text = macro.value(QStringLiteral("text")).toString();
        if (shortcut.isEmpty() || text.isEmpty())
            continue;

        auto *macroShortcut = new QShortcut(shortcut, this);
        macroShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        macroShortcuts.append(macroShortcut);
        connect(macroShortcut, &QShortcut::activated, this, [this, text] {
            QTextEdit *textEditor = currentEditor();
            QWidget *focusedWidget = QApplication::focusWidget();
            if (!textEditor || !focusedWidget
                || (focusedWidget != textEditor && !textEditor->isAncestorOf(focusedWidget)))
                return;

            QTextCursor cursor = textEditor->textCursor();
            cursor.insertText(text);
            textEditor->setTextCursor(cursor);
            textEditor->setFocus(Qt::ShortcutFocusReason);
        });
    }
}

bool Editor::openFile(const QString &filePath) {
    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();

    // Reuse the tab for a file that is already open. Its QTextDocument is the
    // in-memory buffer, including any edits not yet saved to disk.
    for (int index = 0; index < tabs->count(); ++index) {
        if (tabs->tabBar()->tabData(index).toString().compare(absolutePath, Qt::CaseInsensitive) == 0) {
            tabs->setCurrentIndex(index);
            if (QTextEdit *textEditor = currentEditor())
                textEditor->setFocus(Qt::OtherFocusReason);
            return true;
        }
    }

    constexpr qint64 largeFileSizeThreshold = 20 * 1024;
    const QFileInfo fileInfo(absolutePath);
    if (fileInfo.size() > largeFileSizeThreshold) {
        QMessageBox prompt(QMessageBox::Warning,
                           tr("Large File"),
                           tr("This file is larger than 20 KB and may affect editor performance. Open it anyway?"),
                           QMessageBox::Open | QMessageBox::Cancel,
                           this);
        prompt.setDefaultButton(QMessageBox::Cancel);
        if (prompt.exec() != QMessageBox::Open)
            return false;
    }

    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Open Failed"),
                              tr("Could not open %1:\n%2")
                                  .arg(absolutePath, file.errorString()));
        return false;
    }

    const QString contents = QString::fromUtf8(file.readAll());
    QTextEdit *textEditor = createEditorTab(
        QFileInfo(absolutePath).fileName(), absolutePath, &contents);
    textEditor->document()->setModified(false);
    textEditor->setFocus(Qt::OtherFocusReason);

    return true;
}

bool Editor::newTab(const QString &name)
{
    createEditorTab(name);
    return true;
}

bool Editor::closeTab(int index)
{
    if (index < 0 || index >= tabs->count())
        return false;

    auto *textEditor = textEditorForTab(tabs->widget(index));
    if (textEditor && textEditor->document()->isModified()) {
        QMessageBox prompt(this);
        prompt.setIcon(QMessageBox::Warning);
        prompt.setWindowTitle(tr("Unsaved Changes"));
        prompt.setText(tr("Save changes to %1 before closing?").arg(tabs->tabText(index)));
        prompt.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        prompt.setDefaultButton(QMessageBox::Save);

        const QMessageBox::StandardButton choice =
            static_cast<QMessageBox::StandardButton>(prompt.exec());
        if (choice == QMessageBox::Cancel)
            return false;
        if (choice == QMessageBox::Save) {
            tabs->setCurrentIndex(index);
            if (!saveCurrentFile())
                return false;
        }
    }

    QWidget *tab = tabs->widget(index);
    tabs->removeTab(index);
    delete tab;
    if (QTextEdit *editor = currentEditor())
        editor->setFocus();
    return true;
}

bool Editor::saveCurrentFile()
{
    const int index = tabs->currentIndex();
    if (index < 0)
        return false;

    const QString filePath = tabs->tabBar()->tabData(index).toString();
    if (filePath.isEmpty())
        return saveCurrentFileAs();

    return saveCurrentFileToPath(filePath);
}

bool Editor::saveCurrentFileAs()
{
    const int index = tabs->currentIndex();
    QTextEdit *textEditor = currentEditor();
    if (index < 0 || !textEditor)
        return false;

    const QString currentPath = tabs->tabBar()->tabData(index).toString();
    const QString suggestedName = currentPath.isEmpty()
        ? textEditor->property("tabBaseTitle").toString()
        : currentPath;
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Assembly File"),
        suggestedName,
        tr("Assembly files (*.asm *.s);;All files (*)"));

    if (filePath.isEmpty())
        return false;

    return saveCurrentFileToPath(filePath);
}

bool Editor::hasUnsavedFiles() const
{
    for (int index = 0; index < tabs->count(); ++index) {
        auto *textEditor = textEditorForTab(tabs->widget(index));
        if (textEditor && textEditor->document()->isModified())
            return true;
    }
    return false;
}

bool Editor::saveAllModifiedFiles()
{
    for (int index = 0; index < tabs->count(); ++index) {
        auto *textEditor = textEditorForTab(tabs->widget(index));
        if (!textEditor || !textEditor->document()->isModified())
            continue;

        tabs->setCurrentIndex(index);
        if (!saveCurrentFile())
            return false;
    }
    return true;
}

bool Editor::saveCurrentFileToPath(const QString &filePath)
{
    QTextEdit *textEditor = currentEditor();
    const int index = tabs->currentIndex();
    if (!textEditor || index < 0)
        return false;

    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Save Failed"),
                              tr("Could not open %1 for writing:\n%2")
                                  .arg(absolutePath, file.errorString()));
        return false;
    }

    const QByteArray contents = textEditor->toPlainText().toUtf8();
    if (file.write(contents) != contents.size() || !file.commit()) {
        QMessageBox::critical(this, tr("Save Failed"),
                              tr("Could not save %1:\n%2")
                                  .arg(absolutePath, file.errorString()));
        return false;
    }

    tabs->tabBar()->setTabData(index, absolutePath);
    const QString baseTitle = QFileInfo(absolutePath).fileName();
    textEditor->setProperty("tabBaseTitle", baseTitle);
    tabs->setTabText(index, baseTitle);
    textEditor->document()->setModified(false);
    return true;
}

QTextEdit *Editor::currentEditor() const
{
    return textEditorForTab(tabs->currentWidget());
}

QString Editor::currentFilePath() const
{
    const int index = tabs->currentIndex();
    return index >= 0 ? tabs->tabBar()->tabData(index).toString() : QString();
}

QString Editor::currentFileTitle() const
{
    const int index = tabs->currentIndex();
    return index >= 0 ? tabs->tabText(index) : QString();
}

bool Editor::goToLine(int line, QTextEdit *buffer)
{
    if (line < 1)
        return false;

    if (buffer) {
        const int tabIndex = tabs->indexOf(buffer);
        if (tabIndex < 0)
            buffer = nullptr;
        else
            tabs->setCurrentIndex(tabIndex);
    }
    if (!buffer)
        buffer = currentEditor();
    if (!buffer)
        return false;

    QTextCursor cursor(buffer->document());
    cursor.movePosition(QTextCursor::Start);
    const int targetLine = qMin(line, buffer->document()->blockCount());
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, targetLine - 1);
    buffer->setTextCursor(cursor);
    buffer->setFocus();
    buffer->ensureCursorVisible();

    QTextEdit *textEditor = currentEditor();
    if (!textEditor)
        return true;

    const int cursorY = textEditor->cursorRect(textEditor->textCursor()).center().y();
    const int viewportCenterY = textEditor->viewport()->height() / 2;
    QScrollBar *verticalScrollBar = textEditor->verticalScrollBar();
    verticalScrollBar->setValue(verticalScrollBar->value() + cursorY - viewportCenterY);
    return true;
}

void Editor::toggleCommentOut()
{
    QTextEdit *textEditor = currentEditor();
    if (!textEditor)
        return;

    QTextCursor selection = textEditor->textCursor();
    if (!selection.hasSelection())
        return;

    QTextDocument *document = textEditor->document();
    const int selectionStart = selection.selectionStart();
    const int selectionEnd = selection.selectionEnd();
    QTextBlock firstBlock = document->findBlock(selectionStart);
    QTextBlock lastBlock = document->findBlock(selectionEnd);
    if (selectionEnd > selectionStart && lastBlock.position() == selectionEnd)
        lastBlock = lastBlock.previous();
    if (!firstBlock.isValid() || !lastBlock.isValid())
        return;

    // Edit from bottom to top so inserting/removing a marker does not move
    // the positions of blocks that still need to be processed.
    QList<QTextBlock> blocks;
    for (QTextBlock block = firstBlock; block.isValid(); block = block.next()) {
        blocks.append(block);
        if (block == lastBlock)
            break;
    }

    QTextCursor editBlock(document);
    editBlock.beginEditBlock();
    for (auto it = blocks.crbegin(); it != blocks.crend(); ++it) {
        QTextCursor lineCursor(*it);
        if (it->text().startsWith(QLatin1Char(';'))) {
            lineCursor.deleteChar();
        } else {
            lineCursor.insertText(QStringLiteral(";"));
        }
    }
    editBlock.endEditBlock();

    const QTextBlock updatedFirst = document->findBlock(firstBlock.position());
    const QTextBlock updatedLast = document->findBlock(lastBlock.position());
    if (!updatedFirst.isValid() || !updatedLast.isValid())
        return;

    QTextCursor updatedSelection(document);
    updatedSelection.setPosition(updatedFirst.position());
    updatedSelection.setPosition(updatedLast.position() + updatedLast.text().size(),
                                 QTextCursor::KeepAnchor);
    textEditor->setTextCursor(updatedSelection);
    textEditor->setFocus(Qt::ShortcutFocusReason);
}

QString Editor::currentInstruction() const
{
    QTextEdit *textEditor = currentEditor();
    if (!textEditor)
        return QString();

    QString line = textEditor->textCursor().block().text();
    const int semicolonComment = line.indexOf(QLatin1Char(';'));
    const int slashComment = line.indexOf(QStringLiteral("//"));
    int commentPosition = -1;
    if (semicolonComment >= 0)
        commentPosition = semicolonComment;
    if (slashComment >= 0 && (commentPosition < 0 || slashComment < commentPosition))
        commentPosition = slashComment;
    if (commentPosition >= 0)
        line.truncate(commentPosition);

    static const QRegularExpression labelPrefix(
        QStringLiteral("^\\s*[A-Za-z0-9_.$@]+:\\s*"));
    while (true) {
        const auto label = labelPrefix.match(line);
        if (!label.hasMatch() || label.capturedLength() == 0)
            break;
        line.remove(0, label.capturedLength());
    }

    static const QRegularExpression mnemonicPattern(
        QStringLiteral("^\\s*([A-Za-z0-9_.]+)\\b"));
    const auto mnemonic = mnemonicPattern.match(line);
    if (!mnemonic.hasMatch())
        return QString();

    static const QSet<QString> knownInstructions = [] {
        QSet<QString> instructions;
        for (const MipsCompletionItem &item : MipsHighlighter::completionItems()) {
            if (item.kind == MipsCompletionKind::Instruction)
                instructions.insert(item.text.toCaseFolded());
        }
        return instructions;
    }();

    const QString instruction = mnemonic.captured(1).toCaseFolded();
    return knownInstructions.contains(instruction) ? instruction : QString();
}

void Editor::emitCurrentInstruction()
{
    const QString instruction = currentInstruction();
    if (instruction == lastInstruction)
        return;
    lastInstruction = instruction;
    emit currentInstructionChanged(instruction);
}

void Editor::showFindReplace(bool showReplaceField)
{
    if (QTextEdit *textEditor = currentEditor()) {
        const QString selectedText = textEditor->textCursor().selectedText();
        if (!selectedText.isEmpty())
            findField->setText(selectedText);
    }
    replaceRowWidget->setVisible(showReplaceField);
    closeFindReplaceAction->setEnabled(true);
    findReplacePopup->adjustSize();
    positionFindReplacePopup();
    findReplacePopup->show();
    findReplacePopup->raise();
    findField->setFocus();
    findField->selectAll();
}

void Editor::positionFindReplacePopup()
{
    if (!findReplacePopup)
        return;

    const int popupWidth = qMin(findReplacePopup->sizeHint().width(), qMax(1, width() - 16));
    findReplacePopup->resize(popupWidth, findReplacePopup->sizeHint().height());
    const int x = qMax(8, width() - findReplacePopup->width() - 8);
    const int y = editorSplitter ? editorSplitter->y() + 8 : 8;
    findReplacePopup->move(x, y);
}

void Editor::findNext(bool backwards)
{
    QTextEdit *textEditor = currentEditor();
    const QString needle = findField->text();
    if (!textEditor || needle.isEmpty())
        return;

    QTextDocument::FindFlags flags;
    if (backwards)
        flags |= QTextDocument::FindBackward;
    if (textEditor->find(needle, flags))
        return;

    QTextCursor cursor(textEditor->document());
    cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
    textEditor->setTextCursor(cursor);
    textEditor->find(needle, flags);
}

void Editor::replaceCurrentMatch()
{
    QTextEdit *textEditor = currentEditor();
    const QString needle = findField->text();
    if (!textEditor || needle.isEmpty())
        return;

    QTextCursor cursor = textEditor->textCursor();
    if (!cursor.hasSelection()
        || cursor.selectedText().compare(needle, Qt::CaseInsensitive) != 0) {
        findNext();
        cursor = textEditor->textCursor();
    }
    if (!cursor.hasSelection()
        || cursor.selectedText().compare(needle, Qt::CaseInsensitive) != 0)
        return;

    cursor.insertText(replaceField->text());
    textEditor->setTextCursor(cursor);
    findNext();
}

void Editor::replaceAllMatches()
{
    QTextEdit *textEditor = currentEditor();
    const QString needle = findField->text();
    if (!textEditor || needle.isEmpty())
        return;

    QTextDocument *document = textEditor->document();
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    while (true) {
        QTextCursor match = document->find(needle, cursor);
        if (match.isNull())
            break;
        match.insertText(replaceField->text());
        cursor = match;
    }
    cursor.endEditBlock();
    textEditor->setTextCursor(cursor);
}

void Editor::refreshLabels()
{
    labelModel->clear();
    QTextEdit *textEditor = currentEditor();
    if (!textEditor)
        return;

    static const QRegularExpression labelPattern(
        // Treat the colon as the label delimiter and allow other non-space
        // punctuation in the name (for example, @@, @name, or local symbols).
        QStringLiteral("^\\s*([^\\s:;]+):\\s*(?:;.*)?$"));
    for (QTextBlock block = textEditor->document()->begin(); block.isValid(); block = block.next()) {
        const auto match = labelPattern.match(block.text());
        if (!match.hasMatch())
            continue;

        auto *item = new QStandardItem(match.captured(1));
        item->setData(block.blockNumber() + 1, Qt::UserRole + 1);
        labelModel->appendRow(item);
    }
}

void Editor::goToLabel(const QModelIndex &index)
{
    const int line = index.data(Qt::UserRole + 1).toInt();
    if (!goToLine(line))
        return;

    QTextEdit *textEditor = currentEditor();
    if (!textEditor)
        return;

    const int cursorY = textEditor->cursorRect(textEditor->textCursor()).center().y();
    const int viewportCenterY = textEditor->viewport()->height() / 2;
    QScrollBar *verticalScrollBar = textEditor->verticalScrollBar();
    verticalScrollBar->setValue(verticalScrollBar->value() + cursorY - viewportCenterY);
}

void Editor::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (findReplacePopup && findReplacePopup->isVisible())
        positionFindReplacePopup();
}

bool Editor::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == labelSearch && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Down) {
            const QModelIndex firstItem = labelFilterModel->index(0, 0);
            if (firstItem.isValid()) {
                labelList->setCurrentIndex(firstItem);
                labelList->selectionModel()->select(
                    firstItem, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                labelList->scrollTo(firstItem);
                labelList->setFocus(Qt::ShortcutFocusReason);
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QTextEdit *Editor::createEditorTab(const QString &name, const QString &filePath,
                                   const QString *initialContents)
{
    auto *tabPage = new QWidget(tabs);
    auto *tabLayout = new QHBoxLayout(tabPage);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);

    auto *textEditor = new CodeTextEdit(tabPage);
    if (initialContents)
        textEditor->setPlainText(*initialContents);
    applyTextEditorPreferences(textEditor);
    auto *lineNumberArea = new LineNumberArea(textEditor, tabPage);
    lineNumberArea->setEditorFont(editorFont);
    lineNumberArea->setTheme(editorTheme);
    tabLayout->addWidget(lineNumberArea);
    tabLayout->addWidget(textEditor, 1);
    highlighters.append(new MipsHighlighter(textEditor->document()));
    new MipsCompleter(textEditor);

    const int index = tabs->addTab(tabPage, name);
    tabs->tabBar()->setTabData(index, filePath);
    textEditor->setProperty("tabBaseTitle", name);
    connect(textEditor->document(), &QTextDocument::modificationChanged,
            this, [this, textEditor](bool modified) {
        const int tabIndex = tabs->indexOf(textEditor->parentWidget());
        if (tabIndex < 0)
            return;
        const QString baseTitle = textEditor->property("tabBaseTitle").toString();
        tabs->setTabText(tabIndex, modified ? QStringLiteral("•") + baseTitle : baseTitle);
    });
    connect(textEditor->document(), &QTextDocument::contentsChanged,
            this, [this, textEditor] {
        if (textEditor == currentEditor()) {
            refreshLabels();
            emitCurrentInstruction();
        }
    });
    connect(textEditor, &QTextEdit::cursorPositionChanged, this, [this, textEditor] {
        if (textEditor == currentEditor())
            emitCurrentInstruction();
    });
    tabs->setCurrentIndex(index);
    return textEditor;
}

void Editor::applyTextEditorPreferences(QTextEdit *textEditor)
{
    if (!textEditor)
        return;

    textEditor->setFont(editorFont);
    const qreal spaceWidth = QFontMetricsF(editorFont).horizontalAdvance(QLatin1Char(' '));
    textEditor->setTabStopDistance(spaceWidth * editorTabWidth);
    if (textEditor->parentWidget()) {
        if (auto *layout = qobject_cast<QHBoxLayout *>(textEditor->parentWidget()->layout())) {
            auto *lineNumberArea = layout->count() > 0
                ? dynamic_cast<LineNumberArea *>(layout->itemAt(0)->widget()) : nullptr;
            if (lineNumberArea) {
                lineNumberArea->setEditorFont(editorFont);
                lineNumberArea->setTheme(editorTheme);
            }
        }
    }
    const bool lightTheme = editorTheme == QStringLiteral("Light");
    QPalette palette = textEditor->palette();
    palette.setColor(QPalette::Text, lightTheme ? QColor(Qt::black) : QColor(Qt::white));
    textEditor->setPalette(palette);
    textEditor->setStyleSheet(lightTheme
        ? QStringLiteral("background-color: #ffffff;")
        : QStringLiteral("background-color: #1a1a1a;"));
}
