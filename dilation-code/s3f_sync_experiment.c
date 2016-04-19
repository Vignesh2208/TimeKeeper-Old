#include "dilation_module.h"

/*
	Developed to support S3F integration with TimeKeeper. These additional functions provide Composite Synchronization (CS), the simulator
	directly controls how far in virtual time each container should advance, as well as when it can advance. A notion of a 'timeline' is 
	supported in S3F. Multiple containers can be assigned to the same timeline.
*/

#define SIG_END 44

//S3F Specific Functions and variables (CS)
void s3fCalcTaskRuntime(struct dilation_task_struct * task);
enum hrtimer_restart s3f_hrtimer_callback( struct hrtimer *timer);
void s3f_add_to_exp(int pid, int increment);
void s3f_add_to_exp_proc(char *write_buffer);
void s3f_progress_exp(void);
struct timeline* doesTimelineExist(int timeline);
void assign_timeline_to_cpu(struct timeline* tl);
void s3f_add_user_proc(char *write_buffer);
struct dilation_task_struct * s3fGetNextRunnableTask(struct dilation_task_struct * task);
void force_virtual_time(struct task_struct* aTask, s64 time);
int is_off(struct dilation_task_struct *task);
void fix_timeline(int timeline);
void fix_timeline_proc(char *write_buffer);
struct timeline* timelineHead[EXP_CPUS];
int progress_timeline_thread(void *data);
int run_timeline_processes(void * data);
//Functions and variables reused by CBE Implementation
extern void printChainInfo(void);
extern void set_children_time(struct task_struct *aTask, s64 time);
extern void set_children_policy(struct task_struct *aTask, int policy, int priority);
extern void freeze_children(struct task_struct *aTask, s64 time);
extern int freeze_proc_exp_recurse(struct dilation_task_struct *aTask);
extern void unfreeze_children(struct task_struct *aTask, s64 time, s64 expected_time);
extern int unfreeze_proc_exp_recurse(struct dilation_task_struct *aTask, s64 expected_time);
extern void set_children_cpu(struct task_struct *aTask, int cpu);
extern void clean_exp(void);
extern void calculate_virtual_time_difference(struct dilation_task_struct* task, s64 now, s64 expected_time);
extern struct dilation_task_struct* initialize_node(struct task_struct* aTask);
extern void send_a_message(int pid);
extern s64 get_virtual_time(struct dilation_task_struct* task, s64 now);

extern int TOTAL_CPUS;
extern int experiment_type;
extern struct list_head exp_list;

extern s64 PRECISION;   // doing floating point division in the kernel is HARD, therefore, I convert fractions to negative numbers.
			// The Precision specifies how far I scale the number: aka a TDF of 2 is converted to 2000,
extern int proc_num; //the number of containers in the experiment
extern int experiment_stopped; // if == -1 then the experiment is not started yet, if == 0 then the experiment is currently running, if == 1 then the experiment is set to be stopped at the end of the current round.
extern s64 chainlength[EXP_CPUS]; //for every cpu, this represents how many timeline are currently assigned to it
extern struct task_struct* chaintask[EXP_CPUS];
extern int number_of_heads; //specifies how many head containers are in the experiment. This number will most often be equal to EXP_CPUS. Handles the special case if containers < EXP_CPUS so we do not have an array index out of bounds error
extern struct mutex exp_mutex;

spinlock_t cpuLock[EXP_CPUS];
int cpuIdle[EXP_CPUS]; //0 means it is idle, 1 means not idle
struct list_head cpuWorkList[EXP_CPUS];

//general_commands.c
extern void perform_on_children(struct task_struct *aTask, void(*action)(int,int), int val);
extern void change_dilation(int pid, int new_dilation);

/***
Reads a PID from the buffer, and adds the corresponding task to the experiment (i believe this does not support the adding
of processes if the experiment has alreaded started)
***/
void s3f_add_to_exp_proc(char *write_buffer) {
        int pid, value, timeline;

	if (experiment_type == CBE) {
                printk(KERN_INFO "TimeKeeper: Trying to add to wrong experiment type.. exiting\n");
        }
	else if (experiment_stopped == NOTRUNNING) {
	        pid = atoi(write_buffer);
        	value = get_next_value(write_buffer);
	        timeline = atoi(write_buffer + value);
        	s3f_add_to_exp(pid, timeline);
	}
	else {
		printk(KERN_INFO "TimeKeeper: Trying to add a LXC to S3F experiment that is already running\n");
	}
}

