
/** @file libscheduler.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libscheduler.h"
#include "../libpriqueue/libpriqueue.h"


/**
   Stores information making up a job to be scheduled including any statistics.
  You may need to define some global variables or a struct to store your job queue elements.
**/

typedef struct _job_t
{
	int pid;
	int priority;
	int arrivalTime;
	int latencyTime;
	int runningTime;
	int remainingTime;
	int turnAroundTime;
	int endTime;
	bool processing;
	int core_id;
	int last_scheduled;
	int first_scheduled;


} job_t;

typedef struct _core_t
{
	int core_id;
	int current_job_id;
	job_t* current_job;
} core_t;

priqueue_t *jobQ;
priqueue_t *finished_jobs; //queue of jobs that are done

scheme_t schemeType;

int numOfCores;

core_t** availCores_array;

int compareFCFS(const void* jobA, const void* jobB){
	return(((job_t*)jobA)->pid - ((job_t*)jobB)->pid);
}

int compareSJF(const void* jobA, const void* jobB){
	if(((job_t*)jobA)->processing){
		return 1;
	}
	else if(((job_t*)jobA)->runningTime == ((job_t*)jobB)->runningTime){
		return(((job_t*)jobA)->pid - ((job_t*)jobB)->pid);
	}
	else{
		return(((job_t*)jobA)->runningTime - ((job_t*)jobB)->runningTime);
	}
}

int comparePSJF(const void* jobA, const void* jobB){
	return (((job_t*)jobA)->remainingTime - ((job_t*)jobB)->remainingTime);
}

int comparePRI(const void* jobA, const void* jobB){
	if(((job_t*)jobA)->processing){
		return 1;
	}
	else if(((job_t*)jobA)->priority == ((job_t*)jobB)->priority){
                return(((job_t*)jobA)->pid - ((job_t*)jobB)->pid);
        }
	else{
		return (((job_t*)jobA)->priority - ((job_t*)jobB)->priority);
	}
}

int comparePPRI(const void* jobA, const void* jobB){
	return (((job_t*)jobA)->priority - ((job_t*)jobB)->priority);
}

int compareRR(const void* a, const void* b){
	return 1;
}




/**
  Initalizes the scheduler.

  Assumptions:
    - You may assume this will be the first scheduler function called.
    - You may assume this function will be called once once.
    - You may assume that cores is a positive, non-zero number.
    - You may assume that scheme is a valid scheduling scheme.

  @param cores the number of cores that is available by the scheduler. These cores will be known as core(id=0), core(id=1), ..., core(id=cores-1).
  @param scheme  the scheduling scheme that should be used. This value will be one of the six enum values of scheme_t
*/
void scheduler_start_up(int cores, scheme_t scheme)
{
	numOfCores = cores;
	availCores_array = malloc(cores*sizeof(job_t));

	for(int i=0;i<numOfCores;i++)
	{
		core_t* new = malloc(sizeof(core_t*));//init array of cores
		new->core_id = i;
		new -> current_job_id = -1; //-1 if no job on the core
		new -> current_job = NULL; //no current job yet :)
		availCores_array[i] = new;
	}

	schemeType = scheme;
	jobQ = (priqueue_t*)malloc(sizeof(priqueue_t));

	switch(schemeType){
		case FCFS:
			priqueue_init(jobQ, compareFCFS);
			break;
		case SJF:
			priqueue_init(jobQ, compareSJF);
			break;
		case PSJF:
			priqueue_init(jobQ, compareSJF);
			break;
		case PRI:
			priqueue_init(jobQ, compareSJF);
			break;
		case PPRI:
			priqueue_init(jobQ, compareSJF);
			break;
		case RR:
			priqueue_init(jobQ, compareSJF);
			break;
		default:
			printf("error");
		break;
	}


	finished_jobs = malloc(sizeof(priqueue_t));
//	priqueue_init(jobQ, compareSJF);
	priqueue_init(finished_jobs, compareFCFS);

}


/**
  Called when a new job arrives.

  If multiple cores are idle, the job should be assigned to the core with the
  lowest id.
  If the job arriving should be scheduled to run during the next
  time cycle, return the zero-based index of the core the job should be
  scheduled on. If another job is already running on the core specified,
  this will preempt the currently running job.
  Assumptions:
    - You may assume that every job wil have a unique arrival time.

  @param job_number a globally unique identification number of the job arriving.
  @param time the current time of the simulator.
  @param running_time the total number of time units this job will run before it will be finished.
  @param priority the priority of the job. (The lower the value, the higher the priority.)
  @return index of core job should be scheduled on
  @return -1 if no scheduling changes should be made.

 */
