#include "funcserv.h"

QString funcServ::reg(QStringList param, QTcpSocket* socket)
{
    return "reg+";
}

QString funcServ::auth(QStringList param, QTcpSocket* socket)
{
    return "auth+";
}

QString funcServ::func(QStringList param, QTcpSocket* socket)
{
    QStringList result;
    int xMin = -10;
    int xMax = 10;
    int points = 20;
    if (param.size() != 3)
    {
        return "Нужно ввести 3 параметра";
    }
    int a = param[0].toInt();
    int b = param[1].toInt();
    int c = param[2].toInt();
    double step = (xMax - xMin) / (points - 1);
    for (int i = 0; i < points; i++)
    {
        bool valid = true;
        double x = xMin + i * step;
        double y;
        if (x == 1)
        {
            y = INFINITY;
            valid = false;
        }
        else if (x < 0)
        {
            y = std::cosh(x * a);
        }
        else if (x >= 0 && x < 1)
        {
            y = std::log(b * x + 1);
        }
        else
        {
            y = c / (x - 1);
        }
        if (valid)
        {
            result << QString("%1:%2").arg(x).arg(y);
        }
        else
        {
            result << QString("%1:null").arg(x);
        }
    }
    for (const QString& point: result)
    {
        std::cout << point.toStdString() << std::endl;
    }
    return result.join('|');
}

QString funcServ::parsing(QString include, QTcpSocket* socket)
{
    int spc = include.indexOf(' ');
    QString com = include.left(spc);
    QString paramstr = include.mid(spc + 1);
    QStringList param = paramstr.split('|');
    if (com == "reg")
    {
        return reg(param, socket);
    }
    else if (com == "auth")
    {
        return auth(param, socket);
    }
    else if (com == "func")
    {
        return func(param, socket);
    }
    else
    {
        return "";
    }
}
