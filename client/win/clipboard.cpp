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
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

Clipboard::Clipboard(QWidget *parent) : QWidget(parent)
{
    login = new Login;
    client = new Client("110.40.210.125", 8090);
    codec = QTextCodec::codecForName("GB2312");
    board = QGuiApplication::clipboard();

    connect(login, &Login::loginOrRegister, this, &Clipboard::onLoginORegister);
    connect(client, &Client::success, this, &Clipboard::onOperationSuccess);
    connect(client, &Client::fail, this, &Clipboard::onOperationFail);
    connect(board, &QClipboard::dataChanged, this, &Clipboard::onBoardDataChanged);
    connect(client, &Client::hasTextData, this, &Clipboard::onHasText);
    connect(client, &Client::hasFileData, this, &Clipboard::onHasFileData);
    connect(client, &Client::hasDirInfo, this, &Clipboard::onHasDirInfo);

    curDir = QApplication::applicationDirPath() + "/fileRecv";
    struct stat st;
    if(::stat(codec->fromUnicode(curDir).data(), &st) != 0)
        mkdir(codec->fromUnicode(curDir).data());

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

void Clipboard::onLoginORegister(bool reg, QString username, QString password)
{
    this->username = username;
    this->password = password;
    client->loginOrReg(username, password, reg);
}

void Clipboard::onOperationSuccess(char op)
{
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
}

void Clipboard::onOperationFail(char op, Client::ErrCode code)
{
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
    QMessageBox::information(login, opStr + "error", what);
}

void Clipboard::onHasText(QString data)
{
    self = true;
    board->setText(data);
}

void Clipboard::onHasFileData(QByteArray& data)
{
    if(!fp)
    {
        fileUriToPaste = curDir + "/" + data;
        fp = fopen(codec->fromUnicode(fileUriToPaste).data(), "wb");
        return;
    }
    if(data.size() == 0)
    {
        ::fclose(fp);
        fp = nullptr;
        if(!hasDir)
        {
            QMimeData* mime = new QMimeData;
            QString uri = "file:///" + fileUriToPaste;
            self = true;
            mime->setData("text/uri-list", uri.toUtf8());
            board->setMimeData(mime);
        }
        return;
    }
    fwrite(data.data(), 1, data.size(), fp);
}

void Clipboard::onHasDirInfo(QString dir)
{
    if(dir.length() != 0)
    {
        QString newDir = curDir + "/" + dir;
        if(!hasDir)
        {
            hasDir = true;
            dirUriToPaste = "file:///" + newDir;
        }
        dirs.push(curDir);
        ::mkdir(codec->fromUnicode(newDir).data());
        curDir = newDir;
    }
    else
    {
        curDir = dirs.top();
        dirs.pop();
        if(dirs.empty())
        {
            hasDir = false;
            QMimeData* mime = new QMimeData;
            self = true;
            mime->setData("text/uri-list", dirUriToPaste.toUtf8());
            board->setMimeData(mime);
        }
    }
}

void Clipboard::onBoardDataChanged()
{
    if(!self)
    {
        const QMimeData* data =  board->mimeData();
        if(data->hasFormat("text/uri-list"))
        {
            QString path = data->text().split("///")[1];
            struct stat st;
            if(::stat(codec->fromUnicode(path).data(), &st) == 0)
            {
                if(S_ISDIR(st.st_mode))
                    client->sendDir(path);
                else
                    client->sendFile(path);
            }
        }
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
    return true;
}

Clipboard::~Clipboard()
{
    delete login;
    delete trayIcon;
    delete client;
}
