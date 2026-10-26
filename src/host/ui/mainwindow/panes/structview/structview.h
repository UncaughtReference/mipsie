#ifndef STRUCTVIEW_H
#define STRUCTVIEW_H

#include "../searchablelisttable.h"

class StructView final : public SearchableListTable
{
public:
    explicit StructView(QWidget *parent = nullptr);
};

#endif // STRUCTVIEW_H
