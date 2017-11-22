--[[----------------------------------------------------------------------------

      sts - a module of Lua helper routines

  MIT License

  Copyright (c) 2017 Sebastian Steinhauer

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

--]]----------------------------------------------------------------------------

local M = {}

-- local often used functions for speed
local pairs = pairs
local type = type
local setmetatable = setmetatable

--------------------------------------------------------------------------------

function M.map(object, fn)
  local new = {}
  for k, v in pairs(object) do
    new[k] = fn(v, k, object)
  end
  return new
end

function M.reduce(object, fn, accumulator)
  for k, v in pairs(object) do
    accumulator = fn(accumulator, v, k, object)
  end
  return accumulator
end

function M.filter(object, fn, retain_keys)
  local new = {}
  if retain_keys then
    for k, v in pairs(object) do
      if fn(v, k, object) then
        new[k] = v
      end
    end
  else
    for k, v in pairs(object) do
      if fn(v, k, object) then
        new[#new + 1] = v
      end
    end
  end
  return new
end

function M.size(object)
  local count = 0
  for _, v in pairs(object) do
    count = count + 1
  end
  return count
end

function M.each(object, fn)
  for k, v in pairs(object) do
    fn(v, k, object)
  end
  return object
end

function M.keys(object)
  local new = {}
  for k in pairs(object) do
    new[#new + 1] = k
  end
  return new
end

function M.values(object)
  local new = {}
  for _, v in pairs(object) do
    new[#new + 1] = v
  end
  return new
end

function M.assign(object, other)
  for k, v in pairs(other) do
    object[k] = v
  end
  return object
end

function M.merge(object, other)
  for k, v in pairs(other) do
    if type(v) == 'table' then
      object[k] = M.merge({}, v)
    else
      object[k] = v
    end
  end
  return object
end

function M.clone(object)
  return M.merge({}, object)
end

--------------------------------------------------------------------------------

local chain_functions = M.map(M, function(fn)
  return function(self, ...)
    self._value = fn(self._value, ...)
    return self
  end
end)

chain_functions.value = function(self)
  return self._value
end

function M.chain(object)
  return setmetatable({ _value = object }, { __index = chain_functions })
end

--------------------------------------------------------------------------------

function M.once(fn)
  local called = false
  return function(...)
    if not called then fn(...) end
  end
end

--------------------------------------------------------------------------------

function M.clamp(value, min, max)
  if value < min then
    return min
  elseif value > max then
    return max
  else
    return value
  end
end

--------------------------------------------------------------------------------

M.Object = {
  clone = M.clone,
  assign = M.assign,
  merge = M.merge,
}


return M
