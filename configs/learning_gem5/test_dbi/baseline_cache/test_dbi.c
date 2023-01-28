#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[])
{
    // An array on n elements
    int *arr;

    // Command line arguments
    int k_blocks, iter, opt;

    while ((opt = getopt(argc, argv, ":k:t")) != -1)
    {
        switch (opt)
        {

        case 'k':
            if (optarg != NULL)
                k_blocks = atoi(optarg);

        case 't':
            iter = atoi(argv[4]);
            break;

        default:
            break;
        }
    }

    // Allocate memory for the array
    arr = (int *)malloc(4000000 * sizeof(int));

    // Every DRAM row contains 64 cache blocks, and each cache block is 64 bytes
    // Each cache block can store 16 integers
    // So, the number of integers that can be stored in a DRAM row is 1024

    // Initialize the array with random values from 0 to 99
    for (int i = 0; i < 4000000; i++)
        arr[i] = rand() % 100;

    // Choose a random DRAM row and write to k blocks in that row
    int row = rand() % 4000;
    int start = row * 1024;
    int end = start + k_blocks * 16;
    // Write to the array
    for (int i = start; i < end; i++)
        arr[i] = 0;
}