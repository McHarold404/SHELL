#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "proc_stat.h"


#define STARVE_TIME 30
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// structure definitions for Multi level feedback queues
struct proc* queue[5][NPROC];  // 5 queues for multi level feedback queue.
  // functions to add processes to queue and to remove processes from queue.
int queue_top[5] = {-1, -1, -1, -1, -1}; // stores last position till where queue is filled 
int queue_ticks_max[5] = {1,2,4, 8, 16}; // max ticks for every queue

void enque_process(int queue_no, struct proc* p)
{
  //int yeet_flag=0;
  for(int i=0; i < queue_top[queue_no]; i++)
  {
    if(queue[queue_no][i]->pid == p->pid) // process already exists in the queue.
    //yeet_flag = -1;
    return ;
  } 
  p->curr_ticks = 0;
  p->wtime=0;
  p-> curr_queue = queue_no ;
  queue_top[queue_no]++;
  queue[queue_no][queue_top[queue_no]] = p; 
}

void deque_process(int queue_no, struct proc* p)
{
    int pos=-1;
    for( int i=0; i<queue_top[queue_no]; i++)
    {
      if(queue[queue_no][i]->pid == p->pid)
      {
        pos=i;
      }
    }

    if(pos == -1) return ;
    for(int i = pos; i<queue_top[queue_no]; i++)
    {
      queue[queue_no][i] = queue[queue_no][i+1]; // shift all processes by 1 peeche
    }
    queue_top[queue_no] --;
  return ;

}

void print_deets()
{
  acquire(&ptable.lock);
  for(struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(!p || p->pid<3 ) continue;
    cprintf("ToRead %d %d %d \n",ticks,p->pid,p->curr_queue);
  }
  release(&ptable.lock);
}






// functions to increase the ticks and shift_queues
void 
increase_ticks(struct proc *p)
{
  acquire(&ptable.lock);
  p->curr_ticks++;
  p->rtime++;
  p->queue_ticks[p->curr_queue]++;
  /*if(p->state == RUNNING)
  {
    cprintf("PID: %d RUNNING\n",p->pid);
  }*/
  release(&ptable.lock);
}

void increase_wait_time()
{
  acquire(&ptable.lock);
  for(struct proc*p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state == RUNNABLE)
    {
      p->wtime++;
    }
    
  }
  release(&ptable.lock);
}

void increase_sleep_time()
{
  acquire(&ptable.lock);
  for(struct proc*p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p && p->state == SLEEPING)
    {
      p->stime++;
    }
    
  }
  release(&ptable.lock);
}

void 
shift_queues(struct proc* p)
{
  acquire(&ptable.lock);
  p-> is_changed = 1;
  release(&ptable.lock);
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc* 
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->rtime=0;
  p->etime=0;
  p->priority=60;
  p->num_run=0;
  p->stime=0;
  p->ctime=ticks;

  release(&ptable.lock); 

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;


#ifdef MLFQ

    p->curr_ticks=0;
    p->curr_queue=0;
  for(int i=0;i<5;i++)
    p->queue_ticks[i]=0;

#endif

#ifndef MLFQ
    p -> curr_queue = -1;
    for(int i=0; i < 5;i++)
      p->queue_ticks[i] = -1;
#endif    
  return p;
}




//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  #ifdef MLFQ
    // cprintf("Adding Proces %d to Queue 0\n", p->pid);
      p->wtime = 0;
      enque_process(0,p);
  #endif
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  #ifdef MLFQ
    // cprintf("ywu");
    // cprintf("Adding Proces %d to Queue 0\n", np->pid);
      np->wtime = 0;
      enque_process(0, np);
  #endif
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");
  curproc->etime=ticks;
  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  curproc->etime = ticks; // put end time of a process here.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        #ifdef MLFQ
          // cprintf("Removing process %d from Queue %d\n", p->pid, p->queue);
          deque_process(p->curr_queue, p);
        #endif 
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

