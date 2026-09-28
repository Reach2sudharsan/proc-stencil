#include "proc.h"

proc_t *curproc;

/*
 * TODO: implement me!
 * Hints: we don't have any processes running yet... but what from the process
 * subsystem needs to be initialized?
 */
void proc_init() {
    
    list_init(&proc_list);
    spinlock_init(&proc_list_lock);

    next_pid = 1;
    proc_initproc = NULL;
    slab_allocator_init(&proc_allocator, sizeof(proc_t));

    curproc = NULL;

}

/*
 * TODO: implement me!
 * The idle process is a special process that is created by kmain
 * its job is to be the first process on the system, but it does not have any
 * associated threads
 * Hints:
 *   - what would the fields of the process struct be set to for idleproc?
 *   - what is the initial value of curproc? curthr? 
 */
void proc_idleproc_init() {
    idleproc.p_pid = 0;
    idleproc.p_pproc = NULL;
    idleproc.p_status = 0;
    idleproc.p_state = PROC_RUNNING;

    // trying to set pname char "idle"
    const char *idle_name = "idle";
    int i = 0;
    while (idle_name[i] != '\0' && i < MAX_STRING_LEN - 1) {
        idleproc.p_name[i] = idle_name[i];
        i++;
    }
    idleproc.p_name[i] = '\0';

    // initialize threads and children lists
    list_init(&idleproc.p_threads);
    list_init(&idleproc.p_children);

    // initialize the threads and children spinlocks
    spinlock_init(&idleproc.p_threads_lock);
    spinlock_init(&idleproc.p_children_lock);
}

/*
 * This function is implemented to tell the system to shut down and exit
 */
void initproc_finish() {
    context_switch(&curthr->kt_ctx, &bios_ctx);
}

/*
 * TODO: implement me!
 * Hints:
 *   - make space for the new process using the process allocator
 *   - we need to update the global structures
 *   - the process becomes a child of the current process
 *   - don't forget to synchronize on shared structures!
 */
proc_t *proc_create(const char *name) {
    proc_t *new_proc = slab_obj_alloc(proc_allocator);
    
    new_proc->p_pid = next_pid++;
    new_proc->p_pproc = curproc ? curproc : &idleproc; // if there is no curproc, then set parent to be idleproc
    new_proc->p_status = 0;
    new_proc->p_state = PROC_PENDING; // yet to run new process

    // set p_name to name
    int i = 0;
    while (name != NULL && name[i] != '\0' && i < MAX_STRING_LEN - 1) {
        new_proc->p_name[i] = name[i];
        i++;
    }
    new_proc->p_name[i] = '\0';

    // initialize lists and spinlocks
    list_init(&new_proc->p_threads);
    list_init(&new_proc->p_children);
    spinlock_init(&new_proc->p_threads_lock);
    spinlock_init(&new_proc->p_children_lock);

    // attaches links to process object, so we are able to trace back to this new process
    list_link_init(&new_proc->p_list_link, new_proc);
    list_link_init(&new_proc->p_child_link, new_proc);

    // sync + add process to global process list
    spinlock_lock(&proc_list_lock);
    new_proc->p_pid = next_pid++;
    list_insert(&proc_list, &new_proc->p_list_link);
    spinlock_unlock(&proc_list_lock);

    spinlock_lock(&new_proc->p_pproc->p_children_lock);
    list_insert(&new_proc->p_pproc->p_children, &new_proc->p_child_link);
    spinlock_unlock(&new_proc->p_pproc->p_children_lock);

    return new_proc;
}

/*
 * TODO: implement me!
 * Hints: anything that was allocated needs to be deallocated... deallocated
 * objects should not be accessible by anyone else!
 */
void proc_destroy(proc_t *proc) {

    // remove the proc's list and child links from their respective lists
    spinlock_lock(&proc_list_lock);
    list_remove_link(&proc_list, &proc->p_list_link);
    spinlock_unlock(&proc_list_lock);

    if (proc->p_pproc) {
        spinlock_lock(&proc->p_pproc->p_children_lock);
        list_remove_link(&proc->p_pproc->p_children, &proc->p_child_link);
        spinlock_unlock(&proc->p_pproc->p_children_lock);
    }

    // destroy the proc's threads
    for (list_link_t *link = list_remove_front(&proc->p_threads); link != NULL; link = list_remove_front(&proc->p_threads)) {
        kthread_t *thr = (kthread_t *) link->parent;
        kthread_destroy(thr);
    }

    // free the process object
    slab_obj_free(proc_allocator, proc);

}

