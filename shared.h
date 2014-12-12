/*
 * shared.h
 *
 *  Created on: Nov 24, 2014
 *      Author: chris
 */

#ifndef SHARED_H_
#define SHARED_H_

#define true  1
#define false 0

enum error_code
{
    NO_ERROR = 0,
    BAD_ARGUMENT,
    MEMORY_ERROR,
    INVALID_TYPE,
    FILE_NOT_FOUND,
    BAD_FILE_FORMAT,
    MPI_ERROR,
    BAD_MESSAGE_FORMAT,
    OUT_OF_JOBS
};

#endif /* SHARED_H_ */
