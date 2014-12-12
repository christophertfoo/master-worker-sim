/*
 * job.h
 *
 *  Created on: Nov 17, 2014
 *      Author: chris
 */

#ifndef JOB_H_
#define JOB_H_

#define UNINIT_TIME       -1.0

/* Worker Manager */
enum worker_state
{
    STARTING = 0, RUNNING, WAITING, DONE, FAILED, NUM_WORKER_STATES
};

struct worker_info
{
    unsigned int num_workers;
    unsigned int counts[NUM_WORKER_STATES];
    enum worker_state *states;
};

#define WORKER_ALIVE(worker_info, index)       ((worker_info).states[(index)] == STARTING || (worker_info).states[(index)] == RUNNING || (worker_info).states[(index)] == WAITING)
#define WORKER_DEAD(worker_info, index)        ((worker_info).states[(index)] == DONE || (worker_info).states[(index)] == FAILED)
#define NUM_ALIVE_WORKERS(worker_info)         ((worker_info).counts[STARTING] + (worker_info).counts[RUNNING] + (worker_info).counts[WAITING])
#define NUM_DEAD_WORKERS(worker_info)          ((worker_info).counts[DONE] + (worker_info).counts[FAILED])
/* Jobs */
enum job_type
{
    FLOPS_JOB, TIME_JOB
};

struct base_job_fields
{
    enum job_type type;
    unsigned int id;
    double queued_time;
    double assigned_time;
    double completion_time;
    unsigned int num_nodes;
    unsigned long data_size;
    unsigned long result_size;
};

struct base_job
{
    struct base_job_fields base_fields;
};

struct flops_job
{
    struct base_job_fields base_fields;
    double flops;
};

struct time_job
{
    struct base_job_fields base_fields;
    unsigned int expected_time;
    unsigned int actual_time;
};

/* Job List */
struct job_node
{
    struct base_job *job;
    struct job_node *next;
    struct job_node *prev;
};

struct job_list
{
    unsigned long num_jobs;
    struct job_node *start;
    struct job_node *end;
    struct job_node *current_job;
};

/* Macros */
#define TIME_INIT(time)         ((time) < 0)

/* Prototypes */
enum error_code init_job_list(struct job_list *pList);
enum error_code parse_jobs(char *loc, struct job_list *pList);
struct base_job *get_next_job_list(struct job_list *pList);
enum error_code destroy_job_list(struct job_list *pList);

#endif /* JOB_H_ */
