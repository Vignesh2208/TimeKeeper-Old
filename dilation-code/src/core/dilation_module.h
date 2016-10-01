#ifndef HEADER_FILE
#define HEADER_FILE

#include "includes.h"




#define BITS_PER_LONG 32
#define POLLIN_SET (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR)
#define POLLOUT_SET (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR)
#define POLLEX_SET (POLLPRI)
#define FDS_IN(fds, n)          (fds->in + n)
#define FDS_OUT(fds, n)         (fds->out + n)
#define FDS_EX(fds, n)          (fds->ex + n)
#define BITS(fds, n)    (*FDS_IN(fds, n)|*FDS_OUT(fds, n)|*FDS_EX(fds, n))

#define MSEC_PER_SEC    1000L
#define USEC_PER_MSEC   1000L
#define NSEC_PER_USEC   1000L
#define NSEC_PER_MSEC   1000000L
#define USEC_PER_SEC    1000000L
#define NSEC_PER_SEC    1000000000L
#define FSEC_PER_SEC    1000000000000000L
#define EFAULT          14  
#define EINVAL          22
#define EINTR            4 
#define ENOMEM          12 
#define POLL_STACK_ALLOC      256
#define SELECT_STACK_ALLOC      256
#define RLIMIT_NOFILE           5       /* max number of open files */

#define N_STACK_PPS ((POLL_STACK_ALLOC - sizeof(struct poll_list))  / \
                         sizeof(struct pollfd))

#define POLLFD_PER_PAGE  ((4096-sizeof(struct poll_list)) / sizeof(struct pollfd))
#define FINISHED 2
#define GOT_RESULT -1


// the callback functions for the TimeKeeper status file
ssize_t status_read(struct file *pfil, char __user *pBuf, size_t len, loff_t *p_off);
ssize_t status_write(struct file *file, const char __user *buffer, size_t count, loff_t *data);
static const struct file_operations proc_file_fops = {
 .read = status_read,
 .write = status_write,
};



typedef struct sched_queue_element{

	s64 share_factor;
	int static_priority;
	s64 duration_left;
	int pid;
	struct task_struct * curr_task;
	

}lxc_schedule_elem;


// This structure maintains additional info to support TimeKeeper functionality. A dilation_task_struct gets created for every
// process in an experiment
struct dilation_task_struct
{
        struct task_struct *linux_task; // the corresponding task_struct this task is associated with
        struct list_head list; // the linked list
        struct dilation_task_struct *next; // the next dilation_task_struct in the per cpu chain
        struct dilation_task_struct *prev; // the prev dilation_task_struct in the per cpu chain
        struct hrtimer timer; // the hrtimer that will be set to fire some point in the future
		struct hrtimer schedule_timer; // A timer that is used to change the process at the head of the schedule queue;

        short stopped; // a simple flag that gets set if the process dies
        s64 curr_virt_time; // the current virtual time of the corresponding container
        s64 running_time; // how long the container should be allowed to run in the next round
	s64 expected_time; // the expected virtual time the container should be at
	s64 wake_up_time; //if a process was told to sleep, this is the point in virtual time in which it should 'wake up'
	int newDilation; //in a synced experiment, this will store the dilation to change to
	int cpu_assignment; //-1 if it has been assigned to a CPU yet, else the CPU assignment
	llist schedule_queue;
	hashmap valid_children;
	lxc_schedule_elem * last_run;
	int rr_run_time;

	s64 increment; // CS: the increment it should advance in the next round
	struct timeline* tl; // the timeline it is associated with
        struct list_head cpuList; //for CS synchronization among specific cpus
	
};


struct poll_helper_struct
{
	pid_t process_pid;
	struct poll_list *head;
 	struct poll_list *walk;
	struct poll_wqueues *table;
	unsigned int nfds;
	int err;
	wait_queue_head_t w_queue;
	int done;

};

struct select_helper_struct
{
	pid_t process_pid;
	fd_set_bits fds;
	void *bits;
	unsigned long n;
	wait_queue_head_t w_queue;
	int ret;
	int done;
};