/*
 * TODO: implement me!
 * Hints: anything that was allocated needs to be deallocated... deallocated
 * objects should not be accessible by anyone else!
 */
void proc_cleanup() {

    // process is finished, but do not free it
    // the parent still needs to see that it exited
    curproc->p_state = PROC_DEAD;

    // give the children to init so they still have a parent
    spinlock_lock(&curproc->p_children_lock);
    for (list_link_t *link = list_remove_front(&curproc->p_children); link != NULL; link = list_remove_front(&curproc->p_children)) {
        proc_t *child = (proc_t *) link->parent;
        // child->p_pproc = curproc;

        spinlock_lock(&proc_initproc->p_children_lock);
        list_insert(&proc_initproc->p_children, &child->p_child_link);
        spinlock_unlock(&proc_initproc->p_children_lock);
    }
    spinlock_unlock(&curproc->p_children_lock);

    // init finishing means the whole system is done
    if (curproc == proc_initproc) {
        initproc_finish();
    }

}

/*
 * TODO: implement me!
 * Hints: how should a process behave if all threads exit?
 */
void proc_thread_exiting(void *retval) {

    // Assuming curthr exists but its resources just needs to be deallocated
    spinlock_lock(&curproc->p_threads_lock);
    list_remove_link(&curproc->p_threads, &curthr->kt_plink);
    spinlock_unlock(&curproc->p_threads_lock);

    // if there are no more threads for the process, terminate the process, else go to the next thread
    if (curproc->p_threads.size == 0) {
        curproc->p_status = (long) retval;
        proc_cleanup();
    } else {
        sched_switch();
    }
}

/*
 * TODO: implement me!
 * Hints:
 *   - cancel all threads associated with the provided process
 *   - protect access to the threads list
 */
void proc_kill(proc_t *proc, long status) {

    // save the status the process is being killed with
    proc->p_status = status;

    // cancel every thread in the process except the current one
    spinlock_lock(&curproc->p_threads_lock);
    for (list_link_t *link = list_remove_front(&proc->p_threads); link != NULL; link = list_remove_front(&proc->p_threads)) {
        kthread_t *thr = (kthread_t *) link->parent;

        // cant cancel ourselves, we exit at the end instead
        if (thr != curthr) {
            kthread_cancel(thr, (void *) status);
        }
    }
    spinlock_unlock(&curproc->p_threads_lock);

    // if we are killing our own process, the current thread has to exit too
    if (proc == curproc) {
        kthread_exit((void *) status);
    } else {
        proc->p_state = PROC_DEAD;
    }

}

/*
 * TODO: implement me!
 * Hints:
 *  - protect access to the process list
 *  - kill the current process at the very end... don't kill before function
 * finishes!
 */
void proc_kill_all() {
    spinlock_lock(&proc_list_lock);

    // Remove processes that aren't the idle proc or any of its children, while still maintaining the links to proc_list
    // (without removing links from the proc_list)
    int is_curproc_or_idle_or_its_child = 0;
    for (list_link_t* link = proc_list.head; link != NULL; link = link->next) {
        proc_t *proc = (proc_t *) link->parent;

        if (proc == &idleproc || proc == curproc) {
            is_curproc_or_idle_or_its_child = 1;
        } else {

            for (list_link_t* idle_link = idleproc.p_children.head; idle_link != NULL; idle_link = idle_link->next) {
                proc_t *idle_proc_child = (proc_t *) idle_link->parent;
                if (link == idle_link) {
                    is_curproc_or_idle_or_its_child = 1;
                    break;
                }

            }
        }

        if (!is_curproc_or_idle_or_its_child) {
            proc_kill(proc, 0);
        }
    }

    proc_kill(curproc, 0);
    spinlock_unlock(&proc_list_lock);
}
