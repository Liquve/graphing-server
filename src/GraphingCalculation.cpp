#include "GraphingCalculation.h"

#include <QStringList>
#include <QByteArray>
#include <QLibrary>
#include <QDebug>

#include <cmath>
#include <exception>

namespace {

typedef void (*calculate_fn_t)(int, int, int, int*, double**);
typedef void (*free_values_fn_t)(double*);

struct LibFnLoader
{
    QLibrary library;

    calculate_fn_t calculate = nullptr;
    free_values_fn_t free_values = nullptr;

    QString error;
    bool loaded = false;

    LibFnLoader()
    {
        QByteArray path = qgetenv("LIBFN_PATH");

        if (path.isEmpty()) {
            error = "LIBFN_PATH is not set";
            qCritical().noquote() << "[-] Cannot initialize graphing calculation library:" << error;
            return;
        }

        QString libraryPath = QString::fromLocal8Bit(path);
        library.setFileName(libraryPath);

        qInfo().noquote() << "[#] Loading graphing calculation library:" << libraryPath;

        if (!library.load()) {
            error = QString("Cannot load %1: %2").arg(libraryPath, library.errorString());
            qCritical().noquote() << "[-]" << error;
            return;
        }

        qInfo().noquote() << "[+] Graphing calculation library loaded:" << library.fileName();

        calculate = reinterpret_cast<calculate_fn_t>(
            library.resolve("calculate")
        );

        if (!calculate) {
            error = QString("Cannot resolve symbol calculate in %1: %2").arg(library.fileName(), library.errorString());
            qCritical().noquote() << "[-]" << error;
            library.unload();
            return;
        }

        qDebug().noquote() << "[#] Resolved graphing calculation symbol: calculate";

        free_values = reinterpret_cast<free_values_fn_t>(
            library.resolve("free_values")
        );

        if (!free_values) {
            error = QString("Cannot resolve symbol free_values in %1: %2").arg(library.fileName(), library.errorString());
            qCritical().noquote() << "[-]" << error;
            library.unload();
            calculate = nullptr;
            return;
        }

        qDebug().noquote() << "[#] Resolved graphing calculation symbol: free_values";
        qInfo().noquote() << "[+] Graphing calculation library is ready";

        loaded = true;
    }
};

LibFnLoader& getLibFn()
{
    static LibFnLoader loader;
    return loader;
}

}

GraphingCalculation::Result GraphingCalculation::calculate(int a, int b, int c)
{
    LibFnLoader& lib = getLibFn();

    if (!lib.loaded || !lib.calculate || !lib.free_values) {
        return Result{false, QString(), lib.error.isEmpty() ? "Graphing calculation library is not loaded" : lib.error};
    }

    int count = 0;
    double* values = nullptr;

    qDebug().noquote().nospace()
        << "[#] Calling graphing calculation library with a="
        << a
        << ", b="
        << b
        << ", c="
        << c;

    try {
        lib.calculate(a, b, c, &count, &values);
    } catch (const std::exception& e) {
        QString error = QString("Graphing calculation library threw exception: %1").arg(e.what());
        qCritical().noquote() << "[-]" << error;
        return Result{false, QString(), error};
    } catch (...) {
        QString error = "Graphing calculation library threw unknown exception";
        qCritical().noquote() << "[-]" << error;
        return Result{false, QString(), error};
    }

    if (!values || count <= 0) {
        QString error = QString("Graphing calculation library returned no values: count=%1, values=%2")
            .arg(count)
            .arg(values ? "not null" : "null");
        qCritical().noquote() << "[-]" << error;
        return Result{false, QString(), error};
    }

    QStringList result;

    for (int i = 0; i < count; i++) {
        double x = values[i * 2];
        double y = values[i * 2 + 1];

        if (std::isnan(y) || std::isinf(y)) {
            result << QString("%1:null").arg(x);
        } else {
            result << QString("%1:%2").arg(x).arg(y);
        }
    }

    lib.free_values(values);

    QString value = result.join('|');
    qDebug().noquote() << "[+] Graphing calculation completed with" << count << "points";

    return Result{true, value, QString()};
}
