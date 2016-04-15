#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    char ch;
    for (int i = 0; i < 5; i++) {
        scanf("%c", &ch);
        printf("%c", ch);
    }
    return 0;
}
