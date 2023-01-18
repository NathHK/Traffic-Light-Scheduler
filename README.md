# Traffic-Light-Scheduler

This program uses the C `pthread` library on Linux to simulate a 4-way traffic light intersection.

The general goal of the program is to maximize the throughput of the intersection while ensuring that no car waits "too long"—so even if traffic is much heavier in a particular direction, cars in the perpendicular direction should still get a chance to cross. 

The intersection has one lane in each direction, and no turning. This leaves only four directions that cars can travel across the interesection: North, South, East, and West.

Each *direction* is modeled as a pthread that produces cars which wish to cross the intersection. Each thread uses a timer to sleep for a random amount of time before producing the next car. 

## Part 1: Simple Round-robin Scheduler

This section implements a round-robin scheduler, similar to a 4-way stop sign. This scheduler simpy checks each thread's buffer in a loop, to see if a car is waiting. The scheduler then passes the car through the intersection by removing it from the buffer. Each passing car has a cost of 500 ms. That is, if the scheduler finds a car ready to pass, it will remove it from the buffer, wait 500 ms, and then move to the next direction.

## Part 2: Pass All Same-Direction Cars

Similar to Part 1, the scheduler moves one direction at a time. However, it allows all cars moving in the same direction to pass prior to moving on to the next direction. This happens in two steps:

1. Check direction. If a car is waiting, start moving it through the intersection (cost: 500 ms).
2. While the car from (1) is moving through the intersection, check that direction's buffer. If a car is waiting, wait 50 ms and then move it through (cost: 500 ms). Repeat (1) and (2) until there are no cars left in that direction. 

## Part 3: Parallel Round-robin Scheduler ***\*NOT IMPLEMENTED\****

This is the logical next-step of the prior scheduler formats and provides the most efficiency. Cars moving in *parallel* directions do not need to wait for 500 ms to elapse before entering the intersection, but cars moving in *perpendicular* directions always need to wait 500 ms before passing (otherwise, a collission would occur). By modifying the scheduler from Part 2, we allow cars moving in parallel but opposite directions to pass simultaneously:

1. Check direction. If a car is waiting, start moving it through the intersection (cost: 500 ms).
2. While the car from (1) is moving through, check:
    1. The parallel but opposite direction. If there is a car waiting, move it through the intersection (cost: 500 ms). 
    2. The same direction. If a car is waiting, wait 50 ms and then move it through (cost: 500 ms). Repeat (1) and (2) until there are no cars left in the two directions.
  
## Cost Function

Using the equation *cost = t^2*, where *t* := time, we can compare the cost of the different schedulers. Each car tracks the time it spends waiting at the intersection, and when it passes through the intersection, the time waiting is squared and added to the overall sum. 

## Print Statements

The program includes various **print**/**printf** statements to visualize the state of the intersection, as well as to provide a cost comparison when the user chooses to run multiple schedulers (currently allows for those of Part 1 and Part 2). 

## License

This work is published under the **GNU General Public License v3.0**.
