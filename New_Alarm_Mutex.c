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
    time_t              time;           // seconds from EPOCH
    int                 seconds;        // n seconds from the user
    int			        changed;        // boolean for change
    int                 alarm_id;       // alarm id
    char                message[128];   // alarm message from the user
    char                req_type[20];   // for the type of request

} alarm_t;

typedef struct display_tag {

    struct display_tag *link;
    time_t              creation_time;      // Creation seconds from EPOCH
    int                 thread_id;          // same as alarm_id
    char                time_message[128];  // message

} display_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
// Condition checks for display thread(s) and when they run.
pthread_cond_t d_cond = PTHREAD_COND_INITIALIZER;
// Alarm list initialization
alarm_t *alarm_list = NULL;
// Display list initialization
display_t *display_list = NULL;
// Global pointers
alarm_t **globalLast;
alarm_t *globalNext;

/************************* The display thread's start routine *************************/
void *display_thread(void *arg) {
    int status;
    alarm_t *alarm;
    display_t *display;
    /*
    * Loop forever, processing commands. The display thread will
    * be disintegrated when the process exits. (From alarm_mutex.c) Similar to alarm_thread.
    */
    while (1) {
        alarm = alarm_list;
        display = display_list;

        status = pthread_mutex_lock(&alarm_mutex);
        if (status != 0)
            err_abort(status, "Lock mutex");

        // Thread proceeds once a signal has been raised from the alarm thread.
        status = pthread_cond_wait(&d_cond, &alarm_mutex);
        if (status != 0)
            err_abort(status, "Wait on cond");

        // 3.3.3
        // If there are no more alarms in the display thread, terminate the d. thread.
        if (alarm == NULL) {
            printf("Display Thread Terminated (%s) at %d", display->thread_id, time(NULL));
            pthread_exit(NULL);
        } else {

            /*
            * Display message loop which lasts until expiration
                * If-else clause to check if an alarm in order to print something else.
            * Once expiration is reached, print display thead and expiration time.
            */

            if (alarm->time <= time(NULL)) {
                // 3.3.2
                printf("Alarm (%d) Expired; Display Thread (%d) Stopped Printing Alarm Message at %d: %s.",
                       alarm->alarm_id, display->thread_id, time(NULL), alarm->message);
                if (status != 0)
                    err_abort(status, "Unlock mutex");
                free(alarm);
            } else {
                while (alarm->time > time(NULL)) {
                    if (alarm->changed == 0) {
                        // 3.3.4
                        printf("%s", alarm->message);
                        sleep(5);
                    } else {
                        // 3.3.1
                        printf("Display Thread (%d) Has Started to Print Changed Message at %d: %s", display->thread_id,
                               alarm->time, alarm->message);
                        alarm->changed = 0;
                        sleep(5);
                    }
                }
            }
        }
    }
}


