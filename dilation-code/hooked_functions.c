#include "dilation_module.h"


/*
Contains the code for acquiring the syscall table, as well as the 4 system calls Timekeeper currently hooks.
*/

unsigned long **aquire_sys_call_table(void);
asmlinkage long sys_sleep_new(struct timespec __user *rqtp, struct timespec __user *rmtp);
asmlinkage long (*ref_sys_sleep)(struct timespec __user *rqtp, struct timespec __user *rmtp);

asmlinkage int sys_poll_new(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
asmlinkage int (*ref_sys_poll)(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
asmlinkage int (*ref_sys_poll_dialated)(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);


asmlinkage int sys_select_new(int k, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
asmlinkage int (*ref_sys_select)(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
asmlinkage int (*ref_sys_select_dialated)(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);

extern struct list_head exp_list;
//struct poll_list;
extern struct poll_list {
    struct poll_list *next;
    int len;
    struct pollfd entries[0];
};
extern struct poll_helper_struct;
extern struct select_helper_struct;
extern hashmap poll_process_lookup;
extern hashmap select_process_lookup;

extern int find_children_info(struct task_struct* aTask, int pid);
extern int kill(struct task_struct *killTask, int sig, struct dilation_task_struct* dilation_task);
extern int experiment_stopped;
extern s64 Sim_time_scale;



extern int do_dialated_poll(unsigned int nfds,  struct poll_list *list, struct poll_wqueues *wait,struct task_struct * tsk);
extern int do_dialated_select(int n, fd_set_bits *fds,struct task_struct * tsk);

s64 get_dilated_time(struct task_struct * task)
{
	s64 temp_past_physical_time;
	struct timeval tv;

	do_gettimeofday(&tv);
	s64 now = timeval_to_ns(&tv);

	if(task->virt_start_time != 0){
		if (task->group_leader != task) { //use virtual time of the leader thread
                       	task = task->group_leader;
               	}
	
		s32 rem;
		s64 real_running_time;
		s64 dilated_running_time;
		real_running_time = now - task->virt_start_time;
		if (task->freeze_time != 0)
			temp_past_physical_time = task->past_physical_time + (now - task->freeze_time);
		else
			temp_past_physical_time = task->past_physical_time;

		if (task->dilation_factor > 0) {
			dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*1000 ,task->dilation_factor,&rem) + task->past_virtual_time;
			now = dilated_running_time + task->virt_start_time;
		}
		else if (task->dilation_factor < 0) {
			dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*(task->dilation_factor*-1),1000,&rem) + task->past_virtual_time;
			now =  dilated_running_time + task->virt_start_time;
		}
		else {
			dilated_running_time = (real_running_time - temp_past_physical_time) + task->past_virtual_time;
			now = dilated_running_time + task->virt_start_time;
		}
	
	}

	return now;

}

/*
Hooks the sleep system call, so the process will wake up when it reaches the experiment virtual time,
not the system time
*/
asmlinkage long sys_sleep_new(struct timespec __user *rqtp, struct timespec __user *rmtp) {
    struct list_head *pos;
    struct list_head *n;
    struct dilation_task_struct* task;
	struct dilation_task_struct *dilTask;
    struct timeval ktv;
	struct task_struct *current_task;
    s64 now;
	s64 now_new;
    s32 rem;
    s64 real_running_time;
    s64 dilated_running_time;
	current_task = current;

	spin_lock(&current->dialation_lock);
	if (experiment_stopped == RUNNING && current->virt_start_time != NOTSET && current->freeze_time == 0)
	{	spin_unlock(&current->dialation_lock);
        	list_for_each_safe(pos, n, &exp_list)
        	{
                	task = list_entry(pos, struct dilation_task_struct, list);
			if (find_children_info(task->linux_task, current->pid) == 1) { // I think it checks if the curret task belongs to the list of tasks in the experiment (or their children)
	        	        do_gettimeofday(&ktv);
				now = timeval_to_ns(&ktv);

				spin_lock(&current->dialation_lock);				
                	        real_running_time = now - current->virt_start_time;
				//current->freeze_time = now; // ** Added new
                        	if (current->dilation_factor > 0) {
                			dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*1000 ,current->dilation_factor,&rem) + current->past_virtual_time;
	                                now_new = dilated_running_time + current->virt_start_time;
        	                }
                	        else if (current->dilation_factor < 0) {
		        	        dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*(current->dilation_factor*-1),1000,&rem) + current->past_virtual_time;
                                	now_new =  dilated_running_time + current->virt_start_time;
                        	}
                        	else {
                                	dilated_running_time = (real_running_time - current->past_physical_time) + current->past_virtual_time;
	                                now_new = dilated_running_time + current->virt_start_time;
        	                }
				current->wakeup_time = now_new + ((rqtp->tv_sec*1000000000) + rqtp->tv_nsec)*Sim_time_scale; 
				spin_unlock(&current->dialation_lock);

				printk(KERN_INFO "TimeKeeper : PID : %d, New wake up time : %lld\n",current->pid, current->wakeup_time);

				dilTask = container_of(&current_task, struct dilation_task_struct, linux_task);
				kill(current, SIGSTOP, dilTask); // I think the dilation Task of the container (which was running the current task) will wake up the task using some timer.
				return 0;
			} //end if
        	} //end for loop
	} //end if
	spin_unlock(&current->dialation_lock);

        return ref_sys_sleep(rqtp,rmtp);
}



