#include <stdio.h>
#include "externFunc.h"

int GlobalVal;

void test_function(int value) {
    printf("%d", value);
}

int Get_TestValue() {
    return GlobalVal;
}

void Set_TestValue(int value) {
    GlobalVal = value;
}

int main()
{
    int i = 0;
    int cnt = 1;

    int (*fp)();

    fp = Get_TestValue;

    Set_TestValue(2);

    for(i = 0; i < 5; i+=cnt) 
    {
        if(i == 3) {
            test_function(i);
            cnt += fp();
        }
        switch(i) {
            case 0:
                GlobalVal = addFunction(GlobalVal, i);
                break;
            case 1:
                GlobalVal = MULMACRO(GlobalVal, i);
                break;
            case 2:
                GlobalVal = divFunction(GlobalVal, i);
                break;
            case 3:
                GlobalVal = SUBMACRO(GlobalVal, i);
                break;
            default:
                break;
        }
        printf("%d", GlobalVal);
    }

    return 0;
}