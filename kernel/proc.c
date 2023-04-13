#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

//added
int sched_policy = -1; //a global variable used for task 7. the variable stores the current chosen scheduler policy

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

//added
//takes a policy value 0, 1, or 2, for:
//default xv6 policy, priority scheduling, and CFS with priority decay, respectively
void set_policy(int policy){
  sched_policy = policy; //set the global variable to the chosen variable
}

//added
//this function is used for getting a process from a given process id
struct proc* get_proc_from_id(int pid){
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->pid == pid){
      return p;
    }
  }
  return 0;
}

//added
int get_cfs_stats(int pid, int* arr, int print){
  int array[4];
  struct proc *p = get_proc_from_id(pid);
  //acquire(&p->lock);
  array[0] = (p->cfs_priority -75)/25; //return decay to priority
  array[1] = p->rtime;
  array[2] = p->stime;
  array[3] = p->retime;
  if (copyout(myproc()->pagetable, (uint64)arr, (char*)&array, 4*sizeof(int)) < 0) {
    kfree(arr);
    return -1;
  }
  if(print == 1){
    printf("proc id: %d, proc cfs priority: %d, proc cfs rtime: %d, proc cfs stime: %d, proc cfs retime: %d\n",pid , array[0], array[1], array[2], array[3]);
  }
  //release(&p->lock);
  return 0;
}

//added
//a function called by the set_cfs_priority sys call.
//sets a proc's priority to a given value
int set_cfs_priority (int priority){
  int decay = 0;
  if(priority > 2 || priority < 0){ //invalid priority value
    return -1; //failure
  }
  decay = 75 + priority*25; //translate priority to decay
  struct proc *p = myproc(); //get running proc
  acquire(&p->lock);
  p->cfs_priority = decay; 
  release(&p->lock);
  return 0; //success
}

//added
//the cfs update function is called right before a proc's state is changed
//updates the different values according to the previous state of the proc
void update_cfs(struct proc *p){
  enum procstate state = p->state;
  if(state == RUNNABLE){
    p->retime = p->retime + (ticks - p->pr_time); //add the difference between ticks and the last time a change has been made in the status of the proc(during this time gap the proc was in the RUNNABLE state)
  }
  else if(state == RUNNING){
    p->rtime = p->rtime + (ticks - p->pr_time);
  }
  else if(state == SLEEPING){
    p->stime = p->stime + (ticks - p->pr_time);
  }
 p->pr_time = ticks; //update pr_time
}

