#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>

int main(int argc, char *argv[])
{
    int *arr;

    // Command line arguments
    int k_blocks = 1, iter = 1, num_ele, opt;

    while ((opt = getopt(argc, argv, ":n:k:t:")) != -1)
    {
        switch (opt)
        {
        // Use atoi(optarg) instead of atoi(argv[2])
        case 'n':
            if (optarg != NULL)
                num_ele = atoi(optarg);
            break;

        case 'k':
            if (optarg != NULL)
                k_blocks = atoi(optarg);
            break;

        case 't':
            if (optarg != NULL)
                iter = atoi(optarg);
            break;
        }
    }

    // Print the values of the command line arguments
    printf("Number of elements: %d, Number of cache blocks: %d, Number of iterations: %d\n", num_ele, k_blocks, iter);

    // Allocate memory for the array
    arr = (int *)malloc(num_ele * sizeof(int));

    // Initialize the array with zeroes
    for (int i = 0; i < num_ele; i++)
        arr[i] = 0;

    // For t iterations, : choose a random region => write to k cache blocks within the region;
    // Write to random k_blocks of the same DRAM row

    srand(33);

    int i = 0;
    while (i < iter)
    {
        // Choose a random DRAM row
        int row_id = rand() % (num_ele / 1024);

        for (int j = 1; j <= k_blocks; j++)
        {
            // Choose a random cache block within the DRAM row
            int block_id = rand() % 64;

            int d = row_id * 1024 + block_id * 16;

            // Write to the cache block
            arr[d] = 1;
                }

        i++;
    }

    // Compute the sum of all the elements in the array
    int sum = 0;
    for (int i = 0; i < num_ele; i++)
        sum += arr[i];

    // Print the sum
    printf("Sum: %d \n", sum);

    // Free the memory
    free(arr);

    return 0;
}