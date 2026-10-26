#ifndef SEARCHABLELISTTABLE_H
#define SEARCHABLELISTTABLE_H

#include <QWidget>
#include <QStringList>
#include <functional>

struct SearchableTableColumn
{
    QString key;
    QString title;
    int stretch = 1;
};

class SearchableListTable : public QWidget
{
public:
    using CopyFormatter = std::function<QString(const QStringList &)>;

    SearchableListTable(const QString &resourcePath,
                        const QList<SearchableTableColumn> &columns,
                        CopyFormatter copyFormatter,
                        QWidget *parent = nullptr);
};

#endif // SEARCHABLELISTTABLE_H
