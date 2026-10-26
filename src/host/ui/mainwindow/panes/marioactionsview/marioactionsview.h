#ifndef MARIOACTIONSVIEW_H
#define MARIOACTIONSVIEW_H

#include "../searchablelisttable.h"

class MarioActionsView final : public SearchableListTable
{
public:
    explicit MarioActionsView(QWidget *parent = nullptr);
};

#endif // MARIOACTIONSVIEW_H