/*
Reset all specified intervals for a given timeline
*/
void s3f_reset(char *write_buffer) {
	int timeline;
	struct timeline* tl;
	struct dilation_task_struct* task;
	if (experiment_type == CBE) {
		printk(KERN_INFO "TimeKeeper: Error: Trying to mix CBE and CS commands.. exiting\n");
	}
	else if (experiment_stopped != NOTRUNNING) {
		timeline = atoi(write_buffer);
		tl = doesTimelineExist(timeline);
		if (tl != NULL) {
//			printk(KERN_INFO "TimeKeeper: Clearing all task intervals for timeline: %d\n", timeline);
			task = tl->head;
			while (task != NULL) {
				task->increment = 0;
				task = task->next;
			}
		}
	}
	else {
		printk(KERN_INFO "TimeKeeper: Trying to run s3f reset when experiment is not running\n");
	}
}

/*
Progress every container in a timeline by the prespecified intervals
order of arguments: pid, timeline, force
*/
int s3f_progress_timeline(char *write_buffer) {
	int timeline, pid, value, force;
	struct timeline* tl;
	struct task_struct* task;
	struct timeval ktv;
	s64 now;
	struct dilation_task_struct * lxc = NULL;
	timeline = atoi(write_buffer);

	value = get_next_value(write_buffer);
    pid = atoi(write_buffer + value);

	value += get_next_value(write_buffer + value);
    force = atoi(write_buffer + value);

	if (experiment_type == CBE) {
		printk(KERN_INFO "TimeKeeper: Error: Trying to mix CBE and CS commands.. exiting\n");
	}
	else if (experiment_stopped != NOTRUNNING) {
		task = find_task_by_pid(pid);

		tl = doesTimelineExist(timeline);
		if (tl != NULL) {
			tl->user_proc = task;
			tl->force = force;

			do_gettimeofday(&ktv);
            now = timeval_to_ns(&ktv);
			lxc = tl->head;
            while (lxc != NULL) {
              	if (lxc->increment > 0) {
                    lxc->expected_time += lxc->increment;
                    //task->expected_time =  get_virtual_time(task, now) + task->increment;
                    calculate_virtual_time_difference(lxc,now,lxc->expected_time);            	    
                }
                lxc = lxc->next;
            }
			lxc = tl->head;
			lxc = s3fGetNextRunnableTask(lxc);
			if(lxc == NULL){
				printk(KERN_INFO "TimeKeeper: For timeline %d, no tasks to run. no need to wake progress timeline thread\n", timeline);	
				return 255;
			}
			else{

				atomic_set(&tl->done,0);
				//tl->done = 0;
				printk(KERN_INFO "TimeKeeper: For timeline %d, waking up progress timeline thread\n", timeline);
				set_current_state(TASK_INTERRUPTIBLE);
				wake_up_process(tl->thread);
				wait_event_interruptible(tl->w_queue,atomic_read(&tl->done) == 1);

			}
			return 0;
		}
		else {
			printk(KERN_INFO "TimeKeeper: Timeline does not exist..\n");
		}
	}
	else {
		printk(KERN_INFO "TimeKeeper : Send a message called from s3f_progress_timeline for pid %d\n",pid);
		//send_a_message(pid);
		printk(KERN_INFO "TimeKeeper: Trying to progress a timeline when experiment is not running!\n");
	}
	return 0;
}

/*
Set the interval for a container on a timeline
*/
void s3f_set_interval(char *write_buffer) {
	int pid, timeline, value;
	s64 interval;
        struct dilation_task_struct* list_node;
        struct list_head *pos;
        struct list_head *n;

	pid = atoi(write_buffer);

        value = get_next_value(write_buffer);
        interval = atoi(write_buffer + value);

        value += get_next_value(write_buffer + value);
        timeline = atoi(write_buffer + value);
	if (experiment_type == CBE) {
		printk(KERN_INFO "TimeKeeper: Error: Trying to mix CBE and CS commands.. exiting\n");
	}
	else if (experiment_stopped != NOTRUNNING) {
        list_for_each_safe(pos, n, &exp_list)
        {
                list_node = list_entry(pos, struct dilation_task_struct, list);
		if (list_node->linux_task->pid == pid) {
			list_node->increment = interval*1000; //convert us to ns
			s3fCalcTaskRuntime(list_node);
			if (list_node->running_time <= 5000) {
				list_node->increment = 0;
				list_node->running_time = 0;
				printk(KERN_INFO "TimeKeeper: Running time too small, exiting\n");
			}
			//printk(KERN_INFO "TimeKeeper: New interval: %lld, new runtime: %lld\n", list_node->increment, list_node->running_time);
			return;
		}
	}
	printk(KERN_INFO "TimeKeeper: Task not found in exp_list\n");
	}
	else {
		printk(KERN_INFO "TimeKeeper: Trying to run setInterval when experiment is not running!\n");
	}
	return;
}

