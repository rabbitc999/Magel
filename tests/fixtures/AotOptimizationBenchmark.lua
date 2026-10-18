-- 与 Magel AOT 对照的同算法负载；标准输入给出迭代次数与重复轮数。
local MODULUS = 1000000007

local function scalar(n)
  local value = 1
  for i = 0, n - 1 do value = (value * 17 + i) % MODULUS end
  return value
end

-- Lua 的 table 既是定长数组也是动态数组，fixed_array 与 heap_array 语义一致。
local function arrayKernel(n)
  local values = {}
  for j = 1, 64 do values[j] = 0 end
  local total = 0
  for i = 0, n - 1 do
    local k = (i % 64) + 1
    values[k] = i + 1
    total = total + values[k]
  end
  return total
end

local function container(n)
  local values = {}
  for i = 0, 63 do values[i + 1] = i end
  local total = 0
  for i = 0, n - 1 do
    local k = (i % 64) + 1
    values[k] = i + 1
    total = total + values[k]
  end
  return total
end

local function callStep(value, i) return (value * 17 + i) % MODULUS end

local function functionCalls(n)
  local value = 1
  for i = 0, n - 1 do value = callStep(value, i) end
  return value
end

local function report(name, elapsed, value, repeatIndex)
  if repeatIndex >= 0 then
    print(name .. " " .. string.format("%.0f", elapsed * 1e9) .. " " .. string.format("%d", value))
  end
end

local n = math.tointeger(io.read("n"))
local repeats = math.tointeger(io.read("n"))
for r = -1, repeats - 1 do
  local begin = os.clock(); local value = scalar(n); local elapsed = os.clock() - begin
  report("scalar", elapsed, value, r)
  begin = os.clock(); value = arrayKernel(n); elapsed = os.clock() - begin
  report("fixed_array", elapsed, value, r)
  begin = os.clock(); value = arrayKernel(n); elapsed = os.clock() - begin
  report("heap_array", elapsed, value, r)
  begin = os.clock(); value = container(n); elapsed = os.clock() - begin
  report("container", elapsed, value, r)
  begin = os.clock(); value = functionCalls(n); elapsed = os.clock() - begin
  report("function_calls", elapsed, value, r)
end
