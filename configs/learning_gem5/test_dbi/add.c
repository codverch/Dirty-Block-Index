/*Aim: This program performs addition of two arrays and stores the result in the third. However, during every computation a new cacheline is brought in. */

#include <stdio.h>
#include <stdlib.h> // for malloc(), and rand()
#include <getopt.h> // for getopt()
#include <time.h>   // for clock()
#include <string.h> // for strcmp()

int main(int argc, char *argv[])
{
    // Sum of two arrays: three pointers
    int *a1, *a2, *a3;
    // Declaring two variables - for the command line input
    int s = 0, j = 0; // s - size of the arrays, j - for index
    // char i[10]; // i = the error to be injected

    // Declaring a choice variable to figure-out which error needs to be injected
    int ch, size, iter = 0; // iter - iterations

    // To measure the performance

    clock_t start, end;

    double cpu_time_used, time_taken;

    // Get the command-line arguments n and i
    while ((s = getopt(argc, argv, ":n:t")) != -1) // Where, t - number of iterations
    {
        switch (s)
        {

        case 'n':
            if (optarg != NULL)
                size = atoi(optarg);

        case 't':
            iter = atoi(argv[6]);
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

    for (j = 0; j < size; j++)
        a1[j] = rand() % 100;

    for (j = 0; j < size; j++)
        a2[j] = rand() % 100;

    for (int i = 0; i < size; i++)
    {
        a3[i] = a1[i] + a2[i];
    }

    printf("The result of the addition is:\n");
    for (int i = 0; i < size; i++)
    {
        printf("%d ", a3[i]);
    }
    printf("\n");

    free(a1);
    free(a2);
    free(a3);

    return 0;
}