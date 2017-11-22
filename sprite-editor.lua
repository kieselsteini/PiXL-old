--[[----------------------------------------------------------------------------

      PiXL - a simple sprite editor
      written by Sebastian Steinhauer <s.steinhauer@yahoo.de>

--]]----------------------------------------------------------------------------
local pixl = require 'pixl'
local bitmap_size = 8
local bitmap
local show_grid = true
local color = 10
local cursor_sprite = '008888880f08888807f08888077f08880777f0880777ff080077f00880000088'


function fill(sx, sy)
  local replace_color = bitmap[sx][sy]
  if replace_color == color then return end
  local function _fill(x, y)
    if x < 1 or x > bitmap_size or y < 1 or y > bitmap_size then
      return
    end
    if bitmap[x][y] == replace_color then
      bitmap[x][y] = color
      _fill(x - 1, y); _fill(x + 1, y)
      _fill(x, y - 1); _fill(x, y + 1)
    end
  end
  _fill(sx, sy)
end

function init()
  bitmap = {}
  for x = 1, 32 do
    bitmap[x] = {}
    for y = 1, 32 do
      bitmap[x][y] = 0
    end
  end
end

function update()
  -- clear and get mouse
  pixl.clear(5)
  local mx, my = pixl.mouse()
  local A = pixl.btn('A')
  local B = pixl.btnp('B')

  -- draw editor
  local px_size
  if bitmap_size == 8 then px_size = 16
  elseif bitmap_size == 16 then px_size = 8
  elseif bitmap_size == 32 then px_size = 4
  end
  for x = 1, bitmap_size do
    local xl, xh = (x - 1) * px_size, x * px_size - 1
    for y = 1, bitmap_size do
      local yl, yh = (y - 1) * px_size, y * px_size - 1
      if mx >= xl and mx <= xh and my >= yl and my <= yh then
        pixl.print(12, 200, 16, string.format('%dx%d', x - 1, y - 1))
        if A then
          bitmap[x][y] = color
        elseif B then
          fill(x, y)
        end
      end
      pixl.fill(bitmap[x][y], xl, yl, xh, yh)
    end
  end

  -- draw grid
  if show_grid then
    for i = 1, bitmap_size + 1 do
      local j = (i - 1) * px_size
      pixl.line(0, j, 0, j, bitmap_size * px_size)
      pixl.line(0, 0, j, bitmap_size * px_size, j)
    end
  end

  -- draw sprite
  local bytes = {}
  for y = 1, bitmap_size do
    for x = 1, bitmap_size do
      bytes[#bytes + 1] = string.format('%x', bitmap[x][y])
    end
  end
  bytes = table.concat(bytes)
  pixl.sprite(144, 8, bytes)

  -- draw color map
  local c = 0
  for y = 1, 4 do
    for x = 1, 4 do
      local xl, xh = (x - 1) * 8 + 144, x * 8 + 144 - 1
      local yl, yh = (y - 1) * 8 + 64, y * 8 + 64 - 1
      if A and mx >= xl and mx <= xh and my >= yl and my <= yh then
        color = c
      end
      pixl.fill(c, xl, yl, xh, yh)
      if color == c then pixl.rect((c + 8) & 15, xl, yl, xh, yh) end
      c = c + 1
    end
  end

  -- draw menus
  local menuX = 0
  local menuY = 144
  A = pixl.btnp('A')
  local function menu(str)
    local xl, xh = menuX, menuX + #str * 8 - 1
    local yl, yh = menuY, menuY + 8 - 1
    local hover = mx >= xl and mx <= xh and my >= yl and my <= yh
    menuX = menuX + #str * 8 + 8
    if hover and A then
      pixl.print(11, xl, yl, str)
      return true
    elseif hover then pixl.print(10, xl, yl, str)
    else pixl.print(9, xl, yl, str)
    end
  end

  if menu('Clear') then
    for x = 1, bitmap_size do
      for y = 1, bitmap_size do
        bitmap[x][y] = color
      end
    end
  end
  if menu('Save') then
    pixl.clipboard(bytes)
  end
  if menu('Load') then
    local data = pixl.clipboard()
    if data then
      local map = { ['0'] = 0, ['1'] = 1, ['2'] = 2, ['3'] = 3, ['4'] = 4, ['5'] = 5, ['6'] = 6, ['7'] = 7, ['8'] = 8, ['9'] = 9, a = 10, b = 11, c = 12, d = 13, e = 14, f = 15 }
      data = string.lower(data)
      local i = 1
      for y = 1, bitmap_size do
        for x = 1, bitmap_size do
          bitmap[x][y] = map[string.sub(data, i, i)] or 0
          i = i + 1
        end
      end
    end
  end
  if menu('Grid') then show_grid = not show_grid end

  -- size menus
  menuX, menuY = 0, 144 + 8
  if menu('8x8') then bitmap_size = 8 end
  if menu('16x16') then bitmap_size = 16 end
  if menu('32x32') then bitmap_size = 32 end

  -- draw cursor
  pixl.sprite(mx, my, cursor_sprite, 8)
end
