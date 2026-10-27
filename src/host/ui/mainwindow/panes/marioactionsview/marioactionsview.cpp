#include "marioactionsview.h"

MarioActionsView::MarioActionsView(QWidget *parent)
    : SearchableListTable(
          QStringLiteral(":/marioactionsview/actions.json"),
          {{QStringLiteral("section"), tr("Table"), 18},
           {QStringLiteral("action"), tr("Action"), 20},
           {QStringLiteral("group"), tr("Group"), 14},
           {QStringLiteral("handler"), tr("Handler Function"), 22},
           {QStringLiteral("description"), tr("Description"), 40}},
          [](const QStringList &values) {
              return values.value(1);
          }, parent)
{
}
