#include "externFunc.h"

int addFunction(int a, int b) {
    return a + b;
}

int subFunction(int a, int b) {
    return a - b;
}

double mulFunction(double a, double b) {
    return a * b;
}

double divFunction(double a, double b) {
    if( b == 0 ) {
        printf("Err");
        return 0;
    }
    else {
        return a / b;
    }
}