int scheduler_new_job(int job_number, int time, int running_time, int priority)
{

	//Create new job and initialize values
	job_t* new_job = malloc(sizeof(job_t));
	new_job->pid = job_number;
	new_job->arrivalTime = time;
	new_job->runningTime = running_time;
	new_job->priority = priority;
	new_job->last_scheduled = -1;
	new_job->first_scheduled = -1;
	new_job->turnAroundTime = running_time;

	//find core
	for(int i=0; i<numOfCores; i++)//cycle through core array
	{

		if( availCores_array[i]->current_job_id == -1) //no current job
		{
			//find unused core and schedule a job on it
			availCores_array[i]->current_job_id = job_number;
			availCores_array[i]->current_job = new_job;
			new_job->core_id = i;
			new_job->latencyTime = -1; //no latency time yet
			new_job->first_scheduled = time;
			new_job->last_scheduled = time;


			return(i);
		}
	}
	//if core is not found, try to preemt one
	if(schemeType==PPRI || schemeType==PSJF)
	{
		//can preempt
		//update running time so compaisons can be made for the PJSF scheme
		for (int i=0;i<numOfCores;i++)
		{
			job_t* current = availCores_array[i]->current_job;
			current->turnAroundTime = current->turnAroundTime + (time - current->last_scheduled);
			current->last_scheduled = time;
		}

		int lowest_priority_index=0; //index of job with the lowest priority so that
								//its core can be preempted by the new job
								//ONLY ON MULTICORE SYSTEMS!! if there is only
								//one core, we don't have other core to check.


		job_t* lowest_pri_job = availCores_array[lowest_priority_index]->current_job;
		job_t* current_job_on_core = availCores_array[0]->current_job;

		for(int i=0;i<numOfCores;i++)
		{
			current_job_on_core = availCores_array[i]->current_job;
			if(0<compareSJF(new_job, current_job_on_core))
			{
				lowest_priority_index=i;
			}
		}

		job_t* curr = availCores_array[lowest_priority_index]->current_job;

		if(0>compareSJF(new_job, current_job_on_core))
		{
			curr->core_id = -1;
			priqueue_offer(jobQ, curr);
			if(curr->first_scheduled == time)
			{
				//reset first scheduled time because this job didn't get to run before being kicked off core
				curr->first_scheduled=-1;
			}

			availCores_array[lowest_priority_index]->current_job=new_job;
			new_job->core_id = lowest_priority_index;
			new_job->last_scheduled=time;
			new_job->first_scheduled=time;
			//new_job->arrivalTime=time;
			availCores_array[lowest_priority_index]->current_job_id = job_number;
			return(lowest_priority_index);
		}

		//couln't get scheduled so add to queue
		priqueue_offer(jobQ, new_job);
		return(-1);

	}
	else
	{
		priqueue_offer(jobQ, new_job);
		return(-1);
	}

	return -1;
}


/**
  Called when a job has completed execution.

  The core_id, job_number and time parameters are provided for convenience. You may be able to calculate the values with your own data structure.
  If any job should be scheduled to run on the core free'd up by the
  finished job, return the job_number of the job that should be scheduled to
  run on core core_id.

  @param core_id the zero-based index of the core where the job was located.
  @param job_number a globally unique identification number of the job.
  @param time the current time of the simulator.
  @return job_number of the job that should be scheduled to run on core core_id
  @return -1 if core should remain idle.
 */
int scheduler_job_finished(int core_id, int job_number, int time)
{
	job_t* job_done = availCores_array[core_id]->current_job;

	job_done->endTime = time;
	job_done->core_id = -1; // done being scheduled
	job_done->turnAroundTime = (job_done->endTime)-(job_done->arrivalTime); //total time the job ran
	job_done->last_scheduled = time;

	priqueue_offer(finished_jobs, job_done);//add finished job to finished jobs queue

	//reset core values for newly free core
	availCores_array[core_id]->current_job=NULL;
	availCores_array[core_id]->current_job_id=-1;

	//are there any jobs waiting in the jobQ?
	if(priqueue_peek(jobQ)==NULL) //NO
	{
		return -1;
	}
	else //YES
	{
		job_t* temp = priqueue_poll(jobQ);
		temp->core_id = core_id;
		temp->last_scheduled = time; //job is scheduled

		//update core array
		availCores_array[core_id]->current_job_id = temp->pid;
		availCores_array[core_id]->current_job = temp;

		if(temp->first_scheduled == -1)
		{
			temp->first_scheduled=time;
		}
		return(temp->pid);

	}


	return -1;
}


