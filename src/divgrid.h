#ifndef _DIVGRID_H
#define _DIVGRID_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

typedef struct object {
    uint64_t id;
    float x;
    float z;
    float radius;
    int type;
    struct object * pNext;
    struct object * pPrev;
    struct tower * pTower;
} object;

typedef struct tower {
    float cx;
    float cz;
    int row;
    int col;
    object * pHead;
} tower;

typedef struct slot {
    uint64_t id;
    object * obj;
    int next;
} slot;

typedef struct map {
    int size;
    int lastfree;
    slot * slot_list;
    int max_row;
    int max_col;
    int max_x;
    int max_z;
    int grid_size;
    int extra_check_grids;
    tower ** tower_list;
} map;

map* map_new(int, int, int);
void map_delete(map*);
object* map_query_object(map*, uint64_t);
object* map_init_object(map*, uint64_t);
int map_update_object(map*, object*, float, float);
object* map_delete_object(map *, uint64_t);
tower* get_tower(map*, int, int, bool);
void insert_obj_to_tower(tower*, object*);
void delete_obj_from_tower(tower*, object*);

#endif