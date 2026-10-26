#ifndef SPLITTER_H
#define SPLITTER_H

#include <QSplitter>
#include <QList>
#include "panes/pane.h"

class QShowEvent;

class Splitter : public QSplitter
{
    Q_OBJECT

public:
    explicit Splitter(Qt::Orientation orientation = Qt::Horizontal, QWidget *parent = nullptr);
    void addPane(Pane *pane);
    // Register panes nested under this splitter so their menus can use the
    // root splitter's split, delete, and content-change handling.
    void registerPane(Pane *pane);
    void setInitialSizes(const QList<int> &sizes);

signals:
    void contentRequested(Pane *pane, PaneKind kind);

protected:
    void showEvent(QShowEvent *event) override;

private:
    QList<int> initialSizes;
    bool initialSizesApplied = false;

    void splitPane(Pane *pane, Qt::Orientation orientation);
};

#endif // SPLITTER_H
