/*
 * schedulers.c
 *
 *  Created on: Dec 9, 2014
 *      Author: chris
 */

#include <stdlib.h>
#include <math.h>

#include "shared.h"
#include "schedulers.h"

enum error_code init_one_job_args(struct one_job_args *args, struct job_list *pList,
        unsigned char *pMessage)
{
    enum error_code status = NO_ERROR;
    if (args == NULL || pList == NULL || pMessage == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        args->pList = pList;
        args->pMessage = pMessage;
    }
    return status;
}

enum error_code one_job_scheduler(struct one_job_args *args)
{
    struct base_job *pJob;
    unsigned char *pEnd = NULL;
    enum error_code status = NO_ERROR;

    if (args == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {

        MESSAGE_TYPE(args->pMessage) = MASTER_JOBS;
        JOBS_MESSAGE(args->pMessage)->num_jobs = 0;
        args->message_size = 0;

        pJob = get_next_job_list(args->pList);
        if (pJob == NULL)
        {
            status = OUT_OF_JOBS;
        }
        else
        {
            status = add_job(args->pMessage, &pEnd, pJob);
            if (status == NO_ERROR)
            {
                args->message_size = pEnd - args->pMessage;
            }
        }
    }
    return status;
}

enum error_code init_static_job_args(struct static_job_args *args, struct job_list *pList,
        unsigned char *pMessage, int num_procs)
{
    enum error_code status = NO_ERROR;
    if (args == NULL || pList == NULL || pMessage == NULL || num_procs <= 0)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        args->pList = pList;
        args->pMessage = pMessage;
        args->num_workers = num_procs - 1;
    }
    return status;
}

enum error_code static_job_scheduler(struct static_job_args *args)
{
    struct base_job *pJob;
    int i, num_jobs;
    unsigned char *pEnd = NULL;
    enum error_code status = NO_ERROR;

    if (args == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        MESSAGE_TYPE(args->pMessage) = MASTER_JOBS;
        JOBS_MESSAGE(args->pMessage)->num_jobs = 0;
        args->message_size = 0;

        num_jobs = (int) ceil(args->pList->num_jobs / (double) args->num_workers);

        for (i = 0; i < num_jobs && status == NO_ERROR; i++)
        {
            pJob = get_next_job_list(args->pList);
            if (pJob == NULL)
            {
                if (i == 0)
                {
                    status = OUT_OF_JOBS;
                }
                else
                {
                    break;
                }
            }
            else
            {
                status = add_job(args->pMessage, &pEnd, pJob);
            }
        }

        if (status == NO_ERROR)
        {
            args->message_size = pEnd - args->pMessage;
        }
    }
    return status;
}

enum error_code init_dynamic_job_args(struct dynamic_job_args *args, struct job_list *pList,
        unsigned char *pMessage, int num_procs)
{
    int num_jobs, count, i, num_workers;
    struct dynamic_job_count **pNextCount, *newCount;
    enum error_code status = NO_ERROR;
    if (args == NULL || pList == NULL || pMessage == NULL || num_procs <= 0)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        num_workers = num_procs - 1;
        args->pList = pList;
        args->pMessage = pMessage;
        num_jobs = args->pList->num_jobs;
        pNextCount = &args->counts;

        while (num_jobs > 0 && status == NO_ERROR)
        {
            count = ceil(num_jobs / 2.0 / num_workers);
            if (count < 1)
            {
                count = 1;
            }
            for (i = 0; i < num_workers && num_jobs > 0 && status == NO_ERROR; i++)
            {
                newCount = (struct dynamic_job_count *) malloc(sizeof(struct dynamic_job_count));
                if (newCount == NULL)
                {
                    status = MEMORY_ERROR;
                }
                else
                {
                    newCount->count = num_jobs < count ? num_jobs : count;
                    newCount->next = NULL;
                    *pNextCount = newCount;
                    pNextCount = &(newCount->next);
                    num_jobs -= newCount->count;
                }
            }
        }

    }
    return status;
}

enum error_code dynamic_job_scheduler(struct dynamic_job_args *args)
{
    enum error_code status = NO_ERROR;
    int i;
    unsigned char *pEnd = NULL;
    struct dynamic_job_count *count;
    struct base_job *pJob;
    if (args == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else if (args->counts == NULL)
    {
        status = OUT_OF_JOBS;
    }
    else
    {
        MESSAGE_TYPE(args->pMessage) = MASTER_JOBS;
        JOBS_MESSAGE(args->pMessage)->num_jobs = 0;
        args->message_size = 0;

        count = args->counts;
        args->counts = args->counts->next;
        for (i = 0; i < count->count && status == NO_ERROR; i++)
        {
            pJob = get_next_job_list(args->pList);
            if (pJob == NULL)
            {
                if (i == 0)
                {
                    status = OUT_OF_JOBS;
                }
                else
                {
                    break;
                }
            }
            else
            {
                status = add_job(args->pMessage, &pEnd, pJob);
            }
        }

        if (status == NO_ERROR)
        {
            args->message_size = pEnd - args->pMessage;
        }
        free(count);
    }

    return status;
}
