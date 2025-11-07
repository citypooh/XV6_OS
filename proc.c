// proc.c
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct lock_t locks[MAX_LOCKS];

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// Helpers for priority-inheritance recompute
static int  min_waiter_nice_for_owner(struct proc *owner);
static void recalc_effective_nice(struct proc *owner);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");

  // Initialize lock IDs to 1..7 (public IDs), internal index 0..6
  for (int i = 1; i <= MAX_LOCKS; i++) {
    locks[i - 1].id = i;
    locks[i - 1].held = 0;
    locks[i - 1].owner_pid = -1;
  }
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
  p->nice = NICE_DEFAULT;
  p->orig_nice = NICE_DEFAULT;
  p->state = EMBRYO;
  p->pid = nextpid++;

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

  return p;
}

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

  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
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

  np->nice = NICE_DEFAULT;
  np->orig_nice = NICE_DEFAULT;

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
  release(&ptable.lock);

  return pid;
}

// Exit the current process.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

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

  // **Release any locks owned by this process** (safety)
  for (int i = 0; i < MAX_LOCKS; i++) {
    if (locks[i].held && locks[i].owner_pid == curproc->pid) {
      locks[i].held = 0;
      locks[i].owner_pid = -1;
      wakeup1(&locks[i]); // ptable.lock is held
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    sleep(curproc, &ptable.lock);
  }
}

// Per-CPU process scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

#if SCHED_PRIORITY
  int top_prio;
  static int prio_cursor[NICE_MAX + 1] = {0};
#endif

  for(;;){
    sti();

    acquire(&ptable.lock);
#if SCHED_PRIORITY
    // 1) find minimal nice among RUNNABLE
    top_prio = NICE_MAX + 1;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == RUNNABLE && p->nice < top_prio)
        top_prio = p->nice;
    }

    if(top_prio <= NICE_MAX){
      int start = prio_cursor[top_prio] % NPROC;
      int chosen = -1;

      // 2) round-robin within same priority level
      for(int k = 0; k < NPROC; k++){
        int i = (start + k) % NPROC;
        struct proc *q = &ptable.proc[i];
        if(q->state == RUNNABLE && q->nice == top_prio){
          chosen = i;
          break;
        }
      }

      if(chosen >= 0){
        p = &ptable.proc[chosen];

        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        swtch(&(c->scheduler), p->context);
        switchkvm();

        c->proc = 0;

        prio_cursor[top_prio] = (chosen + 1) % NPROC;
      }
    }
#else
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      c->proc = 0;
    }
#endif
    release(&ptable.lock);

  }
}

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

void
yield(void)
{
  acquire(&ptable.lock);
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

void
forkret(void)
{
  static int first = 1;
  release(&ptable.lock);

  if (first) {
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
}

void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  if(lk != &ptable.lock){
    acquire(&ptable.lock);
    release(lk);
  }
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  p->chan = 0;

  if(lk != &ptable.lock){
    release(&ptable.lock);
    acquire(lk);
  }
}

// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

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

// ===== Priority & nice =====

static int
min_waiter_nice_for_owner(struct proc *owner)
{
  int min_n = NICE_MAX + 1; // sentinel: no waiter
  if(owner == 0) return min_n;

  // ptable.lock must be held by callers
  for (int i = 0; i < MAX_LOCKS; i++) {
    if (locks[i].held && locks[i].owner_pid == owner->pid) {
      // Scan sleepers waiting on &locks[i]
      void *chan = (void*)&locks[i];
      for (struct proc *q = ptable.proc; q < &ptable.proc[NPROC]; q++) {
        if (q->state == SLEEPING && q->chan == chan) {
          if (q->nice < min_n) min_n = q->nice;
        }
      }
    }
  }
  return min_n;
}

static void
recalc_effective_nice(struct proc *owner)
{
  if(!owner) return;
  if(owner->orig_nice < NICE_MIN) owner->orig_nice = NICE_MIN;
  if(owner->orig_nice > NICE_MAX) owner->orig_nice = NICE_MAX;

  // ptable.lock must be held by callers
  int min_waiter = min_waiter_nice_for_owner(owner);
  int effective = owner->orig_nice;
  if (min_waiter <= NICE_MAX && min_waiter < effective)
    effective = min_waiter;
  owner->nice = effective;
}

int
setnice(int pid, int val, int *old)
{
  struct proc *p;
  int rc = -1;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED) continue;
    if(p->pid == pid){
      if(old) *old = p->nice;
      // Update base nice and recompute effective (considering current waiters)
      p->orig_nice = val;
      recalc_effective_nice(p);
      rc = 0;
      break;
    }
  }
  release(&ptable.lock);
  return rc;
}

