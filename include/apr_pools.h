/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

#ifndef APR_POOLS_H
#define APR_POOLS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @package APR memory allocation
 */

/*
 * Resource allocation routines...
 *
 * designed so that we don't have to keep track of EVERYTHING so that
 * it can be explicitly freed later (a fundamentally unsound strategy ---
 * particularly in the presence of die()).
 *
 * Instead, we maintain pools, and allocate items (both memory and I/O
 * handlers) from the pools --- currently there are two, one for per
 * transaction info, and one for config info.  When a transaction is over,
 * we can delete everything in the per-transaction apr_pool_t without fear, and
 * without thinking too hard about it either.
 *
 * rst
 */

/* Arenas for configuration info and transaction info
 * --- actual layout of the apr_pool_t structure is private to 
 * alloc.c.  
 */

#include "apr.h"
#include "apr_thread_proc.h"

#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_STDARG_H
#include <stdarg.h>
#endif

enum kill_conditions {
    kill_never,                 /* process is never sent any signals */
    kill_always,                /* process is sent SIGKILL on apr_pool_t cleanup */
    kill_after_timeout,         /* SIGTERM, wait 3 seconds, SIGKILL */
    just_wait,                  /* wait forever for the process to complete */
    kill_only_once              /* send SIGTERM and then wait */
};

/** A list of processes */
struct process_chain {
    /** The process ID */
    apr_proc_t *pid;
    /** When the process should be sent a signal. <PRE>
     *           kill_never   -- process is never sent any signals
     *           kill_always  -- process is sent SIGKILL on apr_pool_t cleanup
     *           kill_after_timeout -- SIGTERM, wait 3 seconds, SIGKILL
     *           just_wait    -- wait forever for the process to complete
     *           kill_only_once -- send SIGTERM and then wait </PRE>
     */
    enum kill_conditions kill_how;
    /** The next process in the list 
     *  @defvar process_chain *next */
    struct process_chain *next;
};

/* pools have nested lifetimes -- sub_pools are destroyed when the
 * parent pool is cleared.  We allow certain liberties with operations
 * on things such as tables (and on other structures in a more general
 * sense) where we allow the caller to insert values into a table which
 * were not allocated from the table's pool.  The table's data will
 * remain valid as long as all the pools from which its values are
 * allocated remain valid.
 *
 * For example, if B is a sub pool of A, and you build a table T in
 * pool B, then it's safe to insert data allocated in A or B into T
 * (because B lives at most as long as A does, and T is destroyed when
 * B is cleared/destroyed).  On the other hand, if S is a table in
 * pool A, it is safe to insert data allocated in A into S, but it
 * is *not safe* to insert data allocated from B into S... because
 * B can be cleared/destroyed before A is (which would leave dangling
 * pointers in T's data structures).
 *
 * In general we say that it is safe to insert data into a table T
 * if the data is allocated in any ancestor of T's pool.  This is the
 * basis on which the POOL_DEBUG code works -- it tests these ancestor
 * relationships for all data inserted into tables.  POOL_DEBUG also
 * provides tools (apr_find_pool, and apr_pool_is_ancestor) for other
 * folks to implement similar restrictions for their own data
 * structures.
 *
 * However, sometimes this ancestor requirement is inconvenient --
 * sometimes we're forced to create a sub pool (such as through
 * apr_sub_req_lookup_uri), and the sub pool is guaranteed to have
 * the same lifetime as the parent pool.  This is a guarantee implemented
 * by the *caller*, not by the pool code.  That is, the caller guarantees
 * they won't destroy the sub pool individually prior to destroying the
 * parent pool.
 *
 * In this case the caller must call apr_pool_join() to indicate this
 * guarantee to the POOL_DEBUG code.  There are a few examples spread
 * through the standard modules.
 */
#ifndef POOL_DEBUG
#ifdef apr_pool_join
#undef apr_pool_join
#endif
#define apr_pool_join(a,b)
#else
APR_EXPORT(void) apr_pool_join(apr_pool_t *p, apr_pool_t *sub);
APR_EXPORT(apr_pool_t *) apr_find_pool(const void *ts);
APR_EXPORT(int) apr_pool_is_ancestor(apr_pool_t *a, apr_pool_t *b);
#endif

#ifdef ULTRIX_BRAIN_DEATH
#define apr_fdopen(d,m) fdopen((d), (char *)(m))
#else
#define apr_fdopen(d,m) fdopen((d), (m))
#endif

/*
 * APR memory structure manipulators (pools, tables, and arrays).
 */

/**
 * Setup all of the internal structures required to use pools
 * @tip Programs do NOT need to call this directly.  APR will call this
 *      automatically from apr_initialize. 
 */
apr_status_t apr_init_alloc(void);	/* Set up everything */

/**
 * Tear down all of the internal structures required to use pools
 * @tip Programs do NOT need to call this directly.  APR will call this
 *      automatically from apr_terminate. 
 */
void apr_term_alloc(void);        /* Tear down everything */

/**
 * make a sub pool from the current pool
 * @param p The pool to use as a parent pool
 * @param apr_abort A function to use if the pool cannot allocate more memory.
 * @return The new sub-pool
 * @tip The apr_abort function provides a way to quit the program if the
 *      machine is out of memory.  By default, APR will return with an
 *      error.
 * @deffunc apr_pool_t *apr_make_sub_pool(apr_pool_t *p, int (*apr_abort)(int retcode))
 */
