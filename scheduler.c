#define _GNU_SOURCE 

#include <pthread.h> 
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include "defs.h"
#include <sys/wait.h>
#include <sched.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

#define STACK_SIZE (1024 * 1024) // Stack size for cloned child (see p2)

#define STOP 60 // how long to run the simulation (seconds)
#define RAND 2000 // upp bound for random wait time (ms) during car production 

// <<< GLOBAL VARIABLES >>>
                        
// Initialized in main(): 
int sum; // overall sum of how long cars have waited at the intersection
int sum1; // when part1 ends, its cost is saved here so part2 can use sum
int count; // used to label each car
int departed; // track how many of the cars have finished moving

// Initialized in scheduler function: 
struct timespec global_ts;

// <<< MUTEX AND ETC. >>> 

pthread_mutex_t intersection_mutex;
pthread_mutex_t printf_mutex;
pthread_mutex_t count_mutex;
pthread_mutex_t sum_mutex;

// Used only in p2 (within p2_produceCars()): 
pthread_mutex_t NS_mutex;
pthread_mutex_t EW_mutex;


// <<< FUNCTIONS >>>

struct car* arrive(struct car *c);
void pass(struct car *c);
void panic(char *s);

void updateCost(struct car *c)
{
    int cost = 0;
    int waited = 0;
    if((c->dt && c->at) && (c->dt > 0)) {
        int dep_t = c->dt;
        int arr_t = c->at;
        waited = dep_t - arr_t; // how long the given car waited 
    }
    else
        panic("updateCost: Car's time values weren't set!\n");
    //printf("Car %d waited for: %dms\n", c->num, waited);
    cost = (int)((waited*waited) + 0.5); // +0.5 ensures correct calculation
    pthread_mutex_lock(&sum_mutex); // obtain lock before updating the sum
    sum = sum + cost; // add cost to the overall sum
    pthread_mutex_unlock(&sum_mutex);
    //printf("\nUpdated cost = %dms\n", sum);
}

// Prints the time t in milliseconds
// intended to be used as a prefix for other print statements
void timestamp()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int sec;
    int nsec;
    int ms; 
    sec = (int)(ts.tv_sec - global_ts.tv_sec);
    nsec = (int)(ts.tv_nsec - global_ts.tv_nsec);
    ms = nsec/1000000;
    if(ms < 0){
        sec--;
        ms = 1000+ms;
    }
    printf("[%ds%dms]   ", sec, ms);
}

// Returns difference between current time and starting time
// argument of 0 => milliseconds
// argument of 1 => seconds
int getRuntime(int units)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int sec;
    int nsec;
    int ms; 
    sec = (int)(ts.tv_sec - global_ts.tv_sec);
    nsec = (int)(ts.tv_nsec - global_ts.tv_nsec);
    ms = nsec/1000000;
    if(ms < 0){
        sec--;
        ms = 1000+ms;
    }
    
    if(units == 0)
        return ms;
    else if(units == 1)
        return sec;
    else
        panic("getRuntime()\n");
    return -1;
}

// Calculates runtime (ms) and assigns it to the requested time variable of c
struct car* car_setTime(int setArrival, struct car *c)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int sec;
    int nsec;
    int ms; 
    sec = (int)(ts.tv_sec - global_ts.tv_sec);
    nsec = (int)(ts.tv_nsec - global_ts.tv_nsec);
    ms = nsec/1000000;
    if(ms < 0){
        sec--;
        ms = 1000+ms;
    }
    
    int sum = (sec*1000)+ms; // total time in milliseconds
    
    if(setArrival == 1) // assign sum to car's arrival time
        c->at = sum;
    else
        c->dt = sum;
    
    return c;
}

// Mock panic function (all it does is add 'panic: ' to the start of s)
void panic(char *s)
{
    printf("panic: %s", s);
}

// msleep(): Sleep for the requested number of milliseconds. 
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

