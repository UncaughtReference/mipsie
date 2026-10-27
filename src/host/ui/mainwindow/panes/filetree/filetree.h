#ifndef FILETREE_H
#define FILETREE_H

#include <QWidget>

class QTreeView;
class QFileSystemModel;
class QEvent;
class QLineEdit;
class QModelIndex;
class FileTreeProxyModel;

class FileTree : public QWidget
{
    Q_OBJECT

public:
    explicit FileTree(QWidget *parent = nullptr);

    void initialize(const QString &rootPath);
    void setRootPath(const QString &rootPath);
    void focusSearch();

signals:
    void fileSelected(const QString &path);
    void fileDoubleClicked(const QString &path);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLineEdit *searchField;
    QTreeView *fileTree;
    QFileSystemModel *model;
    FileTreeProxyModel *proxyModel;

    void onItemClicked(const QModelIndex &index);
    void onItemActivated(const QModelIndex &index);
};

#endif // FILETREE_H
