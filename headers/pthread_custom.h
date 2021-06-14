/**
 * This file contains the interface of a custom set of
 * pthread main functions. Basically, it's an implementation
 * with errors checking.
*/

#include <pthread.h>

/**
 * pthread_create with error checking
*/
void safe_pcreate(pthread_t *t, const pthread_attr_t *attr, void *(*function)(), void *arg);

/**
 * pthread_mutex_lock with error checking
*/
void safe_plock(pthread_mutex_t *mutex);

/**
 * pthread_mutex_unlock with error checking
*/
void safe_punlock(pthread_mutex_t *mutex);

/**
 * pthread_cond_wait with error checking
*/
void safe_cwait(pthread_cond_t *cond, pthread_mutex_t *mutex);

/**
 * pthread_cond_signal with error checking
*/
void safe_csignal(pthread_cond_t *cond);

/**
 * pthread_cond_broadcast with error checking
*/
void safe_cbroadcast(pthread_cond_t *cond);

/**
 * pthread_detach with error checking
*/
void safe_pdetach(pthread_t t);

/**
 * pthread_join with error checking
*/
void safe_pjoin(pthread_t t, void *status);

/**
 * pthread_attr_destroy with error checking
*/
void safe_pattr_destroy(pthread_attr_t attr);

/**
 * pthread_attr_init with error checking
*/
void safe_pattr_init(pthread_attr_t attr);

/**
 * pthread_mutex_init with error checking
*/
void safe_pmutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *restrict attr);