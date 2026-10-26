#include "mipscompleter.h"

#include <QCompleter>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QApplication>
#include <QColor>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QScrollBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>

namespace
{
QString categoryLetter(MipsCompletionKind kind)
{
    switch (kind) {
    case MipsCompletionKind::Directive: return QStringLiteral("D");
    case MipsCompletionKind::Instruction: return QStringLiteral("I");
    case MipsCompletionKind::Register: return QStringLiteral("R");
    case MipsCompletionKind::Label: return QStringLiteral("L");
    case MipsCompletionKind::Constant: return QStringLiteral("C");
    }
    return {};
}

QColor categoryColor(MipsCompletionKind kind)
{
    switch (kind) {
    case MipsCompletionKind::Directive: return QColor("#c586c0");
    case MipsCompletionKind::Instruction: return QColor("#569cd6");
    case MipsCompletionKind::Register: return QColor("#4ec9b0");
    case MipsCompletionKind::Label: return QColor("#d7a34a");
    case MipsCompletionKind::Constant: return QColor("#78a85a");
    }
    return QColor(Qt::white);
}

class CompletionDelegate : public QStyledItemDelegate
{
public:
    explicit CompletionDelegate(QAbstractItemView *view)
        : QStyledItemDelegate(view),
          view(view)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem backgroundOption(option);
        initStyleOption(&backgroundOption, index);
        const QString text = backgroundOption.text;
        const auto kind = static_cast<MipsCompletionKind>(index.data(Qt::UserRole).toInt());
        const bool selected = option.state.testFlag(QStyle::State_Selected)
            || index == view->currentIndex();
        if (selected)
            backgroundOption.state |= QStyle::State_Selected;
        backgroundOption.text.clear();
        const QStyle *style = backgroundOption.widget
            ? backgroundOption.widget->style()
            : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &backgroundOption, painter,
                           backgroundOption.widget);

        painter->save();
        if (selected) {
            painter->fillRect(option.rect, QColor(255, 77, 166, 42));
        }

        const QRect letterRect(option.rect.left() + 7, option.rect.top(),
                               18, option.rect.height());
        QFont letterFont = option.font;
        letterFont.setBold(true);
        painter->setFont(letterFont);
        painter->setPen(categoryColor(kind));
        painter->drawText(letterRect, Qt::AlignLeft | Qt::AlignVCenter,
                          categoryLetter(kind));

        painter->setPen(selected
            ? option.palette.color(QPalette::HighlightedText)
            : option.palette.color(QPalette::Text));
        const QRect textRect = option.rect.adjusted(28, 0, -4, 0);
        const QFontMetrics metrics(option.font);
        painter->setFont(option.font);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(text, Qt::ElideRight, textRect.width()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.rwidth() += 28;
        return size;
    }

private:
    QAbstractItemView *view;
};
}

MipsCompleter::MipsCompleter(QTextEdit *editor)
    : QObject(editor),
      editor(editor),
      completer(new QCompleter(this)),
      model(new QStandardItemModel(completer))
{
    completer->setModel(model);
    completer->setWidget(editor);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setWrapAround(false);
    completer->popup()->setFont(editor->font());
    editor->installEventFilter(this);
    editor->viewport()->installEventFilter(this);
    completer->popup()->installEventFilter(this);
    completer->popup()->setItemDelegate(new CompletionDelegate(completer->popup()));

    rebuildModel();
    connect(editor->document(), &QTextDocument::contentsChange, this,
            [this, editor](int position, int charsRemoved, int charsAdded) {
        bool mightChangeDefinitions = charsRemoved > 0 || charsAdded > 1;
        if (!mightChangeDefinitions && charsAdded == 1) {
            QTextCursor changedText(editor->document());
            changedText.setPosition(position);
            changedText.setPosition(position + 1, QTextCursor::KeepAnchor);
            const QString inserted = changedText.selectedText();
            mightChangeDefinitions = inserted == QStringLiteral(":")
                || inserted == QStringLiteral(" ")
                || inserted == QStringLiteral("\t")
                || inserted == QString(QChar::ParagraphSeparator);
        }
        if (mightChangeDefinitions)
            refreshDynamicCandidates();
    });

    connect(completer, static_cast<void (QCompleter::*)(const QString &)>(
                &QCompleter::activated),
            this, [this](const QString &completion) {
        insertCompletion(completion);
    });

    connect(editor, &QTextEdit::cursorPositionChanged, this, [this] {
        if (!pendingTextUpdate)
            completer->popup()->hide();
    });
    connect(qApp, &QApplication::focusChanged, this,
            [this, editor](QWidget *, QWidget *nowFocused) {
        QWidget *popup = completer->popup();
        const bool focusInEditor = nowFocused
            && (nowFocused == editor || editor->isAncestorOf(nowFocused));
        const bool focusInPopup = nowFocused
            && (nowFocused == popup || popup->isAncestorOf(nowFocused));
        if (!focusInEditor && !focusInPopup)
            popup->hide();
    });
}

