#include "GraphingCalculation.h"
#include <QStringList>


QString GraphingCalculation::getCalculationResult(int a, int b, int c) {
    QStringList result;

    int xMin = -10;
    int xMax = 10;
    int points = 20;

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

    return result.join('|');
}