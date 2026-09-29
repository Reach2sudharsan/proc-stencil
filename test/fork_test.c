#include "kthread.h"
#include "proc.h"
#include "sched.h"

#include <stdio.h>
#include <assert.h>

const int NUM_INITS = 6;
int flag = 0;

typedef void (*init_func_t)();
init_func_t init_funcs[] = {
    mem_init,
    slab_init,
    proc_init,
    kthread_init,
    sched_init,
    proc_idleproc_init
};

static context_t bootstrap_ctx;

static void *childproc_run(long arg1, void *arg2) {
    return NULL;
}

static void *initproc_run(long arg1, void *arg2) {
    //Run this on the first call to initproc_run
    if(flag == 0){
        // create child process
        proc_t* child = proc_create("child");
        if (child == NULL) {
            return NULL;
        }
        // create thread for child preocess
        kthread_t *child_thread = kthread_create(child, childproc_run, 0, NULL);
        if (child_thread == NULL) {
            return NULL;
        }

        // set the thread's state + insert thread to run queue
        curthr->kt_state = KT_ON_CPU;
        spinlock_lock(&kt_runq.tq_lock);
        list_insert_back(&kt_runq.tq_list, &child_thread->kt_qlink);
        spinlock_unlock(&kt_runq.tq_lock);

        //Set flag to 1
        flag = 1;
        // context switch
        sched_switch(curthr->kt_ctx);
    }
    else{
        //resume here after child exits
        flag = 0;
        proc_t *child = curproc->p_children.head->parent;
        long status = child->p_status; // child process should be PROC_DEAD
        proc_destroy(child); // should no longer be running
        return (void *) status;
    }
}

void *start_initproc(long arg1, void *arg2) {
    proc_initproc = proc_create("init");
    kthread_t *init_thread = kthread_create(proc_initproc, initproc_run, 0, NULL);

    // don't worry about using the scheduling system...
    curproc = proc_initproc;
    curthr = init_thread;

    context_make_active(&init_thread->kt_ctx);

    return NULL;
}

int main(int argc, char **argv) {
    // initialize subsystems
    for (int i = 0; i < NUM_INITS; i++) {
        init_funcs[i]();
    }

    void *bootstrap_stack = page_alloc_n(1);
    if (bootstrap_stack == NULL) {
        return -1;
    }

    context_setup(&bootstrap_ctx, start_initproc, 0, NULL, bootstrap_stack, PAGE_SIZE, NULL);
    context_switch(&bios_ctx, &bootstrap_ctx); // saves this as the place where bios ctx will restore

    // TODO: what do you expect when you get here? Add test cases here!
    assert(curproc == proc_initproc);
    proc_destroy(curproc);
    
    printf("ALL is fine so far...\n");

    return 0;
}