// Returns string translation of car's direction
char* dir_to_string(enum direction d)
{
    char *dir = "";
    switch(d)
    {
        case(0): //northbound
            dir = "NORTH";
            return dir;
            break;
        case(1): //southbound
            dir = "SOUTH";
            return dir;
            break;
        case(2): //eastbound
            dir = "EAST";
            return dir;
            break;
        case(3): //westbound
            dir = "WEST";
            return dir;
            break;
        default:
            panic("dir_to_string: Invalid direction!\n");
            dir = "";
            return dir;
            break;
    }
    return dir;
}

// Allocates memory for a new car, initializes, and returns pointer to it
struct car* spawnCar(enum direction d)
{
    // NOTE: NOT INCLUDING MALLOC HERE CAUSES SEGMENTATION FAULTS
    struct car* c = malloc(sizeof(c));
    c->at = 0;
    c->dir = d;
    c->dt = -1; // departure time set to -1 until the car passes through
    
    pthread_mutex_lock(&count_mutex);
    count = count+1; // increment the number of cars
    c->num = count; // and assign count to the one being spawned
    pthread_mutex_unlock(&count_mutex);
    
    // print arrival message and return
    arrive(c);
    return c;
}

// Prints a notification regarding the car's arrival
struct car* arrive(struct car *c)
{
    c = car_setTime(1, c);

    pthread_mutex_lock(&printf_mutex);
    timestamp(); 
    printf("Arrival: Car %d moving %s.\n", c->num, dir_to_string(c->dir));
    pthread_mutex_unlock(&printf_mutex);
    
    return c;
}

// After 500ms, prints a notif that the car has passed through the intersection
void pass(struct car *c)
{
    // Set car's departure time
    car_setTime(0, c);
    
    // Before sleeping, print notification that c is moving
    pthread_mutex_lock(&printf_mutex); 
    timestamp();
    printf("Departure: Car %d moving %s!\n", c->num, dir_to_string(c->dir));
    pthread_mutex_unlock(&printf_mutex);
   
    // Sleep for 500ms to simulate the car passing through the intersection
    msleep(MOVE);
    
    // Print notification that c has finished passing through the intersection
    pthread_mutex_lock(&printf_mutex);
    timestamp();
    printf("Car %d finished moving %s.\n", c->num, dir_to_string(c->dir));
    pthread_mutex_unlock(&printf_mutex);
    
    // Increment number of departed cars
    departed = departed+1; 
    
    // Add this car's waiting time to the overall sum
    updateCost(c);
}

// <<< PART 1: Simple Round-Robin scheduler >>>
void *produceCars(void *arg)
{
    enum direction d;
    uint64 a = (uint64)arg;

    switch(a) // initialize d according to arg
    {
        case(0):
            d = NORTH;
            break;
        case(1):
            d = SOUTH;
            break;
        case(2):
            d = EAST;
            break;
        case(3):
            d = WEST;
            break;
        default:
            panic("produceCars: Invalid direction!\n");
            d = WEST;
            break;
    }
    
    // VARIABLES
    int i = 0; // index of car to send through
    int j = 0; // next empty spot in the array
    int r = 0; // holds random value calculated by call to rand()
    int rt_sec = 0; // total runtime in seconds; calculated based on REALTIME
    struct car *cars[100]; // each direction can spawn max 100 cars during sim

    // Loop until STOP time has been reached
    do {

        r = (rand()%RAND); // random number from 0-RAND (exclusive)
        msleep(r); // sleep for r milliseconds
       
        // if cars[] isn't full:
        // spawn a new car and insert into cars[] at next available index, j:
        if(j < 100) {
            struct car *c;
            c = spawnCar(d);
            cars[j] = c;
            // increment j:
            j++;
        }
        // If there is a car waiting to pass:
        if(cars[i]) {
            pthread_mutex_lock(&intersection_mutex); // obtain the lock
            struct car *go;
            go = cars[i]; 
            i++; // increment i to next awaiting car 
            pass(go); // send the car through (waits 500ms within pass())
            pthread_mutex_unlock(&intersection_mutex); // release the lock
        }

        rt_sec = getRuntime(1); // how long the sim has been running (seconds)
    } while(rt_sec < STOP);
    
    /*// Free the memory allocated to all the cars
    
    for(int k=0; k<100; k++) {
        if(cars[k]) {
            struct car *curr = cars[k];
            free((void *)curr);
        }
    }
    */
}

