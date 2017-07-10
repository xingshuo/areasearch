package.cpath = package.cpath .. ";./build/?.so"
local areasearch = require "areasearch"
local floor = math.floor 
local sfmt = string.format 

print(sfmt("test start!! %sM",collectgarbage("count")))
local max_x = 100
local max_z = 100
local grid_size = 10
local areaobj = areasearch.create(max_x, max_z, grid_size)
areaobj:add(1, 15, 15, 2, 0)
areaobj:add(2, 25, 25, 2, 0)
areaobj:add(3, 35, 35, 2, 0)
areaobj:add(4, 16, 16, 2, 0)
areaobj:add(5, 17, 17, 2, 0)
local cx,cz,radius = 20,20,10

local tbl = areaobj:search_circle_range_objs(cx,cz,radius)
print("circle get11:")
for id in pairs(tbl) do
    print("get obj",id)
end
areaobj:update(3,15,15.5)
local tbl = areaobj:search_circle_range_objs(cx,cz,radius)
print("circle get22:")
for id in pairs(tbl) do
    print("get obj",id)
end

areaobj:update(2,25.3,25.3, 0.45)
areaobj:update(4,30,20)
local cx,cz,unit_dirx,unit_dirz,half_w,half_h = 20,20,0,-1,5,5
local tbl = areaobj:search_rect_range_objs(cx,cz,unit_dirx,unit_dirz,half_w,half_h)
print("rect get33:")
for id in pairs(tbl) do
    print("get obj",id)
end

areaobj = nil
collectgarbage("collect")
print(sfmt("test end!! %sM",collectgarbage("count")))