#include "mytcpserver.h"
#include <QDebug>
#include <QCoreApplication>
#include<QString>
#include "funcserv.h"

MyTcpServer::~MyTcpServer()
{

    mTcpServer->close();
}

MyTcpServer::MyTcpServer(QObject *parent) : QObject(parent){
    mTcpServer = new QTcpServer(this);

    connect(mTcpServer, &QTcpServer::newConnection, this, &MyTcpServer::slotNewConnection);

    if(!mTcpServer->listen(QHostAddress::Any, 33333)){
        qDebug() << "server is not started";
    } else {
        qDebug() << "server is started";
    }
}

void MyTcpServer::slotNewConnection(){
    mTcpSocket = mTcpServer->nextPendingConnection();
    mTcpSocket->write("Hello, World!!! I am echo server!\r\n");
    connect(mTcpSocket, &QTcpSocket::readyRead,this,&MyTcpServer::slotServerRead);
    connect(mTcpSocket,&QTcpSocket::disconnected,this,&MyTcpServer::slotClientDisconnected);
}

void MyTcpServer::slotServerRead(){
    QString res = "";
    while(mTcpSocket->bytesAvailable()>0)
    {
        QByteArray array =mTcpSocket->readAll();
        qDebug()<<array<<"\n";
        if(array=="\x01")
        {
            funcServ processor;
            QString answer = processor.parsing(res, mTcpSocket);
            mTcpSocket->write(answer.toUtf8());
            res = "";
        }
        else
            res.append(array);
    }
    if (!res.isEmpty())
    {
        funcServ processor;
        QString answer = processor.parsing(res, mTcpSocket);
        mTcpSocket->write(answer.toUtf8());
    }

}

void MyTcpServer::slotClientDisconnected(){
    mTcpSocket->close();
}
