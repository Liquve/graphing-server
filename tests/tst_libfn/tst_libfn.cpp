// Юнит-тесты для математического ядра (libfn/libfn.c).
// Покрывает тест-кейсы: TC-01, TC-02, TC-03, TC-04, TC-26, TC-27, TC-28, TC-29, TC-30.

#include <QtTest>
#include <cmath>
#include <vector>

extern "C" {
    void calculate_points(int a, int b, int c, int points, int *count, double **values);
    void free_values(double *values);
}

namespace {

struct CalculatedPoints {
    std::vector<std::pair<double, double>> data;

    CalculatedPoints(int a, int b, int c, int points = 1000) {
        int count = 0;
        double* values = nullptr;
        calculate_points(a, b, c, points, &count, &values);
        for (int i = 0; i < count; ++i) {
            data.emplace_back(values[i * 2], values[i * 2 + 1]);
        }
        if (values) free_values(values);
    }
};

const double kAbsTol = 1e-6;

bool nearlyEqual(double a, double b, double tol = kAbsTol) {
    if (std::isnan(a) || std::isnan(b)) return false;
    return std::abs(a - b) <= tol * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}

}

class TestLibFn : public QObject
{
    Q_OBJECT

private slots:
    // TC-01: Расчет ветки x < 0 при a = 2: y = cosh(2x).
    void calculatesCoshBranchForNegativeX_TC01()
    {
        CalculatedPoints points(2, 1, 1);
        QVERIFY(!points.data.empty());

        int verified = 0;
        for (const auto& point : points.data) {
            double x = point.first;
            double y = point.second;
            if (x >= -0.5) continue;
            if (std::isnan(y) || std::isinf(y)) continue;

            double expected = std::cosh(2.0 * x);
            // Сравнение с допуском, тк значение y может быть огромным.
            QVERIFY2(
                nearlyEqual(y, expected),
                qPrintable(QString("x=%1: y=%2, expected cosh(2x)=%3").arg(x).arg(y).arg(expected))
            );
            ++verified;
        }

        QVERIFY2(verified > 100, "Should verify many negative-x points");
    }

    // TC-02: Расчет ветки 0 <= x < 1 при b = 5: y = ln(5x + 1).
    void calculatesLogBranchForUnitInterval_TC02()
    {
        CalculatedPoints points(1, 5, 1);
        QVERIFY(!points.data.empty());

        int verified = 0;
        for (const auto& point : points.data) {
            double x = point.first;
            double y = point.second;
            if (x <= 0.05 || x >= 0.99) continue;
            if (std::isnan(y) || std::isinf(y)) continue;

            double expected = std::log(5.0 * x + 1.0);
            QVERIFY2(
                nearlyEqual(y, expected),
                qPrintable(QString("x=%1: y=%2, expected ln(5x+1)=%3").arg(x).arg(y).arg(expected))
            );
            ++verified;
        }

        QVERIFY2(verified > 30, "Should verify many points in [0, 1) interval");
    }

    // TC-03: Разрыв функции в x = 1 — в таблице прочерк, на графике разрыв (NaN).
    void returnsNaNAtDiscontinuity_TC03()
    {
        CalculatedPoints points(1, 1, 1);
        bool foundDiscontinuity = false;

        for (const auto& point : points.data) {
            double x = point.first;
            double y = point.second;
            if (std::abs(x - 1.0) < 1e-6 && std::isnan(y)) {
                foundDiscontinuity = true;
                break;
            }
        }

        QVERIFY2(foundDiscontinuity, "Expected NaN marker at x=1 for graph break");
    }

    // TC-04: Масштабирование — большие значения y не должны разрушать вычисления.
    // Для a = 10 значения cosh(10*x) велики, но конечны для x в [-10; 0].
    void handlesLargeCoshValues_TC04()
    {
        CalculatedPoints points(10, 1, 1);
        QVERIFY(!points.data.empty());

        bool foundFinite = false;
        for (const auto& point : points.data) {
            double x = point.first;
            double y = point.second;
            if (x >= 0) continue;
            if (std::isfinite(y)) {
                foundFinite = true;
                // cosh(0) = 1 — наименьшее значение при x->0.
                QVERIFY(y >= 1.0 - 1e-9);
            }
        }

        QVERIFY2(foundFinite, "Should return finite cosh values even for a=10");
    }

    // TC-26: Точность в таблице — значения Y должны быть валидными числами (double).
    // Округление до 1 знака происходит в клиенте, здесь проверяем тип.
    void returnsValidDoubleValues_TC26()
    {
        CalculatedPoints points(1, 1, 1);
        QVERIFY(!points.data.empty());

        for (const auto& point : points.data) {
            // X всегда конечен.
            QVERIFY(std::isfinite(point.first));
            // Y либо конечен, либо NaN (для точки разрыва).
            QVERIFY(std::isfinite(point.second) || std::isnan(point.second));
        }
    }