/*
Check to see if the timeline with a given exists or not. Will return NULL if timeline does not exist
*/
struct timeline* doesTimelineExist(int timeline) {
	int i;
	struct timeline* tempTimeline;
	for (i=0; i< number_of_heads; i++) {
		tempTimeline = timelineHead[i];
		while (tempTimeline != NULL) {
			if (tempTimeline->number == timeline) {
				return tempTimeline;
			}
			tempTimeline = tempTimeline->next;
		}
	}
	return NULL;
}

/*
Assigns a timeline to a specific CPU on the system. Timeline is assigned to CPU with min chainlength
*/
void assign_timeline_to_cpu(struct timeline* tl) {
        int i;
        int index;
        s64 min;
        struct timeline *walk;
        index = 0;
        min = chainlength[index];

        for (i=1; i<number_of_heads; i++)
        {
                if (chainlength[i] < min)
                {
                        min = chainlength[i];
                        index = i;
                }
        }
	printk(KERN_INFO "TimeKeeper: index is %d, number of heads %d\n", index, number_of_heads);
        walk = timelineHead[index];
        if (walk == NULL) {
                timelineHead[index] = tl;
        }
        else {
                while (walk->next != NULL)
                {
                        walk = walk->next;
                }
                walk->next = tl;
        }
        chainlength[index] += 1;
	tl->cpu_assignment = index+(TOTAL_CPUS - EXP_CPUS);
	printk(KERN_INFO "TimeKeeper: Adding timeline %d to index: %d\n",tl->number, index);
}

/*
Adds a dilation_task_struct to the end of a timelines list. Probably adds LXc to timeline.
*/
void add_proc_to_timeline(struct timeline* tl, struct dilation_task_struct* proc) {
	struct dilation_task_struct* tmp;
	tmp = tl->head;
	if (tmp == NULL) {
		tl->head = proc;
		return;
	}
	while (tmp->next != NULL) {
		tmp = tmp->next;
	}
	tmp->next = proc;
	return;
}

/***
Get the next task that is allowed to run this round. (CS specific)
***/
struct dilation_task_struct * s3fGetNextRunnableTask(struct dilation_task_struct * task) {
        if (task == NULL) {
                return NULL;
        }
        if (task->increment > 0 && task->stopped != -1 && task->running_time > 0)
                return task;
        while (task->next != NULL) {
                task = task->next;
                if (task->increment > 0 && task->stopped != -1 && task->running_time > 0)
                        return task;
        }
        //if got through loop, this chain is done for this iteration, return NULL
        return NULL;
}

/***
Gets called by add_to_exp_proc(). Initiazes a containers timer, sets scheduling policy.
***/
void s3f_add_to_exp(int pid, int timeline) {
        struct task_struct* aTask;
        struct dilation_task_struct* list_node;
	struct timeline* targetTimeline;

        aTask = find_task_by_pid(pid);
        if (aTask == NULL)
        {
                printk(KERN_INFO "TimeKeeper: Pid %d is invalid, dropping out\n",pid);
                return;
        }

	mutex_lock(&exp_mutex);
    proc_num++;
	experiment_type = CS;



	//see if the timeline exists yet, if not, add it
	targetTimeline = doesTimelineExist(timeline);
	if (targetTimeline == NULL) {
		printk(KERN_INFO "TimeKeeper: Timeline %d does not exist, creating it\n", timeline);
	    targetTimeline = (struct timeline *)kmalloc(sizeof(struct timeline), GFP_KERNEL);
		targetTimeline->number = timeline;
		targetTimeline->next = NULL;
		targetTimeline->head = NULL;
		targetTimeline->user_proc = NULL;
		spin_lock_init(&targetTimeline->tl_lock);
		targetTimeline->thread = kthread_run(&progress_timeline_thread, targetTimeline, "worker");
		targetTimeline->run_timeline_thread = kthread_run(&run_timeline_processes, targetTimeline, "workertimeline");
		//targetTimeline->done = 0;
		atomic_set(&targetTimeline->done,0);
		init_waitqueue_head(&targetTimeline->w_queue);
		atomic_set(&targetTimeline->pthread_done,0);
		atomic_set(&targetTimeline->stop_thread,0);
		init_waitqueue_head(&targetTimeline->pthread_queue);
		number_of_heads++;
		if (number_of_heads > EXP_CPUS)
        	       	number_of_heads = EXP_CPUS;
		assign_timeline_to_cpu(targetTimeline);
	}


	list_node = initialize_node(aTask);

	list_node->cpu_assignment = targetTimeline->cpu_assignment;
	list_node->tl = targetTimeline;
	list_node->timer.function = &s3f_hrtimer_callback;
	add_proc_to_timeline(targetTimeline, list_node);

    list_add(&(list_node->list), &exp_list);
	mutex_unlock(&exp_mutex);
}

