#include "divgrid.h"
#include "lua.h"
#include "lauxlib.h"

#define HALF_SQRT2 0.7071
#define PER_ANGLE_RADIAN M_PI/180

#define check_area(L, idx)\
    *(map**)luaL_checkudata(L, idx, "areasearch_meta")

static inline bool
is_two_circle_cross(float cx1, float cz1, float R1, float cx2, float cz2, float R2) {
    double dx = cx1 - cx2;
    double dz = cz1 - cz2;
    double dr = R1 + R2;
    return (dx*dx+dz*dz) <= dr*dr;
}

static inline bool
is_circle_rect_cross(float rect_cx, float rect_cz, float rect_dir_x, float rect_dir_z, float rect_half_width, float rect_half_height, float circle_cx, float circle_cz, float circle_radius) {
    float check_width = rect_half_width + circle_radius;
    float check_height = rect_half_height + circle_radius;
    float c2c_dir_x = circle_cx - rect_cx;
    float c2c_dir_z = circle_cz - rect_cz;
    float c2c_width = (c2c_dir_x*rect_dir_z - rect_dir_x*c2c_dir_z); //vector cross (AxB)
    if (c2c_width < 0) {
        c2c_width = -c2c_width;
    }
    if (c2c_width>check_width) {
        return false;
    }
    float c2c_height = (c2c_dir_x*rect_dir_x + c2c_dir_z*rect_dir_z); //vector dot  (A*B)
    if (c2c_height < 0) {
        c2c_height = -c2c_height;
    }
    if (c2c_height>check_height) {
        return false;
    }
    if (c2c_width>=rect_half_width && c2c_height>=rect_half_height) {
        double dw = c2c_width - rect_half_width;
        double dh = c2c_height - rect_half_height;
        double radius2 = (double)circle_radius * (double)circle_radius;
        return (dw*dw + dh*dh) <= radius2;
    }else{
        return true;
    }
}

static inline bool
is_circle_sector_cross(float sector_cx, float sector_cz, float sector_dir_x, float sector_dir_z, float half_angle_rad, float sector_radius, float circle_cx, float circle_cz, float circle_radius) {
    if (!is_two_circle_cross(sector_cx,sector_cz,sector_radius, circle_cx,circle_cz,circle_radius)) {
        return false;
    }
    //approximate calculation
    double cx = sector_cx - circle_radius*sector_dir_x;
    double cz = sector_cz - circle_radius*sector_dir_z;
    double dx = circle_cx - cx;
    double dz = circle_cz - cz;
    double len2 = dx*dx + dz*dz;
    double dot_value = sector_dir_x*dx + sector_dir_z*dz;
    double cos_value = cos(half_angle_rad);
    if (cos_value >= 0){
        if (dot_value >= 0){
            return dot_value*dot_value > len2*cos_value*cos_value;
        }else {
            return false;
        }
    }else{
        if (dot_value < 0){
            return dot_value*dot_value < len2*cos_value*cos_value;
        }else {
            return true;
        }
    }
}

static inline void
vector_rotate(float dir_x, float dir_z, float rotate_rad, float* new_dir_x, float* new_dir_z) {
    float cos_value = cos(rotate_rad);
    float sin_value = sin(rotate_rad);
    *new_dir_x = dir_x*cos_value - dir_z*sin_value;
    *new_dir_z = dir_z*cos_value + dir_x*sin_value;
}

static inline void
check_max_and_min(float* max, float* min, float value){
    if (value > *max) {
        *max = value;
    }
    if (value < *min) {
        *min = value;
    }
}

static inline void
get_cover_row_and_col(map* m, float min_x, float max_x, float min_z, float max_z, int* min_col, int* max_col, int* min_row, int* max_row){
    *min_col = floor(min_x/m->grid_size);
    *max_col = floor(max_x/m->grid_size);
    *min_row = floor(min_z/m->grid_size);
    *max_row = floor(max_z/m->grid_size);
    *min_col -= m->extra_check_grids;
    *max_col += m->extra_check_grids;
    *min_row -= m->extra_check_grids;
    *max_row += m->extra_check_grids;
}

