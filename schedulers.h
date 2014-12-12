/*
 * schedulers.h
 *
 *  Created on: Dec 9, 2014
 *      Author: chris
 */

#ifndef SCHEDULERS_H_
#define SCHEDULERS_H_

#include "shared.h"
#include "job.h"
#include "message.h"

struct one_job_args
{
    struct job_list *pList;
    unsigned char *pMessage;
    unsigned long message_size;
};

struct static_job_args
{
    struct job_list *pList;
    unsigned char *pMessage;
    int num_workers;
    unsigned long message_size;
};

struct dynamic_job_count
{
    unsigned int count;
    struct dynamic_job_count *next;
};

struct dynamic_job_args
{
    struct job_list *pList;
    unsigned char *pMessage;
    struct dynamic_job_count *counts;
    unsigned long message_size;
};

enum error_code init_one_job_args(struct one_job_args *args, struct job_list *pList,
        unsigned char *pMessage);
enum error_code one_job_scheduler(struct one_job_args *args);

enum error_code init_static_job_args(struct static_job_args *args, struct job_list *pList,
        unsigned char *pMessage, int num_procs);
enum error_code static_job_scheduler(struct static_job_args *args);

enum error_code init_dynamic_job_args(struct dynamic_job_args *args, struct job_list *pList,
        unsigned char *pMessage, int num_procs);
enum error_code dynamic_job_scheduler(struct dynamic_job_args *args);

#endif /* SCHEDULERS_H_ */