void simpleScheduler()
{
    clock_gettime(CLOCK_REALTIME, &global_ts); // start-time of the scheduler
    
    // initialize the mutex variables
    pthread_mutex_init(&intersection_mutex, NULL);
    pthread_mutex_init(&printf_mutex, NULL);
    pthread_mutex_init(&count_mutex, NULL);
    pthread_mutex_init(&sum_mutex, NULL);
    
    // one thread per direction
    pthread_t tNorth, tSouth, tEast, tWest; 
    pthread_create(&tNorth, NULL, produceCars, (void*)0);
    pthread_create(&tSouth, NULL, produceCars, (void*)1);
    pthread_create(&tEast, NULL, produceCars, (void*)2);
    pthread_create(&tWest, NULL, produceCars, (void*)3);
   
    // these will hold the exit status of each thread
    void *resN, *resS, *resE, *resW;
    
    // Wait for all 4 threads to finish:
    pthread_join(tNorth, &resN);
    pthread_join(tSouth, &resS);
    pthread_join(tEast, &resE);
    pthread_join(tWest, &resW);
    
    // Finally, print the total wait time and the overall cost:
    double sec = (sqrt(sum))/1000;
    printf("\nTotal time waited ~= %fs\n", sec);
    printf("Final cost = %dms\n\n", sum);
    
    // Store final cost in sum1
    sum1 = sum;
    
    // And destroy the mutex variables
    pthread_mutex_destroy(&intersection_mutex);
    pthread_mutex_destroy(&printf_mutex);
    pthread_mutex_destroy(&count_mutex);
}

// <<< PART 2: Parallel Round-Robin sceduler >>>

// int return type is necessary for clone() to work correctly
int p2_pass(void* c) 
{
    pass(c);
    return 0;
}

