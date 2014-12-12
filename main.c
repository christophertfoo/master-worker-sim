/*
 * main.c
 *
 *  Created on: Nov 23, 2014
 *      Author: chris
 */

#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

#include "config.h"
#include "master.h"
#include "worker.h"

int main(int argc, char ** argv)
{
    int rank, num_procs;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (argc == 1 && rank == MASTER_RANK)
    {
        printf("Usage: %s <path to file containing job descriptions>\n", argv[0]);
    }
    else
    {
        if (rank == MASTER_RANK)
        {
            run_master(argv[1], num_procs);
        }
        else
        {
            run_worker(rank);
        }
    }

    /* Wait for everyone to finish */
    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();
    return 0;
}