int run_timeline_processes(void * data){


	struct timeval ktv;
	struct dilation_task_struct* task;
	s64 now;
	ktime_t ktime;
    int round = 0;
	struct timeline* tl = (struct timeline *)data;
	struct timeline * ntl = NULL;
	int startJob = 0;

	if (round == 0) {
		goto noWork;
	}
	set_current_state(TASK_INTERRUPTIBLE);
    while (!kthread_should_stop())
    {

	    if (tl != NULL) 
	    {

	    	if(atomic_read(&tl->stop_thread) == 1){
	    		printk(KERN_INFO "TimeKeeper: Stopped timeline thread %d\n", tl->number);
	    		atomic_set(&tl->stop_thread, 0);
	    		return 0;
	    	}
			int index = tl->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);
			int isEmpty = 0;
			startJob = 0;
			task = tl->head;
            task = s3fGetNextRunnableTask(task);
            ntl = NULL;
            printk(KERN_INFO "TimeKeeper : Resumed timeline thread for timeline %d\n",tl->number);
			if (task == NULL) {
                printk(KERN_INFO "TimeKeeper: Task is null?? No running tasks for timeline %d\n", tl->number);
                //send_a_message(tl->user_proc->pid);
            }
            else {
				
				
				unfreeze_proc_exp_recurse(task, task->expected_time);

				if (tl->force == FORCE || ( (get_virtual_time(task, now) - task->expected_time) > task->increment) ) { //force the vt to be what you expect
					force_virtual_time(task->linux_task, task->expected_time);
				}
				
				while (task->next != NULL)
			        {

			        	task = task->next;
			                if (task->running_time > 0 && task->increment > 0 && task->stopped != -1)
			                {
			                        unfreeze_proc_exp_recurse(task, task->expected_time);
					       			//startJob = 1;
									if (tl->force == FORCE || ( (get_virtual_time(task, now) - task->expected_time) > task->increment) ) { //force the vt to be what you expect
										force_virtual_time(task->linux_task, task->expected_time);
									}
                        	}
						startJob = 0;
			        }

			}

				
				//if no more tasks need to run, send a message to userspace letting them know
		    if (startJob == 0)
		    {
				index = tl->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);
				//see if there is more work to do
				int isSet = 0;
				spin_lock(&cpuLock[index]);

				if(!list_empty(&cpuWorkList[index])){

					//while(!list_empty(&cpuWorkList[index])){

					task = list_first_entry(&cpuWorkList[index], struct dilation_task_struct, cpuList);
					if(task != NULL){
						printk(KERN_INFO "TimeKeeper: Cpu list : %d not empty. Current timeline = %d, Running next timeline : %d\n", index, tl->number, task->tl->number);
						if(tl->number == task->tl->number){
							printk(KERN_INFO "TimeKeeper: Thats wierd\n");
							ntl = tl;
						}
						else
							ntl = task->tl;
					}

					list_del((&cpuWorkList[index])->next); // *** Not sure. I think this moves on to the queued progress of the next timeline on the same cpu chain. This way the timelines on the same cpu chain are advanced one after the other.										
					
					
					set_current_state(TASK_INTERRUPTIBLE);
					//wake_up_process(task->tl->run_timeline_thread);


					//spin_unlock(&cpuLock[index]);

					/*
					if(task == NULL){
						printk(KERN_INFO "TimeKeeper: Dequeued task is null\n");
					}

					ntl = task->tl;
					printk(KERN_INFO "TimeKeeper: Dequeued task user proc pid : %d\n", ntl->user_proc->pid);
					//task = ntl->head;
        			//task = s3fGetNextRunnableTask(task);

        			printk(KERN_INFO "TimeKeeper: Got next runnable task: pid %d\n", task->linux_task->pid);

					if (task != NULL) {  // *** Not sure							

						printk(KERN_INFO "TimeKeeper: Running next runnable task: pid %d\n", task->linux_task->pid);

						unfreeze_proc_exp_recurse(task, task->expected_time);

						printk(KERN_INFO "TimeKeeper: Ran next runnable task: pid %d\n", task->linux_task->pid);

						if (ntl->force == FORCE || ( (get_virtual_time(task, now) - task->expected_time) > task->increment) ) { //force the vt to be what you expect
							force_virtual_time(task->linux_task, task->expected_time);
						}

						while (task->next != NULL)
					    {
					    		printk(KERN_INFO "TimeKeeper: Running next lxc timeline : cpu index : %d, pid %d\n", index, task->linux_task->pid);
					        	task = task->next;
				                if (task->running_time > 0 && task->increment > 0 && task->stopped != -1)
				                {
			                        unfreeze_proc_exp_recurse(task, task->expected_time);

			                        printk(KERN_INFO "TimeKeeper: Ran next lxc timeline : cpu index : %d, pid %d\n", index, task->linux_task->pid);
									if (ntl->force == FORCE || ( (get_virtual_time(task, now) - task->expected_time) > task->increment) ) { //force the vt to be what you expect
										force_virtual_time(task->linux_task, task->expected_time);
									}
		                   		}

					    }
					    printk(KERN_INFO "TimeKeeper: Ran all tasks : cpu index : %d\n", index);
					    send_a_message(ntl->user_proc->pid);				
					    printk(KERN_INFO "TimeKeeper: Sent user message to : %d\n", ntl->user_proc->pid);
					    
	
					}
					else{
						printk(KERN_INFO "TimeKeeper: Queued next task is null ? sending user message : cpu index : %d, pid : %d\n", index, ntl->user_proc->pid);
						send_a_message(ntl->user_proc->pid);				
					    printk(KERN_INFO "TimeKeeper: Queued next task is null ? sent user message : cpu index : %d. pid : %d\n", index, ntl->user_proc->pid);

					}

					
					

					spin_lock(&cpuLock[index]);
					list_del((&cpuWorkList[index])->next); // *** Not sure. I think this moves on to the queued progress of the next timeline on the same cpu chain. This way the timelines on the same cpu chain are advanced one after the other.
					*/
				}
				else{
					set_current_state(TASK_INTERRUPTIBLE);				
					cpuIdle[index] = 0;	
				}				

				printk(KERN_INFO "TimeKeeper : Send a message called from run timeline processes for timeline %d\n",tl->number);
				//send_a_message(tl->user_proc->pid); // ** modified
				//tl->done = 1;
				atomic_set(&tl->done,1);
				wake_up_interruptible(&tl->w_queue);
				printk(KERN_INFO "TimeKeeper : Sent msg to user proc for timeline %d\n",tl->number);
				spin_unlock(&cpuLock[index]);
				
			}			

	    }
	    else {
			printk(KERN_INFO "TimeKeeper: BIG BUG, the timeline in the timeline thread should never be null...\n");
	    }
	    round++;
	    noWork:
		
        set_current_state(TASK_INTERRUPTIBLE);
        if(ntl == NULL || ntl != tl){
		//printk(KERN_INFO "TimeKeeper : Run timeline thread %d, waiting for wake up signal\n",tl->number);        	
        	//printk(KERN_INFO "TimeKeeper : Finished timeline thread for timeline %d\n",tl->number);

        	if(round > 0 && ntl != NULL){

				// wake_up_process(ntl->run_timeline_thread); //*** modified
				atomic_set(&ntl->pthread_done,1);
				wake_up_interruptible(&ntl->pthread_queue);

        	}
        	
        	//schedule(); // *** modified Start a new process on this processor. The new process will also belong to tl->head list. So it will have the same cpu assignment.
        	wait_event_interruptible(tl->pthread_queue,atomic_read(&tl->pthread_done) == 1);
		
    	}
    	else{
    		if(ntl == tl){
    			printk(KERN_INFO "TimeKeeper: ntl == tl. continue\n");
    		}
    		else{
    			printk(KERN_INFO "TimeKeeper : Finished timeline thread for timeline %d\n",tl->number);
    			//schedule(); // *** modified should never reach this.
    			wait_event_interruptible(tl->pthread_queue,atomic_read(&tl->pthread_done) == 1);
    		}
    	}

    	set_current_state(TASK_RUNNING);
    	atomic_set(&tl->pthread_done,0);
	}
	return 0;
}

