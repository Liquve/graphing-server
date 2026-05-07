#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#define LIBFN_EXPORT __declspec(dllexport)
#else
#define LIBFN_EXPORT
#endif

LIBFN_EXPORT void calculate_points(int a, int b, int c, int points, int *count, double **values)
{
    int xMin = -10;
    int xMax = 10;

    if (points < 2) {
        if (count) *count = 0;
        if (values) *values = NULL;
        return;
    }

    double eps = 1e-9;
    double step = (double)(xMax - xMin) / (double)(points - 1);

    int capacity = points + 3;
    double *arr = (double *)malloc(sizeof(double) * capacity * 2);

    if (arr == NULL) {
        if (count) *count = 0;
        if (values) *values = NULL;
        return;
    }

    int n = 0;

    for (int i = 0; i < points; i++) {
        double x = xMin + i * step;
        double y = NAN;

        if (i > 0) {
            double previousX = xMin + (i - 1) * step;

            if (previousX < 0.0 && x > 0.0) {
                arr[n * 2] = 0.0;
                arr[n * 2 + 1] = NAN;
                n++;

                arr[n * 2] = 0.0;
                arr[n * 2 + 1] = 0.0;
                n++;
            } else if (previousX < 0.0 && fabs(x) < eps) {
                arr[n * 2] = 0.0;
                arr[n * 2 + 1] = NAN;
                n++;
            }

            if (previousX < 1.0 && x > 1.0) {
                arr[n * 2] = 1.0;
                arr[n * 2 + 1] = NAN;
                n++;
            }
        }

        if (fabs(x - 1.0) < eps) {
            y = NAN;
        } else if (x < 0.0) {
            y = cosh((double)a * x);
        } else if (x >= 0.0 && x < 1.0) {
            double logArg = (double)b * x + 1.0;

            if (logArg > 0.0)
                y = log(logArg);
            else
                y = NAN;
        } else {
            y = (double)c / (x - 1.0);
        }

        arr[n * 2] = x;
        arr[n * 2 + 1] = y;
        n++;
    }

    if (count) *count = n;
    if (values) *values = arr;
}

LIBFN_EXPORT void free_values(double *values)
{
    free(values);
}
