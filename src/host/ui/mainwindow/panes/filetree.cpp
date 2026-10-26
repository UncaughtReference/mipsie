#include "filetree.h"

#include <QLayout>
#include <QDir>
#include <QTreeWidget>
#include <QFileSystemModel>

FileTree::FileTree(QWidget *parent)
    : QWidget(parent),
    fileTree(new QTreeView(this)),
    model(new QFileSystemModel(this))
{
}

void FileTree::initialize(const QString &rootPath)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    fileTree->setModel(model);
    fileTree->setHeaderHidden(true);

    // Hide size, type, and modification date columns
    for (int column = 1; column < model->columnCount(); ++column)
        fileTree->hideColumn(column);

    fileTree->setAnimated(true);
    fileTree->setIndentation(16);

    layout->addWidget(fileTree);

    connect(fileTree, &QTreeView::clicked,
            this, &FileTree::onItemClicked);

    connect(fileTree, &QTreeView::doubleClicked,
            this, &FileTree::onItemDoubleClicked);

    setRootPath(rootPath);
}

void FileTree::setRootPath(const QString &rootPath)
{
    if (rootPath.trimmed().isEmpty()) {
        model->setRootPath(QString());
        fileTree->setRootIndex(QModelIndex());
        fileTree->hide();
        return;
    }

    const QString absolutePath = QDir(rootPath).absolutePath();
    model->setRootPath(absolutePath);
    fileTree->setRootIndex(model->index(absolutePath));
    fileTree->show();
}

void FileTree::onItemClicked(const QModelIndex &index)
{
    QFileInfo info = model->fileInfo(index);

    if (info.isFile())
        emit fileSelected(info.absoluteFilePath());
}

void FileTree::onItemDoubleClicked(const QModelIndex &index)
{
    QFileInfo info = model->fileInfo(index);

    if (info.isFile())
        emit fileDoubleClicked(info.absoluteFilePath());
}
