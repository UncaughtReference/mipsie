#include "splitter.h"
#include "panes/pane.h"

#include <QShowEvent>
#include <QKeySequence>
#include <QTimer>

Splitter::Splitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent)
{
}

void Splitter::addPane(Pane *pane)
{
    addWidget(pane);
    registerPane(pane);
}

void Splitter::setInitialSizes(const QList<int> &sizes)
{
    initialSizes = sizes;
    if (isVisible())
        setSizes(initialSizes);
}

void Splitter::showEvent(QShowEvent *event)
{
    QSplitter::showEvent(event);

    if (initialSizesApplied || initialSizes.isEmpty())
        return;

    initialSizesApplied = true;
    QTimer::singleShot(0, this, [this] {
        if (count() == initialSizes.size())
            setSizes(initialSizes);
    });
}

void Splitter::registerPane(Pane *pane)
{
    rootSplitter()->addPaneShortcut(pane);
    connect(pane, &Pane::splitRequested, this, &Splitter::splitPane);
    connect(pane, &Pane::contentRequested, this, &Splitter::contentRequested);
    connect(pane, &Pane::deleteRequested, this, [this](Pane *paneToDelete) {
        auto *parentSplitter = qobject_cast<Splitter *>(paneToDelete->parentWidget());
        if (!parentSplitter)
            return;

        const int paneIndex = parentSplitter->indexOf(paneToDelete);
        if (paneIndex < 0)
            return;

        rootSplitter()->removePaneShortcut(paneToDelete);
        parentSplitter->widget(paneIndex)->hide();
        parentSplitter->widget(paneIndex)->setParent(nullptr);
        delete paneToDelete;

        // Keep the root splitter alive, but collapse nested splitters that
        // have been reduced to one remaining pane.
        if (parentSplitter == this || parentSplitter->count() != 1)
            return;

        QWidget *remainingWidget = parentSplitter->widget(0);
        auto *grandparentSplitter = qobject_cast<Splitter *>(parentSplitter->parentWidget());
        if (!grandparentSplitter)
            return;

        const int splitterIndex = grandparentSplitter->indexOf(parentSplitter);
        if (splitterIndex < 0)
            return;

        grandparentSplitter->replaceWidget(splitterIndex, remainingWidget);
        delete parentSplitter;
    });
}

Splitter *Splitter::rootSplitter()
{
    Splitter *root = this;
    QWidget *ancestor = parentWidget();
    while (ancestor) {
        if (auto *splitter = qobject_cast<Splitter *>(ancestor))
            root = splitter;
        ancestor = ancestor->parentWidget();
    }
    return root;
}

void Splitter::addPaneShortcut(Pane *pane)
{
    if (shortcutPanes.contains(pane))
        return;
    shortcutPanes.append(pane);
    refreshPaneShortcuts();
}

void Splitter::removePaneShortcut(Pane *pane)
{
    shortcutPanes.removeAll(pane);
    refreshPaneShortcuts();
}

void Splitter::refreshPaneShortcuts()
{
    for (int index = 0; index < shortcutPanes.size(); ++index) {
        const QKeySequence shortcut = index < 9
            ? QKeySequence(Qt::CTRL | (Qt::Key_1 + index))
            : QKeySequence();
        shortcutPanes.at(index)->setPaneShortcut(shortcut);
    }
}

void Splitter::splitPane(Pane *pane, Qt::Orientation orientation)
{
    auto *parentSplitter = qobject_cast<Splitter *>(pane->parentWidget());
    if (!parentSplitter)
        return;

    const int index = parentSplitter->indexOf(pane);
    if (index < 0)
        return;

    const QList<int> parentSizes = parentSplitter->sizes();
    auto *nestedSplitter = new Splitter(orientation);
    if (parentSplitter->replaceWidget(index, nestedSplitter) != pane) {
        delete nestedSplitter;
        return;
    }

    nestedSplitter->addWidget(pane);
    auto *emptyPane = new Pane;
    nestedSplitter->addWidget(emptyPane);
    nestedSplitter->setStretchFactor(0, 1);
    nestedSplitter->setStretchFactor(1, 1);
    nestedSplitter->setSizes({1000, 1000});

    registerPane(emptyPane);
    if (parentSizes.size() == parentSplitter->count())
        parentSplitter->setSizes(parentSizes);
}
