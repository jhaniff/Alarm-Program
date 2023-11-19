
/*
 * alarm_mutex.c
 *
 * This is an enhancement to the alarm_thread.c program, which
 * created an "alarm thread" for each alarm command. This new
 * version uses a single alarm thread, which reads the next
 * entry in a list. The main thread places new requests onto the
 * list, in order of absolute expiration time. The list is
 * protected by a mutex, and the alarm thread sleeps for at
 * least 1 second, each iteration, to ensure that the main
 * thread can lock the mutex to add new work to the list.
 */

#include <pthread.h>
#include <time.h>
#include "errors.h"
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

//#define DEBUG;
/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    int alarm_id;
    time_t              time;   /* seconds from EPOCH */
    char                message[128];
    int                 time_group;
} alarm_t;

//Keeps track of thread IDs based on alarm time group number
typedef struct thread_group {
    struct thread_group *nextthread;
    pthread_t pthrid;
    int thread_group;
    int seconds;
} thread_grp;

int threadcount = 0;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;

alarm_t **head, *element, *previous, *tmp, **dalarm, *alarm2;
pthread_t display_thread;

thread_grp *thread_list = NULL;

alarm_t *display_list = NULL;

struct cancel_args
{
    int p_alarm_id;
    alarm_t *p_alarm_list;
};

struct replace_args
{
    int p_alarm_id;
    int p_seconds;
    char p_message[128];
    int p_time_group;
    time_t p_time;

    alarm_t *p_alarm_list;
};

int check_thread(int group){
    
    if(thread_list == NULL){
        return 0;
    }
    thread_grp *t;
    t = thread_list;
    while(t!=NULL){

        if(t->thread_group==group){
            return 1;
        }
        t = t->nextthread;
    }
    return 0;
}

/**
 * thread that displays each alarm
*/
/////OLD
/**void* display_alarm(void *arg){
    int status = pthread_mutex_lock(&alarm_mutex);
    if (status != 0)
        err_abort (status, "Lock mutex");
    printf("Running display thread\n");

    long group_id;
    group_id = (long) arg;

    thread_grp *t;

    t = (thread_grp*)malloc (sizeof (thread_grp));
    if (t == NULL){
        errno_abort ("Could not allocate thread list");
    }

    pthread_t p = pthread_self();

    t->pthrid = p;
    t->thread_group = (int)group_id;
    
    t = t->nextthread;
    
    //dalarm = alarm_list; //dalarm returns NULL
    
    //if alarm->timegroup == the time group number set by the thread
   
    //if(alarm2->time_group==group_id)
    //{
        printf ("(%d) %s\n", alarm2->seconds, alarm2->message);
        printf("Alarm (%d) Printed by Alarm Thread %lu > for Alarm_Time_Group_Number %d at %ld: %d %s\n", alarm2->alarm_id, (long)pthread_self(), alarm2->time_group, time(NULL), alarm2->seconds, alarm2->message);
    //}
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0)
        err_abort (status, "UnLock mutex");
  
    
    
    return 0;
}**/

void *display_alarm(void *arg){

    long groupID = (long) arg;

    //Save current thread in a list;
    thread_grp *t;
    t = (thread_grp*)malloc (sizeof (thread_grp));
    if (t == NULL){
        errno_abort ("Could not allocate thread list");
    }
    //Correct the thread ID
    //pthread_t p = pthread_self();
    #ifdef SYS_gettid
        pid_t tidDisplay = syscall(SYS_gettid);
    #else
        #error "SYS_gettid unavailable on this system"
    #endif
    //printf("Process ID is %d\n",tidDisplay);
    t->pthrid = tidDisplay;
    t->thread_group = (long)groupID;
    t = t->nextthread;

    //Loop continously checks for alarms in the alarm list
    while(1)
    {   
       int status = pthread_mutex_lock(&alarm_mutex);
       if (status != 0)
            err_abort (status, "Lock mutex");
        //check if the list is empty or not 
       if(display_list !=NULL)
        {
            //Check for alarms that match the current threads group number
            if(display_list->time_group == groupID)
            {
                printf ("(%d) %s\n", display_list->seconds, display_list->message);
                //printf("Alarm (%d) Printed by Alarm Thread %lu > for Alarm_Time_Group_Number %d at %ld: %d %s\n", display_list->alarm_id, (long)pthread_self(), display_list->time_group, time(NULL), display_list->seconds, display_list->message);
                printf("Alarm (%d) Printed by Alarm Thread %d for Alarm_Time_Group_Number %d at %ld: %d %s\n", display_list->alarm_id, tidDisplay, display_list->time_group, time(NULL), display_list->seconds, display_list->message);

                //Move list pointer
                display_list = display_list->link;
            }
            
        }
        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");
    }
    
}

