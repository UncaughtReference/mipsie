#ifndef FILETREE_H
#define FILETREE_H

#include <QWidget>
#include <QTreeWidget>

class QTreeView;
class QFileSystemModel;
class QModelIndex;

class FileTree : public QWidget
{
    Q_OBJECT

public:
    explicit FileTree(QWidget *parent = nullptr);

    void initialize(const QString &rootPath);
    void setRootPath(const QString &rootPath);

signals:
    void fileSelected(const QString &path);
    void fileDoubleClicked(const QString &path);

private:
    QTreeView *fileTree;
    QFileSystemModel *model;

    void onItemClicked(const QModelIndex &index);
    void onItemDoubleClicked(const QModelIndex &index);
};

#endif // FILETREE_H
