/*
 * New_Alarm_Mutex.c
 *
 * This is an enhancement to the alarm_mutex.c program. This new
 * version uses a single alarm thread, which reads the next
 * entry in a list. The main thread places new requests onto the
 * list, in order of alarm id time. The list is protected by a mutex,
 * and the alarm thread sleeps for at least 1 second, each iteration,
 * to ensure that the main thread can lock the mutex to add new work
 * to the list.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"

/*
 * The "alarm" structure now contains the req_type "Start_Alarm" &
 * "Change_alarm" inputs from the user. A unique alarm_id has also
 * been added to allow alarms to be accessed and changed by id as
 * well as added to the list in order of id.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;           /* seconds from EPOCH */
    char                message[128];
    char                req_type[20];   // for the type of request
    int                 alarm_id;       // alarm id
    int			changed;	// boolean for change
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;

// Condition checks for display thread(s) and when they run.
pthread_cond_t d_cond = PTHREAD_MUTEX_INITIALIZER;

// Alarm list
alarm_t *alarm_list = NULL;

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
     * be disintegrated when the process exits. (From alarm_mutex.c)
     */
    while (1) {
        status = pthread_mutex_lock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Lock mutex");
        alarm = alarm_list;

        /*
         * If the alarm list is empty, wait for one second. This
         * allows the main thread to run, and read another
         * command. If the list is not empty, remove the first
         * item. Compute the number of seconds to wait -- if the
         * result is less than 0 (the time has passed), then set
         * the sleep_time to 0. (From alarm_mutex.c)
         */
        if (alarm == NULL)
            sleep_time = 1;
        else {
            alarm_list = alarm->link;
            now = time (NULL);
            if (alarm->time <= now)
                sleep_time = 0;
            else
                sleep_time = alarm->time - now;
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
         * if there's no input. (From alarm_mutex.c)
         */
        status = pthread_mutex_unlock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");
        if (sleep_time > 0)
            sleep (sleep_time);
        else
            sched_yield ();

        /*
         * If a timer expired, print the time of expriation, req_type,
         * alarm_id, alarm seconds & the message as well as and free the
         * alarm structure.
         */
        if (alarm != NULL) {
            printf ("Alarm(%d): Alarm Expired at %d: Alarm Removed From Alarm List\n", alarm->alarm_id, time(NULL));
            printf ("req_type is: %s\n", alarm->req_type);
            printf("alarm_id: %d\n", alarm->alarm_id);
            printf("alarm seconds: %d\n", alarm->seconds);
            printf("alarm message: %s\n", alarm->message);
            free (alarm);
        }
    }
}

