#include "filetree.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QVBoxLayout>

class FileTreeProxyModel : public QSortFilterProxyModel
{
public:
    explicit FileTreeProxyModel(QFileSystemModel *fileSystemModel, QObject *parent = nullptr)
        : QSortFilterProxyModel(parent),
          fileSystemModel(fileSystemModel)
    {
        setRecursiveFilteringEnabled(true);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
    }

    void setSearchRootPath(const QString &path)
    {
        searchRootPath = QDir::cleanPath(path);
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        if (isSearchRootOrAncestor(sourceIndex))
            return true;

        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }

private:
    QFileSystemModel *fileSystemModel;
    QString searchRootPath;

    bool isSearchRootOrAncestor(const QModelIndex &index) const
    {
        if (!index.isValid() || searchRootPath.isEmpty())
            return false;

        QString candidatePath = QDir::cleanPath(fileSystemModel->filePath(index));
        if (candidatePath.compare(searchRootPath, Qt::CaseInsensitive) == 0)
            return true;

        if (!candidatePath.endsWith(QLatin1Char('/')))
            candidatePath.append(QLatin1Char('/'));
        return searchRootPath.startsWith(candidatePath, Qt::CaseInsensitive);
    }
};

FileTree::FileTree(QWidget *parent)
    : QWidget(parent),
    searchField(new QLineEdit(this)),
    fileTree(new QTreeView(this)),
    model(new QFileSystemModel(this)),
    proxyModel(new FileTreeProxyModel(model, this))
{
}

void FileTree::initialize(const QString &rootPath)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    searchField->setPlaceholderText(tr("Search files..."));
    searchField->setClearButtonEnabled(true);
    searchField->installEventFilter(this);

    proxyModel->setSourceModel(model);
    fileTree->setModel(proxyModel);
    fileTree->setHeaderHidden(true);

    // Hide size, type, and modification date columns
    for (int column = 1; column < model->columnCount(); ++column)
        fileTree->hideColumn(column);

    fileTree->setAnimated(true);
    fileTree->setIndentation(16);

    layout->addWidget(searchField);
    layout->addWidget(fileTree);

    connect(searchField, &QLineEdit::textChanged,
            this, [this](const QString &text) {
        proxyModel->setFilterFixedString(text.trimmed());
    });

    connect(fileTree, &QTreeView::clicked,
            this, &FileTree::onItemClicked);

    connect(fileTree, &QTreeView::activated,
            this, &FileTree::onItemActivated);

    setRootPath(rootPath);
}

void FileTree::focusSearch()
{
    if (!searchField->isVisible())
        return;
    searchField->setFocus(Qt::ShortcutFocusReason);
    searchField->selectAll();
}

void FileTree::setRootPath(const QString &rootPath)
{
    if (rootPath.trimmed().isEmpty()) {
        searchField->clear();
        proxyModel->setSearchRootPath(QString());
        model->setRootPath(QString());
        fileTree->setRootIndex(QModelIndex());
        searchField->hide();
        fileTree->hide();
        return;
    }

    const QString absolutePath = QDir(rootPath).absolutePath();
    searchField->clear();
    proxyModel->setSearchRootPath(absolutePath);
    model->setRootPath(absolutePath);
    fileTree->setRootIndex(proxyModel->mapFromSource(model->index(absolutePath)));
    searchField->show();
    fileTree->show();
}

void FileTree::onItemClicked(const QModelIndex &index)
{
    const QModelIndex sourceIndex = proxyModel->mapToSource(index);
    const QFileInfo info = model->fileInfo(sourceIndex);

    if (info.isFile())
        emit fileSelected(info.absoluteFilePath());
}

void FileTree::onItemActivated(const QModelIndex &index)
{
    const QModelIndex sourceIndex = proxyModel->mapToSource(index);
    const QFileInfo info = model->fileInfo(sourceIndex);

    if (info.isFile())
        emit fileDoubleClicked(info.absoluteFilePath());
}

bool FileTree::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == searchField && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Down) {
            const QModelIndex firstItem = proxyModel->index(0, 0, fileTree->rootIndex());
            if (firstItem.isValid()) {
                fileTree->setCurrentIndex(firstItem);
                fileTree->selectionModel()->select(
                    firstItem, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                fileTree->scrollTo(firstItem);
                fileTree->setFocus(Qt::ShortcutFocusReason);
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
