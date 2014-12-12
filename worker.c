/*
 * worker.c
 *
 *  Created on: Nov 23, 2014
 *      Author: chris
 */

#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>

#include "config.h"
#include "job.h"
#include "message.h"
#include "shared.h"
#include "worker.h"

enum worker_request_types
{
    RECV_REQUEST = 0, SEND_REQUEST, NUM_REQUEST_TYPES
};

static inline enum error_code send_signal(enum message_type type)
{
    enum error_code status = NO_ERROR;

    struct message msg;

    msg.type = type;

    if (MPI_Send(&msg, sizeof(struct message), MPI_UNSIGNED_CHAR, MASTER_RANK, 0,
    MPI_COMM_WORLD) != MPI_SUCCESS)
    {
        status = MPI_ERROR;
    }
    return status;
}

static inline enum error_code get_num_jobs(unsigned char *message, int *pNumJobs)
{
    enum error_code status = NO_ERROR;

    if (message == NULL || pNumJobs == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        *pNumJobs = JOBS_MESSAGE(message)->num_jobs;
    }
    return status;
}

static inline enum error_code worker_get_next_job(unsigned char *message,
        struct base_job_message **pCurrent, int *pIndex, int num_jobs)
{
    enum error_code status = NO_ERROR;

    if (message == NULL || pCurrent == NULL || pIndex == NULL || num_jobs < 0)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        /* The previous job was the last one, reset current */
        if (*pIndex == num_jobs)
        {
            *pIndex = -1;
            *pCurrent = NULL;
        }

        /* If pCurrent is not set, grab the first job */
        else if (*pCurrent == NULL)
        {
            *pCurrent = (struct base_job_message *) JOBS_MESSAGE(message)->jobs;
            *pIndex = 1;
        }

        /* Otherwise, grab the next one */
        else
        {
            *pCurrent = (struct base_job_message *) get_next_job((unsigned char *) *pCurrent);
            (*pIndex)++;
        }
    }
    return status;
}

enum error_code run_worker(int rank)
{
    unsigned char *recv_message, *jobs_message, *send_message, *queued_message, *tmp;
    size_t send_size = 0;
    MPI_Request requests[2];
    MPI_Status mpi_status;
    int flag;

    enum error_code status = NO_ERROR;

    char master_running = true;
    char queued = false;
    char running = true;
    char waiting = false;
    char sending = false;

    int request_index;
    int job_index = 0;
    int num_jobs = 0;
    unsigned char *pEnd = NULL;
    struct base_job_message *pCurrent = NULL;

#ifdef DEBUG
    printf("Worker[%d]: Start Initializing\n", rank);
#endif

    recv_message = (unsigned char *) malloc(sizeof(unsigned char) * MAX_MESSAGE_SIZE);
    jobs_message = (unsigned char *) malloc(sizeof(unsigned char) * MAX_MESSAGE_SIZE);
    send_message = (unsigned char *) malloc(sizeof(unsigned char) * MAX_MESSAGE_SIZE);
    queued_message = (unsigned char *) malloc(sizeof(unsigned char) * MAX_MESSAGE_SIZE);

#ifdef DEBUG
    printf("Worker[%d]: Done Initializing\n", rank);
#endif

    /* Wait for everyone to finish setting up */
    MPI_Barrier(MPI_COMM_WORLD);

