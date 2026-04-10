#include <QCoreApplication>
#include "TCPGraphingServerManager.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".UTF8");

    QCoreApplication qtApp(argc, argv);

    TCPGraphingServerManager manager(QHostAddress::AnyIPv4, 13579);

    try {
        manager.startServer();
    } catch (const std::exception& e) {
        qFatal() << QString("Fatal exception while trying to start TCP Server: %1").arg(e.what());
        return 1;
    }

    return qtApp.exec();
}
