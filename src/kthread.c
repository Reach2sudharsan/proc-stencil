#include "kthread.h"

kthread_t *curthr;

/*
 * TODO: implement me!
 * Hints: we don't have any threads running yet... but what from the thread
 * subsystem needs ot be initialized?
 */
void kthread_init() {
    //Initalize kthread_allocator
    slab_allocator_init(&kthread_allocator, sizeof(kthread_t));
    curthr = NULL;
}


/*
 * TODO: implement me!
 * Hints:
 *   - make space for the new thread using the kthread allocator
 *   - set default values for thread fields
 *   - you will need to allocate a kernel stack
 *   - you will need to set up the thread's context
 *     --> for now, the page table for the process is NULL
 *   - remember to add the thread to the proc's p_thread list
 *   - initialize the kt_recent_core to ~0UL (unsigned -1)
 *   - return NULL if allocation not possible
 */
kthread_t *kthread_create(proc_t *proc, kthread_func_t func, long arg1,
                          void *arg2) {
    //Allocate new thread and return NULL if unsucessful
    kthread_t* new_thread = (kthread_t*) slab_obj_alloc(kthread_allocator);
    if(new_thread == NULL){
        return NULL;
    }

    //Allocate the stack
    new_thread->kt_kstack = (char*) page_alloc_n(DEFAULT_STACK_SIZE_PAGES);

    //Prepare the thread's context
    context_setup(&(new_thread->kt_ctx), func, arg1, arg2, &(new_thread->kt_kstack), DEFAULT_STACK_SIZE_PAGES * PAGE_SIZE , NULL);

    //Set its parent processor, state
    new_thread->kt_proc = proc;
    new_thread->kt_state = KT_RUNNABLE;
    (new_thread->kt_cancelled) = 0; 

    //Add the thread to the proc's p_thread list
    list_link_init(&(new_thread->kt_plink), &new_thread);
    list_insert(&(proc->p_threads), &(new_thread->kt_plink));

    //Prepare its run_queue link
    list_link_init(&(new_thread->kt_qlink), &new_thread);

    spinlock_init(&new_thread->kt_lock);

    new_thread->kt_retval = 0;
    new_thread->kt_errno = 0;

    return new_thread;
}

/*
 * TODO: implement me!
 * Hints:
 *   - the only parts of the context that must be initialized are c_kstack and
 *     c_kstacksz
 *   - the thread's process should be set outside of this function
 *   - copy over the retval, errno, and cancelled... other fields should be
 *     freshly initialized
 *   - remember to protect access to the thread via its spinlock
 *   - see kthread_create for more hints!
 */
kthread_t *kthread_clone(kthread_t *thread) {

    //Allocate clone thread and return NULL if unsucessful
    kthread_t* clone_thread = (kthread_t*) slab_obj_alloc(kthread_allocator);
    if(clone_thread == NULL){
        return NULL;
    }
    //Copy original thread's context into clone thread
    //But change the stack to the clone's stack
    clone_thread->kt_kstack = (char*) page_alloc_n(DEFAULT_STACK_SIZE_PAGES);
    clone_thread->kt_ctx = thread->kt_ctx;
    (clone_thread->kt_ctx).c_kstack = clone_thread->kt_kstack;
    (clone_thread->kt_ctx).c_kstacksz = DEFAULT_STACK_SIZE_PAGES * PAGE_SIZE;

    //Copy other fields from thread
    (clone_thread->kt_proc) = thread->kt_proc;

    (clone_thread->kt_state) = KT_RUNNABLE; 

    (clone_thread->kt_cancelled) = 0; 

    //Add the thread to the proc's p_thread list
    list_link_init(&(clone_thread->kt_plink), &clone_thread);
    list_insert(&((clone_thread->kt_proc)->p_threads), &(clone_thread->kt_plink));

    list_link_init(&(clone_thread->kt_qlink), &clone_thread);

    spinlock_init(&clone_thread->kt_lock);

    clone_thread->kt_retval = thread->kt_retval;
    clone_thread->kt_errno = thread->kt_errno;


    return NULL;
}

/*
 * TODO: implement me!
 * Hints:
 *   - deallocate thread memory
 *   - remove thread from process' thread list
 *   - protect all accesses to shared data
 *   - don't forget to free thread's stack!
 */
void kthread_destroy(kthread_t *thread) {
    //No need to have concurrency safety
    //remove thread from process' thread list
    if(thread == NULL){
        return;
    }
    list_remove_link(&((thread->kt_proc)->p_threads), &(thread->kt_plink)); 

    //free stack
    slab_obj_free(kthread_allocator, &(thread->kt_kstack));

    //deallocate thread memory
    slab_obj_free(kthread_allocator, &thread);
}

/*
 * TODO: implement me!
 * Hints:
 *   - cannot "cancel" the current thread, so call exit
 *   - mark the thread as cancelled and stop executing
 *   - remember to the protect access to the thread
 */
void kthread_cancel(kthread_t *thread, void *retval) {
    //same as exit but for some thread other than current thread
    if(thread == NULL){
        return;
    }
    
    //Set retval, cancelled and state
    thread->kt_retval = retval;
    thread->kt_cancelled = 1;
    thread->kt_state = KT_NO_STATE;

    //Notify parent process
    proc_thread_exiting(retval);
}

/*
 * TODO: implement me!
 * Hints: there's (some but) not much to do here... remember, it's up to the
 * parent process to manage its threads!
 */
void kthread_exit(void *retval) {
    //set ret,change thread state, call thread_finish(min remove thread from list)
    if(curthr == NULL){
        return;
    }

    //Set retval, cancelled and state
    curthr->kt_retval = retval;
    curthr->kt_cancelled = 1;
    curthr->kt_state = KT_EXITED;

    //Notify parent process
    proc_thread_exiting(retval);
}
