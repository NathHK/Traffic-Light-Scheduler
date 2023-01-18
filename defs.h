#define MOVE 500 // each car takes 500ms to move through the intersection

typedef unsigned long uint64;

enum direction {NORTH, SOUTH, EAST, WEST};

struct car
{
    enum direction dir; // direction the car is traveling
    int at; // time at which the car spawned
    int dt; // time at which the car is allowed to pass through the intersection
            // (i.e. they 'depart' from the cue of waiting cars)
    int num; // number assigned to the car
};
