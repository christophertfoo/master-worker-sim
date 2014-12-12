/*
 * master.c
 *
 *  Created on: Nov 23, 2014
 *      Author: chris
 */

#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include "config.h"
#include "shared.h"
#include "message.h"
#include "job.h"
#include "schedulers.h"
#include "master.h"

static inline enum error_code init_workers(struct worker_info *workers, int num_workers)
{
    int i;
    enum error_code status = NO_ERROR;

    if (workers == NULL || num_workers <= 0)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        workers->states = (enum worker_state *) malloc(sizeof(enum worker_state) * num_workers);

        if (workers->states == NULL)
        {
            status = MEMORY_ERROR;
        }
        else
        {
            workers->num_workers = num_workers;

            memset(workers->counts, 0, sizeof(unsigned int) * NUM_WORKER_STATES);

            for (i = 0; i < num_workers; i++)
            {
                workers->states[i] = STARTING;
            }

            workers->counts[STARTING] = num_workers;

        }
    }
    return status;
}

static inline unsigned int get_worker_index(int rank, struct worker_info *workers,
        enum error_code *pStatus)
{
    unsigned int index;
    enum error_code status = NO_ERROR;
    if (rank == MASTER_RANK || rank > workers->num_workers)
    {
        index = 0;
        status = BAD_ARGUMENT;
    }
    else if (rank < MASTER_RANK)
    {
        index = rank;
    }
    else
    {
        index = rank - 1;
    }
    if (pStatus != NULL)
    {
        *pStatus = status;
    }
    return index;
}

static inline unsigned int worker_index_to_rank(int index)
{
    return index < MASTER_RANK ? index : index + 1;
}

static inline enum error_code destroy_workers(struct worker_info *workers)
{
    enum error_code status = NO_ERROR;
    if (workers == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        free(workers->states);
        workers->states = NULL;
        workers->num_workers = 0;
    }

    return status;
}

static inline enum error_code bcast_signal(enum message_type type, struct worker_info *workers)
{
    struct message msg;
    MPI_Request *requests;
    MPI_Status *statuses;
    int i, j;
    enum error_code status = NO_ERROR;
    unsigned int num_alive = NUM_ALIVE_WORKERS(*workers);
    requests = (MPI_Request *) malloc(sizeof(MPI_Request) * num_alive);
    statuses = (MPI_Status *) malloc(sizeof(MPI_Status) * num_alive);
    if (requests == NULL || statuses == NULL)
    {
        status = MEMORY_ERROR;
    }
    else
    {
        msg.type = type;
        for (i = 0, j = 0; i < workers->num_workers; i++)
        {
            if (WORKER_ALIVE(*workers, i))
            {
                MPI_Isend(&msg, sizeof(struct message), MPI_UNSIGNED_CHAR, worker_index_to_rank(i),
                        0, MPI_COMM_WORLD, &requests[j]);
                j++;
            }
        }

        MPI_Waitall(NUM_ALIVE_WORKERS(*workers), requests, statuses);
    }

    /* Clean up */
    free(requests);
    free(statuses);

    return status;
}

static inline enum error_code change_worker_state(int rank, enum worker_state state,
        struct worker_info *workers)
{
    enum error_code status = NO_ERROR;
    unsigned int index;
    if (workers == NULL || rank == MASTER_RANK)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        index = get_worker_index(rank, workers, &status);
        if (status == NO_ERROR)
        {
            workers->counts[workers->states[index]]--;
            workers->states[index] = state;
            workers->counts[state]++;
        }
    }
    return status;
}

enum error_code run_master(char *job_loc, int num_ranks)
{
    enum error_code status;
    MPI_Status mpi_status;
    char running = true;
    unsigned char *message;
    struct worker_info workers;
    struct job_list list;
    double start_time;

#ifdef ONE_JOB
    struct one_job_args scheduler_args;
#elif STATIC
    struct static_job_args scheduler_args;
#elif DYNAMIC
    struct dynamic_job_args scheduler_args;
#endif

#ifdef DEBUG
    printf("Master[%d]: Start Initializing\n", MASTER_RANK);
#endif

    status = init_job_list(&list);
    status = parse_jobs(job_loc, &list);

    message = (unsigned char *) malloc(sizeof(unsigned char) * MAX_MESSAGE_SIZE);
    status = init_workers(&workers, num_ranks - 1);

#ifdef ONE_JOB
    status = init_one_job_args(&scheduler_args, &list, message);
#elif STATIC
    status = init_static_job_args(&scheduler_args, &list, message, num_ranks);
#elif DYNAMIC
    status = init_dynamic_job_args(&scheduler_args, &list, message, num_ranks);
#endif

#ifdef DEBUG
    printf("Master[%d]: Done Initializing\n", MASTER_RANK);
#endif