bool MipsCompleter::eventFilter(QObject *watched, QEvent *event)
{
    QWidget *popup = completer->popup();
    if (watched == editor && event->type() == QEvent::FontChange)
        popup->setFont(editor->font());

    const bool editorWidget = watched == editor || watched == editor->viewport();
    if (!editorWidget && watched != popup)
        return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonPress) {
        if (editorWidget)
            popup->hide();
        return QObject::eventFilter(watched, event);
    }
    if (event->type() == QEvent::KeyRelease) {
        if (pendingTextUpdate) {
            updatePopup();
            pendingTextUpdate = false;
        }
        return QObject::eventFilter(watched, event);
    }
    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    const int key = keyEvent->key();
    const bool popupVisible = popup->isVisible();

    if (popupVisible) {
        if (key == Qt::Key_Escape) {
            completer->popup()->hide();
            return true;
        }
        if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Tab) {
            const bool hasSelection = userSelectedSuggestion
                && completer->currentIndex().isValid();
            const QString completion = hasSelection ? completer->currentCompletion() : QString();
            completer->popup()->hide();
            userSelectedSuggestion = false;
            if (hasSelection && !completion.isEmpty()) {
                insertCompletion(completion);
                return true;
            }

            // If the popup owned focus, send the key back to the editor so it
            // performs its normal newline or tab behavior.
            if (watched == popup) {
                editor->setFocus();
                QKeyEvent forwarded(QEvent::KeyPress, key, keyEvent->modifiers(),
                                    keyEvent->text(), keyEvent->isAutoRepeat(),
                                    keyEvent->count());
                QApplication::sendEvent(editor, &forwarded);
                return true;
            }
            return QObject::eventFilter(watched, event);
        }
        if (key == Qt::Key_Down || key == Qt::Key_Up) {
            const int count = completer->completionCount();
            if (count > 0) {
                int row = userSelectedSuggestion ? completer->currentRow() : -1;
                if (!userSelectedSuggestion)
                    row = key == Qt::Key_Down ? 0 : count - 1;
                else
                    row = qBound(0, row + (key == Qt::Key_Down ? 1 : -1), count - 1);
                completer->setCurrentRow(row);
                const QModelIndex selectedIndex = completer->completionModel()->index(row, 0);
                completer->popup()->setCurrentIndex(selectedIndex);
                completer->popup()->selectionModel()->select(
                    selectedIndex,
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                completer->popup()->viewport()->update();
                userSelectedSuggestion = completer->currentIndex().isValid();
            }
            return true;
        }
    }

    if (popupVisible && (key == Qt::Key_Left || key == Qt::Key_Right
                         || key == Qt::Key_Home || key == Qt::Key_End
                         || key == Qt::Key_PageUp || key == Qt::Key_PageDown)) {
        completer->popup()->hide();
    }

    if (key == Qt::Key_Space && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
        updatePopup(true);
        return true;
    }

    const bool textKey = !keyEvent->text().isEmpty()
        && !(keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    if (textKey || key == Qt::Key_Backspace || key == Qt::Key_Delete)
        pendingTextUpdate = true;

    return QObject::eventFilter(watched, event);
}

QString MipsCompleter::currentPrefix(int *startPosition) const
{
    const QTextCursor cursor = editor->textCursor();
    const QString text = editor->toPlainText();
    int start = cursor.position();
    while (start > 0) {
        const QChar ch = text.at(start - 1);
        if (!(ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('.')
              || ch == QLatin1Char('$') || ch == QLatin1Char('@'))) {
            break;
        }
        --start;
    }
    if (startPosition)
        *startPosition = start;
    return text.mid(start, cursor.position() - start);
}