void
inherit_priority(struct proc *p, int newnice)
{
  if(!p) return;
  if(newnice < NICE_MIN) newnice = NICE_MIN;
  if(newnice > NICE_MAX) newnice = NICE_MAX;
  if(newnice < p->nice){
    p->nice = newnice;
  }
}

void
restore_priority(struct proc *p)
{
  if(!p) return;

  if(p->orig_nice < NICE_MIN) p->orig_nice = NICE_MIN;
  if(p->orig_nice > NICE_MAX) p->orig_nice = NICE_MAX;

  acquire(&ptable.lock);

  int min_waiter = NICE_MAX + 1;
  for (int i = 0; i < MAX_LOCKS; i++) {
    if (locks[i].held && locks[i].owner_pid == p->pid) {
      void *chan = (void*)&locks[i];
      for (struct proc *q = ptable.proc; q < &ptable.proc[NPROC]; q++) {
        if (q->state == SLEEPING && q->chan == chan) {
          if (q->nice < min_waiter) min_waiter = q->nice;
        }
      }
    }
  }

  int eff = p->orig_nice;
  if (min_waiter <= NICE_MAX && min_waiter < eff)
    eff = min_waiter;

  p->nice = eff;

  release(&ptable.lock);
}


struct proc*
find_proc_by_pid(int pid)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED) continue;
    if(p->pid == pid)
      return p;
  }
  return 0;
}

// Public IDs: 1..7 only
static int
lock_index_from_id(int id) {
  if (id >= 1 && id <= MAX_LOCKS) return id - 1;  // 1..7 -> 0..6
  return -1; // invalid
}

int
k_lock_acquire(int id)
{
  int idx = lock_index_from_id(id);
  if (idx < 0) return -1;

  struct lock_t *lk;
  struct proc *owner;
  struct proc *me = myproc();

  acquire(&ptable.lock);
  lk = &locks[idx];

  // Re-entrance not allowed: same owner tries to acquire again
  if (lk->held && lk->owner_pid == me->pid) {
    release(&ptable.lock);
    return -1;
  }

  for (;;) {
    if (!lk->held) {
      lk->held = 1;
      lk->owner_pid = me->pid;
      release(&ptable.lock);
      return 0;
    }

    // Priority inheritance
    owner = find_proc_by_pid(lk->owner_pid); // ptable.lock held
    if (owner && me->nice < owner->nice) {
      inherit_priority(owner, me->nice);
    }

    // sleep on lock channel; releases & reacquires ptable.lock around sched
    sleep(lk, &ptable.lock);
  }
}

int
k_lock_release(int id)
{
  int idx = lock_index_from_id(id);
  if (idx < 0) return -1;

  struct lock_t *lk;
  struct proc *me = myproc();

  acquire(&ptable.lock);
  lk = &locks[idx];

  if (lk->held && lk->owner_pid == me->pid) {
    lk->held = 0;
    lk->owner_pid = -1;

    // Recompute (may keep inherited priority if other lock(s) still have waiters)
    recalc_effective_nice(me);

    // Wake waiters; ptable.lock is held -> use wakeup1
    wakeup1(lk);

    release(&ptable.lock);
    return 0;
  }

  release(&ptable.lock);
  return -1;
}