asmlinkage int sys_select_new(int k, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp){

	printk(KERN_INFO "TimeKeeper : Calling Ref select PID : %d\n",current->pid);

	struct list_head *pos;
    struct list_head *n;
    struct dilation_task_struct* task;
	struct dilation_task_struct *dilTask;
    struct timeval ktv;
	struct task_struct *current_task;
    s64 now;
	s64 now_new;
    s32 rem;
    s64 real_running_time;
    s64 dilated_running_time;
	current_task = current;
	struct timespec end_time, *to = NULL;
	int ret;
	struct select_helper_struct * select_helper;
	int err = -EFAULT, fdcount, len, size;
	s64 secs_to_sleep;
	s64 nsecs_to_sleep;
	struct timeval tv;
	struct timeval rtv;
	int max_fds;
	struct fdtable *fdt;
	long stack_fds[SELECT_STACK_ALLOC/sizeof(long)];
	void * bits;

	

	if(experiment_stopped == RUNNING && current->virt_start_time != NOTSET && tvp != NULL){	
		//list_for_each_safe(pos, n, &exp_list){
	        //        task = list_entry(pos, struct dilation_task_struct, list);
		//	if (find_children_info(task->linux_task, current->pid) == 1) {
				if (copy_from_user(&tv, tvp, sizeof(tv)))
					return -EFAULT;

				secs_to_sleep = tv.tv_sec + (tv.tv_usec / USEC_PER_SEC);
				nsecs_to_sleep = (tv.tv_usec % USEC_PER_SEC) * NSEC_PER_USEC;

				ret = -EINVAL;
				if (k < 0)
					goto out_nofds;

				select_helper = hmap_get(&select_process_lookup, &current->pid);
				if(select_helper == NULL){
					select_helper = kmalloc(sizeof(struct select_helper_struct), GFP_KERNEL);
					if(select_helper == NULL){
						printk(KERN_INFO "TimeKeeper : Select Process NOMEM");
						return -ENOMEM;
					}
					
					
				}

				select_helper->bits = (long *) kmalloc(SELECT_STACK_ALLOC/sizeof(long), GFP_KERNEL);
				init_waitqueue_head(&select_helper->w_queue);
				select_helper->done = 0;
				select_helper->ret = -EFAULT;

				if(select_helper->bits == NULL){
					kfree(select_helper);
					return -ENOMEM;
				}


				/* max_fds can increase, so grab it once to avoid race */
				rcu_read_lock();
				fdt = files_fdtable(current->files);
				max_fds = fdt->max_fds;
				rcu_read_unlock();
				if (k > max_fds)
					k = max_fds;
				/*
				 * We need 6 bitmaps (in/out/ex for both incoming and outgoing),
				 * since we used fdset we need to allocate memory in units of
				 * long-words. 
				 */
				size = FDS_BYTES(k);
				if (size > sizeof(stack_fds) / 6) {
					/* Not enough space in on-stack array; must use kmalloc */
					ret = -ENOMEM;
					kfree(select_helper->bits);
					select_helper->bits = kmalloc(6 * size, GFP_KERNEL);
					if (!select_helper->bits)
						goto out_nofds;
				}
				bits = select_helper->bits;
				select_helper->fds.in      = bits;
				select_helper->fds.out     = bits +   size;
				select_helper->fds.ex      = bits + 2*size;
				select_helper->fds.res_in  = bits + 3*size;
				select_helper->fds.res_out = bits + 4*size;
				select_helper->fds.res_ex  = bits + 5*size;

				if ((ret = get_fd_set(k, inp, select_helper->fds.in)) ||
				    (ret = get_fd_set(k, outp, select_helper->fds.out)) ||
				    (ret = get_fd_set(k, exp, select_helper->fds.ex)))
					goto out;

				zero_fd_set(k, select_helper->fds.res_in);
				zero_fd_set(k, select_helper->fds.res_out);
				zero_fd_set(k, select_helper->fds.res_ex);

				do_gettimeofday(&ktv);
				now = timeval_to_ns(&ktv);
				
				spin_lock(&current->dialation_lock);
                real_running_time = now - current->virt_start_time;
				//current->freeze_time = now; // ** Added new
                if (current->dilation_factor > 0) {
                	dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*1000 ,current->dilation_factor,&rem) + current->past_virtual_time;
	                now_new = dilated_running_time + current->virt_start_time;
        	    }
                else if (current->dilation_factor < 0) {
		            dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*(current->dilation_factor*-1),1000,&rem) + current->past_virtual_time;
                   	now_new =  dilated_running_time + current->virt_start_time;
                }
                else {
                 	dilated_running_time = (real_running_time - current->past_physical_time) + current->past_virtual_time;
	                now_new = dilated_running_time + current->virt_start_time;
        	    }
				current->freeze_time = 0;
				s64 wakeup_time;
				wakeup_time = now_new + ((secs_to_sleep*1000000000) + nsecs_to_sleep)*Sim_time_scale; 
				printk(KERN_INFO "TimeKeeper : Select Process Waiting %d. Timeout sec %d, nsec %d.",current->pid,secs_to_sleep,nsecs_to_sleep);
				printk(KERN_INFO "TimeKeeper : Select Process Waiting %d. Curr time %lld, Wakeup time %lld",current->pid,now_new,current->wakeup_time);				
				select_helper->n = k;	
				printk(KERN_INFO "TimeKeeper : Select nfds = %lu\n",select_helper->n);
				hmap_put(&select_process_lookup, &current->pid, select_helper);
				spin_unlock(&current->dialation_lock);

				while(1){

					now_new = get_dilated_time(current);
					if(now_new < wakeup_time){

						wait_event(select_helper->w_queue,select_helper->done == 1);
						set_current_state(TASK_RUNNING);
						
						spin_lock(&current->dialation_lock);
						ret = do_dialated_select(select_helper->n,&select_helper->fds,current);
						if(ret || select_helper->ret == FINISHED){
							
							select_helper->ret = ret;
							spin_unlock(&current->dialation_lock);
							break;
						}
						select_helper->done = 0;
						spin_unlock(&current->dialation_lock);
						

					}
					else{
						spin_lock(&current->dialation_lock);
						select_helper->ret = 0;
						spin_unlock(&current->dialation_lock);
						break;
					}

				}
				
				
				printk(KERN_INFO "TimeKeeper : Resumed Select Process %d\n",current->pid);

				ret = select_helper->ret;
				memset(&rtv, 0, sizeof(rtv));
				copy_to_user(tvp, &rtv, sizeof(rtv));
				
				if (set_fd_set(k, inp, select_helper->fds.res_in) ||
				    set_fd_set(k, outp, select_helper->fds.res_out) ||
				    set_fd_set(k, exp, select_helper->fds.res_ex))
					ret = -EFAULT;

				spin_lock(&current->dialation_lock);
				hmap_remove(&select_process_lookup, &current->pid);
				spin_unlock(&current->dialation_lock);

			out:
				kfree(select_helper->bits);

			out_nofds:
				kfree(select_helper);
				printk(KERN_INFO "TimeKeeper : Select finished PID %d\n",current->pid);
				return ret;


		//	}
		//}
	}

	return ref_sys_select(k,inp,outp,exp,tvp);
}

