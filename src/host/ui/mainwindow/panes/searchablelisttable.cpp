#include "searchablelisttable.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace
{
constexpr int FirstColumnRole = Qt::UserRole + 1;

class SearchFilterModel final : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setSearchText(const QString &text)
    {
        query = text.trimmed();
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        if (query.isEmpty())
            return true;
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        const int columnCount = sourceModel()->data(index, Qt::UserRole).toInt();
        for (int column = 0; column < columnCount; ++column) {
            if (sourceModel()->data(index, FirstColumnRole + column).toString()
                    .contains(query, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

private:
    QString query;
};

class MultiColumnDelegate final : public QStyledItemDelegate
{
public:
    MultiColumnDelegate(QSplitter *columnWidths, int columnCount, QObject *parent)
        : QStyledItemDelegate(parent), columnWidths(columnWidths), columnCount(columnCount)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);
        const QWidget *widget = itemOption.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &itemOption, painter, widget);

        const QRect row = itemOption.rect.adjusted(6, 0, -6, 0);
        const QList<int> widths = columnWidths->sizes();
        int totalWidth = 0;
        for (int i = 0; i < columnCount; ++i)
            totalWidth += widths.value(i);
        totalWidth = qMax(1, totalWidth);

        painter->save();
        painter->setPen(itemOption.state & QStyle::State_Selected
                            ? itemOption.palette.color(QPalette::HighlightedText)
                            : itemOption.palette.color(QPalette::Text));
        painter->setFont(itemOption.font);

        const auto drawCell = [painter, &itemOption](const QRect &cell, const QString &text) {
            const QString elided = QFontMetrics(itemOption.font).elidedText(
                text, Qt::ElideRight, qMax(0, cell.width() - 8));
            painter->drawText(cell.adjusted(4, 0, -4, 0),
                              Qt::AlignVCenter | Qt::AlignLeft, elided);
        };

        int x = row.left();
        for (int i = 0; i < columnCount; ++i) {
            const int width = i == columnCount - 1
                ? row.right() - x + 1
                : row.width() * widths.value(i) / totalWidth;
            drawCell(QRect(x, row.top(), width, row.height()),
                     index.data(FirstColumnRole + i).toString());
            x += width;
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), 24));
        return size;
    }

private:
    QSplitter *columnWidths;
    int columnCount;
};
}

SearchableListTable::SearchableListTable(
    const QString &resourcePath,
    const QList<SearchableTableColumn> &columns,
    CopyFormatter copyFormatter,
    QWidget *parent)
    : QWidget(parent)
{
    auto *searchBar = new QLineEdit(this);
    searchBar->setPlaceholderText(tr("Search entries"));
    auto *listView = new QListView(this);
    listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    listView->setUniformItemSizes(true);

    auto *sourceModel = new QStandardItemModel(this);
    QFile dataFile(resourcePath);
    if (dataFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(dataFile.readAll());
        const QJsonArray entries = document.object().value(QStringLiteral("entries")).toArray();
        for (const QJsonValue &entryValue : entries) {
            const QJsonObject entry = entryValue.toObject();
            QStringList values;
            for (const SearchableTableColumn &column : columns)
                values.append(entry.value(column.key).toString());

            auto *item = new QStandardItem(values.value(0));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setData(columns.size(), Qt::UserRole);
            QStringList tooltipValues;
            for (int i = 0; i < values.size(); ++i) {
                item->setData(values[i], FirstColumnRole + i);
                if (!values[i].isEmpty())
                    tooltipValues.append(QStringLiteral("%1: %2").arg(columns[i].title, values[i]));
            }
            item->setToolTip(tooltipValues.join(QLatin1Char('\n')));
            sourceModel->appendRow(item);
        }
    }

    auto *filterModel = new SearchFilterModel(this);
    filterModel->setSourceModel(sourceModel);
    listView->setModel(filterModel);

    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 0, 6, 0);
    headerLayout->setSpacing(0);
    auto *columnHeader = new QSplitter(Qt::Horizontal, header);
    columnHeader->setChildrenCollapsible(false);
    columnHeader->setHandleWidth(5);
    QList<int> initialSizes;
    for (const SearchableTableColumn &column : columns) {
        columnHeader->addWidget(new QLabel(column.title, columnHeader));
        columnHeader->setStretchFactor(columnHeader->count() - 1, column.stretch);
        initialSizes.append(qMax(1, column.stretch) * 10);
    }
    columnHeader->setSizes(initialSizes);
    headerLayout->addWidget(columnHeader);
    listView->setItemDelegate(new MultiColumnDelegate(columnHeader, columns.size(), listView));
    connect(columnHeader, &QSplitter::splitterMoved,
            listView->viewport(), qOverload<>(&QWidget::update));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(searchBar);
    layout->addWidget(header);
    layout->addWidget(listView, 1);

    connect(searchBar, &QLineEdit::textChanged, this,
            [filterModel](const QString &text) { filterModel->setSearchText(text); });
    connect(listView, &QListView::doubleClicked, this,
            [copyFormatter](const QModelIndex &index) {
        if (!copyFormatter)
            return;
        QStringList values;
        const int columnCount = index.data(Qt::UserRole).toInt();
        for (int i = 0; i < columnCount; ++i)
            values.append(index.data(FirstColumnRole + i).toString());
        const QString text = copyFormatter(values);
        if (!text.isEmpty())
            QApplication::clipboard()->setText(text);
    });
}