static inline bool
get_safe_row_and_col(map* m, float min_x, float max_x, float min_z, float max_z, int* min_col, int* max_col, int* min_row, int* max_row){
    *min_col = ceil(min_x/m->grid_size);
    *max_col = floor(max_x/m->grid_size)-1;
    *min_row = ceil(min_z/m->grid_size);
    *max_row = floor(max_z/m->grid_size)-1;
    return (min_row<=max_row && min_col<=max_col);
}


static int
area_new(lua_State* L) {
    int max_x = luaL_checknumber(L, 1);
    int max_z = luaL_checknumber(L, 2);
    int grid_size = luaL_checknumber(L, 3);
    map * m = map_new(max_x, max_z, grid_size);
    *(map**)lua_newuserdata(L, sizeof(void*)) = m;
    luaL_getmetatable(L, "areasearch_meta");
    lua_setmetatable(L, -2);
    return 1;
};

static int
area_release(lua_State* L) {
    map* m = check_area(L, 1);
    map_delete(m);
    return 0;
}

static int
area_add(lua_State* L) {
    map* m = check_area(L, 1);
    uint64_t id = luaL_checkinteger(L, 2);
    float x = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);
    float radius = luaL_checknumber(L, 5);
    object * obj = map_query_object(m, id);
    if (obj) {
        return 0;
    }
    int row = z/m->grid_size;
    int col = x/m->grid_size;
    tower *t = get_tower(m, row, col);
    if (!t) {
        return 0;
    }
    obj = map_init_object(m, id);
    obj->x = x;
    obj->z = z;
    obj->radius = radius;
    if (radius > m->extra_check_grids*m->grid_size) {
        m->extra_check_grids = ceil(radius/m->grid_size);
    }
    if (lua_isnumber(L, 6)) {
        obj->type = luaL_checknumber(L, 6);
    }
    insert_obj_to_tower(t, obj);
    lua_pushboolean(L, 1);
    return 1;
}

static int
area_update(lua_State* L) {
    map* m = check_area(L, 1);
    uint64_t id = luaL_checkinteger(L, 2);
    float x = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);
    object * obj = map_query_object(m, id);
    if (!obj) {
        return 0;
    }
    if (lua_isnumber(L, 5)) {
        float radius = luaL_checknumber(L, 5);
        obj->radius = radius;
        if (radius > m->extra_check_grids*m->grid_size) {
            m->extra_check_grids = ceil(radius/m->grid_size);
        }
    }
    int suc = map_update_object(m,obj,x,z);
    lua_pushboolean(L, suc);
    return 1;
}

static int
area_delete(lua_State* L) {
    map* m = check_area(L, 1);
    uint64_t id = luaL_checkinteger(L, 2);
    object * obj = map_query_object(m, id);
    if (obj){
        map_delete_object(m, id);
    }
    return 0;
}

static int
area_query(lua_State* L) {
    map* m = check_area(L, 1);
    uint64_t id = luaL_checkinteger(L, 2);
    object * obj = map_query_object(m, id);
    lua_settop(L, 2);
    lua_newtable(L);
    if (obj){
        lua_pushstring(L, "id");
        lua_pushinteger(L, id);
        lua_rawset(L,3);
        lua_pushstring(L, "radius");
        lua_pushinteger(L, obj->radius);
        lua_rawset(L,3);
        lua_pushstring(L, "x");
        lua_pushinteger(L, obj->x);
        lua_rawset(L,3);
        lua_pushstring(L, "z");
        lua_pushinteger(L, obj->z);
        lua_rawset(L,3);
        lua_pushstring(L, "type");
        lua_pushinteger(L, obj->type);
        lua_rawset(L,3);
        lua_pushstring(L, "tower_row");
        lua_pushinteger(L, obj->pTower->row);
        lua_rawset(L,3);
        lua_pushstring(L, "tower_col");
        lua_pushinteger(L, obj->pTower->col);
        lua_rawset(L,3);
    }
    return 1;
}

