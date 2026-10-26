#ifndef FUNCTIONVIEW_H
#define FUNCTIONVIEW_H

#include <QWidget>

class QLineEdit;
class QListView;
class QSortFilterProxyModel;

class FunctionView : public QWidget
{
    Q_OBJECT

public:
    explicit FunctionView(QWidget *parent = nullptr);

private:
    QLineEdit *searchBar;
    QListView *functionList;
    QSortFilterProxyModel *filterModel;
};

#endif // FUNCTIONVIEW_H
