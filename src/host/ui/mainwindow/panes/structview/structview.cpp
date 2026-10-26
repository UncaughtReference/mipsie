#include "structview.h"

StructView::StructView(QWidget *parent)
    : SearchableListTable(
          QStringLiteral(":/structview/structs.json"),
          {{QStringLiteral("name"), tr("Name"), 18},
           {QStringLiteral("offset"), tr("Offset"), 15},
           {QStringLiteral("type"), tr("Type"), 17},
           {QStringLiteral("description"), tr("Description"), 50}},
          [](const QStringList &values) { return values.value(1); }, parent)
{
}
