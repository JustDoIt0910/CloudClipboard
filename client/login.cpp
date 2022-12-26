#include "login.h"
#include "ui_login.h"
#include <QDebug>
#include <QMessageBox>

Login::Login(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Login)
{
    ui->setupUi(this);
    connect(ui->loginBtn, &QPushButton::clicked, this, [=](){
        if(!ui->usernameInput->text().size() ||
                !ui->usernameInput->text().size())
        {
            QMessageBox::critical(this, "wrong input", "please enter username and password");
            return;
        }
        emit loginOrRegister(false, ui->usernameInput->text(), ui->passwordInput->text());
    });
    connect(ui->regBtn, &QPushButton::clicked, this, [=](){
        if(!ui->usernameInput->text().size() ||
                !ui->usernameInput->text().size())
        {
            QMessageBox::critical(this, "wrong input", "please enter username and password");
            return;
        }
        emit loginOrRegister(true, ui->usernameInput->text(), ui->passwordInput->text());
    });
}

Login::~Login()
{
    delete ui;
}
