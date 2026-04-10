#ifndef FUNCSERV_H
#define FUNCSERV_H
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <iostream>
#include <cmath>

class funcServ
{
public:
    QString parsing(QString include, QTcpSocket* socket);

private:
    QString reg(QStringList param, QTcpSocket* socket);
    QString auth(QStringList param, QTcpSocket* socket);
    QString func(QStringList param, QTcpSocket* socket);
};

#endif // FUNCSERV_H