QVector<MipsCompletionItem> MipsCompleter::dynamicCompletionCandidates() const
{
    QVector<MipsCompletionItem> candidates;
    QSet<QString> seen;
    for (const MipsCompletionItem &keyword : MipsHighlighter::completionItems())
        seen.insert(keyword.text.toCaseFolded());

    const QRegularExpression equDefinition(
        QStringLiteral(R"(^\s*([@A-Za-z_][@A-Za-z0-9_.$]*)\s+equ\b)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator matches = equDefinition.globalMatch(editor->toPlainText());
    while (matches.hasNext()) {
        const QString constant = matches.next().captured(1);
        const QString folded = constant.toCaseFolded();
        if (!seen.contains(folded)) {
            seen.insert(folded);
            candidates.append({constant, MipsCompletionKind::Constant});
        }
    }

    const QRegularExpression labelDefinition(
        QStringLiteral(R"((?:^|::)[ \t]*([@A-Za-z_.$][@A-Za-z0-9_.$]*)[ \t]*:)"),
        QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator labels = labelDefinition.globalMatch(editor->toPlainText());
    while (labels.hasNext()) {
        const QString label = labels.next().captured(1);
        const QString folded = label.toCaseFolded();
        if (!seen.contains(folded)) {
            seen.insert(folded);
            candidates.append({label, MipsCompletionKind::Label});
        }
    }
    return candidates;
}

void MipsCompleter::rebuildModel()
{
    model->clear();
    const QVector<MipsCompletionItem> keywords = MipsHighlighter::completionItems();
    for (const MipsCompletionItem &candidate : keywords) {
        auto *item = new QStandardItem(candidate.text);
        item->setEditable(false);
        item->setData(static_cast<int>(candidate.kind), Qt::UserRole);
        model->appendRow(item);
    }
    dynamicKeys.clear();
    refreshDynamicCandidates();
}

void MipsCompleter::refreshDynamicCandidates()
{
    const QVector<MipsCompletionItem> candidates = dynamicCompletionCandidates();
    QStringList newKeys;
    for (const MipsCompletionItem &candidate : candidates)
        newKeys.append(QStringLiteral("%1:%2").arg(static_cast<int>(candidate.kind))
                       .arg(candidate.text.toCaseFolded()));
    if (newKeys == dynamicKeys)
        return;

    for (int row = model->rowCount() - 1; row >= 0; --row) {
        if (model->item(row)->data(Qt::UserRole + 1).toBool())
            model->removeRow(row);
    }

    for (const MipsCompletionItem &candidate : candidates) {
        auto *item = new QStandardItem(candidate.text);
        item->setEditable(false);
        item->setData(static_cast<int>(candidate.kind), Qt::UserRole);
        item->setData(true, Qt::UserRole + 1);
        model->appendRow(item);
    }
    dynamicKeys = newKeys;
}

void MipsCompleter::updatePopup(bool force)
{
    const QString prefix = currentPrefix();
    if (!force && prefix.size() < 2) {
        completer->popup()->hide();
        return;
    }

    completer->setCompletionPrefix(prefix);
    userSelectedSuggestion = false;
    if (completer->completionCount() == 0) {
        completer->popup()->hide();
        return;
    }

    QRect rect = editor->cursorRect();
    rect.setWidth(completer->popup()->sizeHintForColumn(0)
                  + 32 + completer->popup()->verticalScrollBar()->sizeHint().width());
    if (!completer->popup()->isVisible())
        completer->complete(rect);

    QAbstractItemView *popup = completer->popup();
    const int visibleRows = qMin(completer->completionCount(), completer->maxVisibleItems());
    int rowHeight = popup->sizeHintForRow(0);
    if (rowHeight <= 0)
        rowHeight = QFontMetrics(popup->font()).height() + 4;
    const int popupHeight = visibleRows * rowHeight + 2 * popup->frameWidth();
    popup->setMinimumHeight(0);
    popup->resize(popup->width(), popupHeight);
}

void MipsCompleter::insertCompletion(const QString &completion)
{
    completer->popup()->hide();
    userSelectedSuggestion = false;
    int start = 0;
    currentPrefix(&start);
    QTextCursor cursor = editor->textCursor();
    const int end = cursor.position();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.insertText(completion);
    editor->setTextCursor(cursor);
}
