#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[])
{
    // An array on n elements
    int *arr;

    // Command line arguments
    int size, iter, opt;

    while ((opt = getopt(argc, argv, ":n:t")) != -1)
    {
        switch (opt)
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

    // Allocate memory for the array
    arr = (int *)malloc(size * sizeof(int));

    // Every DRAM row contains 64 cache blocks, and each cache block is 64 bytes
    // Each cache block can store 16 integers
    // So, the number of integers that can be stored in a DRAM row is 1024

    // Initialize the array with random values from 0 to 99
    for (int i = 0; i < size; i++)
        arr[i] = rand() % 100;

    // Print the array
    for (int i = 0; i < size; i++)
        printf("%d ", arr[i]);

    printf("\n");

    // Access the first element of every DRAM row and write 0 to it
    for (int i = 0; i < iter; i++)
        for (int j = 0; j < size; j += 1024)
            arr[j] = 0;

    // Print the array, print a new line after every 1024 elements
    for (int i = 0; i < size; i++)
    {
        printf("%d ", arr[i]);
        if ((i + 1) % 1024 == 0)
            printf("\n");
    }
}