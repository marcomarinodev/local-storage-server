/**
 * This file contains the interface of a custom set of
 * pthread main functions. Basically, it's an implementation
 * with errors checking.
*/

#include <pthread.h>

/**
 * pthread_create with error checking
*/
void Pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*function)(), void *arg);

/**
 * pthread_mutex_lock with error checking
*/
void Pthread_mutex_lock(pthread_mutex_t *mutex);

/**
 * pthread_mutex_unlock with error checking
*/
void Pthread_mutex_unlock(pthread_mutex_t *mutex);

/**
 * pthread_cond_wait with error checking
*/
void Pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

/**
 * pthread_cond_signal with error checking
*/
void Pthread_cond_signal(pthread_cond_t *cond);

/**
 * pthread_cond_broadcast with error checking
*/
void Pthread_cond_broadcast(pthread_cond_t *cond);

/**
 * pthread_detach with error checking
*/
void Pthread_detach(pthread_t t);

/**
 * pthread_join with error checking
*/
void Pthread_join(pthread_t t, void *status);