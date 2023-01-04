#include "client.h"
#include <QHostAddress>
#include <QCryptographicHash>
#include <QtEndian>
#include <QThread>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


const static int BUFSIZE = 60 * 1024;

Client::Client(QString _ip, quint16 _port):
    ip(_ip), port(_port), noAck(0)
{
    client = new QTcpSocket();
    codec = QTextCodec::codecForName("GB2312");
    timer = new QTimer();
    timer->setInterval(20 * 1000);

    connect(timer, &QTimer::timeout, this, &Client::onTimeout);
    connect(client, &QTcpSocket::readyRead, this, &Client::decode);
    connect(client, &QTcpSocket::connected, this, &Client::onConnected);
}

void Client::loginOrReg(QString username, QString password, bool reg)
{
    this->username = username;
    this->password = password;
    this->reg = reg;
    client->disconnectFromHost();
    client->connectToHost(QHostAddress(ip), port);
}

void Client::onConnected()
{
    timer->setInterval(20 * 1000);
    retrying = false;
    char op = reg ? 0 : 1;
    QString pwd = password + "cdq3kf9dh";
    QByteArray encoded = QCryptographicHash::hash(pwd.toLatin1(), QCryptographicHash::Md5);
    QString encodedPwd = encoded.toHex();

    QByteArray userData = username.toLatin1();
    QByteArray pwdData = encodedPwd.toLatin1();
    QByteArray pkt;
    pkt.append(op);
    uchar buf[2];
    quint16 len = userData.size() << 8 | pwdData.size();
    qToBigEndian(len, buf);
    pkt.append((char*)buf, 2);
    pkt.append(userData);
    pkt.append(pwdData);
    client->write(pkt);
}

void Client::decode()
{
    QByteArray resp = client->readAll();
    inputBuf.append(resp);
    while(inputBuf.size() >= 2)
    {
        if(inputBuf.at(0) == OP_HEARTBEAT)
        {
            if(inputBuf.at(1) != HB_ACK)
                qDebug() << "invalid heartbeat ack";
            else
                noAck = 0;
            inputBuf.remove(0, 2);
        }
        if(inputBuf.size() >= 3)
        {file:///F:/qtProjects/build-clipboard-Desktop_Qt_5_12_0_MinGW_64_bit-Release/release/fileRecv/'0
            if(inputBuf.at(0) == OP_SUCCESS_RESP || inputBuf.at(0) == OP_FAIL_RESP)
            {
                if(inputBuf.at(0) == OP_SUCCESS_RESP)
                {
                    if(inputBuf.at(1) == OP_LOGIN)
                        timer->start();
                    emit success(inputBuf.at(1));
                }
                else
                {
                    client->disconnectFromHost();
                    emit fail(inputBuf.at(1), static_cast<ErrCode>(inputBuf.at(2)));
                }
                inputBuf.remove(0, 3);
            }
            else if(inputBuf.at(0) == OP_DATA)
            {
                if(inputBuf.size() < 4)
                    break;
                char mime = inputBuf.at(1);
                uint16_t len = inputBuf.at(2);
                len = len << 8 | (inputBuf.at(3) & 0xFF);
                if(inputBuf.size() >= 4 + len)
                {
                    inputBuf.remove(0, 4);
                    QByteArray data = inputBuf.left(len);
                    if(mime == MIME_TEXT)
                        emit hasTextData(QString(data));
                    else if(mime == MIME_FILE)
                        emit hasFileData(data);
                    else
                        emit hasDirInfo(QString(data));
                    inputBuf.remove(0, len);
                }
                else
                    break;

            }
            else
                inputBuf.clear();
        }
    }
}

void Client::onTimeout()
{
    if(retrying)
        client->connectToHost(QHostAddress(ip), port);
    else
    {
        if(noAck >= 2)
        {
            noAck = 0;
            qDebug() << "no heartbeat ack, reconnecting...";
            retrying = true;
            timer->setInterval(10 * 1000);
            client->abort();
            reg = false;
            client->connectToHost(QHostAddress(ip), port);
            return;
        }
        QByteArray pkt;
        pkt.append(OP_HEARTBEAT);
        pkt.append(HB_SEND);
        client->write(pkt);
        noAck++;
    }
}

void Client::sendText(QString text)
{
    QByteArray pkt = makeHeader(MIME_TEXT, text.toUtf8().length());
    pkt.append(text);
    client->write(pkt);
}

void Client::sendFile(QString& filepath)
{
    FILE* fp = ::fopen(codec->fromUnicode(filepath).data(), "rb");
    if(!fp)
        return;
    QStringList list = filepath.split("/");
    QString filename = list[list.size() - 1];
    QByteArray pkt = makeHeader(MIME_FILE, filename.toUtf8().length());
    pkt.append(filename);
    client->write(pkt);

    char buf[BUFSIZE];
    size_t nread = fread(buf, 1, sizeof(buf), fp);
    while(nread > 0)
    {
        pkt = makeHeader(MIME_FILE, nread);
        pkt.append(buf, nread);
        client->write(pkt);
        nread = fread(buf, 1, sizeof(buf), fp);
    }
    pkt = makeHeader(MIME_FILE, 0);
    client->write(pkt);
    ::fclose(fp);
}

void Client::sendDir(QString &base)
{
    QStringList l = base.split("/");
    QString dirName = l[l.size() - 1];
    QByteArray pkt = makeHeader(MIME_DIR, dirName.toUtf8().length());
    pkt.append(l[l.size() - 1]);
    client->write(pkt);
    _sendDir(base);
    pkt = makeHeader(MIME_DIR, 0);
    client->write(pkt);
}

void Client::_sendDir(QString &base)
{
    DIR* dir = nullptr;
    struct dirent* entry = nullptr;
    struct stat st;
    if((dir = ::opendir(codec->fromUnicode(base).data())) == nullptr)
        return;
    QString filepath;
    QByteArray pkt;
    while((entry = ::readdir(dir)) != nullptr)
    {
        filepath = base + "/" + codec->toUnicode(entry->d_name);
        if(strcmp("..", entry->d_name) == 0 || strcmp(".", entry->d_name) == 0)
            continue;
        if(::stat(codec->fromUnicode(filepath).data(), &st) != 0)
            return;
        if(S_ISDIR(st.st_mode))
        {
            pkt = makeHeader(MIME_DIR, entry->d_namlen);
            pkt.append(entry->d_name);
            client->write(pkt);
            _sendDir(filepath);
            pkt = makeHeader(MIME_DIR, 0);
            client->write(pkt);
        }
        else
            sendFile(filepath);
    }
    ::closedir(dir);
}

QByteArray Client::makeHeader(MimeType mime, quint16 len)
{
    QByteArray header;
    header.append(OP_DATA);
    header.append((char)mime);
    uchar buf[2];
    qToBigEndian(len, buf);
    header.append((char*)buf, 2);
    return header;
}

Client::~Client()
{
    timer->stop();
    client->disconnectFromHost();
    delete client;
    delete timer;
}