/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    int sleep_time;
    time_t now;
    int status;
    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits.
     */
    while (1) {
        //printf("im in the while loop of the routine\n");
        status = pthread_mutex_lock (&alarm_mutex);
        if (status != 0)
        {
            err_abort (status, "Lock mutex");
        }
        
        //Save the current head of the alarm list
        alarm = alarm_list;
        //Check if alarm struct is not NULL
        if(alarm!=NULL)
        {
            long tg = (long)alarm->time_group;
            //Check if the alarms specific time group already has its own thread
            if(check_thread(alarm->time_group)==0){
                //printf("Creating New Thread\n");
                status = pthread_create(&display_thread, NULL, display_alarm, (void *)tg);
                if (status != 0)
                {
                    err_abort (status, "Could NOT create alarm thread");
                }
            }
        }       
        /*
         * If the alarm list is empty, wait for one second. This
         * allows the main thread to run, and read another
         * command. If the list is not empty, remove the first
         * item. Compute the number of seconds to wait -- if the
         * result is less than 0 (the time has passed), then set
         * the sleep_time to 0.
         */
        if (alarm == NULL){ 
            sleep_time = 1; 
        }
        else {
            alarm_list = alarm->link;
            now = time (NULL);
            //printf("check time\n");
            //printf("%p\n",&alarm_list);
            //printf(*last);
            //Runs of the alarms elapsed time already runs out and sets the sleep time to zero
            if (alarm->time <= now){
                sleep_time = 0;
            }
            else{
                sleep_time = alarm->time - now;   
            }
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                sleep_time, alarm->message);
#endif
        }
        /*
         * Unlock the mutex before waiting, so that the main
         * thread can lock it to insert a new alarm request. If
         * the sleep_time is 0, then call sched_yield, giving
         * the main thread a chance to run if it has been
         * readied by user input, without delaying the message
         * if there's no input.
         */
        status = pthread_mutex_unlock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");
        //Make sure sleep time is greater than zero
        if (sleep_time > 0)
            sleep (sleep_time);
        else
            sched_yield ();

        //If a timer expired, add the alarm to the display alarms list.
        if (alarm != NULL) 
        {
            display_list = alarm;
            //free (alarm);
        }
    }
}


void* replace_alarm(void *input)
{   
    //Lock mutex
    int status = pthread_mutex_lock(&alarm_mutex);
    if (status != 0)
    {
        err_abort (status, "Lock mutex");
    }
    //printf("Running Replace_Alarm function\n");

    //Save the variables for the replacing alarm
    element = ((struct replace_args*)input)->p_alarm_list;
    
            //Loop ends if the alarm is NULL
        if(element == NULL)
        {
            printf("Requested Alarm ID to be Replaced is not found or It is currently Running!\n");
            status = pthread_mutex_unlock(&alarm_mutex);
            if (status != 0)
            {
                err_abort (status, "UnLock mutex");
            	
            }
            return 0;
        }


    int rpl_alarm_id = ((struct replace_args*)input)->p_alarm_id;
    int rpl_seconds = ((struct replace_args*)input)->p_seconds;
    char rpl_message[128];
    strcpy(rpl_message,((struct replace_args*)input)->p_message);
    int rpl_timegroup = ((struct replace_args*)input)->p_time_group;
    time_t rpl_time = ((struct replace_args*)input)->p_time;

    //Loop runs to check for the alarm to be replaced
    while(1)
    {
        //Loop ends if the alarm is NULL
        if(element == NULL)
        {
            printf("Requested Alarm ID to be Replaced is not found!\n");
            status = pthread_mutex_unlock(&alarm_mutex);
            if (status != 0)
            {
                err_abort (status, "UnLock mutex");
            	
            }
            return 0;
        }

        //Find matching alarm IDs to replace the alarm
        if(element->alarm_id == rpl_alarm_id)
        {
            element->seconds = rpl_seconds;
            strcpy(element->message,rpl_message);
            element->alarm_id = rpl_alarm_id;
            element->time_group = rpl_timegroup;
            element->time = rpl_time;

            printf("Alarm (%d) Replaced at <%ld>: %d %s.\n", element->alarm_id,time(NULL), element->seconds, element->message);
            status = pthread_mutex_unlock(&alarm_mutex);
            if (status != 0)
                err_abort (status, "UnLock mutex");
            return 0;
        }
        
        //Traverse list
        element = element->link;
    }
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0)
        err_abort (status, "UnLock mutex");
    return 0;
}

