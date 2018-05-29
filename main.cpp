#include "drimgwidgetbase.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    drimgwidgetbase w;
    w.show();

    return a.exec();
}
