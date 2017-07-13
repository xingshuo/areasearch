#include "divgrid.h"
#include "lua.h"
#include "lauxlib.h"

#define HALF_SQRT2 0.7071

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

static inline void
get_min_and_max(float a, float b, float c, float d, float* min, float* max) {
    *min = a;
    *max = a;
    if (b < *min) {
        *min = b;
    }
    if (b > *max) {
        *max = b;
    }
    if (c < *min) {
        *min = c;
    }
    if (c > *max) {
        *max = c;
    }
    if (d < *min) {
        *min = d;
    }
    if (d > *max) {
        *max = d;
    }
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
    float half_grid = 0.5*m->grid_size;
    float dx = x - t->cx;
    float dz = z - t->cz;
    float lr = radius - (half_grid + dx); //left
    float rr = radius - (half_grid - dx); //right
    float tr = radius - (half_grid + dz); //top
    float br = radius - (half_grid - dz); //bottom
    float lr_grid = lr/m->grid_size;
    int lr_cover_grid = ceil(lr_grid);
    float rr_grid = rr/m->grid_size;
    int rr_cover_grid = ceil(rr_grid);
    if (rr_cover_grid == floor(rr_grid)) { //to cover boundary points
        rr_cover_grid++;
    }
    float tr_grid = tr/m->grid_size;
    int tr_cover_grid = ceil(tr_grid);
    float br_grid = br/m->grid_size;
    int br_cover_grid = ceil(br_grid);
    if (br_cover_grid == floor(br_grid)) { //to cover boundary points
        br_cover_grid++;
    }

    lr_cover_grid = lr_cover_grid + m->extra_check_grids;
    rr_cover_grid = rr_cover_grid + m->extra_check_grids;
    tr_cover_grid = tr_cover_grid + m->extra_check_grids;
    br_cover_grid = br_cover_grid + m->extra_check_grids;

    float safe_radius = HALF_SQRT2*radius;
    lr = safe_radius - (half_grid + dx);
    rr = safe_radius - (half_grid - dx);
    tr = safe_radius - (half_grid + dz);
    br = safe_radius - (half_grid - dz);
    lr_grid = lr/m->grid_size;
    int lr_safe_grid = floor(lr_grid);
    rr_grid = rr/m->grid_size;
    int rr_safe_grid = floor(rr_grid);
    tr_grid = tr/m->grid_size;
    int tr_safe_grid = floor(tr_grid);
    br_grid = br/m->grid_size;
    int br_safe_grid = floor(br_grid);

    int n = 0;
    int i,j;
    for (i=-lr_cover_grid; i<=rr_cover_grid; i++) {
        for (j=-tr_cover_grid; j<=br_cover_grid; j++) {
            int r = row + j;
            int c = col + i;
            tower *t = get_tower(m, r, c);
            if (!t) {
                continue;
            }
            if (i>=-lr_safe_grid && i<=rr_safe_grid && j>=-tr_safe_grid && j<=br_safe_grid) { //safe area
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
    get_min_and_max(lt_pos_x, rt_pos_x, lb_pos_x, rb_pos_x, &min_x, &max_x);
    get_min_and_max(lt_pos_z, rt_pos_z, lb_pos_z, rb_pos_z, &min_z, &max_z);

    float half_grid = 0.5*m->grid_size;
    float lr = t->cx - half_grid - min_x; //left
    float rr = max_x - (t->cx + half_grid); //right
    float tr = t->cz - half_grid - min_z; //top
    float br = max_z - (t->cz + half_grid); //bottom
    float lr_grid = lr/m->grid_size;
    int lr_cover_grid = ceil(lr_grid);
    float rr_grid = rr/m->grid_size;
    int rr_cover_grid = ceil(rr_grid);
    if (rr_cover_grid == floor(rr_grid)) { //to cover boundary points
        rr_cover_grid++;
    }
    float tr_grid = tr/m->grid_size;
    int tr_cover_grid = ceil(tr_grid);
    float br_grid = br/m->grid_size;
    int br_cover_grid = ceil(br_grid);
    if (br_cover_grid == floor(br_grid)) { //to cover boundary points
        br_cover_grid++;
    }

    lr_cover_grid = lr_cover_grid + m->extra_check_grids;
    rr_cover_grid = rr_cover_grid + m->extra_check_grids;
    tr_cover_grid = tr_cover_grid + m->extra_check_grids;
    br_cover_grid = br_cover_grid + m->extra_check_grids;

    float safe_radius = HALF_SQRT2*((half_width<half_height) ? half_width : half_height);
    float dx = x - t->cx;
    float dz = z - t->cz;
    lr = safe_radius - (half_grid + dx);
    rr = safe_radius - (half_grid - dx);
    tr = safe_radius - (half_grid + dz);
    br = safe_radius - (half_grid - dz);
    lr_grid = lr/m->grid_size;
    int lr_safe_grid = floor(lr_grid);
    rr_grid = rr/m->grid_size;
    int rr_safe_grid = floor(rr_grid);
    tr_grid = tr/m->grid_size;
    int tr_safe_grid = floor(tr_grid);
    br_grid = br/m->grid_size;
    int br_safe_grid = floor(br_grid);

    int n = 0;
    int i,j;
    for (i=-lr_cover_grid; i<=rr_cover_grid; i++) {
        for (j=-tr_cover_grid; j<=br_cover_grid; j++) {
            int r = row + j;
            int c = col + i;
            tower *t = get_tower(m, r, c);
            if (!t) {
                continue;
            }
            if (i>=-lr_safe_grid && i<=rr_safe_grid && j>=-tr_safe_grid && j<=br_safe_grid) {
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

int luaopen_areasearch(lua_State* L) {
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