void *display_thread(void *arg)
{
	int status;
	alarm_t *alarm;
	
	/*
	* Loop forever, processing commands. The alarm thread will
	* be disintegrated when the process exits. (From alarm_mutex.c) Similar to alarm_thread.
	*/
	while(1)
	{
	status = pthread_mutex_lock(alarm_mutex);
	if (status != 0)
		err_abort(status, "Lock mutex");

	// Thread proceeds once a signal has been raised from the alarm thread.
	status = pthread_cond_wait(&d_cond, &alarm_mutex);
	if (status != 0)
		err_abort(status, "Wait on cond");


	// Display message loop which lasts until expiration
	// If-else clause to check if an alarm in order to print something else.
	while(alarm->time > time (NULL))
	{
	if (alarm->Changed == 0) {
		printf(alarm->message);
		sleep(5);
	} else {
		printf("Display Thread (%d) Has Started to Print Changed Message at %d: %s", alarm->alarm_id, , alarm->message);
		alarm->Changed = 0;
		sleep(5);	
	}
	printf("Alarm (%d) Expired; Display Thread (%d) Stopped Printing Alarm Message at %d: %s.", alarm->alarm_id, , time(NULL), alarm->message);
	if (status != 0)
		err_abort(status, "Unlock mutex");
	free(alarm);
	}

}

int main (int argc, char *argv[])
{
    int status;
    char line[128];
    alarm_t *alarm, **last, *next;
    pthread_t thread;
    // Use temp variables to store user input
    int tid;
    char treq[20];
    char tmessage[128];
    int tsec;

    status = pthread_create (
            &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
    while (1) {
        printf ("alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");

        /*
         * Parse input line into alarm request (%20[^(]), alarm id (&d),
         * seconds (%d), and a message (%128[^\n]),
         * consisting of up to 128 characters.
         */
        // if (sscanf (line, "%20[^(](%d): %d %128[^\n]", alarm->req_type, &alarm->alarm_id,
        //     &alarm->seconds, alarm->message) < 4)
        if (sscanf (line, "%20[^(](%d): %d %128[^\n]", treq, &tid, &tsec, tmessage) < 4)
        {
            fprintf (stderr, "Bad command\n");
            free (alarm);
        }
        else if (strcmp(treq, "Change_Alarm") == 0)
        {
            // fprintf (stderr, "Good start of Change_Alarm command :)\n");
            status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");
            /*
                Make change to existing alarm
                Alarm must with id "alarm_id" must exist to be able to make changes
            */
            int exist = 0;
            last = &alarm_list;
            next = *last;
            while (next != NULL)
            {
                if (tid == next->alarm_id)
                {
                    exist = 1;

                    printf("ID found\n");

                    next->seconds = tsec;
                    next->time = time (NULL) + next->seconds;
                    alarm->alarm_id = tid;
                    memcpy(next->req_type, treq, sizeof next->req_type);
                    memcpy(next->message, tmessage, sizeof next->message);
		    next->changed = 1;

                    printf("Alarm(%d) Changed at %d: %d %s\n", next->alarm_id, time(NULL), next->seconds, next->message);

                    break;
                }
                last = &next->link;
                next = next->link;
            }
            // if reach end of list and alarm_id does not match then request not valid
            if (exist == 0)
            {
                fprintf (stderr, "Bad command, Alarm ID not found\n");
                printf("alarm id: %d\n", alarm->alarm_id);
                printf("tid: %d\n", tid);
                free (alarm);
            }
            // debudding statements
            printf("&alarm->id: %d\n", &alarm->alarm_id);
            printf("alarm->id: %d\n", alarm->alarm_id);
            printf("&tid: %d\n", &tid);
            printf("tid: %d\n", tid);
#ifdef DEBUG
            printf ("[list: ");
            for (next = alarm_list; next != NULL; next = next->link)
                printf ("%d(%d)[\"%s\"] ", next->time,
                    next->time - time (NULL), next->message);
            printf ("]\n");
#endif
            status = pthread_mutex_unlock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock mutex");
        }
        else if (strcmp(treq, "Start_Alarm") == 0)    // Start_Alarm request
        {
            status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");
            // Assign temp variables to the new alarm
            alarm->seconds = tsec;
            alarm->time = time (NULL) + alarm->seconds;
            alarm->alarm_id = tid;
            memcpy(alarm->req_type, treq, sizeof alarm->req_type);
            memcpy(alarm->message, tmessage, sizeof alarm->message);
	    alarm->changed = 0;

            /*
             * Insert the new alarm into the list of alarms,
             * sorted by expiration time.
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
            // Print out message that new alarm was added
            printf("Alarm(%d) Inserted by Main Thread(%u) ", alarm->alarm_id, pthread_self());
            printf("Into Alarm List at %d: %d %s\n", time(NULL), alarm->seconds, alarm->message);
#ifdef DEBUG
            printf ("[list: ");
            for (next = alarm_list; next != NULL; next = next->link)
                printf ("%d(%d)[\"%s\"] ", next->time,
                    next->time - time (NULL), next->message);
            printf ("]\n");
#endif
            status = pthread_mutex_unlock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock mutex");
        } else {
            fprintf (stderr, "Bad command, invalid Alarm Request\n");
            free (alarm);
        }
    }
}