    // TC-27: Узлы таблицы — должно возвращаться 1000 точек + дополнительные точки разрыва.
    void returnsRequestedNumberOfPoints_TC27()
    {
        CalculatedPoints points(1, 1, 1, 1000);
        QVERIFY2(
            points.data.size() >= 1000,
            qPrintable(QString("Expected >= 1000 points, got %1").arg(points.data.size()))
        );
        QVERIFY2(
            points.data.size() <= 1005,
            "Should not insert too many auxiliary discontinuity markers"
        );

        // Первая точка — x = -10.
        QCOMPARE(points.data.front().first, -10.0);
        // Последняя — около x = 10.
        QVERIFY(std::abs(points.data.back().first - 10.0) < 1e-6);
    }

    // TC-28: Коэффициент a = 0 — cosh(0 * x) = 1: горизонтальная линия для x < 0.
    void zeroAGivesHorizontalCoshLine_TC28()
    {
        CalculatedPoints points(0, 1, 1);

        int checked = 0;
        for (const auto& point : points.data) {
            double x = point.first;
            double y = point.second;
            if (x >= -1e-9) continue;
            if (std::isnan(y) || std::isinf(y)) continue;

            QVERIFY2(
                std::abs(y - 1.0) < 1e-9,
                qPrintable(QString("x=%1: expected y=1.0, got %2").arg(x).arg(y))
            );
            ++checked;
        }

        QVERIFY(checked > 100);
    }

    // TC-29: Отрицательный c — гипербола зеркалится относительно оси X (y становится отрицательным).
    void negativeCGivesNegativeHyperbola_TC29()
    {
        CalculatedPoints negative(1, 1, -5);
        CalculatedPoints positive(1, 1, 5);

        // Сравниваем знаки на x > 1.
        for (std::size_t i = 0; i < negative.data.size() && i < positive.data.size(); ++i) {
            double x = negative.data[i].first;
            if (x <= 1.0 + 1e-3) continue;

            double yNeg = negative.data[i].second;
            double yPos = positive.data[i].second;
            if (std::isnan(yNeg) || std::isnan(yPos)) continue;

            QVERIFY2(yNeg < 0, qPrintable(QString("c=-5, x=%1: expected y<0, got %2").arg(x).arg(yNeg)));
            QVERIFY2(yPos > 0, qPrintable(QString("c=5, x=%1: expected y>0, got %2").arg(x).arg(yPos)));
            // Зеркальное отображение: y(c) = -y(-c).
            QVERIFY2(
                std::abs(yNeg + yPos) < 1e-9,
                qPrintable(QString("Mirror property violated at x=%1: yNeg=%2, yPos=%3").arg(x).arg(yNeg).arg(yPos))
            );
        }
    }

    // TC-30: Поведение в x = 1 при c = 100 — точка разрыва должна быть представлена через NaN,
    // чтобы клиент не рисовал «синюю стену».
    void discontinuityWithLargeC_TC30()
    {
        CalculatedPoints points(1, 1, 100);

        // Должен быть маркер (1.0, NaN), вставленный между x<1 и x>1.
        bool foundMarker = false;
        for (std::size_t i = 0; i < points.data.size(); ++i) {
            double x = points.data[i].first;
            double y = points.data[i].second;
            if (std::abs(x - 1.0) < 1e-6 && std::isnan(y)) {
                foundMarker = true;
                break;
            }
        }
        QVERIFY2(foundMarker, "Expected NaN marker at x=1 to break the line and avoid 'blue wall'");

        // Сразу после разрыва значения большие, но конечные.
        for (const auto& point : points.data) {
            double x = point.first;
            double y = point.second;
            if (x > 1.0 + 1e-3 && x < 2.0) {
                if (!std::isnan(y)) {
                    QVERIFY(std::isfinite(y));
                    QVERIFY(y > 0); // c=100, x-1>0 → y>0
                }
            }
        }
    }

    // Дополнительно: pathological case — мало точек.
    void rejectsTooFewPoints()
    {
        int count = 999;
        double* values = reinterpret_cast<double*>(0xDEADBEEF);
        calculate_points(1, 1, 1, 1, &count, &values);
        QCOMPARE(count, 0);
        QCOMPARE(values, static_cast<double*>(nullptr));
    }

    // Дополнительно: zero points.
    void rejectsZeroPoints()
    {
        int count = 999;
        double* values = reinterpret_cast<double*>(0xDEADBEEF);
        calculate_points(1, 1, 1, 0, &count, &values);
        QCOMPARE(count, 0);
        QCOMPARE(values, static_cast<double*>(nullptr));
    }
};

QTEST_APPLESS_MAIN(TestLibFn)
#include "tst_libfn.moc"