//added
void set_ps_priority(int change, int cas, int pid){ 
  struct proc *p = get_proc_from_id(pid);
  //acquire(&p->lock); 
  int min_acc = -1;
  int p_acc = -1;
  int locked = 0;
  if(cas == 0){ //cas 0 represents the case in which a process finished running(but remained runnable)
    p->accumulator = p->accumulator + p->ps_priority;
  }
  else if(cas == 1){ //cas 1 represents the case in which a new process is created / a process moves from blocked to runnable
    int pc = 0; //runnable process count
    struct proc *pr;
    for(pr = proc; pr < &proc[NPROC]; pr++){
      if(!holding(&p->lock) && pr->pid != p->pid){ //avoid acquiring p's lock again
        acquire(&pr->lock);
        locked = 1;
      }
      if(pr->state == (RUNNABLE || RUNNING)){ 
        if(pr->accumulator != -1){ //in fork() a new process is given the default value -1. We avoid these process' while searching for the minimum accumulator
          p_acc = pr->accumulator;
        }
        if(pc == 0){ //set min_acc to some valid value
          min_acc = p_acc;
        }
        else if(p_acc < min_acc){ //a process with a smaller accumulator has been found
          min_acc = p_acc;
        }
      }
      pc++;
      if(locked && pr->pid != p->pid){
        release(&pr->lock);
        locked = 0;
      } 
    }
    if(pc > 1){ //indicates that there are additional runnable process' in addition to the process in question
      p->accumulator = min_acc;
      }
    else{ //if the process in question is the only runnable one, set accumulator to 0
      p->accumulator = 0;
    }
  }
  //release(&p->lock);
}

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  //added
  initproc->cfs_priority = 100; //give the init proc the normal priority
  initproc->rtime = 0;
  initproc->stime = 0;
  initproc->retime = 0;
  initproc->pr_time = ticks;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  int parent_priority = p->cfs_priority;
  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  //added
  //set fields for priority_scheduler
  np->ps_priority = 5;
  np->accumulator = -1;
  set_ps_priority(0, 1, pid); //sets process accumulator  
  //set fields for cfs_scheduler
  np->cfs_priority = parent_priority;
  np->rtime = 0;
  np->stime = 0;
  np->retime = 0;
  np->pr_time = ticks;
  release(&np->lock);
  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status, char* exit_msg)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);
  p->xstate = status;
  safestrcpy(p->exit_msg, exit_msg, 32); //added. this function is needed to copy to p->exit_msg since arrays in C aren't assignable
  p->state = ZOMBIE;
  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr, uint64 c_msg)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && (copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0)) {                    
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          if(c_msg != 0 && (copyout(p->pagetable, c_msg, (char *)&pp->exit_msg, sizeof(pp->exit_msg)) < 0)){
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          } //added copies the exit messgae of the son process to c_msg, this is used by the father process
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.

int vruntime_cal(struct proc* p){
  int vruntime = p->cfs_priority * (p->rtime/(p->rtime + p->stime + p->retime));
  return vruntime;
}

//added
//a switch function that runs the requested scheduling policy.
void scheduler(void){ 
  for(;;){
    if(sched_policy == 1){
      priority_scheduler();
    }
    else if(sched_policy == 2){
      cfs_scheduler();
    }
    else{
      default_scheduler();
    }
  } //fix this? this is needed for compiling
}

//default scheduler
void
default_scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    if(sched_policy == 1 || sched_policy == 2){
      break;
    }
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

//assignment 5 scheduler
void priority_scheduler(void)
{
  int min_acc_val = -1;
  struct proc *min_acc_proc = 0;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for (;;) {
    if(sched_policy != 1){
      break;
    }
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for (struct proc *p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state != RUNNABLE){
        release(&p->lock);
        continue;
      }
      if (min_acc_val == -1 || p->accumulator < min_acc_val) {
        if (min_acc_proc != 0) //a min_acc_proc has been found previously
          release(&min_acc_proc->lock);
        min_acc_val = p->accumulator;
        min_acc_proc = p;
      } else {
        release(&p->lock);
      }
    }

    if (min_acc_proc != 0) {
      min_acc_proc->state = RUNNING;
      c->proc = min_acc_proc;
      swtch(&c->context, &min_acc_proc->context);

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      release(&min_acc_proc->lock);
      min_acc_proc = 0;
      min_acc_val = -1;
    }
  }
}


//assignment 6 scheduler
void cfs_scheduler(void)
{
  int min_vruntime = -1;
  int cur_proc_vruntime = 0;
  struct proc *min_vruntime_proc = 0;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for (;;) {
    if(sched_policy != 2){
      break;
    }
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for (struct proc *p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state != RUNNABLE){
        release(&p->lock);
        continue;
      }
      cur_proc_vruntime = vruntime_cal(p);
      if (min_vruntime == -1 || cur_proc_vruntime < min_vruntime) {
        if (min_vruntime_proc != 0) //a min_acc_proc has been found previously
          release(&min_vruntime_proc->lock);
        min_vruntime = cur_proc_vruntime;
        min_vruntime_proc = p;
      } else {
        release(&p->lock);
      }
    }

    if (min_vruntime_proc != 0) {
      min_vruntime_proc->state = RUNNING;
      c->proc = min_vruntime_proc;
      swtch(&c->context, &min_vruntime_proc->context);

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      release(&min_vruntime_proc->lock);
      min_vruntime_proc = 0;
      min_vruntime = -1;
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  update_cfs(p); //cfs_sched
  p->state = RUNNABLE;
  set_ps_priority(p->ps_priority, 0, p->pid); //priority_sched
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  update_cfs(p); //cfs_scheduler
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        if(p->pid != 1){
          update_cfs(p); //cfs_scheduler
        }
        p->state = RUNNABLE;
        set_ps_priority(0,1,p->pid); //priority_scheduler
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}




