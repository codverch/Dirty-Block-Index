#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <string.h>

int main(int argc, char *argv[])
{

    int *a1, *a2, *a3;

    int s = 0, j = 0;

    int size, iter;

    while ((s = getopt(argc, argv, ":n:t")) != -1)
    {
        switch (s)
        {

        case 'n':
            if (optarg != NULL)
                size = atoi(optarg);

        case 't':
            iter = atoi(argv[4]);
            break;

        default:
            break;
        }
    }

    // Allocate memory in the heap region to the arrays

    a1 = (int *)malloc(size * sizeof(int));
    a2 = (int *)malloc(size * sizeof(int));
    a3 = (int *)malloc(size * sizeof(int));

    // Assign values to the arrays - random values between 0 and 99
    srand(33);
    for (j = 0; j < size; j++)
        a1[j] = rand() % 100;
    srand(33);
    for (j = 0; j < size; j++)
        a2[j] = rand() % 100;

    for (int i = 0; i < size; i++)
    {
        a3[i] = a1[i] + a2[i];
    }

    while (iter != 0)
    {
        for (int i = 0; i < size; i++)
        {
            a3[i] = a1[i] + a2[i];
        }

        iter--;
    }

    printf("%d\n", a3[0]);

    free(a1);
    free(a2);
    free(a3);

    return 0;
}