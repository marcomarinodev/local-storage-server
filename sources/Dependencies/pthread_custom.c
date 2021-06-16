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

void safe_pcreate(pthread_t *t, const pthread_attr_t *attr, void *(*function)(), void *arg)
{
    if ((pthread_create(t, attr, function, (void *)arg)) != 0)
    {
        perror("safe_pcreate: ");
        exit(EXIT_FAILURE);
    }
}

void safe_plock(pthread_mutex_t *mutex)
{
    int err;
    if ((err = pthread_mutex_lock(mutex)) != 0)
    {
        perror("safe_plock:");
        exit(EXIT_FAILURE);
    }
}

void safe_punlock(pthread_mutex_t *mutex)
{

    int err;

    if ((err = pthread_mutex_unlock(mutex)) != 0)
    {
        perror("safe_punlock:");
        exit(EXIT_FAILURE);
    }
}

void safe_cwait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{

    int err;

    if ((err = pthread_cond_wait(cond, mutex)) != 0)
    {
        perror("safe_cwait:");
        exit(EXIT_FAILURE);
    }
}

void safe_csignal(pthread_cond_t *cond)
{

    int err;

    if ((err = pthread_cond_signal(cond)) != 0)
    {
        perror("safe_csignal:");
        exit(EXIT_FAILURE);
    }
}

void safe_cbroadcast(pthread_cond_t *cond)
{

    int err;

    if ((err = pthread_cond_broadcast(cond)) != 0)
    {
        perror("safe_cbroadcast:");
        exit(EXIT_FAILURE);
    }
}

void safe_pdetach(pthread_t t)
{
    int err;

    if ((err = pthread_detach(t)) != 0)
    {
        perror("safe_pdetach:");
        exit(EXIT_FAILURE);
    }
}

void safe_pjoin(pthread_t t, void *status)
{

    int err;

    if ((err = pthread_join(t, status)) != 0)
    {
        perror("safe_pjoin:");
        exit(EXIT_FAILURE);
    }
}

void safe_pattr_destroy(pthread_attr_t *attr)
{
    int err = 1;
    if ((err = pthread_attr_destroy(attr)) != 0)
    {
        perror("safe_pattr_destroy:");
        exit(EXIT_FAILURE);
    }
}

void safe_pattr_init(pthread_attr_t *attr)
{
    int err = 1;
    if ((err = pthread_attr_init(attr)) != 0)
    {
        perror("safe_pattr_init:");
        exit(EXIT_FAILURE);
    }
}

void safe_pmutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *restrict attr)
{
    int err = 1;

    if ((err = pthread_mutex_init(mutex, attr)) != 0)
    {
        perror("safe_pmutex_init:");
        exit(EXIT_FAILURE);
    }
}