void *p2_produceCars(void *arg)
{
    enum direction d;
    uint64 a = (uint64)arg;
    enum direction parallel_dir;

    switch(a) // initialize d and parallel_dir according to arg
    {
        case(0):
            d = NORTH;
            parallel_dir = SOUTH;
            break;
        case(1):
            d = SOUTH;
            parallel_dir = NORTH;
            break;
        case(2):
            d = EAST;
            parallel_dir = WEST;
            break;
        case(3):
            d = WEST;
            parallel_dir = EAST;
            break;
        default:
            panic("produceCars: Invalid direction!\n");
            d = WEST;
            parallel_dir = EAST;
            break;
    }
    
    // VARIABLES
    int i = 0; // index of car to send through
    int j = 0; // next empty spot in the array
    int r = 0; // holds random value calculated by call to rand()
    int rt_sec = 0; // total runtime in seconds; calculated based on REALTIME
    struct car *cars[100]; // each direction can spawn max 100 cars during sim
    
    do {
        
        r = (rand()%RAND); // random number from 0-RAND (exclusive)
        msleep(r); // sleep for r milliseconds
       
        // if cars[] isn't full: 
        // spawn a new car and insert into cars[] at next available index, j:
        if(j < 100) {
            struct car *c;
            c = spawnCar(d);
            cars[j] = c;
            // increment j:
            j++;
            
            // TESTING: added temporarily for testing purposes
            if((d==0) || (d==2)) {
                struct car *c2;
                c2 = spawnCar(d);
                cars[j] = c2;
                j++;
            }
            
        }
        
        // check for cars waiting to pass
        if(cars[i]) {
            
            pthread_mutex_lock(&intersection_mutex);
            // If there are multiple cars waiting in this queue, 
            // we need to send each car through using the clone() function.
            int numWaiting = j-i;
            if(numWaiting > 1) {
                while((i<j) && cars[i]) // loop until waiting-queue is empty
                {
                    char *stack;      /* Start of stack buffer */
                    char *stackTop;   /* End of stack buffer */
                    pid_t pid;
                    
                    /* Allocate stack for child */
                    stack = malloc(STACK_SIZE);
                    if (stack == NULL)
                        errExit("malloc");
                    /* Assume stack grows downward */
                    stackTop = stack + STACK_SIZE; 

                    /* Create child; child commences execution in p2_pass() */
                    pid = clone(p2_pass, stackTop, SIGCHLD, (void*)(cars[i]));
                    if (pid == -1)
                        errExit("clone");
                    //printf("clone() returned %ld\n", (long) pid);

                    /* Parent falls through to here */
                    i++; // increment i to next index in cars
                    
                    /* If no more cars waiting, wait 4 child to change state */
                    if(i==j) {
                        if (waitpid(pid, NULL, 0) == -1) /* Wait for child */
                            errExit("waitpid");
                        //printf("child has terminated\n");
                    } // otherwise...
                    else // wait 50 ms before sending next car
                        msleep(50); 
                }
            } // otherwise... 
            else { // send the single awaiting car through the intersection
                struct car *go;
                go = cars[i]; 
                i++; // increment i to next awaiting car 
                pass(go); // send the car through (waits 500ms within pass())
            }
            pthread_mutex_unlock(&intersection_mutex);
        }
        rt_sec = getRuntime(1); // get runtime (in seconds)
    } while(rt_sec < STOP);
}

void parallelScheduler()
{
    clock_gettime(CLOCK_REALTIME, &global_ts); // start-time of the scheduler
    
    // initialize the mutex variables
    pthread_mutex_init(&intersection_mutex, NULL);
    pthread_mutex_init(&printf_mutex, NULL);
    pthread_mutex_init(&count_mutex, NULL);
    pthread_mutex_init(&sum_mutex, NULL);
   
    // one thread per direction
    pthread_t tNorth, tSouth, tEast, tWest; 
    pthread_create(&tNorth, NULL, p2_produceCars, (void*)0);
    pthread_create(&tSouth, NULL, p2_produceCars, (void*)1);
    pthread_create(&tEast, NULL, p2_produceCars, (void*)2);
    pthread_create(&tWest, NULL, p2_produceCars, (void*)3);
  
    // these will hold the exit status of each thread
    void *resN, *resS, *resE, *resW;
    
    // Wait for all 4 threads to finish:
    pthread_join(tNorth, &resN);
    pthread_join(tSouth, &resS);
    pthread_join(tEast, &resE);
    pthread_join(tWest, &resW);
    
    // Finally, print the total wait time and the overall cost:
    double sec = (sqrt(sum))/1000;
    printf("\nTotal time waited ~= %fs\n", sec);
    printf("Final cost = %dms\n", sum);
    
    // NOTE: Comment this if running only one of the two parts
    // Calculate the difference in cost between simpleRR and parallelRR:
    int diff = sum1 - sum;
    // And print the results:
    printf("\nDifference in cost = %dms\n", diff);
    
}

// Compile with:
// gcc scheduler.c -pthread -lpthread -lrt -lm
int main()
{
    // initialize global variables
    sum = 0;
    sum1 = 0;
    count = 0;
    departed = 0;
    
    // <<< PART 1 >>>
    printf("Running Part 1: Simple Scheduler\n\n");
    simpleScheduler();
    
    // reset vars
    sum = 0; // the p1 scheduler has already saved its cost to sum1
    count = 0;
    departed = 0;
    
    // <<< PART 2 >>>
    printf("Running Part 2: Parallel Scheduler\n\n");
    parallelScheduler();
   
    return 0;
}
