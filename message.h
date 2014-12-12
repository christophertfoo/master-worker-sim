/*
 * message.h
 *
 *  Created on: Nov 30, 2014
 *      Author: chris
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include "job.h"

/* Messages */
enum message_type
{
    NULL_TYPE, MASTER_JOBS, MASTER_DONE, MASTER_ERROR, WORKER_REQUEST, WORKER_RESULT, WORKER_ERROR, WORKER_DONE
};

struct message
{
    enum message_type type;
    unsigned char message[];
};

/* Job Message */
struct jobs_message
{
    unsigned int num_jobs;
    unsigned char jobs[];
};

struct base_job_message_fields
{
    enum job_type type;
    unsigned int id;
    unsigned long data_size;
    unsigned long result_size;
};

struct base_job_message
{
    struct base_job_message_fields base_fields;
};

struct flops_job_message
{
    struct base_job_message_fields base_fields;
    double flops;
    unsigned char data[];
};

struct time_job_message
{
    struct base_job_message_fields base_fields;
    unsigned int actual_time;
    unsigned char data[];
};

/* Job Results Message */
struct results_message
{
    unsigned int num_results;
    unsigned char results[];
};

struct result_message
{
    unsigned int id;
    unsigned int result_size;
    unsigned char result[];
};

/* Macros */
#define MESSAGE_TYPE(msg)            (((struct message *)(msg))->type)
#define MESSAGE(msg)                 (((struct message *)(msg))->message)

#define JOBS_MESSAGE(msg)            ((struct jobs_message *) MESSAGE(msg))
#define RESULTS_MESSAGE(msg)         ((struct results_message *) MESSAGE(msg))

/* Prototypes */
unsigned char *get_next_result(unsigned char *current);
enum error_code add_result(unsigned char *message, unsigned char **pEnd, struct base_job_message *pJob);

unsigned char *get_next_job(unsigned char *current);
enum error_code add_job(unsigned char *message, unsigned char **pEnd, struct base_job *job);

#endif /* MESSAGE_H_ */
