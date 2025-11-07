// proc.h

#define NICE_MIN 0
#define NICE_MAX 4
#define NICE_DEFAULT 2

#define MAX_LOCKS 7

// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  int nice;                    // nice value (relative) 0 to 4
  int orig_nice;               // nice value (original)
};

int setnice(int pid, int val, int *old); // set nice value of a process

struct lock_t {
  int id;          // valid public IDs: 1..7  (internal index: 0..6)
  int held;        // 0=free, 1=held
  int owner_pid;   // holder pid, -1 if free
};

extern struct lock_t locks[MAX_LOCKS];

int k_lock_acquire(int id);
int k_lock_release(int id);
struct proc* find_proc_by_pid(int pid);
void inherit_priority(struct proc *p, int newnice);
void restore_priority(struct proc *p);   // recompute effective nice

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
