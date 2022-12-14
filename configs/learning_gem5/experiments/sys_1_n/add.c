/*Aim: This program performs addition of two arrays and stores the result in the third. However, during every computation a new cacheline is brought in. */

#include <stdio.h>
#include <stdlib.h>     // for malloc(), and rand()
#include <getopt.h>     // for getopt()
#include <time.h>       // for clock() 
#include <string.h>     // for strcmp()
#include "/home/gandiva/deepanjali/gem5/include/gem5/asm/generic/m5ops.h"

int main(int argc, char *argv[])
{
    // Sum of two arrays: three pointers
    int *a1, *a2, *a3;

    // Declaring two variables - for the command line input
    int s = 0, j = 0; // s - size of the arrays, j - for index
    // char i[10]; // i = the error to be injected

    // Declaring a choice variable to figure-out which error needs to be injected
    int ch, size, iter=0; // iter - iterations

    // To measure the performance 

    clock_t start, end; 

    double cpu_time_used, time_taken;

    // Get the command-line arguments n and i
    while (( s = getopt(argc, argv, ":n:i:t")) != -1) // Where, t - number of iterations
    {
      switch(s){ 

            case 'n': if(optarg != NULL)
                        size = atoi(optarg);
            case 'i':
                      if(strcmp(argv[4], "bo") == 0) // Buffer - Overflow
                          ch = 1;
                      
                      else if(strcmp(argv[4], "uaf")==0) // Use - After - Free
                        ch = 2;

                      else if(strcmp(argv[4], "df")==0) // Double - Free
                        ch = 3;
                      
                      else if(strcmp(argv[4], "u")==0) // Unallocated - Memory
                        ch = 4;

                      else printf("We do not support injecting this error currently\n");
              
           case 't': iter = atoi(argv[6]);
                      break;

            default: break;
        }
    }

    // Allocate memory in the heap region to the arrays

    a1 = (int *) malloc (16 * size * sizeof(int));
    a2 = (int *) malloc (16 * size * sizeof(int));
    a3 = (int *) malloc (16 * size * sizeof(int));

    // Assign values to the arrays - random values between 0 and 99

    for( j = 0; j < size; j++)
        a1[j] = rand() % 100;

    for( j = 0; j < size; j++)
        a2[j] = rand() % 100;


//----------------------------------------------------------------------------------------------------------------------

    // Run one iteration of the computation before measuring the computation - why??
    for ( j = 0; j < size; j++)
        {
            *(a3 + j*16) = *(a1 + j*16) + *(a2 + j*16);
            
        }
        
//-----------------------------------------------------------------------------------------------------------------------
    start = clock(); // Start the timer
    // Perform the computation
    // The computation needs to occur for multiple iterations - for us to observe the overhead
    m5_reset_stats(0,0);
    while(iter != 0)
    {
            for ( j = 0; j < size; j++)
            {
            *(a3 + j*16) = *(a1 + j*16) + *(a2 + j*16);
            //*(a3 + j) = *(a1 + j) + *(a2 + j);
            }
        
            iter--;
    }
    m5_dump_stats(0,0);
    
        end = clock();

        cpu_time_used = ((double) end - start)/CLOCKS_PER_SEC; // In seconds

        time_taken = cpu_time_used * 1000; // In milliseconds
        printf("\nThe time taken is %lf milliseconds\n", time_taken); // lf is the format specifier for double in c

        // Inject the errors based on the choice
        
        if( ch == 1 ) // Buffer - Overflow
        {
            printf("Introducing buffer overflow - Accessing a location beyond the range");
            // Access a location beyond the range
            *(a3 + s + 1) = 4; // The value 4 is randomly chosen
            printf("\nThe value written is: %d\n", *(a3 + s + 1));
        }


        else if( ch == 2 ) // Use - After - Free
        {
            printf("\nBefore freeing: %d\n", *(a3 + 1));

            // Free the memory locations and then try to access it
            free(a1);
            free(a2);
            free(a3);

            printf("After freeing: %d\n", *(a3 + 1));

            // Writing to a memory location that has been freed

            *(a3 + 1) = 4;
            printf("New value: %d\n", *(a3 + 1));

        }

        else if( ch == 3 ) // Double - Free
        {
            // Free the memory twice : to introduce double-free-error

            free(a1);
            free(a2);
            free(a3);

            free(a1);
            free(a2);
            free(a3); 

            // Occurs in multi-threaded programs   
        }

    /*  else if ( ch == 4) // Unallocated - Memory
        {
        } 
    */
        else {
        printf ( " To be filled \n");
        }

        
        free(a1);
        free(a2);
        free(a3);
    
        return 0;

    }