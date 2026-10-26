#include "ramaddressview.h"

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
enum AddressRoles
{
    AddressRole = Qt::UserRole + 1,
    LengthRole,
    TypeRole,
    DescriptionRole,
    NotesRole
};

class AddressFilterModel final : public QSortFilterProxyModel
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
        const int roles[] = {AddressRole, LengthRole, TypeRole, DescriptionRole, NotesRole};
        for (int role : roles) {
            if (sourceModel()->data(index, role).toString().contains(query, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

private:
    QString query;
};

class AddressDelegate final : public QStyledItemDelegate
{
public:
    explicit AddressDelegate(QSplitter *columnWidths, QObject *parent = nullptr)
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
        int totalWidth = 0;
        for (int width : widths)
            totalWidth += width;
        totalWidth = qMax(1, totalWidth);

        const int addressWidth = row.width() * widths[0] / totalWidth;
        const int lengthWidth = row.width() * widths[1] / totalWidth;
        const int typeWidth = row.width() * widths[2] / totalWidth;
        const int descriptionWidth = row.width() * widths[3] / totalWidth;

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
        drawCell(QRect(x, row.top(), addressWidth, row.height()), index.data(AddressRole).toString());
        x += addressWidth;
        drawCell(QRect(x, row.top(), lengthWidth, row.height()), index.data(LengthRole).toString());
        x += lengthWidth;
        drawCell(QRect(x, row.top(), typeWidth, row.height()), index.data(TypeRole).toString());
        x += typeWidth;
        drawCell(QRect(x, row.top(), descriptionWidth, row.height()), index.data(DescriptionRole).toString());
        x += descriptionWidth;
        drawCell(QRect(x, row.top(), row.right() - x + 1, row.height()), index.data(NotesRole).toString());
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

RamAddressView::RamAddressView(QWidget *parent)
    : QWidget(parent),
      searchBar(new QLineEdit(this)),
      addressList(new QListView(this)),
      filterModel(new AddressFilterModel(this))
{
    searchBar->setPlaceholderText(tr("Search address, length, type, description, or notes"));

    auto *sourceModel = new QStandardItemModel(this);
    QFile dataFile(QStringLiteral(":/ramaddressview/ram_addresses.json"));
    if (dataFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(dataFile.readAll());
        const QJsonArray entries = document.object().value(QStringLiteral("entries")).toArray();
        for (const QJsonValue &entryValue : entries) {
            const QJsonObject entry = entryValue.toObject();
            auto *item = new QStandardItem(entry.value(QStringLiteral("description")).toString());
            item->setData(entry.value(QStringLiteral("address")).toString(), AddressRole);
            item->setData(entry.value(QStringLiteral("length")).toString(), LengthRole);
            item->setData(entry.value(QStringLiteral("type")).toString(), TypeRole);
            item->setData(entry.value(QStringLiteral("description")).toString(), DescriptionRole);
            item->setData(entry.value(QStringLiteral("notes")).toString(), NotesRole);
            item->setToolTip(entry.value(QStringLiteral("notes")).toString());
            sourceModel->appendRow(item);
        }
    }

    filterModel->setSourceModel(sourceModel);
    addressList->setModel(filterModel);
    addressList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    addressList->setSelectionBehavior(QAbstractItemView::SelectRows);
    addressList->setUniformItemSizes(true);

    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 0, 6, 0);
    headerLayout->setSpacing(0);
    auto *columnHeader = new QSplitter(Qt::Horizontal, header);
    columnHeader->setChildrenCollapsible(false);
    columnHeader->setHandleWidth(5);
    columnHeader->addWidget(new QLabel(tr("RAM Address"), columnHeader));
    columnHeader->addWidget(new QLabel(tr("Length"), columnHeader));
    columnHeader->addWidget(new QLabel(tr("Type"), columnHeader));
    columnHeader->addWidget(new QLabel(tr("Description"), columnHeader));
    columnHeader->addWidget(new QLabel(tr("Notes"), columnHeader));
    const int columnStretch[] = {18, 12, 14, 36, 20};
    columnHeader->setSizes({180, 120, 140, 360, 200});
    for (int i = 0; i < 5; ++i)
        columnHeader->setStretchFactor(i, columnStretch[i]);
    headerLayout->addWidget(columnHeader);
    addressList->setItemDelegate(new AddressDelegate(columnHeader, addressList));
    connect(columnHeader, &QSplitter::splitterMoved,
            addressList->viewport(), qOverload<>(&QWidget::update));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(searchBar);
    layout->addWidget(header);
    layout->addWidget(addressList, 1);

    connect(searchBar, &QLineEdit::textChanged, this, [this](const QString &text) {
        static_cast<AddressFilterModel *>(filterModel)->setSearchText(text);
    });
    connect(addressList, &QListView::doubleClicked, this,
            [](const QModelIndex &index) {
        QApplication::clipboard()->setText(
            QStringLiteral("jal 0x%1").arg(index.data(AddressRole).toString()));
    });
}