// An additional structure necessary to provide integration of TimeKeeper with S3F. This provides CS. Each 'timeline' is assigned to a specific
// CPU, and each timeline can have multiple containers assigned to it. Each timeline must have a unique id ( >= 0)
struct timeline
{
    int number; // the unique timeline id ( >= 0)
    struct dilation_task_struct* head; // the head of a doubly-linked list that has all the containers associated with the timeline
	spinlock_t tl_lock; 	// timeline lock
    int cpu_assignment; // the specific CPU this timeline is assigned to
    struct timeline* next; // points to the next timeline assigned to this cpu
    struct task_struct* user_proc; // the task_struct to send the 'finished' message to when the timeline has finished advaincing in time
  	wait_queue_head_t w_queue;
  	wait_queue_head_t pthread_queue;
  	wait_queue_head_t progress_thread_queue;
  	wait_queue_head_t unfreeze_proc_queue;
  	atomic_t stop_thread;
  	atomic_t done;
  	atomic_t pthread_done;
  	atomic_t progress_thread_done;
  	atomic_t hrtimer_done;
	int force; // a flag to determine if the virtual time should be forced to be exact as the user expects or not
	struct task_struct* thread; // the kernel thread associated with this timeline
	struct task_struct* run_timeline_thread; // the kernel thread associated with this timeline
};


#define STATUS_MAXSIZE 1004
#define DILATION_DIR "dilation"
#define DILATION_FILE "status"

#define EXP_CPUS 4
                          // How many processors are dedicated to the experiment. My system has 8, so I set it to 6 so 
                          //background tasks can run on the other 2.
                          // This needs to be >= 2 and your system needs to have at least 4 vCPUs

#define NETLINK_USER 31

//macros for experiment_type
#define NOTSET 0 //not set yet
#define CBE 1 //best effort (ns3, core)
#define CS 2 //concurrent synchronized (S3F)

//macros for experiment_stopped
#define NOTRUNNING -1
#define RUNNING 0
#define FROZEN 1
#define STOPPING 2

//macros to determine if the experiment should be forced (CS)
#define NOFORCE 0
#define FORCE 1

void progress_exp(void);

// *** general_commands.c
extern void freeze_proc(struct task_struct *aTask);
extern void unfreeze_proc(struct task_struct *aTask);
extern void freeze_or_unfreeze(int pid, int sig);
extern void yield_proc(char *write_buffer);
extern void yield_proc_recurse(char *write_buffer);

extern void change_dilation(int pid, int new_dilation);
extern void dilate_proc_recurse(char *write_buffer);
extern void dilate_proc(char *write_buffer);
extern void timer_callback(unsigned long task_ul);

extern void leap_proc(char *write_buffer);

// *** sync_experiment.c
extern int catchup_func(void *data);
extern void core_sync_exp(void);
extern void set_clean_exp(void);
extern void add_to_exp(int pid);
extern void add_to_exp_proc(char *write_buffer);
extern void add_sim_to_exp_proc(char *write_buffer);
extern void sync_and_freeze(void);
extern void progress_exp(void);
extern void set_cbe_exp_timeslice(char *write_buffer);

// *** s3f_sync_experiment.c
extern void s3f_add_to_exp_proc(char *write_buffer);
extern void s3f_set_interval(char *write_buffer);
extern int s3f_progress_timeline(char *write_buffer);
extern void s3f_reset(char *write_buffer);
extern void fix_timeline_proc(char *write_buffer);

// *** hooked_functions.c
extern unsigned long **aquire_sys_call_table(void);
extern asmlinkage long sys_sleep_new(struct timespec __user *rqtp, struct timespec __user *rmtp);
extern asmlinkage int sys_poll_new(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
extern asmlinkage int sys_select_new(int k, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);

// *** util.c
extern void send_a_message(int pid);
extern void send_a_message_proc(char * write_buffer);

extern int get_next_value (char *write_buffer);
extern int atoi(char *s);
extern struct task_struct* find_task_by_pid(unsigned int nr);
extern int kill(struct task_struct *killTask, int sig, struct dilation_task_struct* dilation_task);

extern void print_proc_info(char *write_buffer);
extern void print_rt_info(char *write_buffer);

extern void print_children_info_proc(char *write_buffer);
extern void print_threads_proc(char *write_buffer);

extern int add_to_schedule_list(struct dilation_task_struct * lxc, struct task_struct *new_task, s64 window_duration, s64 expected_increase);
extern struct task_struct * pop_schedule_list(struct dilation_task_struct * lxc);
extern lxc_schedule_elem * schedule_list_get_head(struct dilation_task_struct * lxc);
extern void requeue_schedule_list(struct dilation_task_struct * lxc);
extern void clean_up_schedule_list(struct dilation_task_struct * lxc);
extern int schedule_list_size(struct dilation_task_struct * lxc);



#endif