/**
  When the scheme is set to RR, called when the quantum timer has expired
  on a core.

  If any job should be scheduled to run on the core free'd up by
  the quantum expiration, return the job_number of the job that should be
  scheduled to run on core core_id.

  @param core_id the zero-based index of the core where the quantum has expired.
  @param time the current time of the simulator.
  @return job_number of the job that should be scheduled on core cord_id
  @return -1 if core should remain idle
 */
int scheduler_quantum_expired(int core_id, int time)
{
	//check if queue is empty
	if(priqueue_peek(jobQ)==NULL && availCores_array[core_id]->current_job_id==-1)
	{
		return -1;
	}
	else if(priqueue_peek(jobQ)==NULL && availCores_array[core_id]->current_job_id!=-1)
	{
		return(availCores_array[core_id]->current_job_id);
	}
	else
	{
		job_t* prev_job = availCores_array[core_id]->current_job;
		availCores_array[core_id]->current_job = NULL;
		availCores_array[core_id]->current_job_id = -1;

		prev_job->turnAroundTime = prev_job->turnAroundTime + (time - prev_job->last_scheduled);
		prev_job->last_scheduled = time;

		priqueue_offer(jobQ,prev_job);

		job_t* next_job = priqueue_poll(jobQ);

		availCores_array[core_id]->current_job = next_job;
		availCores_array[core_id]->current_job_id = next_job->pid;

		if(next_job->first_scheduled==-1)
		{
			next_job->first_scheduled=time;
		}

		return(next_job->pid);
	}



	return -1;
}


/**
  Returns the average waiting time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average waiting time of all jobs scheduled.
 */
float scheduler_average_waiting_time()
{
	int num_jobs = 0;
	int waiting_times = 0;
	float avg = 0;

	num_jobs = priqueue_size(finished_jobs); //number of jobs in finished_jobs queue
	for(int i=0;i<num_jobs;i++)
	{
		job_t* temp = priqueue_at(finished_jobs, i); //gets job from finished job queue

		waiting_times = waiting_times + (temp->endTime - temp->arrivalTime - temp->runningTime);

	}

	avg = (float)waiting_times/(float)num_jobs;

	return avg;
}


/**
  Returns the average turnaround time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average turnaround time of all jobs scheduled.
 */
float scheduler_average_turnaround_time()
{
	int num_jobs = 0;
	int waiting_times = 0;
	float avg = 0;

	num_jobs = priqueue_size(finished_jobs); //number of jobs in finished_jobs queue
	for(int i=0;i<num_jobs;i++)
	{
		job_t* temp = priqueue_at(finished_jobs, i); //gets job from finished job queue

		waiting_times = waiting_times + (temp->endTime - temp->arrivalTime);

	}

	avg = (float)waiting_times/(float)num_jobs;

	return avg;
}


/**
  Returns the average response time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average response time of all jobs scheduled.
 */
float scheduler_average_response_time()
{
	int num_jobs = 0;
	int waiting_times = 0;
	float avg = 0;

	num_jobs = priqueue_size(finished_jobs); //number of jobs in finished_jobs queue
	for(int i=0;i<num_jobs;i++)
	{
		job_t* temp = priqueue_at(finished_jobs, i); //gets job from finished job queue

		waiting_times = waiting_times + (temp->first_scheduled - temp->arrivalTime);

	}

	avg = (float)waiting_times/(float)num_jobs;

	return avg;
}


/**
  Free any memory associated with your scheduler.

  Assumptions:
    - This function will be the last function called in your library.
*/
void scheduler_clean_up()
{
	//all jobs in jobQ have been taken care of, therefore it should already be empty.
	priqueue_destroy(jobQ);
	free(jobQ);

	int s = finished_jobs->size;

	for (int i;i<s;i++)
	{
		//frees all jobs in the finished jobs queue
		free(priqueue_poll(finished_jobs));
	}
	//destroy empty finsished jobs queue
	priqueue_destroy(finished_jobs);
	free(finished_jobs);


	//free cores
	for(int i=0;i<numOfCores;i++)
	{
		free(availCores_array[i]);
	}
	free(availCores_array);
}


/**
  This function may print out any debugging information you choose. This
  function will be called by the simulator after every call the simulator
  makes to your scheduler.
  In our provided output, we have implemented this function to list the jobs in the order they are to be scheduled. Furthermore, we have also listed the current state of the job (either running on a given core or idle). For example, if we have a non-preemptive algorithm and job(id=4) has began running, job(id=2) arrives with a higher priority, and job(id=1) arrives with a lower priority, the output in our sample output will be:

    2(-1) 4(0) 1(-1)

  This function is not required and will not be graded. You may leave it
  blank if you do not find it useful.
 */
void scheduler_show_queue()
{
	//OPTIONAL!
}
