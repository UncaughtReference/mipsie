#include "filetree.h"

#include <QAction>
#include <QAbstractItemView>
#include <QFile>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
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
    model->setReadOnly(false);
    fileTree->setHeaderHidden(true);
    fileTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fileTree->installEventFilter(this);
    fileTree->setContextMenuPolicy(Qt::CustomContextMenu);
    fileTree->setDragEnabled(true);
    fileTree->setAcceptDrops(true);
    fileTree->setDropIndicatorShown(true);
    fileTree->setDragDropMode(QAbstractItemView::InternalMove);
    fileTree->setDefaultDropAction(Qt::MoveAction);

    auto *renameShortcut = new QAction(this);
    renameShortcut->setShortcut(QKeySequence(Qt::Key_F2));
    renameShortcut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    fileTree->addAction(renameShortcut);
    connect(renameShortcut, &QAction::triggered, this, [this] {
        const QModelIndex sourceIndex = proxyModel->mapToSource(fileTree->currentIndex());
        if (!sourceIndex.isValid())
            return;
        const QString path = model->filePath(sourceIndex);
        if (QDir::cleanPath(path) != QDir::cleanPath(this->rootPath))
            renameItem(path);
    });

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
    connect(fileTree, &QTreeView::customContextMenuRequested,
            this, &FileTree::showContextMenu);

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
        this->rootPath.clear();
        searchField->clear();
        proxyModel->setSearchRootPath(QString());
        model->setRootPath(QString());
        fileTree->setRootIndex(QModelIndex());
        searchField->hide();
        fileTree->hide();
        return;
    }

    const QString absolutePath = QDir(rootPath).absolutePath();
    this->rootPath = absolutePath;
    searchField->clear();
    proxyModel->setSearchRootPath(absolutePath);
    model->setRootPath(absolutePath);
    fileTree->setRootIndex(proxyModel->mapFromSource(model->index(absolutePath)));
    searchField->show();
    fileTree->show();
}

void FileTree::showContextMenu(const QPoint &position)
{
    const QModelIndex clickedIndex = fileTree->indexAt(position);
    if (clickedIndex.isValid()) {
        fileTree->setCurrentIndex(clickedIndex);
    } else {
        fileTree->clearSelection();
        fileTree->setCurrentIndex(QModelIndex());
    }

    const QModelIndex sourceIndex = proxyModel->mapToSource(fileTree->currentIndex());
    const QString selectedPath = sourceIndex.isValid() ? model->filePath(sourceIndex) : QString();
    const QFileInfo selectedInfo(selectedPath);
    const QString parentPath = selectedPath.isEmpty()
        ? rootPath
        : (selectedInfo.isDir() ? selectedPath : selectedInfo.dir().absolutePath());
    const bool selectedIsRoot = !selectedPath.isEmpty()
        && QDir::cleanPath(selectedPath) == QDir::cleanPath(rootPath);

    QMenu menu(this);
    QAction *newFolderAction = menu.addAction(tr("New Folder..."));
    QAction *newFileAction = menu.addAction(tr("New File..."));
    QAction *renameAction = menu.addAction(tr("Rename..."));
    QAction *moveAction = menu.addAction(tr("Move to..."));
    QAction *removeAction = menu.addAction(tr("Delete..."));
    const bool hasSelection = !selectedPath.isEmpty();
    renameAction->setEnabled(hasSelection && !selectedIsRoot);
    moveAction->setEnabled(hasSelection && !selectedIsRoot);
    removeAction->setEnabled(hasSelection && !selectedIsRoot);

    QAction *chosenAction = menu.exec(fileTree->viewport()->mapToGlobal(position));
    if (chosenAction == newFolderAction)
        createFolder(parentPath);
    else if (chosenAction == newFileAction)
        createFile(parentPath);
    else if (chosenAction == renameAction)
        renameItem(selectedPath);
    else if (chosenAction == moveAction)
        moveItem(selectedPath);
    else if (chosenAction == removeAction)
        removeItem(selectedPath);
}

void FileTree::createFolder(const QString &parentPath)
{
    if (parentPath.isEmpty())
        return;

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("New Folder"), tr("Folder name:"), QLineEdit::Normal,
        QString(), &accepted).trimmed();
    if (!accepted || name.isEmpty())
        return;
    if (QFileInfo(name).fileName() != name || name == QStringLiteral(".")
        || name == QStringLiteral("..")) {
        QMessageBox::warning(this, tr("Invalid Name"),
                             tr("Enter a folder name without a path."));
        return;
    }

    if (!QDir(parentPath).mkdir(name))
        QMessageBox::warning(this, tr("Create Folder Failed"),
                             tr("Could not create folder '%1'.").arg(name));
}