/*
The main worker thread for each timeline. Whenever a user calls 'progress', the corresponding worker thread will wake up, determine how
long each container in the timeline should run, then start the chain of running containers on the CPU (if the CPU is idle). If the CPU
is already running containers of a different timeline, then add the containers to a wait queue
*/
int progress_timeline_thread(void *data)
{
	struct timeval ktv;
	struct dilation_task_struct* task;
	s64 now;
	ktime_t ktime;
        int round = 0;
        int send_message = 0;
	struct timeline* tl = (struct timeline *)data;
	if (round == 0) {
		goto noWork;
	}
	set_current_state(TASK_INTERRUPTIBLE);
        while (!kthread_should_stop())
        {
        	send_message = 0;
		if (tl != NULL) {

			printk(KERN_INFO "TimeKeeper : Resumed progress timeline thread for timeline %d\n",tl->number);


			do_gettimeofday(&ktv);
            now = timeval_to_ns(&ktv);
			
			/*task = tl->head;
            while (task != NULL) {
              	if (task->increment > 0) {
                    task->expected_time += task->increment;
                    //task->expected_time =  get_virtual_time(task, now) + task->increment;
                    calculate_virtual_time_difference(task,now,task->expected_time);
            	    if (task->running_time > 1000000 || task->running_time < 10000 ) {
						//printk(KERN_INFO "TimeKeeper: %d This task should run for %lld if %lld is > 0\n",task->linux_task->pid, task->running_time, task->increment);
                    }
                }
                task = task->next;
            }*/
			task = tl->head;
            task = s3fGetNextRunnableTask(task);

			//if cpu is idle, unfreeze n go, if it is not idle, add to a queue
			if (task == NULL) {
                //printk(KERN_INFO "TimeKeeper: Task is null?? No running tasks for timeline %d\n", tl->number);
                set_current_state(TASK_INTERRUPTIBLE);
                printk(KERN_INFO "TimeKeeper : Send a message called from progress timeline thread for timeline %d\n",tl->number);
                send_message = 1;
                //send_a_message(tl->user_proc->pid);
                /*int index = tl->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);
                preempt_disable();
				local_irq_disable();
				spin_lock(&cpuLock[index]);

				if(!list_empty(&cpuWorkList[index])){
					spin_unlock(&cpuLock[index]);
					local_irq_enable();
					preempt_enable();
					set_current_state(TASK_INTERRUPTIBLE);
                	wake_up_process(tl->run_timeline_thread); // incase there is some queued job					
				}
				else{
					spin_unlock(&cpuLock[index]);
					local_irq_enable();
					preempt_enable();
				}*/

            }
            else {
            	task->tl->user_proc->pid = tl->user_proc->pid;
				int index = tl->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);
				int isEmpty = 0;
				int is_found = 0;

				// START CRITICAL REGION
				preempt_disable();
				local_irq_disable();
				spin_lock(&cpuLock[index]);

				if (cpuIdle[index] == 0) { // the cpu is idle, so we can start ours
					isEmpty = 1;
					cpuIdle[index] = 1; //set it to busy
				}
				else { //add to the work queue
					struct list_head *  ptr;
					struct dilation_task_struct * temp = NULL;
					list_for_each(ptr, &cpuWorkList[index]) {
						 temp = list_entry(ptr, struct dilation_task_struct, cpuList);
						 if( temp != NULL){
						 	if(temp->tl == task->tl){
						 		is_found = 1;
						 	}
						 }
					}
					if(is_found == 0){
						list_add_tail(&(task->cpuList), &cpuWorkList[index]); // This will probably happen if two timelines are assigned the same cpu. When they both call progress, one of them will 												      // be queued.
						printk(KERN_INFO "TimeKeeper : progress timeline thread queued new job for timeline %d\n",task->tl->number);
					}
					else{
						printk(KERN_INFO "TimeKeeper : progress_timeline_thread did not queue job. timeline %d already exists\n", task->tl->number);
					}
				}
				spin_unlock(&cpuLock[index]);
				local_irq_enable();
				preempt_enable();
				// END CRITICAL REGION
				if (isEmpty == 1) { // *** What is going on here ? I think the cpu is idle. So the task is immediately started.
                                	//unfreeze_proc_exp_recurse(task, task->expected_time);
                                	//ktime = ktime_set( 0, task->running_time );
					//hrtimer_start( &task->timer, ktime, HRTIMER_MODE_REL ); // The timer will fire after the running time has elapsed and call the callback function
					printk(KERN_INFO "TimeKeeper : run timeline thread wake up sent to timeline %d\n",tl->number);
					set_current_state(TASK_INTERRUPTIBLE);
					//wake_up_process(tl->run_timeline_thread); // *** modified
					atomic_set(&tl->pthread_done,1);
					wake_up_interruptible(&tl->pthread_queue);
				}
            }
			round++;
		}
		else {
			printk(KERN_INFO "TimeKeeper: BIG BUG, the timeline in the timeline thread should never be null...\n");
		}
		noWork:
                set_current_state(TASK_INTERRUPTIBLE);
                printk(KERN_INFO "TimeKeeper : Finished progress timeline thread for timeline %d\n",tl->number);
				if(send_message){
					if(tl != NULL){
						////send_a_message(tl->user_proc->pid); // ** modified
						//tl->done = 1;
						atomic_set(&tl->done,1);
						wake_up_interruptible(&tl->w_queue);
					}
				}
                schedule(); // Start a new process on this processor. The new process will also belong to tl->head list. So it will have the same cpu assignment.
		//printk(KERN_INFO "TimeKeeper : progress timeline thread timeline %d woken up by new progress call\n", tl->number);
	}
	return 0;
}

