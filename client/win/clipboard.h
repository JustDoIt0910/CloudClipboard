#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QWidget>
#include "login.h"
#include "client.h"
#include <QSystemTrayIcon>
#include <QClipboard>
#include <stdio.h>
#include <QTextCodec>
#include <stack>

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
    bool hasDir = false;
    QString curDir;
    QString dirUriToPaste;
    QString fileUriToPaste;
    std::stack<QString> dirs;

    QTextCodec* codec;

    bool checkAccount();
    void onLoginORegister(bool reg, QString username, QString password);
    void onOperationSuccess(char op);
    void onOperationFail(char op, Client::ErrCode code);
    void onHasText(QString data);
    void onHasFileData(QByteArray& data);
    void onHasDirInfo(QString dir);
    void onBoardDataChanged();
};

#endif // CONTROLLER_H