asmlinkage int sys_poll_new(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs){

	struct list_head *pos;
    struct list_head *n;
    struct dilation_task_struct* task;
	struct dilation_task_struct *dilTask;
    struct timeval ktv;
	struct task_struct *current_task;
    s64 now;
	s64 now_new;
    s32 rem;
    s64 real_running_time;
    s64 dilated_running_time;
	current_task = current;
	struct timespec end_time, *to = NULL;
	int ret;
	struct poll_helper_struct * poll_helper;
	int err = -EFAULT, fdcount, len, size;
 	unsigned long todo ;
	struct poll_list *head;
 	struct poll_list *walk;
	s64 secs_to_sleep;
	s64 nsecs_to_sleep;

	
	if(experiment_stopped == RUNNING && current->virt_start_time != NOTSET && timeout_msecs >= 0){
	

		list_for_each_safe(pos, n, &exp_list)
        {
            task = list_entry(pos, struct dilation_task_struct, list);
			if (find_children_info(task->linux_task, current->pid) == 1) {
				printk(KERN_INFO "TimeKeeper : Processing Poll Process %d\n",current->pid);
				secs_to_sleep = timeout_msecs / MSEC_PER_SEC;
				nsecs_to_sleep = (timeout_msecs % MSEC_PER_SEC) * NSEC_PER_MSEC;

				if (nfds > RLIMIT_NOFILE){
					printk(KERN_INFO "TimeKeeper : Poll Process Invalid");
					return -EINVAL;
				}


				poll_helper = hmap_get(&poll_process_lookup, &current->pid);
				if(poll_helper == NULL){
					poll_helper = kmalloc(sizeof(struct poll_helper_struct), GFP_KERNEL);
					if(poll_helper == NULL){
						printk(KERN_INFO "TimeKeeper : Poll Process NOMEM");
						return -ENOMEM;
					}
					
					
				}

				
				printk(KERN_INFO "TimeKeeper : Poll Process Allocated Poll Helper %d",current->pid);
				poll_helper->head = (struct poll_list *) kmalloc(POLL_STACK_ALLOC/sizeof(long), GFP_KERNEL);
				poll_helper->table = (struct poll_wqueues *) kmalloc(sizeof(struct poll_wqueues), GFP_KERNEL);

				if(poll_helper->head == NULL || poll_helper->table == NULL){
					printk(KERN_INFO "TimeKeeper : Poll Process NOMEM Here");
					return -ENOMEM;
				}
				printk(KERN_INFO "TimeKeeper : Poll Process Allocated Poll Head and Table %d",current->pid);


				
				head = poll_helper->head;	
				poll_helper->err = -EFAULT;
				poll_helper->done = 0;
				poll_helper->walk = head;
				walk = head;
				init_waitqueue_head(&poll_helper->w_queue);

				
				len = (nfds < N_STACK_PPS ? nfds: N_STACK_PPS);
				todo = nfds;
				for (;;) {
					walk->next = NULL;
					walk->len = len;
					if (!len)
						break;

					if (copy_from_user(walk->entries, ufds + nfds-todo,
							sizeof(struct pollfd) * walk->len))
						goto out_fds;

					todo -= walk->len;
					if (!todo)
						break;

					len = (todo < POLLFD_PER_PAGE ? todo : POLLFD_PER_PAGE );
					size = sizeof(struct poll_list) + sizeof(struct pollfd) * len;
					walk = walk->next = kmalloc(size, GFP_KERNEL);
					if (!walk) {
						err = -ENOMEM;
						goto out_fds;
					}
				}
				printk(KERN_INFO "TimeKeeper : Poll Process Completed Loop %d",current->pid);	
				poll_initwait(poll_helper->table);

				do_gettimeofday(&ktv);
				now = timeval_to_ns(&ktv);
				
				spin_lock(&current->dialation_lock);
                real_running_time = now - current->virt_start_time;
				//current->freeze_time = now; // ** Added new
                if (current->dilation_factor > 0) {
                	dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*1000 ,current->dilation_factor,&rem) + current->past_virtual_time;
	                now_new = dilated_running_time + current->virt_start_time;
        	    }
                else if (current->dilation_factor < 0) {
		            dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*(current->dilation_factor*-1),1000,&rem) + current->past_virtual_time;
                   	now_new =  dilated_running_time + current->virt_start_time;
                }
                else {
                   	dilated_running_time = (real_running_time - current->past_physical_time) + current->past_virtual_time;
	                now_new = dilated_running_time + current->virt_start_time;
        	    }

				current->freeze_time = 0;
				//current->wakeup_time = now_new + ((secs_to_sleep*1000000000) + nsecs_to_sleep)*Sim_time_scale; 
				s64 wakeup_time;
				wakeup_time = now_new + ((secs_to_sleep*1000000000) + nsecs_to_sleep)*Sim_time_scale; 

				printk(KERN_INFO "TimeKeeper : Poll Process Waiting %d. Timeout sec %d, nsec %d.",current->pid,secs_to_sleep,nsecs_to_sleep);
				printk(KERN_INFO "TimeKeeper : Poll Process Waiting %d. Curr time %lld, Wakeup time %lld",current->pid,now_new,current->wakeup_time);

				hmap_put(&poll_process_lookup,&current->pid,poll_helper);			
				spin_unlock(&current->dialation_lock);

				while(1){

					now_new = get_dilated_time(current);
					if(now_new < wakeup_time){

						wait_event(poll_helper->w_queue,poll_helper->done == 1);
						set_current_state(TASK_RUNNING);

						if(experiment_stopped == NOTRUNNING || experiment_stopped == STOPPING)
							return -EFAULT;
						
						spin_lock(&current->dialation_lock);
						err = do_dialated_poll(poll_helper->nfds, poll_helper->head,poll_helper->table,current);
						if(err || poll_helper->err == FINISHED){
							
							poll_helper->err = err;
							spin_unlock(&current->dialation_lock);
							break;
						}
						poll_helper->done = 0;
						spin_unlock(&current->dialation_lock);
						

					}
					else{
						spin_lock(&current->dialation_lock);
						poll_helper->err = 0;
						spin_unlock(&current->dialation_lock);
						break;
					}

				}
				
				printk(KERN_INFO "TimeKeeper : Resumed Poll Process %d\n",current->pid);
				poll_freewait(poll_helper->table);
	
				for (walk = head; walk; walk = walk->next) {
					struct pollfd *fds = walk->entries;
					int j;

					for (j = 0; j < walk->len; j++, ufds++)
						if (__put_user(fds[j].revents, &ufds->revents))
							goto out_fds;
			  	}

				err = poll_helper->err;

				out_fds:
				walk = head->next;
				while (walk) {
					struct poll_list *pos = walk;
					walk = walk->next;
					kfree(pos);
				}
				kfree(head);
				kfree(poll_helper->table);
				spin_lock(&current->dialation_lock);
				hmap_remove(&poll_process_lookup, &current->pid);
				spin_unlock(&current->dialation_lock);
				kfree(poll_helper);
				printk(KERN_INFO "TimeKeeper : Poll Process Finished %d",current->pid);

				return err;

				
			}
		}
	}
	
	return ref_sys_poll(ufds,nfds,timeout_msecs);
	



}


/***
Finds us the location of the system call table
***/
unsigned long **aquire_sys_call_table(void)
{
        unsigned long int offset = PAGE_OFFSET;
        unsigned long **sct;
        while (offset < ULLONG_MAX) {
                sct = (unsigned long **)offset;

                if (sct[__NR_close] == (unsigned long *) sys_close)
                        return sct;

                offset += sizeof(void *);
        }
        return NULL;
}

