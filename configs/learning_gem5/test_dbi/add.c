#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[])
{
    int *arr_1, *arr_2, *arr_3;
    int opt, ch, size, iter = 0, j;
    clock_t start, end;
    double cpu_time_used, time_taken;

    while ((opt = getopt(argc, argv, ":n:i:t") != -1))
    {
        switch (opt)
        {
        case 'n':
            if (optarg != NULL)
                size = atoi(optarg);
        case 'i':
            if (strcmp(argv[4], "bo") == 0) // Buffer - Overflow
                ch = 1;
            else if (strcmp(argv[4], "uaf") == 0) // Use - After - Free
                ch = 2;
            else if (strcmp(argv[4], "df") == 0) // Double - Free
                ch = 3;
            else if (strcmp(argv[4], "u") == 0) // Unallocated - Memory
                ch = 4;
            else
                printf("We do not support injecting this error currently\n");
        case 't':
            iter = atoi(argv[6]);
            break;

        default:
            break;
        }
    }

    arr_1 = (int *)malloc(16 * size * sizeof(int));
    arr_2 = (int *)malloc(16 * size * sizeof(int));
    arr_3 = (int *)malloc(16 * size * sizeof(int));

    for (j = 0; j < size; j++)
        arr_1[j] = rand() % 100;

    for (j = 0; j < size; j++)
        arr_2[j] = rand() % 100;

    // Run one iteration of the computation before measuring the computation - why??
    for (j = 0; j < size; j++)
    {
        *(arr_3 + j * 16) = *(arr_1 + j * 16) + *(arr_2 + j * 16);
    }

    start = clock();
    while (iter != 0)
    {
        for (j = 0; j < size; j++)
        {
            *(arr_3 + j * 16) = *(arr_1 + j * 16) + *(arr_2 + j * 16);
            //*(arr_3 + j) = *(arr_1 + j) + *(arr_2 + j);
        }

        iter--;
    }
    end = clock();

    cpu_time_used = ((double)end - start) / CLOCKS_PER_SEC;
    time_taken = cpu_time_used * 1000;
    printf("\nThe time taken is %lf milliseconds\n", time_taken);

    if (ch == 1) // Buffer - Overflow
    {
        printf("Introducing buffer overflow - Accessing a location beyond the range");
        *(arr_3 + opt + 1) = 4;
        printf("\nThe value written is: %d\n", *(arr_3 + opt + 1));
    }

    else if (ch == 2) // Use - After - Free
    {
        printf("\nBefore freeing: %d\n", *(arr_3 + 1));

        // Free the memory locations and then try to access it
        free(arr_1);
        free(arr_2);
        free(arr_3);

        printf("After freeing: %d\n", *(arr_3 + 1));

        // Writing to a memory location that has been freed
        *(arr_3 + 1) = 4;
        printf("New value: %d\n", *(arr_3 + 1));
    }

    else if (ch == 3) // Double - Free
    {
        // Free the memory twice : to introduce double-free-error
        free(arr_1);
        free(arr_2);
        free(arr_3);

        free(arr_1);
        free(arr_2);
        free(arr_3);

        // Occurs in multi-threaded programs
    }

    else
    {
        printf(" To be filled \n");
    }

    free(arr_1);
    free(arr_2);
    free(arr_3);

    return 0;
}