int
waitx(int* wtime, int* rtime)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        *rtime = p->rtime;
        *wtime = p->etime - p->ctime - p->rtime - p->stime;

        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        #ifdef MLFQ
          // cprintf("Removing process %dfrom Queue %d", p->pid, p->queue);
          deque_process(p->curr_queue, p);
        #endif 
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

int pinfo() 
{
    struct proc *p;
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) 
    {
        //cprintf("yeet\n");
        if (!p || p->state == UNUSED || !p->pid)
        {
          continue;
        }
        #ifndef MLFQ
          p -> curr_queue = -1;
          for(int i=0; i < 5;i++)
            p->queue_ticks[i] = -1;
        #endif    
        //cprintf("LMLML\n");    
        cprintf("PID: %d \n",p->pid);
        if(p->state == RUNNING)
          cprintf("STATE: RUNNING\n");
        if(p->state == SLEEPING)
          cprintf("STATE: SLEEPING\n");
        if(p->state == ZOMBIE)
          cprintf("STATE: ZOMBIE\n");
        if(p->state == RUNNABLE)
          cprintf("STATE: RUNNABLE\n");
        if(p->state == EMBRYO)
          cprintf("STATE: EMBRYO\n");
        cprintf("Priority: %d\n",p->priority);
        cprintf("Rtime: %d\n",p->rtime);
        cprintf("Wtime: %d\n",p->wtime);
        cprintf("Num_run: %d\n",p->num_run);
        cprintf("Current Queue: %d\n",p->curr_queue);
      for(int i=0;i<5;i++)
      {
        cprintf("q[%d]: %d \n",i,p->queue_ticks[i]);
      }
      cprintf("\n\n");
      //release(&ptable.lock);
        //return 0;
    }
    
    release(&ptable.lock);
    return -1;
}