static int
area_search_circle_range_objs(lua_State* L) {
    map* m = check_area(L, 1);
    float x = luaL_checknumber(L, 2);
    float z = luaL_checknumber(L, 3);
    float radius = luaL_checknumber(L, 4);
    int type = 0;
    if (lua_isnumber(L, 5)) {
        type = luaL_checknumber(L, 5);
    }
    int limit_cnt = 0x7fff;
    if (lua_isnumber(L, 6)) {
        limit_cnt = luaL_checknumber(L, 6);
    }
    lua_settop(L, 4);
    lua_newtable(L);
    int row = z/m->grid_size;
    int col = x/m->grid_size;
    tower *t = get_tower(m, row, col);
    if (!t) {
        return 1;
    }
    int min_cover_col,max_cover_col,min_cover_row,max_cover_row;
    get_cover_row_and_col(m, x-radius, x+radius, z-radius, z+radius, &min_cover_col, &max_cover_col, &min_cover_row, &max_cover_row);

    float safe_radius = HALF_SQRT2*radius;
    int min_safe_col,max_safe_col,min_safe_row,max_safe_row;
    bool has_safe = get_safe_row_and_col(m, x-safe_radius, x+safe_radius, z-safe_radius, z+safe_radius, &min_safe_col, &max_safe_col, &min_safe_row, &max_safe_row);
    int n = 0;
    int r,c;
    for (r=min_cover_row; r<=max_cover_row; r++){
        for (c=min_cover_col; c<=max_cover_col; c++){
            tower *t = get_tower(m, r, c);
            if (!t) {
                continue;
            }
            if (has_safe && r>=min_safe_row && r<=max_safe_row && c>=min_safe_col && c<=max_safe_col) { //safe area
                object* pCur = t->pHead->pNext;
                while (pCur != t->pHead) {
                    if ((type&pCur->type) == type) {
                        lua_pushinteger(L,pCur->id);
                        lua_pushinteger(L,1);
                        lua_rawset(L,5);
                        n++;
                        if (n >= limit_cnt) {
                            return 1;
                        }
                    }
                    pCur = pCur->pNext;
                }
            }else {
                object* pCur = t->pHead->pNext;
                while (pCur != t->pHead) {
                    if ((type&pCur->type) == type) {
                        if (is_two_circle_cross(x,z,radius,pCur->x,pCur->z,pCur->radius)) {
                            lua_pushinteger(L,pCur->id);
                            lua_pushinteger(L,1);
                            lua_rawset(L,5);
                            n++;
                            if (n >= limit_cnt) {
                                return 1;
                            }
                        }
                    }
                    pCur = pCur->pNext;
                }
            }
        }
    }
    return 1;
}

