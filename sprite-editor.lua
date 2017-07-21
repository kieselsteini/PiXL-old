--[[----------------------------------------------------------------------------

      PiXL - a simple sprite editor

--]]----------------------------------------------------------------------------
local pixl = require 'pixl'
local bitmap
local color = 10
local cursor_sprite = '008888880f08888807f08888077f08880777f0880777ff080077f00880000088'

function init()
  bitmap = {}
  for x = 1, 8 do
    bitmap[x] = {}
    for y = 1, 8 do
      bitmap[x][y] = 0
    end
  end
end

function update()
  -- clear and get mouse
  pixl.clear(5)
  local mx, my = pixl.mouse()
  local A = pixl.btn('A')

  -- draw editor
  for x = 1, 8 do
    local xl, xh = (x - 1) * 16, x * 16 - 1
    for y = 1, 8 do
      local yl, yh = (y - 1) * 16, y * 16 - 1
      if A and mx >= xl and mx <= xh and my >= yl and my <= yh then
        bitmap[x][y] = color
      end
      pixl.fill(bitmap[x][y], xl, yl, xh, yh)
    end
  end

  -- draw sprite
  local bytes = {}
  for y = 1, 8 do
    for x = 1, 8 do
      bytes[#bytes + 1] = string.format('%x', bitmap[x][y])
    end
  end
  bytes = table.concat(bytes)
  pixl.sprite(144, 16, bytes)

  -- draw color map
  local c = 0
  for y = 1, 4 do
    for x = 1, 4 do
      local xl, xh = (x - 1) * 8 + 144, x * 8 + 144 - 1
      local yl, yh = (y - 1) * 8 + 32, y * 8 + 32 - 1
      if A and mx >= xl and mx <= xh and my >= yl and my <= yh then
        color = c
      end
      pixl.fill(c, xl, yl, xh, yh)
      --pixl.print((c + 5) & 15, xl, yl, string.format('%X', c))
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
    for x = 1, 8 do
      for y = 1, 8 do
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
      for y = 1, 8 do
        for x = 1, 8 do
          bitmap[x][y] = map[string.sub(data, i, i)]
          i = i + 1
        end
      end
    end
  end

  -- palette menus
  menuX, menuY = 0, 144 + 8

  if menu('PiXL') then pixl.palette('pixl') end
  if menu('C64') then pixl.palette('c64') end
  if menu('CGA') then pixl.palette('cga') end
  if menu('16pal') then pixl.palette('16pal') end

  -- draw cursor
  pixl.sprite(mx, my, cursor_sprite, 8)
end