/************************* The alarm thread's start routine *************************/
void *alarm_thread (void *arg)
{
    time_t now;
    alarm_t *alarm;
    display_t *display;
    pthread_t thread;

    int status;
    int sleep_time;
    int thread_id = 1;
    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits. (From alarm_mutex.c)
     */
    while (1) {
        status = pthread_mutex_lock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Lock mutex");
        alarm = alarm_list;
        display = display_list;

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
            globalLast = &alarm_list;
            globalNext = *globalLast;
            alarm_list = alarm->link;
            now = time(NULL);
            if (alarm->time <= now)
                sleep_time = 0;
            else
                sleep_time = alarm->time - now;
            /* A.3.3.2
             * Check if display list is empty, and create a new display thread
             * if it is. Print the creation message and add the new display
             * thread to the display list.
             */

            if (display == NULL) {

                display = (display_t *) malloc(sizeof(display_t));
                display->link = NULL;
                display->thread_id = 1;
                display->creation_time = time(NULL);
                display_list = display;

                status = pthread_create(&thread, NULL, display_thread, NULL);
                if (status != 0) {
                    err_abort(status, "Create display thread");
                }

                printf("New Display Thread (%d) Created at %d \n", thread_id, display->creation_time);
                thread_id++;

            } else {

                alarm->changed = 1;
                pthread_mutex_lock(&alarm_mutex);
                pthread_cond_signal(&d_cond);
                pthread_mutex_unlock(&alarm_mutex);
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
         * If a timer expired, print the time of expiration, req_type,
         * alarm_id, alarm seconds & the message as well as and free the
         * alarm structure.
         */
        if (alarm != NULL) {
            printf ("Alarm(%d): Alarm Expired at %d: Alarm Removed From Alarm List\n", alarm->alarm_id, time(NULL));
            /* Testing and Debugging
            printf ("req_type is: %s\n", alarm->req_type);
            printf("alarm_id: %d\n", alarm->alarm_id);
            printf("alarm seconds: %d\n", alarm->seconds);
            printf("alarm message: %s\n", alarm->message);
            */
            free (alarm);
        }
    }
}

/************************* The main thread's start routine *************************/
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
        /*
         * Parse input line into alarm request (%20[^(]), alarm id (&d),
         * seconds (%d), and a message (%128[^\n]),
         * consisting of up to 128 characters.
         */
        if (sscanf (line, "%20[^(](%d): %d %128[^\n]", treq, &tid, &tsec, tmessage) < 4) {
            fprintf (stderr, "Bad command\n");

        } else if (strcmp(treq, "Start_Alarm") == 0) { // Start_Alarm request

            status = pthread_mutex_lock (&alarm_mutex);
            printf("Status of mutex lock: %d \n", status);
            if (status != 0)
                err_abort (status, "Lock mutex");
            //Allocate memory for a new alarm
            alarm = (alarm_t*)malloc (sizeof (alarm_t));
            if (alarm == NULL)
                errno_abort ("Allocate alarm");
            // Assign temp variables to the new alarm
            alarm->seconds = tsec;
            alarm->time = time (NULL) + alarm->seconds;
            alarm->alarm_id = tid;
            memcpy(alarm->req_type, treq, sizeof alarm->req_type);
            memcpy(alarm->message, tmessage, sizeof alarm->message);
            /*
             * Insert the new alarm into the list of alarms,
             * sorted by alarm id.
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
                printf ("<%d>%d(%d)[\"%s\"] ", next->alarm_id, next->time, next->time - time (NULL), next->message);
            printf ("]\n");
#endif
            status = pthread_mutex_unlock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock mutex");

        } else if (strcmp(treq, "Change_Alarm") == 0) {

            status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");

            int exist = 0;

            while (globalNext != NULL)
            {
                if (tid == globalNext->alarm_id)
                {
                    exist = 1;

                    printf("ID found\n");

                    globalNext->seconds = tsec;
                    globalNext->time = time (NULL) + globalNext->seconds;
                    memcpy(globalNext->req_type, treq, sizeof globalNext->req_type);
                    memcpy(globalNext->message, tmessage, sizeof globalNext->message);

                    printf("Alarm(%d) Changed at %d: %d %s\n", globalNext->alarm_id, time(NULL), globalNext->seconds, globalNext->message);

                    break;
                }else{
                    // Testing and Debugging
                    printf("List of ids: %d\n", globalNext->alarm_id);
                }
                globalLast = &globalNext->link;
                globalNext = globalNext->link;
            }


            // if reach end of list and alarm_id does not match then request not valid
            if (exist == 0)
            {
                fprintf (stderr, "Bad command, Alarm ID (%d) not found\n", tid );
            }

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

// Testing and Debugging
/*
Start_Alarm(123): 20 This message
Start_Alarm(125): 20 This message
Start_Alarm(128): 20 This message
Start_Alarm(130): 20 This message

Change_Alarm(123): 20 New message
Change_Alarm(125): 20 New message
Start_Alarm(123): 1 This message
 */
