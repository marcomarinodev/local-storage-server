/**
 * This file contains the implementation of a custom set of
 * pthread main functions with errors checking.
*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "pthread_custom.h"

void Pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*function)(), void *arg)
{
    if ((pthread_create(t, attr, function, (void *)arg)) != 0)
    {
        perror("Pthread_create: ");
        exit(EXIT_FAILURE);
    }
}

void Pthread_mutex_lock(pthread_mutex_t *mutex)
{
    int err;
    if ((err = pthread_mutex_lock(mutex)) != 0)
    {
        perror("Pthread_mutex_lock:");
        exit(EXIT_FAILURE);
    }
}

void Pthread_mutex_unlock(pthread_mutex_t *mutex)
{

    int err;

    if ((err = pthread_mutex_unlock(mutex)) != 0)
    {

        perror("Pthread_mutex_unlock:");

        exit(EXIT_FAILURE);
    }
}

void Pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{

    int err;

    if ((err = pthread_cond_wait(cond, mutex)) != 0)
    {

        perror("Pthread_cond_wait:");

        exit(EXIT_FAILURE);
    }
}

void Pthread_cond_signal(pthread_cond_t *cond)
{

    int err;

    if ((err = pthread_cond_signal(cond)) != 0)
    {

        perror("Pthread_cond_signal:");

        exit(EXIT_FAILURE);
    }
}

void Pthread_cond_broadcast(pthread_cond_t *cond)
{

    int err;

    if ((err = pthread_cond_broadcast(cond)) != 0)
    {

        perror("Pthread_cond_broadcast:");

        exit(EXIT_FAILURE);
    }
}

void Pthread_detach(pthread_t t)
{
    int err;

    if ((err = pthread_detach(t)) != 0)
    {

        perror("Pthread_detach:");

        exit(EXIT_FAILURE);
    }
}

void Pthread_join(pthread_t t, void *status)
{

    int err;

    if ((err = pthread_join(t, status)) != 0)
    {

        perror("Pthread_join:");

        exit(EXIT_FAILURE);
    }
}