/*
 * message.c
 *
 *  Created on: Dec 4, 2014
 *      Author: chris
 */

#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "shared.h"
#include "message.h"

unsigned char *get_next_result(unsigned char *current)
{
    struct result_message *pResult;
    unsigned char *pNext;
    unsigned int result_size;

    if (current == NULL)
    {
        pNext = NULL;
    }
    else
    {
        pResult = (struct result_message *) current;

        result_size = pResult->result_size;
        pResult++;
        pNext = (unsigned char *) pResult;
        pNext += result_size;
    }
    return pNext;
}

enum error_code add_result(unsigned char *message, unsigned char **pEnd,
        struct base_job_message *pJob)
{
    int i = 0;
    unsigned char *end;
    enum error_code status = NO_ERROR;
    if (message == NULL || pJob == NULL)
    {
        status = BAD_ARGUMENT;
    }

    /* Find the end of the message */
    if (pEnd == NULL || *pEnd == NULL)
    {
        end = RESULTS_MESSAGE(message)->results;
        for (i = 0; i < RESULTS_MESSAGE(message)->num_results; i++)
        {
            end = get_next_result(end);
        }
    }
    else
    {
        end = *pEnd;
    }

    if (end == NULL)
    {
        status = BAD_MESSAGE_FORMAT;
    }
    else
    {
        RESULTS_MESSAGE(message)->num_results++;
        ((struct result_message *) end)->id = pJob->base_fields.id;
        ((struct result_message *) end)->result_size = pJob->base_fields.result_size;

        if (pEnd != NULL)
        {
            *pEnd = get_next_result(end);
        }
    }
    return status;
}

unsigned char *get_next_job(unsigned char *current)
{
    unsigned char *pNext;
    enum job_type type;
    unsigned int data_size;

    struct flops_job_message *pFlops;
    struct time_job_message *pTime;

    if (current == NULL)
    {
        pNext = NULL;
    }
    else
    {
        type = ((struct base_job_message *) current)->base_fields.type;
        data_size = ((struct base_job_message *) current)->base_fields.data_size;

        if (type == FLOPS_JOB)
        {
            pFlops = (struct flops_job_message *) current;
            pFlops++;
            pNext = (unsigned char *) pFlops;
            pNext += data_size;
        }
        else if (type == TIME_JOB)
        {
            pTime = (struct time_job_message *) current;
            pTime++;
            pNext = (unsigned char *) pTime;
            pNext += data_size;
        }
        else
        {
            pNext = NULL;
        }
    }
    return pNext;
}

enum error_code add_job(unsigned char *message, unsigned char **pEnd, struct base_job *job)
{
    int i;
    unsigned char *end;
    struct flops_job *pFlops;
    struct time_job *pTime;
    struct flops_job_message *pFlopsMsg;
    struct time_job_message *pTimeMsg;
    enum error_code status = NO_ERROR;

    if (pEnd == NULL)
    {
        end = NULL;
    }
    else
    {
        end = *pEnd;
    }

    /* Find the end of the message */
    if (end == NULL)
    {
        end = JOBS_MESSAGE(message)->jobs;
        for (i = 0; i < JOBS_MESSAGE(message)->num_jobs; i++)
        {
            end = get_next_job(end);
        }
    }

    if (end == NULL)
    {
        status = BAD_MESSAGE_FORMAT;
    }
    else
    {
        JOBS_MESSAGE(message)->num_jobs++;

        switch (job->base_fields.type)
        {
            case FLOPS_JOB:
                pFlops = (struct flops_job *) job;
                pFlopsMsg = (struct flops_job_message *) end;

                pFlopsMsg->base_fields.type = FLOPS_JOB;
                pFlopsMsg->base_fields.id = pFlops->base_fields.id;
                pFlopsMsg->base_fields.data_size = pFlops->base_fields.data_size;
                pFlopsMsg->base_fields.result_size = pFlops->base_fields.result_size;

                pFlopsMsg->flops = pFlops->flops;
                break;

            case TIME_JOB:
                pTime = (struct time_job *) job;
                pTimeMsg = (struct time_job_message *) end;

                pTimeMsg->base_fields.type = TIME_JOB;
                pTimeMsg->base_fields.id = pTime->base_fields.id;
                pTimeMsg->base_fields.data_size = pTime->base_fields.data_size;
                pTimeMsg->base_fields.result_size = pTime->base_fields.result_size;

                pTimeMsg->actual_time = pTime->actual_time;
                break;

            default:
                break;
        }

        if (pEnd != NULL)
        {
            *pEnd = get_next_job(end);
        }

    }

    return status;
}