/***
Given a task with a TDF, determine how long it should be allowed to run in each round, stored in running_time field
***/
void s3fCalcTaskRuntime(struct dilation_task_struct * task) {
	int dil;
        s32 rem;
	s64 tempVal;

	dil = task->linux_task->dilation_factor;

	if (dil > 0) {
		tempVal = task->increment * task->linux_task->dilation_factor;
                task->running_time = div_s64_rem(tempVal,1000,&rem);
	}
	else if (dil < 0) {
                task->running_time = div_s64_rem(task->increment*1000,task->linux_task->dilation_factor*(-1),&rem);
	}
	else {
		task->running_time = task->increment;
	}
        return;
}

/***
What gets called when a containers hrtimer interrupt occurs: the task is frozen, then it determines the next container that
should be ran within that round.
***/
enum hrtimer_restart s3f_hrtimer_callback( struct hrtimer *timer )
{
        int dil;
        struct dilation_task_struct *task;
        struct dilation_task_struct * callingtask;
        struct timeval ktv;
	s64 now;
    int startJob;
	struct timeline* tl;
    ktime_t ktime;
	do_gettimeofday(&ktv);
    now = timeval_to_ns(&ktv);
    task = container_of(timer, struct dilation_task_struct, timer);
	if (task == NULL) {
		printk(KERN_INFO "TimeKeeper: This should never be null... task in hrtimer\n");
		return HRTIMER_NORESTART;
	}
    dil = task->linux_task->dilation_factor;
	callingtask = task;

