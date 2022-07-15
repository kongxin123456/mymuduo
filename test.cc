# include <iostream>
# include <cstdio>

#include <iomanip>

int main(){
    int a = 23;
    printf("%d\n",a);
    printf("%5d\n",a);
    printf("%15d\n",a);
    std::cout << std::setw(6) << std::setfill('1') << a << std::endl;
    return 0;
}

