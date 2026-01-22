#ifndef EXTERNFUNC_H
#define EXTERNFUNC_H

#define ADDMACRO(x, y) addFunction(x, y);
#define SUBMACRO(x, y) subFunction(x, y);
#define MULMACRO(x, y) ((x)*(y))
#define DIVMACRO(x, y) ((x)/(y))

extern int      addFunction(int a, int b);
extern int      subFunction(int a, int b);
extern double   mulFunction(double a, double b);
extern double   divFunction(double a, double b);

#endif