	//get timeline the task belongs to.
	tl = task->tl;
	if (tl == NULL) {
		printk(KERN_INFO "TimeKeeper: The timeline of the task is null.. \n");
		return HRTIMER_NORESTART;
	}

	//printk(KERN_INFO "TimeKeeper : Waking up run timeline thread %d\n",tl->number);
	wake_up_process(tl->run_timeline_thread);
	//if the process is done, dont bother freezing it, just set flag so it gets cleaned in sync phase

	/*

	if (callingtask->stopped == -1) {
		//stopped_change = 1;
        }
	else { //its not done, so freeze
		task->stopped = 1;
		if (freeze_proc_exp_recurse(task) == -1) {
				printk(KERN_INFO "TimeKeeper: Trying to freeze the task fails, exiting.. \n");
                        	return HRTIMER_NORESTART;
		}
//		if (tl->force == FORCE || is_off(task) == 1 ) { //force the vt to be what you expect
		if (tl->force == FORCE || ( (get_virtual_time(task, now) - task->expected_time) > task->increment) ) { //force the vt to be what you expect
			force_virtual_time(task->linux_task, task->expected_time);
		}
	}

	startJob = 0;
	//find next task that has needs to run this round, then unfreeze it and start it's timer
        while (task->next != NULL)
        {
        	task = task->next;
                if (task->running_time > 0 && task->increment > 0 && task->stopped != -1)
                {
                        unfreeze_proc_exp_recurse(task, task->expected_time);
                        ktime = ktime_set( 0, task->running_time );
                        hrtimer_start( &task->timer, ktime, HRTIMER_MODE_REL );
                        startJob = 1;
                        break;
                }
        }
	//if no more tasks need to run, send a message to userspace letting them know
        if (startJob == 0)
        {
		int index = tl->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);
		//see if there is more work to do
		int isSet = 0;
		spin_lock(&cpuLock[index]);
		if (list_empty(&cpuWorkList[index])) {
			cpuIdle[index] = 0;
		}
		else {
			task = list_first_entry(&cpuWorkList[index], struct dilation_task_struct, cpuList);
			list_del((&cpuWorkList[index])->next); // *** Not sure. I think this moves on to the queued progress of the next timeline on the same cpu chain. This way the timelines on the same cpu chain are advanced one after the other.
			isSet = 1;
		}
		spin_unlock(&cpuLock[index]);
		if (task != NULL && isSet == 1) {  // *** Not sure
			unfreeze_proc_exp_recurse(task, task->expected_time);
      	                ktime = ktime_set( 0, task->running_time );
                        hrtimer_start( &task->timer, ktime, HRTIMER_MODE_REL );
		}

		send_a_message(tl->user_proc->pid);
	}

	*/
        return HRTIMER_NORESTART;
}

