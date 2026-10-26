#ifndef MENUBAR_H
#define MENUBAR_H

#include "mainwindow.h"

class MenuBar
{
public:
    void initialize();

    MenuBar(Ui::MainWindow *ui, MainWindow *window);
private:
    Ui::MainWindow *ui;
    MainWindow *window;
};

#endif // MENUBAR_H