    if (recv_message == NULL || send_message == NULL || jobs_message == NULL
            || queued_message == NULL)
    {
        status = MEMORY_ERROR;
    }
    else
    {

        /* Start listening for message sfrom the master */
        MPI_Irecv(recv_message, MAX_MESSAGE_SIZE, MPI_UNSIGNED_CHAR, MASTER_RANK, 0, MPI_COMM_WORLD,
                &requests[RECV_REQUEST]);

        /* While running */
        while (running && status == NO_ERROR)
        {

            /* If received message then process it*/
            if (MPI_Test(&requests[RECV_REQUEST], &flag, &mpi_status) == MPI_SUCCESS && flag)
            {
                switch (MESSAGE_TYPE(recv_message))
                {
                    case MASTER_DONE:
                    case MASTER_ERROR:
#ifdef DEBUG
                        if(MESSAGE_TYPE(recv_message) == MASTER_DONE)
                        {
                            printf("Worker[%d]: Receive MASTER_DONE\n", rank);
                        }
                        else
                        {
                            printf("Worker[%d]: Receive MASTER_ERROR\n", rank);
                        }
#endif
                        master_running = false;
                        running = false;
                        break;

                    case MASTER_JOBS:
#ifdef DEBUG
                        printf("Worker[%d]: Receive MASTER_JOBS\n", rank);
#endif
                        job_index = 0;

                        /* Switch the receive message buffers */
                        tmp = jobs_message;
                        jobs_message = recv_message;
                        recv_message = tmp;

                        /* Parse the recieved message for the number of jobs and the first job */
                        get_num_jobs(jobs_message, &num_jobs);
                        status = worker_get_next_job(jobs_message, &pCurrent, &job_index, num_jobs);

                        /* Update state flags */
                        waiting = false;
                        break;

                    default:
#ifdef DEBUG
                        printf("Worker[%d]: Receive UNKNOWN\n", rank);
#endif
                        break;
                }

                /* Listen for the next message */
                MPI_Irecv(recv_message, MAX_MESSAGE_SIZE, MPI_UNSIGNED_CHAR, MASTER_RANK, 0,
                MPI_COMM_WORLD, &requests[RECV_REQUEST]);
            }

            /* Else if message was sent and need to send a message, send the message */
            else if ((!sending && queued)
                    || (sending
                            && MPI_Test(&requests[SEND_REQUEST], &flag, &mpi_status) == MPI_SUCCESS
                            && flag))
            {
                if (queued)
                {
#ifdef DEBUG
                    printf("Worker[%d]: Sending message\n", rank);
#endif
                    /* Switch message buffers */
                    tmp = send_message;
                    send_message = queued_message;
                    queued_message = tmp;

                    MPI_Isend(send_message, send_size, MPI_UNSIGNED_CHAR, MASTER_RANK, 0,
                    MPI_COMM_WORLD, &requests[SEND_REQUEST]);
                    sending = true;
                    queued = false;
                }
                else
                {
#ifdef DEBUG
                    printf("Worker[%d]: Sent message\n", rank);
#endif
                    sending = false;
                    queued = false;
                }
            }

            /*
             * Not waiting for a message to be sent, can do more processing without worrying
             * about overflowing our "queue".
             */
            else if (!queued && !sending)
            {
                if (pCurrent != NULL)
                {
#ifdef DEBUG
                    printf("Worker[%d]: Do job [%d] => %d / %d\n", rank, pCurrent->base_fields.id, job_index, num_jobs);
#endif
                    /* "Run" Job */
                    switch (pCurrent->base_fields.type)
                    {
                        case FLOPS_JOB:
#ifdef DEBUG
                            printf("Worker[%d]: FLOPS Job\n", rank);
#endif
                            SMPI_SAMPLE_FLOPS(((struct flops_job_message *) pCurrent)->flops);
                            break;

                        case TIME_JOB:
#ifdef DEBUG
                            printf("Worker[%d]: Time Job\n", rank);
#endif
                            sleep(((struct time_job_message *) pCurrent)->actual_time);
                            break;

                        default:
#ifdef DEBUG
                            printf("Worker[%d]: Invalid Job type\n", rank);
#endif
                            status = INVALID_TYPE;
                            running = false;
                    }

                    if (status == NO_ERROR)
                    {

                        if (pCurrent->base_fields.result_size > 0)
                        {
#ifdef DEBUG
                            printf("Worker[%d]: Queuing Result\n", rank);
#endif
                            MESSAGE_TYPE(queued_message) = WORKER_RESULT;
                            RESULTS_MESSAGE(queued_message)->num_results = 0;
                            pEnd = NULL;
                            status = add_result(queued_message, &pEnd, pCurrent);
                            send_size = pEnd - queued_message;
                            queued = true;
                        }
                        status = worker_get_next_job(jobs_message, &pCurrent, &job_index, num_jobs);
                    }
                }

                /* No jobs, ask master for more work */
                else if (pCurrent == NULL && !waiting)
                {
#ifdef DEBUG
                    printf("Worker[%d]: Out of jobs, asking for more\n", rank);
#endif
                    MESSAGE_TYPE(queued_message) = WORKER_REQUEST;
                    send_size = sizeof(struct message);
                    queued = true;
                    waiting = true;
                }
            }
            /* Else, wait for message to be received or sent before moving on */
            else
            {
#ifdef DEBUG
                printf("Worker[%d]: Blocked on communications, waiting for send / receive to complete\n", rank);
#endif
                MPI_Waitany(NUM_REQUEST_TYPES, requests, &request_index, &mpi_status);
            }
        }
    }

#ifdef DEBUG
    printf("Worker[%d]: Shutting down\n", rank);
#endif

    if (master_running)
    {
        if (status != NO_ERROR)
        {

#ifdef DEBUG
            printf("Worker[%d]: Sending WORKER_ERROR to master\n", rank);
#endif

            send_signal(WORKER_ERROR);
        }
        else
        {

#ifdef DEBUG
            printf("Worker[%d]: Sending WORKER_DONE to master\n", rank);
#endif

            send_signal(WORKER_DONE);
        }
    }

    free(recv_message);
    free(jobs_message);
    free(send_message);
    free(queued_message);

    return status;
}