void* cancel_alarm(void *input)
{
    int status = pthread_mutex_lock(&alarm_mutex);
    if (status != 0)
        err_abort (status, "UnLock mutex");
    
    //Save head of the alarm list
    head = &alarm_list;
    element = ((struct cancel_args*)input)->p_alarm_list;
    tmp     = ((struct cancel_args*)input)->p_alarm_list;

    //Initialize the alarm ID to be deleted
    int del_alarm_id = ((struct cancel_args*)input)->p_alarm_id;
    alarm_t *del_alarm_list = ((struct cancel_args*)input)->p_alarm_list;
    
    //Deletes element if it is the only one in the list
    if(tmp != NULL && tmp->link == NULL && tmp->alarm_id == del_alarm_id){
        printf("Alarm(%d) Canceled at %ld: %d %s.\n", del_alarm_id,time(NULL),alarm_list->seconds,alarm_list->message);
        alarm_list = NULL;
        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0)
            err_abort (status, "UnLock mutex");
        return 0;
    }

    //Check if head element has to be deleted
    if(tmp!=NULL && tmp->alarm_id == del_alarm_id)
    {
        *head = tmp->link;
        free(tmp);
        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0)
            err_abort (status, "UnLock mutex");
        return 0;
    }

    //Traverse through list while checking for matching alarm ID
    while(tmp!=NULL && tmp->alarm_id != del_alarm_id)
    {
        previous = tmp;
        tmp = tmp->link;
    }
    //Break the loop if the alarm cannot be found
    if(tmp==NULL)
    {
        printf("Requested Alarm ID to be Cancelled is not found!\n");
        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0)
            err_abort (status, "UnLock mutex");
        return 0;
    }

    printf("Alarm(%d) Canceled at %ld: %d %s.\n", del_alarm_id,time(NULL),tmp->seconds,tmp->message);

    //Remove the links to the deleted alarm and free it from memory
    previous->link = tmp->link;
    free(tmp);

    element = ((struct cancel_args*)input)->p_alarm_list;
    
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0)
        err_abort (status, "UnLock mutex");
    return 0;

}
int main (int argc, char *argv[])
{
    fflush(stdout);
    int status;
    char line[1000];
    char ar[128];
    int alarm_id;
    char request[128];
    alarm_t *alarm, **last, *next;
    pthread_t thread;
    int a = 0;

    #ifdef SYS_gettid
        pid_t tidMain = syscall(SYS_gettid);
    #else
        #error "SYS_gettid unavailable on this system"
    #endif
    //printf("Process ID is %d\n",tidMain);

    //Create alarm thread
    status = pthread_create (&thread, NULL, alarm_thread, NULL);
    
    if (status != 0)
        err_abort (status, "Create alarm thread");
    //Loop to run the program    
    while (1) {
        printf ("alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL){
            exit (0);
        } 
        if (strlen (line) <= 1){
            continue;
        } 
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL){
            errno_abort ("Allocate alarm");
        }
        
        /*
         * Parse input line into seconds (%d) and a message
         * (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.
         */

        //Scanning the input line, parsing each pararmeter and storing it into its 
        //respective variables
        if (sscanf (line, "%s %d %128[^\n]", ar, &alarm->seconds, alarm->message) < 3) 
        {
            //Validate input values for alarm_id, keyword, seconds
            sscanf(ar, "%[^()]",request);
            int x = sscanf(ar, "%*[^\(](%d[^)])",&alarm_id);
            //printf("%s(%d)\n", request, alarm_id);
            
            //Validates the cancel_alarm command
            //x is the output from sscanf to check if the value in brackets is a digit
            if(x != 1 || alarm_id < 1 || (alarm->seconds < 0)) { 
                fprintf (stderr, "This is a Bad command, one of the input values is not correct\n");
            }
            else
            {
                //Validates cancel_alarm, makes sure that it has the right number of inputs
                
                //if(sscanf (line, "%s %d %128[^\n]", ar, &alarm->seconds, alarm->message)!=1 || strcasecmp("cancel_alarm",request)!=0)
                if(sscanf (line, "%s %d %128[^\n]", ar, &alarm->seconds, alarm->message)!=1 || strcmp("Cancel_Alarm",request)!=0)
                {
                    fprintf (stderr, "1- This is a Bad command\n");
                    free (alarm);
                }
                else{
                    //Save the paramters for cancel_alarm into a struct
                    struct cancel_args *cancelA = (struct cancel_args *)malloc(sizeof(struct cancel_args));
                    cancelA->p_alarm_id = alarm_id;
                    cancelA->p_alarm_list = alarm_list;

                    //Creates a new thread for cancel_alarm function
                    status = pthread_create(&thread, NULL, &cancel_alarm,(void *)cancelA);

                    //Checks if thread is created without errors
                    if(status!=0){
                        err_abort(status, "Could not create thread");
                    }
                    
                }
        }
    }
        else {
            //Validate input values for start_alarm and replace_alarm
            sscanf(ar, "%[^()]",request);
            int x = sscanf(ar, "%*[^\(](%d[^)])",&alarm_id);
            
            //x is the output from sscanf to check if the value in brackets is a digit 
            if(x!=1 || alarm_id < 1 || (alarm->seconds < 0)){
                fprintf (stderr, "This is a Bad command, one of the input values is not correct\n");
                //free (alarm);
            }
            
            else {
                //Calculate the alarm time group number
                int b = 5;
                alarm->time_group =  (alarm->seconds + b - 1) / b;
                //Checks if input keyword is 'start_alarm', not case-sensitive
                
                //if(strcasecmp("start_alarm",request)==0){
                if(strcmp("Start_Alarm",request)==0){
                //printf("Running Start_Alarm\n");

                //Save the alarm ID into the alarm struct
                alarm->alarm_id = alarm_id;

                //Create mutex lock for alarm_mutex
                status = pthread_mutex_lock (&alarm_mutex);
                
                //Makes sure that the mutex is locked without errors
                if (status != 0){
                    err_abort (status, "Locking mutex");
                }
                //Calculates the alarm time from EPOCH + alarm time(seconds)
                alarm->time = time (NULL) + alarm->seconds;
                /*
                * Insert the new alarm into the list of alarms,
                * sorted by alarm ID.
                * 
                * Places the newly created alarm at the beginning of the alarm list
                */
                last = &alarm_list;
                next = *last;    
                while (next != NULL) {
                    if (next->alarm_id >= alarm->alarm_id) {
                        alarm->link = next;
                        *last = alarm;
                        break;
                    }
                    last = &next->link;
                    next = next->link;
                }
                /*
                * If we reached the end of the list, insert the new
                * alarm there. ("next" is NULL, and "last" points
                * to the link field of the last item, or to the
                * list header).
                */
                if (next == NULL) {
                    *last = alarm;
                    alarm->link = NULL;
                    
                }
                //printf("Alarm(%d) Inserted by Main Thread %lu Into Alarm List at %ld: %d %s\n", alarm->alarm_id, (long)pthread_self(), alarm->time, alarm->seconds, alarm->message);
                printf("Alarm(%d) Inserted by Main Thread %d Into Alarm List at %ld: %d %s\n", alarm->alarm_id, tidMain, alarm->time, alarm->seconds, alarm->message);
            }
            else 
          	//if(strcasecmp("replace_alarm",request)==0){
            if(strcmp("Replace_Alarm",request)==0){
                //Initialize the parameters for the replace_alarm method
                struct replace_args *replaceA = (struct replace_args *)malloc(sizeof(struct replace_args));
                replaceA->p_alarm_id = alarm_id;
                replaceA->p_alarm_list = alarm_list;
                replaceA->p_seconds = alarm->seconds;
                replaceA->p_time = alarm->time;
                replaceA->p_time_group = alarm->time_group;
                strcpy(replaceA->p_message,alarm->message);

                status = pthread_create(&thread, NULL, &replace_alarm,(void *)replaceA);
                if(status!=0){
                    err_abort(status, "Could not create thread");
                }
            }
            else{
                //Free the alarm from memory if the input is invalid
                fprintf (stderr, "2- This is a Bad command\n");
                free (alarm);
            }
        }
#ifdef DEBUG
            printf ("[list: ");
            for (next = alarm_list; next != NULL; next = next->link)
                printf ("%d(%d)[\"%s\"] ", next->alarm_id, next->time - time (NULL), next->message);
            printf ("]\n");
            
#endif
            status = pthread_mutex_unlock (&alarm_mutex);
            if (status != 0)
            {
                err_abort (status, "Unlock mutex");
            }
        }
    }
}