APR_EXPORT(apr_pool_t *) apr_make_sub_pool(apr_pool_t *p, int (*apr_abort)(int retcode));

/**
 * clear all memory in the pool
 * @param p The pool to clear
 * @tip  This does not actually free the memory, it just allows the pool
 *       to re-use this memory for the next allocation.
 * @deffunc void apr_clear_pool(apr_pool_t *p)
 */
APR_EXPORT(void) apr_clear_pool(apr_pool_t *p);

/**
 * destroy the pool
 * @param p The pool to destroy
 * @tip This will actually free the memory
 * @deffunc void apr_destroy_pool(apr_pool_t *p)
 */
APR_EXPORT(void) apr_destroy_pool(apr_pool_t *p);

/**
 * report the number of bytes currently in the pool
 * @param p The pool to inspect
 * @return The number of bytes
 * @deffunc apr_size_t apr_bytes_in_pool(apr_pool_t *p)
 */
APR_EXPORT(apr_size_t) apr_bytes_in_pool(apr_pool_t *p);

/**
 * report the number of bytes currently in the list of free blocks
 * @return The number of bytes
 * @deffunc apr_size_t apr_bytes_in_free_blocks(void)
 */
APR_EXPORT(apr_size_t) apr_bytes_in_free_blocks(void);

/**
 * Determine if pool a is an ancestor of pool b
 * @param a The pool to search 
 * @param b The pool to search for
 * @return True if a is an ancestor of b, NULL is considered an ancestor
 *         of all pools.
 * @deffunc int apr_pool_is_ancestor(apr_pool_t *a, apr_pool_t *b)
 */
APR_EXPORT(int) apr_pool_is_ancestor(apr_pool_t *a, apr_pool_t *b);

/**
 * Allocate a block of memory from a pool
 * @param c The pool to allocate out of 
 * @param reqsize The amount of memory to allocate 
 * @return The allocated memory
 * @deffunc void *apr_palloc(apr_pool_t *c, apr_size_t reqsize)
 */
APR_EXPORT(void *) apr_palloc(apr_pool_t *c, apr_size_t reqsize);

/**
 * Allocate a block of memory from a pool and set all of the memory to 0
 * @param p The pool to allocate out of 
 * @param size The amount of memory to allocate 
 * @return The allocated memory
 * @deffunc void *apr_pcalloc(apr_pool_t *p, apr_size_t size)
 */
APR_EXPORT(void *) apr_pcalloc(apr_pool_t *p, apr_size_t size);

/**
 * Register a function to be called when a pool is cleared or destroyed
 * @param p The pool register the cleanup with 
 * @param data The data to pass to the cleanup function.
 * @param plain_cleanup The function to call when the pool is cleared 
 *                      or destroyed
 * @param child_cleanup The function to call when a child process is created 
 * @deffunc void apr_register_cleanup(apr_pool_t *p, const void *data, apr_status_t (*plain_cleanup) (void *), apr_status_t (*child_cleanup) (void *))
 */
APR_EXPORT(void) apr_register_cleanup(apr_pool_t *p, const void *data,
                                     apr_status_t (*plain_cleanup) (void *),
                                     apr_status_t (*child_cleanup) (void *));

/**
 * remove a previously registered cleanup function
 * @param p The pool remove the cleanup from 
 * @param data The data to remove from cleanup
 * @param cleanup The function to remove from cleanup
 * @deffunc void apr_kill_cleanup(apr_pool_t *p, const void *data, apr_status_t (*cleanup) (void *))
 */
APR_EXPORT(void) apr_kill_cleanup(apr_pool_t *p, const void *data,
                                 apr_status_t (*cleanup) (void *));

/**
 * Run the specified cleanup function immediately and unregister it
 * @param p The pool remove the cleanup from 
 * @param data The data to remove from cleanup
 * @param cleanup The function to remove from cleanup
 * @deffunc apr_status_t apr_run_cleanup(apr_pool_t *p, void *data, apr_status_t (*cleanup) (void *))
 */
APR_EXPORT(apr_status_t) apr_run_cleanup(apr_pool_t *p, void *data,
                                       apr_status_t (*cleanup) (void *));

/* Preparing for exec() --- close files, etc., but *don't* flush I/O
 * buffers, *don't* wait for subprocesses, and *don't* free any memory.
 */
/**
 * Run all of the child_cleanups, so that any unnecessary files are 
 * closed because we are about to exec a new program
 * @deffunc void apr_cleanup_for_exec(void)
 */
APR_EXPORT(void) apr_cleanup_for_exec(void);

/**
 * An empty cleanup function 
 * @param data The data to cleanup
 * @deffunc apr_status_t apr_null_cleanup(void *data)
 */
APR_EXPORT_NONSTD(apr_status_t) apr_null_cleanup(void *data);


/* used to guarantee to the apr_pool_t debugging code that the sub apr_pool_t will not be
 * destroyed before the parent pool
 */
#ifndef POOL_DEBUG
#ifdef apr_pool_join
#undef apr_pool_join
#endif /* apr_pool_join */
#define apr_pool_join(a,b)
#endif /* POOL_DEBUG */

#ifdef __cplusplus
}
#endif

#endif	/* !APR_POOLS_H */