    /* Wait for everyone to finish setting up */
    MPI_Barrier(MPI_COMM_WORLD);

    if (message == NULL || status != NO_ERROR)
    {

#ifdef DEBUG
        printf("Master[%d]: Error with initialization\n", MASTER_RANK);
#endif
        /* Can't send a message since the allocation failed or
         we couldn't initialize our worker tracking struct...
         I guess we're just screwed? */
    }
    else
    {
        start_time = MPI_Wtime();
        while (running)
        {
#ifdef DEBUG
            printf("Master[%d]: Waiting for message from workers\n", MASTER_RANK);
#endif
            MPI_Recv(message, MAX_MESSAGE_SIZE, MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG,
            MPI_COMM_WORLD, &mpi_status);

            if (mpi_status.MPI_ERROR == MPI_SUCCESS)
            {

                switch (MESSAGE_TYPE(message))
                {
                    case WORKER_DONE:
#ifdef DEBUG
                        printf("Master[%d]: Received WORKER_DONE from Worker[%d]\n", MASTER_RANK, mpi_status.MPI_SOURCE);
#endif
                        change_worker_state(mpi_status.MPI_SOURCE, DONE, &workers);
                        break;
                    case WORKER_ERROR:
#ifdef DEBUG
                        printf("Master[%d]: Received WORKER_ERROR from Worker[%d]\n", MASTER_RANK, mpi_status.MPI_SOURCE);
#endif
                        change_worker_state(mpi_status.MPI_SOURCE, FAILED, &workers);
                        break;
                    case WORKER_REQUEST:

#ifdef DEBUG
                        printf("Master[%d]: Received WORKER_REQUEST from Worker[%d]\n", MASTER_RANK, mpi_status.MPI_SOURCE);
#endif
                        change_worker_state(mpi_status.MPI_SOURCE, FAILED, &workers);

#ifdef ONE_JOB
                        status = one_job_scheduler(&scheduler_args);
#elif STATIC
                        status = static_job_scheduler(&scheduler_args);
#elif DYNAMIC
                        status = dynamic_job_scheduler(&scheduler_args);
#endif
#ifdef DEBUG
                        printf("Master[%d]: Created work response for Worker[%d]\n", MASTER_RANK, mpi_status.MPI_SOURCE);
#endif
                        if (status == NO_ERROR)
                        {
#ifdef DEBUG
                            printf("Master[%d]: Sending MASTER_JOBS [%d bytes] to Worker[%d]\n", MASTER_RANK, scheduler_args.message_size, mpi_status.MPI_SOURCE);
#endif
                            MPI_Send(message, scheduler_args.message_size, MPI_UNSIGNED_CHAR,
                                    mpi_status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                            change_worker_state(mpi_status.MPI_SOURCE, RUNNING, &workers);
#ifdef DEBUG
                            printf("Master[%d]: Jobs sent to Worker[%d]\n", MASTER_RANK, mpi_status.MPI_SOURCE);
#endif
                        }
                        else if (status == OUT_OF_JOBS)
                        {
#ifdef DEBUG
                            printf("Master[%d]: Out of jobs, cannot send jobs to Worker[%d]\n", MASTER_RANK, mpi_status.MPI_SOURCE);
#endif
                            MESSAGE_TYPE(message) = MASTER_DONE;
                            MPI_Send(message, sizeof(struct message), MPI_UNSIGNED_CHAR,
                                    mpi_status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                        }

                        break;
                    case WORKER_RESULT:
#ifdef DEBUG
                        printf("Master[%d]: Received WORKER_RESULT from Worker[%d]\n", MASTER_RANK, mpi_status.MPI_SOURCE);
#endif
                        /* Ignore results */
                        break;
                    default:
#ifdef DEBUG
                        printf("Master[%d]: Received UNKNOWN message [%d] from Worker[%d]\n", MASTER_RANK, MESSAGE_TYPE(message), mpi_status.MPI_SOURCE);
#endif
                        break;
                }
            }
#ifdef DEBUG
            else
            {
                printf("Master[%d]: Error [%d] when receiving from workers\n", MASTER_RANK, mpi_status.MPI_ERROR);
            }
#endif

            if (NUM_ALIVE_WORKERS(workers) == 0)
            {
                running = false;
            }
        }

#ifdef DEBUG
        printf("Master[%d]: DONE!\n", MASTER_RANK);
#endif
        printf("==================================\n");
        printf("Execution time: %f seconds\n", MPI_Wtime() - start_time);
        printf("==================================\n");
    }

    /* Clean up */
    destroy_workers(&workers);
    free(message);
    destroy_job_list(&list);
    return status;
}
