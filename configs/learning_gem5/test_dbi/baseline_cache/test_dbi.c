#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>
#include <typeinfo>

int main(int argc, char *argv[])
{
    // An array of 4 million integer elements
    int *arr;

    // Command line arguments
    int k_blocks = 1, iter = 1, opt;
    while ((opt = getopt(argc, argv, ":k:t")) != -1)
    {
        switch (opt)
        {

        case 'k':
            if (optarg != NULL)
                k_blocks = atoi(optarg);

        case 't':
            if (optarg != NULL)
                iter = atoi(optarg);
            break;
        }
    }

    // Allocate memory for the array
    arr = (int *)malloc(4000000 * sizeof(int));
    if (arr == NULL)
    {
        printf("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Every DRAM row contains 64 cache blocks, and each cache block is 64 bytes
    // Each cache block can store 16 integers
    // So, the number of integers that can be stored in a DRAM row is 1024

    // Initialize the array with zeroes
    for (int i = 0; i < 4000000; i++)
        arr[i] = 0;

    // For t iterations, : choose a random region => write to k cache blocks within the region;

    int i = 0;
    while (i < iter)
    {
        // Write to one element each in random k cache blocks

        for (int j = 1; j <= k_blocks; j++)
        {
            // Choose a random DRAM row
            int row_id = rand() % 4000;

            // Choose a random cache block within the DRAM row
            int block_id = rand() % 64;

            // Write to one element in the cache block
            arr[row_id * 1024 + block_id * 16] = 1;
        }

        i++;
    }

    // Free the memory
    free(arr);
}