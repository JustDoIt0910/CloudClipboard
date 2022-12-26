#include "client.h"
#include <QHostAddress>
#include <QCryptographicHash>
#include <QtEndian>

Client::Client(QString _ip, quint16 _port):
    ip(_ip), port(_port)
{
    client = new QTcpSocket();
    connect(client, &QTcpSocket::readyRead, this, [&](){
        QByteArray resp = client->readAll();
        inputBuf.append(resp);

        if(inputBuf.size() >= 3)
        {
            if(inputBuf.at(0) == 3 || inputBuf.at(0) == 4)
            {
                if(inputBuf.at(0) == 3)
                    emit success(inputBuf.at(1));
                else
                {
                    client->disconnectFromHost();
                    emit fail(inputBuf.at(1), static_cast<ErrCode>(inputBuf.at(2)));
                }
                inputBuf.remove(0, 3);
            }
            else if(inputBuf.at(0) == 2)
            {
                if(inputBuf.size() >= 4)
                {
                    char mime = inputBuf.at(1);
                    uint16_t len = inputBuf.at(2);
                    len = len << 8 | (inputBuf.at(3) & 0xFF);
                    if(mime == MIME_TEXT)
                    {
                        if(inputBuf.size() >= 4 + len)
                        {
                            inputBuf.remove(0, 4);
                            QByteArray data = inputBuf.left(len);
                            inputBuf.remove(0, len);
                            emit hasTextData(QString(data));
                        }
                    }
                }
            }
            else
                inputBuf.clear();
        }
    });

    connect(client, &QTcpSocket::connected, this, [=](){
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
    });
}

void Client::loginOrReg(QString username, QString password, bool reg)
{
    this->username = username;
    this->password = password;
    this->reg = reg;
    client->disconnectFromHost();
    client->connectToHost(QHostAddress(ip), port);
}

void Client::sendText(QString text)
{
    QByteArray pkt = makeHeader(MIME_TEXT, text.toUtf8().length());
    pkt.append(text);
    client->write(pkt);
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
    client->disconnectFromHost();
    delete client;
}
