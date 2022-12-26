#include "clipboard.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Clipboard board;
    return a.exec();
}
