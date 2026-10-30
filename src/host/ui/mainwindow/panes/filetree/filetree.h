#ifndef FILETREE_H
#define FILETREE_H

#include <QWidget>

class QTreeView;
class QFileSystemModel;
class QEvent;
class QLineEdit;
class QModelIndex;
class QPoint;
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
    QString rootPath;

    void onItemClicked(const QModelIndex &index);
    void onItemActivated(const QModelIndex &index);
    void showContextMenu(const QPoint &position);
    void createFolder(const QString &parentPath);
    void createFile(const QString &parentPath);
    void renameItem(const QString &path);
    void removeItem(const QString &path);
    void moveItem(const QString &path);
};

#endif // FILETREE_H