int
area_search_rect_range_objs(lua_State* L) {
    map* m = check_area(L, 1);
    float x = luaL_checknumber(L, 2);
    float z = luaL_checknumber(L, 3);
    float dir_x = luaL_checknumber(L, 4);
    float dir_z = luaL_checknumber(L, 5);
    float dir_len = sqrt(dir_x*dir_x + dir_z*dir_z);
    if (dir_len > 0 && dir_len != 1) { //convert to unit dir
        dir_x = dir_x/dir_len;
        dir_z = dir_z/dir_len;
    }
    float half_width = luaL_checknumber(L, 6);
    float half_height = luaL_checknumber(L, 7);

    int type = 0;
    if (lua_isnumber(L, 8)) {
        type = luaL_checknumber(L, 8);
    }
    int limit_cnt = 0x7fff;
    if (lua_isnumber(L, 9)) {
        limit_cnt = luaL_checknumber(L, 9);
    }
    lua_settop(L, 4);
    lua_newtable(L);
    int row = z/m->grid_size;
    int col = x/m->grid_size;
    tower *t = get_tower(m, row, col);
    if (!t) {
        return 1;
    }
    float top_dx = dir_x*half_height;
    float top_dz = dir_z*half_height;
    float bottom_dx = -top_dx;
    float bottom_dz = -top_dz;
    float left_dx = (-dir_z)*half_width;
    float left_dz = dir_x*half_width;
    float right_dx = -left_dx;
    float right_dz = -left_dz;

    float lt_pos_x = x + top_dx + left_dx;
    float lt_pos_z = z + top_dz + left_dz;
    float rt_pos_x = x + top_dx + right_dx;
    float rt_pos_z = z + top_dz + right_dz;
    float lb_pos_x = x + bottom_dx + left_dx;
    float lb_pos_z = z + bottom_dz + left_dz;
    float rb_pos_x = x + bottom_dx + right_dx;
    float rb_pos_z = z + bottom_dz + right_dz;

    float min_x,max_x,min_z,max_z;
    min_x = max_x = lt_pos_x;
    min_z = max_z = lt_pos_z;
    check_max_and_min(&max_x, &min_x, rt_pos_x);
    check_max_and_min(&max_x, &min_x, lb_pos_x);
    check_max_and_min(&max_x, &min_x, rb_pos_x);

    check_max_and_min(&max_z, &min_z, rt_pos_z);
    check_max_and_min(&max_z, &min_z, lb_pos_z);
    check_max_and_min(&max_z, &min_z, rb_pos_z);

    int min_cover_col,max_cover_col,min_cover_row,max_cover_row;
    get_cover_row_and_col(m, min_x, max_x, min_z, max_z, &min_cover_col, &max_cover_col, &min_cover_row, &max_cover_row);

    float safe_radius = HALF_SQRT2*((half_width<half_height) ? half_width : half_height);
    int min_safe_col,max_safe_col,min_safe_row,max_safe_row;
    bool has_safe = get_safe_row_and_col(m, x-safe_radius, x+safe_radius, z-safe_radius, z+safe_radius, &min_safe_col, &max_safe_col, &min_safe_row, &max_safe_row);
    int n = 0;
    int r,c;
    for (r=min_cover_row; r<=max_cover_row; r++){
        for (c=min_cover_col; c<=max_cover_col; c++){
            tower *t = get_tower(m, r, c);
            if (!t) {
                continue;
            }
            if (has_safe && r>=min_safe_row && r<=max_safe_row && c>=min_safe_col && c<=max_safe_col) {
                object* pCur = t->pHead->pNext;
                while (pCur != t->pHead) {
                    if ((type&pCur->type) == type) {
                        lua_pushinteger(L,pCur->id);
                        lua_pushinteger(L,1);
                        lua_rawset(L,5);
                        n++;
                        if (n >= limit_cnt) {
                            return 1;
                        }
                    }
                    pCur = pCur->pNext;
                }
            }else {
                object* pCur = t->pHead->pNext;
                while (pCur != t->pHead) {
                    if ((type&pCur->type) == type) {
                        if (is_circle_rect_cross(x,z,dir_x,dir_z,half_width,half_height,pCur->x,pCur->z,pCur->radius)) {
                            lua_pushinteger(L,pCur->id);
                            lua_pushinteger(L,1);
                            lua_rawset(L,5);
                            n++;
                            if (n >= limit_cnt) {
                                return 1;
                            }
                        }
                    }
                    pCur = pCur->pNext;
                }
            }
        }
    }
    return 1;
}

