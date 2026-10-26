#include "editor.h"

#include "editor/mipscompleter.h"
#include "editor/mipshighlighter.h"

#include <QBoxLayout>
#include <QAction>
#include <QAbstractItemView>
#include <QFrame>
#include <QFileInfo>
#include <QFile>
#include <QFileDialog>
#include <QIcon>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QSaveFile>
#include <QScrollBar>
#include <QSplitter>
#include <QStandardItem>
#include <QStyle>
#include <QTextBlock>
#include <QTabBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

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
    editorFont.setPointSize(12);

    tabs->setTabsClosable(true);
    connect(tabs, &QTabWidget::tabCloseRequested,
            this, [this](int index) { closeTab(index); });
    connect(tabs, &QTabWidget::currentChanged, this, [this] {
        refreshLabels();
        emitCurrentInstruction();
    });

    auto *closeTabAction = new QAction(this);
    closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    closeTabAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(closeTabAction);
    connect(closeTabAction, &QAction::triggered, this, [this] {
        closeTab(tabs->currentIndex());
    });

    newTab("new");

    labelSearch->setPlaceholderText(tr("Search labels"));
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
    connect(labelList, &QListView::doubleClicked, this, &Editor::goToLabel);
    connect(labelSearch, &QLineEdit::textChanged, labelFilterModel,
            &QSortFilterProxyModel::setFilterFixedString);

    QVBoxLayout* editorWidgetLayout = new QVBoxLayout();
    editorWidgetLayout->setContentsMargins(0, 0, 0, 0);
    editorWidgetLayout->addWidget(editorSplitter);

    this->setLayout(editorWidgetLayout);

    findReplacePopup->setObjectName(QStringLiteral("findReplacePopup"));
    findReplacePopup->setFrameShape(QFrame::StyledPanel);
    findReplacePopup->setAutoFillBackground(true);
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
    connect(closeFindButton, &QToolButton::clicked, findReplacePopup, &QWidget::hide);
    connect(findField, &QLineEdit::returnPressed, this, [this] { findNext(); });
    connect(replaceField, &QLineEdit::returnPressed, this, &Editor::replaceCurrentMatch);

    auto *findAction = new QAction(tr("Find"), this);
    findAction->setShortcut(QKeySequence::Find);
    findAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(findAction);
    connect(findAction, &QAction::triggered, this, [this] { showFindReplace(false); });

    auto *replaceAction = new QAction(tr("Find and Replace"), this);
    replaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    replaceAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(replaceAction);
    connect(replaceAction, &QAction::triggered, this, [this] { showFindReplace(true); });

    auto *searchLabelsAction = new QAction(tr("Search Labels"), this);
    searchLabelsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    searchLabelsAction->setShortcutContext(Qt::WindowShortcut);
    addAction(searchLabelsAction);
    connect(searchLabelsAction, &QAction::triggered, this, [this] {
        labelSearch->setFocus();
        labelSearch->selectAll();
    });

    refreshLabels();
}

bool Editor::openFile(const QString &filePath) {
    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();

    // Reuse the tab for a file that is already open. Its QTextDocument is the
    // in-memory buffer, including any edits not yet saved to disk.
    for (int index = 0; index < tabs->count(); ++index) {
        if (tabs->tabBar()->tabData(index).toString().compare(absolutePath, Qt::CaseInsensitive) == 0) {
            tabs->setCurrentIndex(index);
            return true;
        }
    }

    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Open Failed"),
                              tr("Could not open %1:\n%2")
                                  .arg(absolutePath, file.errorString()));
        return false;
    }

    const QString contents = QString::fromUtf8(file.readAll());
    QTextEdit *textEditor = createEditorTab(QFileInfo(absolutePath).fileName(), absolutePath);
    textEditor->setPlainText(contents);
    textEditor->document()->setModified(false);

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

    auto *textEditor = qobject_cast<QTextEdit *>(tabs->widget(index));
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
        ? tabs->tabText(index)
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
    return qobject_cast<QTextEdit *>(tabs->currentWidget());
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
    return true;
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
        QStringLiteral("^\\s*([A-Za-z0-9_.$]+):\\s*(?:;.*)?$"));
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

QTextEdit *Editor::createEditorTab(const QString &name, const QString &filePath)
{
    auto *textEditor = new QTextEdit(tabs);
    textEditor->setFont(editorFont);
    textEditor->setStyleSheet("background-color: #1a1a1a; color: white;");
    new MipsHighlighter(textEditor->document());
    new MipsCompleter(textEditor);

    const int index = tabs->addTab(textEditor, name);
    tabs->tabBar()->setTabData(index, filePath);
    textEditor->setProperty("tabBaseTitle", name);
    connect(textEditor->document(), &QTextDocument::modificationChanged,
            this, [this, textEditor](bool modified) {
        const int tabIndex = tabs->indexOf(textEditor);
        if (tabIndex < 0)
            return;
        const QString baseTitle = textEditor->property("tabBaseTitle").toString();
        tabs->setTabText(tabIndex, modified ? QStringLiteral("*") + baseTitle : baseTitle);
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