// Forces the virtual time of a task. This is a user specified option
void force_virtual_time(struct task_struct* aTask, s64 time) {
        struct list_head *list;
        struct task_struct *taskRecurse;
        struct task_struct *me;
        struct task_struct *t;

        if (aTask == NULL) {
                printk(KERN_INFO "TimeKeeper: Task does not exist\n");
                return;
        }
        if (aTask->pid == 0) {
                return;
        }

        me = aTask;
        t = me;
        //set if for all threads
        do {
			spin_lock(&t->dialation_lock);
            if (t->pid != aTask->pid) {
                t->virt_start_time = time;
                t->freeze_time = time;
                t->past_physical_time = 0;
                t->past_virtual_time = 0;
            }
			spin_unlock(&t->dialation_lock);
           } while_each_thread(me, t);

		spin_lock(&aTask->dialation_lock);
		aTask->virt_start_time = time;
    	aTask->freeze_time = time;
    	aTask->past_physical_time = 0;
    	aTask->past_virtual_time = 0;
		spin_unlock(&aTask->dialation_lock);

        list_for_each(list, &aTask->children)
        {
                taskRecurse = list_entry(list, struct task_struct, sibling);
                if (taskRecurse->pid == 0) {
                        return;
                }
                set_children_time(taskRecurse, time);
        }
}

// Wrapper function for fix_timeline, will simply extract necessary arguments
void fix_timeline_proc(char *write_buffer) {
	int timeline;
	timeline = atoi(write_buffer);
	fix_timeline(timeline);
	return;
}

// Will get called if the virtual time of a timeline gets way out of whack, will try to fix it
void fix_timeline(int timeline) {
	struct timeline* tl = doesTimelineExist(timeline);
        struct dilation_task_struct* tmp;

	if (tl != NULL) {
	        tmp = tl->head;
        	if (tmp == NULL) {
                	printk("No tasks assigned to timeline %d\n", timeline);
                	return;
        	}
		printk(KERN_INFO "TimeKeeper: Calling Fix timeline\n");
	        while (tmp != NULL) {
			force_virtual_time(tmp->linux_task, tmp->expected_time);
        	        tmp = tmp->next;
        	}
	}
	else {
		printk("Timeline %d does not exist\n", timeline);
	}
	return;
}

// An attempt to fix virtual time errors. Currently not utilized, as it did not work correctly.
int is_off(struct dilation_task_struct *task) {
	struct timeval ktv;
	s64 now;
        s64 real_running_time;
        s64 temp_past_physical_time;
        s64 dilated_running_time;
        s64 change;
        s32 rem;
	s64 expected_time = task->expected_time;
	do_gettimeofday(&ktv);
        now = timeval_to_ns(&ktv);

        real_running_time = now - task->linux_task->virt_start_time;
        temp_past_physical_time = task->linux_task->past_physical_time + (now - task->linux_task->freeze_time);
	
        if (task->linux_task->dilation_factor > 0)
        {
dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*PRECISION ,task->linux_task->dilation_factor,&rem) + task->linux_task->past_virtual_time;
                now = dilated_running_time + task->linux_task->virt_start_time;
        }
        else if (task->linux_task->dilation_factor < 0)
        {
dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*(task->linux_task->dilation_factor*-1), PRECISION, &rem) + task->linux_task->past_virtual_time;
                now =  dilated_running_time + task->linux_task->virt_start_time;
        }
        else
        {
                dilated_running_time = (real_running_time - temp_past_physical_time) + task->linux_task->past_virtual_time;
                now = dilated_running_time + task->linux_task->virt_start_time;
        }

        if (expected_time - now < 0)
        {
                if (task->linux_task->dilation_factor > 0)
                        change = div_s64_rem( ((expected_time - now)*-1)*task->linux_task->dilation_factor, PRECISION, &rem);
                else if (task->linux_task->dilation_factor < 0)
                {
                        change = div_s64_rem( ((expected_time - now)*-1)*PRECISION, task->linux_task->dilation_factor*-1,&rem);
                        change += rem;
                }
                else if (task->linux_task->dilation_factor == 0)
                        change = (expected_time - now)*-1;
                }
        else
        {
                if (task->linux_task->dilation_factor > 0)
                        change = div_s64_rem( (expected_time - now)*task->linux_task->dilation_factor, PRECISION, &rem);
                else if (task->linux_task->dilation_factor < 0)
                {
                        change = div_s64_rem((expected_time - now)*PRECISION, task->linux_task->dilation_factor*-1,&rem);
                        change += rem;
                }
                else if (task->linux_task->dilation_factor == 0)
                {
                        change = (expected_time - now);
                }
                change *= -1; //make negative
        }

        if (change > task->increment*2)
        {
		printk(KERN_INFO "TimeKeeper: %d *** resetting task with change %lld\n", task->linux_task->pid, change);
		return 1;
        }
return 0;
}
