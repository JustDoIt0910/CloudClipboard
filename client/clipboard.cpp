#include "clipboard.h"
#include "QMessageBox"
#include <QKeyEvent>
#include <QGuiApplication>
#include <fstream>
#include <string>
#include <QAction>
#include <QMenu>
#include <QMimeData>
#include <QApplication>

using namespace std;

Clipboard::Clipboard(QWidget *parent) : QWidget(parent)
{
    login = new Login;
    client = new Client("110.40.210.125", 8090);
    connect(login, &Login::loginOrRegister, this, [=](bool reg, QString username, QString password){
        this->username = username;
        this->password = password;
        client->loginOrReg(username, password, reg);
    });
    connect(client, &Client::success, this, [=](char op){
        QString opStr = op == 1 ? "Login" : "Register";
        if(op == Client::OP_LOGIN)
        {
            login->hide();
            trayIcon->show();
            if(!hasLogin)
            {
                hasLogin = true;
                ofstream ofs("account", ios::out);
                ofs << username.toStdString() << ":" << password.toStdString();
                ofs.close();
            }
        }
        else
            QMessageBox::information(login, "OK", "register success");
    });
    connect(client, &Client::fail, this, [=](char op, Client::ErrCode code){
        QString opStr = op == 1 ? "Login" : "Register";
        QString what;
        switch (code)
        {
        case Client::ERR_USERNAME_EXIST:
            what = "username already exists";
            break;
        case Client::ERR_USER_NOT_FOUND:
            what = "user not found";
            break;
        case Client::ERR_WRONG_PASSWORD:
            what = "wrong password";
            break;
        default: break;
        }
        QMessageBox::information(login, "error", what);
    });

    board = QGuiApplication::clipboard();
    connect(board, &QClipboard::dataChanged, this, &Clipboard::onBoardDataChanged);

    connect(client, &Client::hasTextData, this, [=](QString data){
        self = true;
        board->setText(data);
    });

    connect(client, &Client::hasFileData, this, [=](QByteArray& data){
        if(!fp)
        {
            filepath = "copy\nfile://"  + QApplication::applicationDirPath();
            filepath.append("/");
            filepath.append(data);
            fp = fopen(data.data(), "wb");
            return;
        }
        if(data.size() == 0)
        {
            ::fclose(fp);
            fp = nullptr;
            self = true;
            QMimeData* mime = new QMimeData;
            mime->setData("x-special/gnome-copied-files", filepath.toLocal8Bit());
            board->setMimeData(mime);
        }
        fwrite(data.data(), 1, data.size(), fp);
    });

    QMenu* menu = new QMenu(this);
    QAction* quit = new QAction("quit",this);
    menu->addAction(quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    QIcon icon = QIcon(":/images/images/logo.jpg");
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(icon);
    trayIcon->setContextMenu(menu);

    if(!checkAccount())
        login->show();
    else
        client->loginOrReg(username, password, false);
}

void Clipboard::onBoardDataChanged()
{
    if(!self)
    {
        const QMimeData* data =  board->mimeData();
        if(data->hasFormat("x-special/gnome-copied-files"))
            client->sendFile(data->text());
        else
            client->sendText(board->text());
    }
    else
        self = false;
}

bool Clipboard::checkAccount()
{
    ifstream ifs("account", ios::in);
    if(!ifs.is_open())
        return false;
    hasLogin = true;
    char buf[100];
    memset(buf, 0, sizeof(buf));
    ifs.getline(buf, sizeof(buf));
    QString line(buf);
    username = line.split(":")[0];
    password = line.split(":")[1];
    ifs.close();
}

Clipboard::~Clipboard()
{
    delete login;
    delete trayIcon;
    delete client;
}
