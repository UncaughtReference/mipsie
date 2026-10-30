#include "functionview.h"

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
#include <QMenu>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>

namespace
{
enum FunctionRoles
{
    AddressRole = Qt::UserRole + 1,
    ReturnTypeRole,
    ArgumentsRole,
    PrototypeRole
};

class FunctionFilterModel final : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setSearchText(const QString &text)
    {
        query = text.trimmed();
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (query.isEmpty())
            return true;

        const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
        return sourceModel()->data(index, AddressRole).toString().contains(query, Qt::CaseInsensitive)
            || sourceModel()->data(index, Qt::DisplayRole).toString().contains(query, Qt::CaseInsensitive)
            || sourceModel()->data(index, ReturnTypeRole).toString().contains(query, Qt::CaseInsensitive)
            || sourceModel()->data(index, ArgumentsRole).toString().contains(query, Qt::CaseInsensitive)
            || sourceModel()->data(index, PrototypeRole).toString().contains(query, Qt::CaseInsensitive);
    }

private:
    QString query;
};

class FunctionDelegate final : public QStyledItemDelegate
{
public:
    explicit FunctionDelegate(QSplitter *columnWidths, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), columnWidths(columnWidths)
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
        const int totalWidth = qMax(1, widths[0] + widths[1] + widths[2] + widths[3]);
        const int addressWidth = row.width() * widths[0] / totalWidth;
        const int returnTypeWidth = row.width() * widths[1] / totalWidth;
        const int nameWidth = row.width() * widths[2] / totalWidth;
        const QColor textColor = itemOption.state & QStyle::State_Selected
            ? itemOption.palette.color(QPalette::HighlightedText)
            : itemOption.palette.color(QPalette::Text);

        painter->save();
        painter->setPen(textColor);
        painter->setFont(itemOption.font);
        const auto drawCell = [painter, &itemOption](const QRect &cell, const QString &text) {
            const QString elided = QFontMetrics(itemOption.font).elidedText(
                text, Qt::ElideRight, qMax(0, cell.width() - 8));
            painter->drawText(cell.adjusted(4, 0, -4, 0),
                              Qt::AlignVCenter | Qt::AlignLeft, elided);
        };
        int x = row.left();
        drawCell(QRect(x, row.top(), addressWidth, row.height()),
                 index.data(AddressRole).toString());
        x += addressWidth;
        drawCell(QRect(x, row.top(), returnTypeWidth, row.height()),
                 index.data(ReturnTypeRole).toString());
        x += returnTypeWidth;
        drawCell(QRect(x, row.top(), nameWidth, row.height()),
                 index.data(Qt::DisplayRole).toString());
        x += nameWidth;
        drawCell(QRect(x, row.top(), row.right() - x + 1, row.height()),
                 index.data(ArgumentsRole).toString());
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
};
}

QString getFunctionUrl(QString functionName) {
    return "https://github.com/search?q=repo%3An64decomp%2Fsm64+" + functionName + "&type=code";
}

FunctionView::FunctionView(QWidget *parent)
    : QWidget(parent),
      searchBar(new QLineEdit(this)),
      functionList(new QListView(this)),
      filterModel(new FunctionFilterModel(this))
{
    searchBar->setPlaceholderText(tr("Search address, return type, function name, or arguments"));

    auto *sourceModel = new QStandardItemModel(this);
    QFile dataFile(QStringLiteral(":/functionview/functions.json"));
    if (dataFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(dataFile.readAll());
        const QJsonArray entries = document.object().value(QStringLiteral("entries")).toArray();
        for (const QJsonValue &entryValue : entries) {
            const QJsonObject entry = entryValue.toObject();
            auto *item = new QStandardItem(entry.value(QStringLiteral("name")).toString());
            item->setData(entry.value(QStringLiteral("address")).toString(), AddressRole);
            item->setData(entry.value(QStringLiteral("returnType")).toString(), ReturnTypeRole);
            item->setData(entry.value(QStringLiteral("arguments")).toString(), ArgumentsRole);
            item->setData(entry.value(QStringLiteral("prototype")).toString(), PrototypeRole);
            item->setToolTip(entry.value(QStringLiteral("prototype")).toString());
            sourceModel->appendRow(item);
        }
    }

    filterModel->setSourceModel(sourceModel);
    functionList->setModel(filterModel);
    functionList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    functionList->setSelectionBehavior(QAbstractItemView::SelectRows);
    functionList->setUniformItemSizes(true);
    functionList->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 0, 6, 0);
    headerLayout->setSpacing(0);
    auto *columnHeader = new QSplitter(Qt::Horizontal, header);
    columnHeader->setChildrenCollapsible(false);
    columnHeader->setHandleWidth(5);
    columnHeader->addWidget(new QLabel(tr("RAM Address"), columnHeader));
    columnHeader->addWidget(new QLabel(tr("Return Type"), columnHeader));
    columnHeader->addWidget(new QLabel(tr("Function Name"), columnHeader));
    columnHeader->addWidget(new QLabel(tr("Arguments"), columnHeader));
    columnHeader->setStretchFactor(0, 18);
    columnHeader->setStretchFactor(1, 18);
    columnHeader->setStretchFactor(2, 24);
    columnHeader->setStretchFactor(3, 40);
    columnHeader->setSizes({180, 180, 240, 400});
    headerLayout->addWidget(columnHeader);
    functionList->setItemDelegate(new FunctionDelegate(columnHeader, functionList));
    connect(columnHeader, &QSplitter::splitterMoved,
            functionList->viewport(), qOverload<>(&QWidget::update));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(searchBar);
    layout->addWidget(header);
    layout->addWidget(functionList, 1);

    connect(searchBar, &QLineEdit::textChanged, this, [this](const QString &text) {
        static_cast<FunctionFilterModel *>(filterModel)->setSearchText(text);
    });
    connect(functionList, &QListView::doubleClicked, this,
            [this](const QModelIndex &index) {
        const QString address = index.data(AddressRole).toString();
        QApplication::clipboard()->setText(QStringLiteral("jal 0x%1").arg(address));
    });
    connect(functionList, &QListView::customContextMenuRequested,
            this, [this](const QPoint &position) {
        const QModelIndex index = functionList->indexAt(position);
        if (!index.isValid())
            return;

        functionList->setCurrentIndex(index);

        QMenu contextMenu(functionList);

        QAction* copyPrototype = contextMenu.addAction("Copy prototype");
        QAction* copyJal = contextMenu.addAction("Copy JAL");
        QAction* decompSearch = contextMenu.addAction("Search in decomp");

        QAction* selection = contextMenu.exec(functionList->viewport()->mapToGlobal(position));

        if (selection == decompSearch) {
            QString decompLink = getFunctionUrl(index.data(Qt::DisplayRole).toString());
            const QUrl url(decompLink);
            if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https"))
                QDesktopServices::openUrl(url);
        } else if (selection == copyPrototype) {
            QApplication::clipboard()->setText(index.data(PrototypeRole).toString());
        } else if (selection == copyJal) {
            const QString address = index.data(AddressRole).toString();
            QApplication::clipboard()->setText(QStringLiteral("jal 0x%1").arg(address));
        }
    });
}
