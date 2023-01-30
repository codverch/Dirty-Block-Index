#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>

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
                k_blocks = atoi(argv[2]);

        case 't':
            iter = atoi(argv[4]);
            break;
        }
    }

    // Allocate memory for the array
    arr = (int *)malloc(4000000 * sizeof(int));

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
        // Choose a random DRAM row
        int row = rand() % 4000;

        // Choose a random start index within the row
        int start = row * 1024 + rand() % 1024;

        // Choose an end index based on the number of blocks to be written
        int end = start + (k_blocks * 16 <= (4000000 - start)) ? k_blocks * 16 : 4000000 - start;

        // Write 1 to the array for iter times
        for (int j = start; j < end; j++)
            if (j < 4000000)
                arr[j] = 1;
            else
                break;

        i++;
    }

    // Free the memory
    free(arr);
}