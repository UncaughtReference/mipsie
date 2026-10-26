#ifndef RAMADDRESSVIEW_H
#define RAMADDRESSVIEW_H

#include <QWidget>

class QLineEdit;
class QListView;
class QSortFilterProxyModel;

class RamAddressView : public QWidget
{
    Q_OBJECT

public:
    explicit RamAddressView(QWidget *parent = nullptr);

private:
    QLineEdit *searchBar;
    QListView *addressList;
    QSortFilterProxyModel *filterModel;
};

#endif // RAMADDRESSVIEW_H