void FileTree::createFile(const QString &parentPath)
{
    if (parentPath.isEmpty())
        return;

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("New File"), tr("File name:"), QLineEdit::Normal,
        QStringLiteral(".asm"), &accepted).trimmed();
    if (!accepted || name.isEmpty())
        return;
    if (QFileInfo(name).fileName() != name || name == QStringLiteral(".")
        || name == QStringLiteral("..")) {
        QMessageBox::warning(this, tr("Invalid Name"),
                             tr("Enter a file name without a path."));
        return;
    }

    QFile file(QDir(parentPath).filePath(name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        QMessageBox::warning(this, tr("Create File Failed"),
                             tr("Could not create '%1': %2")
                                 .arg(name, file.errorString()));
    }
}

void FileTree::renameItem(const QString &path)
{
    if (path.isEmpty())
        return;

    const QFileInfo info(path);
    bool accepted = false;
    const QString newName = QInputDialog::getText(
        this, tr("Rename"), tr("New name:"), QLineEdit::Normal,
        info.fileName(), &accepted).trimmed();
    if (!accepted || newName.isEmpty() || newName == info.fileName())
        return;
    if (QFileInfo(newName).fileName() != newName || newName == QStringLiteral(".")
        || newName == QStringLiteral("..")) {
        QMessageBox::warning(this, tr("Invalid Name"),
                             tr("Enter a name without a path."));
        return;
    }

    const QString targetPath = info.dir().filePath(newName);
    if (QFileInfo::exists(targetPath)
        || !QDir().rename(path, targetPath)) {
        QMessageBox::warning(this, tr("Rename Failed"),
                             tr("Could not rename '%1'.").arg(info.fileName()));
    }
}

void FileTree::removeItem(const QString &path)
{
    if (path.isEmpty())
        return;

    const QFileInfo info(path);
    const bool isDirectory = info.isDir() && !info.isSymLink();
    const QString kind = isDirectory
        ? tr("folder and all of its contents")
        : tr("file");
    if (QMessageBox::question(
            this, tr("Delete Item"),
            tr("Permanently delete this %1?\n\n%2")
                .arg(kind, QDir::toNativeSeparators(path)),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }

    const bool removed = isDirectory ? QDir(path).removeRecursively() : QFile::remove(path);
    if (!removed)
        QMessageBox::warning(this, tr("Delete Failed"),
                             tr("Could not delete '%1'.")
                                 .arg(QDir::toNativeSeparators(path)));
}

void FileTree::moveItem(const QString &path)
{
    if (path.isEmpty())
        return;

    const QFileInfo sourceInfo(path);
    const QString destination = QFileDialog::getExistingDirectory(
        this, tr("Move Item To"), rootPath, QFileDialog::ShowDirsOnly);
    if (destination.isEmpty())
        return;

    const QString destinationPath = QDir(destination).absolutePath();
    const QString sourcePath = QDir(path).absolutePath();
    if (QDir(sourceInfo.absolutePath()).absolutePath() == destinationPath)
        return;
    if (sourceInfo.isDir() && !sourceInfo.isSymLink()) {
        const QString relativePath = QDir(sourcePath).relativeFilePath(destinationPath);
        const QString normalizedRelativePath = QDir::fromNativeSeparators(relativePath);
        const bool destinationIsOutside = normalizedRelativePath == QStringLiteral("..")
            || normalizedRelativePath.startsWith(QStringLiteral("../"));
        if (relativePath == QStringLiteral(".")
            || (!destinationIsOutside && !QDir::isAbsolutePath(relativePath))) {
            QMessageBox::warning(this, tr("Invalid Destination"),
                                 tr("A folder cannot be moved into itself or one of its subfolders."));
            return;
        }
    }

    const QString targetPath = QDir(destinationPath).filePath(sourceInfo.fileName());
    if (QFileInfo::exists(targetPath)) {
        QMessageBox::warning(this, tr("Move Failed"),
                             tr("An item with that name already exists in the destination."));
        return;
    }
    if (QDir().rename(sourcePath, targetPath))
        return;

    QMessageBox::warning(this, tr("Move Failed"),
                         tr("Could not move '%1' to '%2'.")
                             .arg(QDir::toNativeSeparators(sourcePath),
                                  QDir::toNativeSeparators(destinationPath)));
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
    if (watched == fileTree && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            const QModelIndex index = fileTree->currentIndex();
            if (index.isValid()) {
                const QModelIndex sourceIndex = proxyModel->mapToSource(index);
                if (model->fileInfo(sourceIndex).isDir()) {
                    fileTree->setExpanded(index, !fileTree->isExpanded(index));
                    return true;
                }
            }
        }
    }

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
