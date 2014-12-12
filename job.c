/*
 * job.c
 *
 *  Created on: Nov 30, 2014
 *      Author: chris
 */

#include <stdlib.h>
#include <stdio.h>

#include "shared.h"
#include "job.h"

static enum error_code read_base_job(FILE *fp, struct base_job_fields *pFields)
{
    enum error_code status = NO_ERROR;
    if (fp == NULL || pFields == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        pFields->assigned_time = UNINIT_TIME;
        pFields->completion_time = UNINIT_TIME;
        if (fscanf(fp, "ID = %u\n", &pFields->id) != 1
                || fscanf(fp, "Queued Time = %lf\n", &pFields->queued_time) != 1
                || fscanf(fp, "Number of Nodes = %u\n", &pFields->num_nodes) != 1
                || fscanf(fp, "Data Size = %lu\n", &pFields->data_size) != 1
                || fscanf(fp, "Result Size = %lu\n", &pFields->result_size) != 1)
        {
            status = BAD_FILE_FORMAT;
        }
    }
    return status;
}

static struct time_job *read_time_job(FILE *fp, enum error_code *pError)
{
    struct time_job *pJob;
    enum error_code status = NO_ERROR;

    if (fp == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        pJob = (struct time_job *) malloc(sizeof(struct time_job));
        if (pJob == NULL)
        {
            status = MEMORY_ERROR;
        }
        else
        {
            pJob->base_fields.type = TIME_JOB;

            if ((status = read_base_job(fp, &pJob->base_fields)) == NO_ERROR
                    && (fscanf(fp, "Expected Time = %u\n", &pJob->expected_time) != 1
                            || fscanf(fp, "Actual Time = %u\n\n", &pJob->actual_time) != 1))
            {
                status = BAD_FILE_FORMAT;
            }

            if (status != NO_ERROR)
            {
                free(pJob);
                pJob = NULL;
            }
        }
    }

    if (pError != NULL)
    {
        *pError = status;
    }
    return pJob;
}

static struct flops_job *read_flop_job(FILE *fp, enum error_code *pError)
{
    struct flops_job *pJob;
    enum error_code status = NO_ERROR;

    if (fp == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        pJob = (struct flops_job *) malloc(sizeof(struct flops_job));
        if (pJob == NULL)
        {
            status = MEMORY_ERROR;
        }
        else
        {
            pJob->base_fields.type = FLOPS_JOB;

            if ((status = read_base_job(fp, &pJob->base_fields)) == NO_ERROR
                    && fscanf(fp, "FLOPs = %lf\n\n", &pJob->flops) != 1)
            {
                status = BAD_FILE_FORMAT;
            }

            if (status != NO_ERROR)
            {
                free(pJob);
                pJob = NULL;
            }
        }
    }

    if (pError != NULL)
    {
        *pError = status;
    }

    return pJob;
}

static enum error_code add_job(struct job_list *pList, struct base_job *pJob)
{
    struct job_node *pNode;
    enum error_code status = NO_ERROR;

    if (pList == NULL || pJob == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        pNode = (struct job_node *) malloc(sizeof(struct job_node));
        if (pNode == NULL)
        {
            status = MEMORY_ERROR;
        }
        else
        {
            pNode->job = pJob;
            pNode->next = NULL;
            pNode->prev = pList->end;
            if (pList->start == NULL)
            {

                pList->start = pNode;
                pList->end = pNode;
                pList->current_job = pNode;
                pList->num_jobs = 1;
            }
            else
            {
                pList->end->next = pNode;
                pList->end = pNode;
                pList->num_jobs++;
            }
        }
    }
    return status;
}

enum error_code init_job_list(struct job_list *pList)
{
    enum error_code status = NO_ERROR;
    if (pList == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        pList->num_jobs = 0;
        pList->start = NULL;
        pList->end = NULL;
        pList->current_job = NULL;
    }
    return status;
}

enum error_code parse_jobs(char *loc, struct job_list *pList)
{
    char type;
    struct base_job *pJob;
    enum error_code status = NO_ERROR;

    if (loc == NULL || pList == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        FILE *fp = fopen(loc, "r");
        if (fp == NULL)
        {
            status = FILE_NOT_FOUND;
        }
        else
        {
            while (status == NO_ERROR && fscanf(fp, "%c\n", &type) == 1)
            {
                pJob = NULL;

                switch (type)
                {
                    case 'f':
                    case 'F':
                        pJob = (struct base_job *) read_flop_job(fp, &status);
                        break;

                    case 't':
                    case 'T':
                        pJob = (struct base_job *) read_time_job(fp, &status);
                        break;
                    default:
                        status = BAD_FILE_FORMAT;
                        break;
                }

                if (pJob != NULL && status == NO_ERROR)
                {
                    status = add_job(pList, pJob);
                }
            }
            fclose(fp);
        }
    }
    return status;
}

struct base_job *get_next_job_list(struct job_list *pList)
{
    struct base_job *pNext = NULL;
    if (pList != NULL && pList->start != NULL && pList->current_job != NULL)
    {
        pNext = pList->current_job->job;
        pList->current_job = pList->current_job->next;
    }
    return pNext;
}

enum error_code destroy_job_list(struct job_list *pList)
{
    enum error_code status = NO_ERROR;
    struct job_node *pDestroy;
    if (pList == NULL)
    {
        status = BAD_ARGUMENT;
    }
    else
    {
        while (pList->start != NULL)
        {
            pDestroy = pList->start;
            pList->start = pList->start->next;

            free(pDestroy->job);
            free(pDestroy);
        }

        pList->num_jobs = 0;
        pList->start = NULL;
        pList->current_job = NULL;
    }
    return status;
}
