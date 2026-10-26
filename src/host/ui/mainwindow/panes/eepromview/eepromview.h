#ifndef EEPROMVIEW_H
#define EEPROMVIEW_H

#include "../searchablelisttable.h"

class EepromView final : public SearchableListTable
{
public:
    explicit EepromView(QWidget *parent = nullptr);
};

#endif // EEPROMVIEW_H