int
area_search_sector_range_objs(lua_State* L) {
    map* m = check_area(L, 1);
    float x = luaL_checknumber(L, 2);
    float z = luaL_checknumber(L, 3);
    float dir_x = luaL_checknumber(L, 4);
    float dir_z = luaL_checknumber(L, 5);
    float dir_len = sqrt(dir_x*dir_x + dir_z*dir_z);
    if (dir_len > 0 && dir_len != 1) { //convert to unit dir
        dir_x = dir_x/dir_len;
        dir_z = dir_z/dir_len;
    }
    float angle = luaL_checknumber(L, 6);
    float radius = luaL_checknumber(L, 7);
    int type = 0;
    if (lua_isnumber(L, 8)) {
        type = luaL_checknumber(L, 8);
    }
    int limit_cnt = 0x7fff;
    if (lua_isnumber(L, 9)) {
        limit_cnt = luaL_checknumber(L, 9);
    }
    lua_settop(L, 4);
    lua_newtable(L);
    int row = z/m->grid_size;
    int col = x/m->grid_size;
    tower *t = get_tower(m, row, col);
    if (!t) {
        return 1;
    }
    float min_x,min_z,max_x,max_z;
    min_x = max_x = x;
    min_z = max_z = z;
    float half_angle = angle*0.5;
    float half_angle_rad = half_angle*PER_ANGLE_RADIAN;
    float edge_dir_x,edge_dir_z;
    int i;
    for (i=0; i<2; i++) {
        if (i==0) {
            vector_rotate(dir_x, dir_z, half_angle_rad, &edge_dir_x, &edge_dir_z);
        }else {
            vector_rotate(dir_x, dir_z, -half_angle_rad, &edge_dir_x, &edge_dir_z);
        }
        float edge_vx = x + edge_dir_x*radius;
        float edge_vz = z + edge_dir_z*radius;
        check_max_and_min(&max_x, &min_x, edge_vx);
        check_max_and_min(&max_z, &min_z, edge_vz);
    }

    float rad = atan2(dir_z, dir_x);
    float min_pi_rad = (rad - half_angle_rad)/M_PI;
    float max_pi_rad = (rad + half_angle_rad)/M_PI;
    float f;
    float check_x,check_z;
    for (f = -2; f <= 2; f += 0.5) {
        if (f > max_pi_rad) {
            break;
        }
        if (f < min_pi_rad) {
            continue;
        }
        if (f==-2 || f==0 || f==2) {
            check_x = x + radius;
            check_z = 0;
        }else if (f==1 || f==-1) {
            check_x = x - radius;
            check_z = 0;
        }else if (f==-1.5 || f==0.5) {
            check_x = 0;
            check_z = z + radius;
        }else {
            check_x = 0;
            check_z = z - radius;
        }
        check_max_and_min(&max_x, &min_x, check_x);
        check_max_and_min(&max_z, &min_z, check_z);
    }

    int min_cover_col,max_cover_col,min_cover_row,max_cover_row;
    get_cover_row_and_col(m, min_x, max_x, min_z, max_z, &min_cover_col, &max_cover_col, &min_cover_row, &max_cover_row);

    float min_safe_x, max_safe_x, min_safe_z, max_safe_z;
    if (half_angle < 90) {
        float L = radius/(1 + sin(half_angle_rad));
        float R = L*sin(half_angle_rad)*HALF_SQRT2;
        float cx = x + L*dir_x;
        float cz = z + L*dir_z;
        min_safe_x = cx - R;
        max_safe_x = cx + R;
        min_safe_z = cz - R;
        max_safe_z = cz + R;
    }else {
        float L = radius/2;
        float R = L*HALF_SQRT2;
        float cx = x + L*dir_x;
        float cz = z + L*dir_z;
        min_safe_x = cx - R;
        max_safe_x = cx + R;
        min_safe_z = cz - R;
        max_safe_z = cz + R;
    }
    int min_safe_col,max_safe_col,min_safe_row,max_safe_row;
    bool has_safe = get_safe_row_and_col(m, min_safe_x, max_safe_x, min_safe_z, max_safe_z, &min_safe_col, &max_safe_col, &min_safe_row, &max_safe_row);
    int n = 0;
    int r,c;
    for (r=min_cover_row; r<=max_cover_row; r++){
        for (c=min_cover_col; c<=max_cover_col; c++){
            tower *t = get_tower(m, r, c);
            if (!t) {
                continue;
            }
            if (has_safe && r>=min_safe_row && r<=max_safe_row && c>=min_safe_col && c<=max_safe_col){
                object* pCur = t->pHead->pNext;
                while (pCur != t->pHead) {
                    if ((type&pCur->type) == type) {
                        lua_pushinteger(L,pCur->id);
                        lua_pushinteger(L,1);
                        lua_rawset(L,5);
                        n++;
                        if (n >= limit_cnt) {
                            return 1;
                        }
                    }
                    pCur = pCur->pNext;
                }
            }else{
                object* pCur = t->pHead->pNext;
                while (pCur != t->pHead) {
                    if ((type&pCur->type) == type) {
                        if (is_circle_sector_cross(x,z,dir_x,dir_z,half_angle_rad,radius,pCur->x,pCur->z,pCur->radius)) {
                            lua_pushinteger(L,pCur->id);
                            lua_pushinteger(L,1);
                            lua_rawset(L,5);
                            n++;
                            if (n >= limit_cnt) {
                                return 1;
                            }
                        }
                    }
                    pCur = pCur->pNext;
                }
            }
        }
    }
    return 1;
}

int
luaopen_areasearch(lua_State* L) {
    luaL_checkversion(L);
    luaL_Reg l1[] = {
        {"create", area_new},
        {NULL, NULL},
    };
    luaL_Reg l2[] = {
        {"add", area_add},
        {"update", area_update},
        {"delete", area_delete},
        {"query", area_query},
        {"search_circle_range_objs", area_search_circle_range_objs},
        {"search_rect_range_objs", area_search_rect_range_objs},
        {"search_sector_range_objs", area_search_sector_range_objs},
        {NULL, NULL},
    };
    luaL_newmetatable(L, "areasearch_meta");
    luaL_newlib(L, l2);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, area_release);
    lua_setfield(L, -2, "__gc");

    luaL_newlib(L, l1);
    return 1;
}