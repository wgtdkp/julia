#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gen(char* str, int base, int mul)
{
    unsigned int hash = 0;
    for (char* p = str; *p != 0 && *p != ' '; ++p) {
        char ch = *p;
        if (ch >= 'A' && ch <= 'Z')
            ch += 'a' - 'A';
        if ('-' == ch)
            ch = '-';
        else 
            ch -= 'a';
        hash = hash * mul + ch;
        //if (hash > 0x00ffffff)
        //    hash %= base;
    }
    hash %= base;
    return hash;   
}

int main(void)
{
    int primers[168] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997};
    //for (int n = 0; n < 168; ++n) {
    //    for (int mul = 1; mul < 128; ++mul) {
            freopen("headers.txt", "r", stdin);
            char str[100] = {0};
            int table[100];
            for (int k = 0; k < 100; ++k)
                table[k] = -1;
            int i = 0;
            while (scanf("%s", str) != EOF) {
                int hash = gen(str, 131, 3);
                printf("%s: %d\n", str, hash);
                table[i++] = hash;
            }
            for (i = 0; i < 100; ++i) {
                for (int j = i + 1; j < 100; ++j) {
                    if (table[i] != -1 && table[i] == table[j])
                        printf("collision: %d, %d: %d\n", i, j, table[i]);
                }
            }
            //printf("%d: %d\n", primers[n], mul);
            //return 0;
        end:
            fclose(stdin);
    //    }
    //}
    return 0;
}