int set_priority(int pid, int new_priority)
{
  if(new_priority <-1)
  {
      cprintf("Error:Invalid Priority Number (<0) \n");
      return -1;
  }
  if(new_priority >100)
  {
    cprintf("Error: Priority number Invalid (>100) \n");\
    return -1;
  }
  struct proc *p;
  int flag=0, old_priority;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->pid == pid)
    {
      flag=1;
      acquire(&ptable.lock);
      old_priority = p->priority;
      p->priority = new_priority;

      release(&ptable.lock);
      break;

    }
  }
  if(flag==0)
  {
    cprintf("Error : Invalid Arguments \n");
    return -1;
  }
  if(old_priority > new_priority)
  {
    yield();
  }
  return old_priority;
  
  //accquire(&ptable.lock);
  
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  #ifdef RR   // Round robin scheduling by default
    for(;;)
    {
      // Enable interrupts on this processor.
      sti();
      struct proc* p;
      // Loop over process table looking for process to run.
      acquire(&ptable.lock);
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state != RUNNABLE)
          continue;

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        p->num_run++;
        swtch(&(c->scheduler), p->context);  // context switch
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&ptable.lock);
    }
  #endif

  #ifdef FCFS
  while(1)
  {
    sti(); // enable interrupts
    int min_time=ticks+100000; // minimum time here
    struct proc* fcfs_proc=0;
    struct proc* p = 0;
    acquire(&ptable.lock);

    for(p=ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if(p->state == RUNNABLE)
      {
        if(p->ctime < min_time)
        {
          min_time=p->ctime;
          fcfs_proc=p;
        }
      }
    }
    //  release(&ptable.lock);
    if (fcfs_proc == 0) {
      release(&ptable.lock);
      continue;
    }
      //cprintf("On core: %d, scheduling %d %s with stime %d\n", c->apicid, fcfs_proc->pid,fcfs_proc->name, fcfs_proc->stime);

    c->proc= fcfs_proc;
    fcfs_proc->num_run++;
    switchuvm(fcfs_proc);
    fcfs_proc->state=RUNNING;

    swtch(&(c->scheduler), fcfs_proc->context);
    switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
    c->proc = 0;

    release(&ptable.lock);
    }
  #endif
  #ifdef PBS
    while(1)
    {
      sti();
      struct proc* p;
      struct proc* pbs_proc = 0;
      int min_priority = 1001;
      acquire(&ptable.lock);
      for(p=ptable.proc;p < &ptable.proc[NPROC]; p++)
      {
        if(p->state == RUNNABLE)
        {
            if(p->priority < min_priority)
            {
              min_priority=p->priority;
            }
          
        }
      }
      if(min_priority == 1001){ 
        release(&ptable.lock);
        continue; // no runnable process has been selected
      }
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      {
        if(p->state != RUNNABLE) continue;
        int yeet=0; // flag to check if a process has come which has less priority than the previously selected process, start the scheduling again
        for(struct proc* q= ptable.proc; q < &ptable.proc[NPROC]; q++)
        {
          if(q->state!=RUNNABLE) continue;
          if(q->priority < min_priority)
          {
            yeet=1;
            break;
          }
        }
        if(yeet) break;
        if(p->priority == min_priority)
        {
          pbs_proc = p;
          c->proc = pbs_proc;
          switchuvm(pbs_proc);
          pbs_proc->num_run++;
          pbs_proc->state = RUNNING;
          swtch(&(c->scheduler), pbs_proc->context);
          switchkvm();
          c->proc=0;
        

        }
      }
      release(&ptable.lock);
    }
    #endif
    #ifdef MLFQ

    while(1)
    {
      sti();
      int pos = -1;
      int age;
      struct proc* p;
      struct proc *mlfq_proc =0;
      acquire(&ptable.lock);
      //if(queue_top[1]==9)
      //cprintf("Queue Size %d\n PID: %d \n",queue_top[0],queue[0][0]);
      for(int i=1; i < 5; i++)
      {
        for(int j=0; j <queue_top[i]+1; j++)
        {
          p = queue[i][j];
          age = p->wtime;
          if(age > STARVE_TIME)
          {
            // if process has aged enough, shift it up the priority queues.
            //cprintf("AGED:%d\n",p->curr_queue);
            deque_process(i, p);
            enque_process(i-1, p);
          }

        }
      }
      for(int i=0; i < 5; i++)
      {
        if(queue_top[i]==-1) {
          continue; cprintf("%d\n",i);}
          pos=i;
          mlfq_proc = queue[i][0];
          deque_process(i,mlfq_proc);
          break;
        
      }

      if(pos!=-1) 
      {
        if(mlfq_proc->state == RUNNABLE)
        {
      
          mlfq_proc->num_run++;
          //cprintf("%d \n",pos);
          //mlfq_proc->queue_ticks[mlfq_proc->curr_queue]++;
          c->proc = mlfq_proc;
          switchuvm(mlfq_proc);
          mlfq_proc->state = RUNNING;
          mlfq_proc->wtime = 0;
          swtch(&c->scheduler, mlfq_proc->context);
          switchkvm();
          c->proc = 0;
          if(mlfq_proc->state == RUNNABLE)
          {
            if(mlfq_proc->is_changed == 1)
            {
              // if process is taking too much time, shifting down the queue
              mlfq_proc->is_changed = 0;
             // mlfq_proc->curr_ticks = 0;
              if(mlfq_proc->curr_queue < 4)
                mlfq_proc->curr_queue++;
              //cprintf("PID:%d Curr Queue: %d \n",mlfq_proc->pid,mlfq_proc->curr_queue);
              // cprintf("Moving Process from Queue %d to Queue %d\n", old,p->queue);

            }
            //else mlfq_proc->curr_ticks = 0;
            // cprintf("Adding Process %d to Queue %d\n",p->pid ,p->queue);
            enque_process(mlfq_proc->curr_queue,mlfq_proc);
          }
        }
      }
      release(&ptable.lock);
    }

  #endif
  
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.

  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      #ifdef MLFQ
        p->curr_ticks = 0;
        p->wtime = 0;
        enque_process(p->curr_queue, p);
      #endif 
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
      {
        p->state = RUNNABLE;
        #ifdef MLFQ 
          p->wtime = 0;
          enque_process(p->curr_queue, p);
        #endif
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

