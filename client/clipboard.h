#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QWidget>
#include "login.h"
#include "client.h"
#include <QSystemTrayIcon>
#include <QClipboard>
#include <stdio.h>

class Clipboard : public QWidget
{
    Q_OBJECT
public:
    explicit Clipboard(QWidget *parent = nullptr);
    ~Clipboard();

private:
    Login* login;
    Client* client;
    QClipboard *board;
    QSystemTrayIcon *trayIcon;
    bool self = false;
    QString username, password;
    bool hasLogin = false;
    FILE* fp = nullptr;
    QString filepath;

    bool checkAccount();
    void onBoardDataChanged();
};

#endif // CONTROLLER_H
