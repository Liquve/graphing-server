#include "GraphingCalculation.h"
#include <QStringList>
#include <cmath>


QString GraphingCalculation::getCalculationResult(int a, int b, int c) {
    QStringList result;

    int xMin = -10;
    int xMax = 10;
    int points = 1000;

    double step = static_cast<double>(xMax - xMin) / static_cast<double>(points - 1);
    for (int i = 0; i < points; i++)
    {
        double previousX = i > 0 ? xMin + (i - 1) * step : xMin;
        bool valid = true;
        double x = xMin + i * step;
        double y;

        // Если шаг не попадает точно в x = 1, всё равно явно обозначаем разрыв функции.
        if (i > 0 && ((previousX < 1.0 && x > 1.0) || (previousX > 1.0 && x < 1.0)))
        {
            result << "1:null";
        }

        if (std::abs(x - 1.0) < 1e-9)
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
            y = static_cast<double>(c) / (x - 1.0);
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

    return result.join('|');
}
