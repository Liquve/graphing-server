#ifndef GRAPHINGCALCULATION_H
#define GRAPHINGCALCULATION_H

#include <QString>

namespace GraphingCalculation {
    struct Result {
        bool success = false;
        QString value;
        QString errorMessage;
    };

    Result calculate(int, int, int);
}

#endif // GRAPHINGCALCULATION_H
