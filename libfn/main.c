#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

typedef void (*libfn_calculate_t)(int, int, int, int*, double**);
typedef void (*libfn_free_values_t)(double*);

int main(int argc, char *argv[]) {
    if (argc != 2) {
#ifdef _WIN32
        printf("Usage: main.exe <libpath.dll>\n");
#else
        printf("Usage: ./main <libpath.so>\n");
#endif
        return 1;
    }

#ifdef _WIN32
    HMODULE handle = LoadLibraryA(argv[1]);
    if (!handle) {
        printf("LoadLibrary error: %lu\n", GetLastError());
        return 1;
    }

    libfn_calculate_t calculate =
        (libfn_calculate_t)GetProcAddress(handle, "calculate");

    if (!calculate) {
        printf("GetProcAddress calculate error: %lu\n", GetLastError());
        FreeLibrary(handle);
        return 1;
    }

    libfn_free_values_t free_values =
        (libfn_free_values_t)GetProcAddress(handle, "free_values");

    if (!free_values) {
        printf("GetProcAddress free_values error: %lu\n", GetLastError());
        FreeLibrary(handle);
        return 1;
    }
#else
    dlerror();

    void *handle = dlopen(argv[1], RTLD_NOW);
    char *error = dlerror();

    if (error) {
        printf("dlopen error: %s\n", error);
        return 1;
    }

    dlerror();

    libfn_calculate_t calculate =
        (libfn_calculate_t)dlsym(handle, "calculate");

    error = dlerror();

    if (error) {
        printf("dlsym calculate error: %s\n", error);
        dlclose(handle);
        return 1;
    }

    dlerror();

    libfn_free_values_t free_values =
        (libfn_free_values_t)dlsym(handle, "free_values");

    error = dlerror();

    if (error) {
        printf("dlsym free_values error: %s\n", error);
        dlclose(handle);
        return 1;
    }
#endif

    int a, b, c;

    printf("Enter a b c: ");
    if (scanf("%d %d %d", &a, &b, &c) != 3) {
        printf("Invalid input\n");

#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return 1;
    }

    int count = 0;
    double *values = NULL;

    calculate(a, b, c, &count, &values);

    if (!values) {
        printf("Function returned NULL\n");

#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return 1;
    }

    for (int i = 0; i < count; i++) {
        double x = values[i * 2];
        double y = values[i * 2 + 1];

        if (isnan(y))
            printf("(%f; null)\n", x);
        else
            printf("(%f; %f)\n", x, y);
    }

    free_values(values);

#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif

    return 0;
}
