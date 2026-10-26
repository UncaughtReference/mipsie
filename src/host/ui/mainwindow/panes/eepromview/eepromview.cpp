#include "eepromview.h"

EepromView::EepromView(QWidget *parent)
    : SearchableListTable(
          QStringLiteral(":/eepromview/eeprom.json"),
          {{QStringLiteral("section"), tr("Table"), 24},
           {QStringLiteral("range"), tr("Range"), 22},
           {QStringLiteral("description"), tr("Description"), 54}},
          [](const QStringList &values) {
              return values.value(1);
          }, parent)
{
}
