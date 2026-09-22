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
    slab_allocator_init(&proc_allocator, sizeof(proc_t))
    curproc = NULL;
    curproc->p_state = PROC_PENDING;

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
    idleproc->p_pproc == NULL;
    list_init(&idleproc->p_threads);
    list_init(&idleproc->p_children);
    list_link_init(&idleproc->p_threads_lock);
    list_link_init(&idleproc->p_children_lock);
    spinlock_init(&proc_list_lock);
    spinlock_init(&proc_list_lock);
    idleproc->p_state = PROC_RUNNING;
    // COME BACK LATER, SEE IF THERE IS ANYTHING LEFT TO INITIALIZE
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
    
    // making space for new process
    proc_t * new_process_pointer = slab_obj_alloc(proc_allocator);

    // new pid = pid of current process + 1, NEED TO SEE IF WE NEED TO UPDATE MORE GLOBALS
    spinlock_lock(&proc_list_lock);
    next_pid = curproc->pid+1;

    // adds new process to proc list
    list_link_init(&new_process.p_list_link);
    list_insert(&proc_list, new_process.p_list_link);

    spinlock_unlock(&proc_list_lock);


    // create a new process
    proc_t new_process;
    *new_process_pointer = new_process;

    // add this new process as child to parent curproc
    spinlock_lock(&p_children_lock);

    list_link_init(&curproc->p_child_link);
    list_insert(&curproc->p_children, curproc->p_child_link);

    list_link_init(&curproc->p_child_link.next);
    curproc->p_child_link.next = new_process_pointer;
    list_insert(&curproc->p_children, curproc->p_child_link);
    spinlock_unlock(&p_children_lock);

    // process state
    new_process->p_state = PROC_PENDING;

    return new_process;
}

/*
 * TODO: implement me!
 * Hints: anything that was allocated needs to be deallocated... deallocated
 * objects should not be accessible by anyone else!
 */
void proc_destroy(proc_t *proc) {

    // other attributes

    // Right now, figuring out what to deallocate, how to ensure that child processes are not still
    // in the heap


    // remove elements from the proc_list by unlinking them individually
    for (list_link_t *link = list_remove_front (& proc_list);
        link != NULL; link = list_remove_front (& proc_list)) {
        // if we want to access the parent
        proc_t *parent = (proc_t *) link ->p_pproc;


    }



    // NEED TO FIGURE HOW TO DEALLOCATE SPACE IN HEAP FOR THE DESTROYED PROC
}

/*
 * TODO: implement me!
 * Hints: anything that was allocated needs to be deallocated... deallocated
 * objects should not be accessible by anyone else!
 */
void proc_cleanup() {

}

/*
 * TODO: implement me!
 * Hints: how should a process behave if all threads exit?
 */
void proc_thread_exiting(void *retval) {

}

/*
 * TODO: implement me!
 * Hints:
 *   - cancel all threads associated with the provided process
 *   - protect access to the threads list
 */
void proc_kill(proc_t *proc, long status) {

}

/*
 * TODO: implement me!
 * Hints:
 *  - protect access to the process list
 *  - kill the current process at the very end... don't kill before function
 * finishes!
 */
void proc_kill_all() {

}
