# VM and AOT baselines: 30 complete test sources

Five languages and five workloads, with an additional Magel AOT timing harness per workload. Both versions share each VM/AOT input; JVM modes share Java input. Only trailing spaces are removed. See the [performance report](../12-vm-performance.md).

## integer_loop

### Magel VM

```cpp
int64 mix(int64 x){return x*17;}
int64 main(){int64 n=inputInt(),i=0;int64 total=0;while(i<n){total+=i;i+=1;}println("{}",total);return 0;}
```

### Magel AOT 0.18.3 / 0.18.2

```cpp
import std::clock;
int64 mix(int64 x){return x*17;}
int64 calculate(int64 n){int64 i=0;int64 total=0;while(i<n){total+=i;i+=1;}return total;}
int64 main(){int64 first=inputInt();int64 warmup=calculate(first);int64 n=inputInt();int64 start=std::clock::nowNanos();int64 value=calculate(n);int64 elapsed=std::clock::nowNanos()-start;if(warmup!=value){return 1;}println("{} {}",elapsed/1000000.0,value);return 0;}
```

### Java / JVM

```java
import java.util.Scanner;
public final class Case_integer_loop {
    private static volatile long sink;
    private static long mix(long x) { return x*17; }
    private static long calculate(long n) {
        long i=0; long total=0;
        while(i<n) { total+=i; ++i; }
        return total;
    }
    public static void main(String[] args) {
        Scanner input=new Scanner(System.in);
        int warmups=Integer.parseInt(args[0]);
        for(int i=0;i<warmups;++i) sink=calculate(input.nextLong());
        long n=input.nextLong();
        long start=System.nanoTime();
        long value=calculate(n);
        long elapsed=System.nanoTime()-start;
        sink=value;
        System.out.println((elapsed/1e6)+" "+value);
    }
}
```

### Python

```python
import sys,time
n=int(sys.argv[1])
def mix(x): return x*17
def calculate(n):
    i,total=0,0
    values=[1,2,3,4]
    while i<n:
        total += i
        i+=1
    return total
calculate(n)
start=time.perf_counter_ns()
result=calculate(n)
print((time.perf_counter_ns()-start)/1e6,result)
```

### Lua

```lua
local n=tonumber(arg[1])
local function mix(x) return x*17 end
local function calculate(n)
local i,total=0,0
local values={1,2,3,4}
while i<n do total=total+i;i=i+1 end
return total end
calculate(n)
local start=os.clock()
local result=calculate(n)
print((os.clock()-start)*1000,result)
```

### C++

```cpp
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
static volatile std::int64_t sink;
static std::int64_t mix(std::int64_t x) { return x*17; }
__attribute__((noinline)) static std::int64_t calculate(std::int64_t n) {
    std::int64_t i=0; std::int64_t total=0;
    while(i<n) { total+=i; ++i; }
    return total;
}
int main() {
    std::int64_t n; if(!(std::cin>>n)) return 2;
    sink=calculate(n);
    if(!(std::cin>>n)) return 2;
    const auto start=std::chrono::steady_clock::now();
    const auto value=calculate(n);
    const auto end=std::chrono::steady_clock::now();
    sink=value;
    std::cout<<std::setprecision(17)<<std::chrono::duration<double,std::milli>(end-start).count()<<' '<<value<<'\n';
}
```

## branches

### Magel VM

```cpp
int64 mix(int64 x){return x*17;}
int64 main(){int64 n=inputInt(),i=0;int64 total=0;while(i<n){if((i&1)==0){total+=i;}else{total-=i;}i+=1;}println("{}",total);return 0;}
```

### Magel AOT 0.18.3 / 0.18.2

```cpp
import std::clock;
int64 mix(int64 x){return x*17;}
int64 calculate(int64 n){int64 i=0;int64 total=0;while(i<n){if((i&1)==0){total+=i;}else{total-=i;}i+=1;}return total;}
int64 main(){int64 first=inputInt();int64 warmup=calculate(first);int64 n=inputInt();int64 start=std::clock::nowNanos();int64 value=calculate(n);int64 elapsed=std::clock::nowNanos()-start;if(warmup!=value){return 1;}println("{} {}",elapsed/1000000.0,value);return 0;}
```

### Java / JVM

```java
import java.util.Scanner;
public final class Case_branches {
    private static volatile long sink;
    private static long mix(long x) { return x*17; }
    private static long calculate(long n) {
        long i=0; long total=0;
        while(i<n) { if((i&1)==0){total+=i;}else{total-=i;} ++i; }
        return total;
    }
    public static void main(String[] args) {
        Scanner input=new Scanner(System.in);
        int warmups=Integer.parseInt(args[0]);
        for(int i=0;i<warmups;++i) sink=calculate(input.nextLong());
        long n=input.nextLong();
        long start=System.nanoTime();
        long value=calculate(n);
        long elapsed=System.nanoTime()-start;
        sink=value;
        System.out.println((elapsed/1e6)+" "+value);
    }
}
```

### Python

```python
import sys,time
n=int(sys.argv[1])
def mix(x): return x*17
def calculate(n):
    i,total=0,0
    values=[1,2,3,4]
    while i<n:
        total += i if (i & 1)==0 else -i
        i+=1
    return total
calculate(n)
start=time.perf_counter_ns()
result=calculate(n)
print((time.perf_counter_ns()-start)/1e6,result)
```

### Lua

```lua
local n=tonumber(arg[1])
local function mix(x) return x*17 end
local function calculate(n)
local i,total=0,0
local values={1,2,3,4}
while i<n do if (i & 1)==0 then total=total+i else total=total-i end;i=i+1 end
return total end
calculate(n)
local start=os.clock()
local result=calculate(n)
print((os.clock()-start)*1000,result)
```

### C++

```cpp
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
static volatile std::int64_t sink;
static std::int64_t mix(std::int64_t x) { return x*17; }
__attribute__((noinline)) static std::int64_t calculate(std::int64_t n) {
    std::int64_t i=0; std::int64_t total=0;
    while(i<n) { if((i&1)==0){total+=i;}else{total-=i;} ++i; }
    return total;
}
int main() {
    std::int64_t n; if(!(std::cin>>n)) return 2;
    sink=calculate(n);
    if(!(std::cin>>n)) return 2;
    const auto start=std::chrono::steady_clock::now();
    const auto value=calculate(n);
    const auto end=std::chrono::steady_clock::now();
    sink=value;
    std::cout<<std::setprecision(17)<<std::chrono::duration<double,std::milli>(end-start).count()<<' '<<value<<'\n';
}
```

## float64_loop

### Magel VM

```cpp
int64 mix(int64 x){return x*17;}
int64 main(){int64 n=inputInt(),i=0;float64 total=0;while(i<n){total+=0.5;i+=1;}println("{}",total);return 0;}
```

### Magel AOT 0.18.3 / 0.18.2

```cpp
import std::clock;
int64 mix(int64 x){return x*17;}
float64 calculate(int64 n){int64 i=0;float64 total=0;while(i<n){total+=0.5;i+=1;}return total;}
int64 main(){int64 first=inputInt();float64 warmup=calculate(first);int64 n=inputInt();int64 start=std::clock::nowNanos();float64 value=calculate(n);int64 elapsed=std::clock::nowNanos()-start;if(warmup!=value){return 1;}println("{} {}",elapsed/1000000.0,value);return 0;}
```

### Java / JVM

```java
import java.util.Scanner;
public final class Case_float64_loop {
    private static volatile double sink;
    private static long mix(long x) { return x*17; }
    private static double calculate(long n) {
        long i=0; double total=0;
        while(i<n) { total+=0.5; ++i; }
        return total;
    }
    public static void main(String[] args) {
        Scanner input=new Scanner(System.in);
        int warmups=Integer.parseInt(args[0]);
        for(int i=0;i<warmups;++i) sink=calculate(input.nextLong());
        long n=input.nextLong();
        long start=System.nanoTime();
        double value=calculate(n);
        long elapsed=System.nanoTime()-start;
        sink=value;
        System.out.println((elapsed/1e6)+" "+value);
    }
}
```

### Python

```python
import sys,time
n=int(sys.argv[1])
def mix(x): return x*17
def calculate(n):
    i,total=0,0.0
    values=[1,2,3,4]
    while i<n:
        total += 0.5
        i+=1
    return total
calculate(n)
start=time.perf_counter_ns()
result=calculate(n)
print((time.perf_counter_ns()-start)/1e6,result)
```

### Lua

```lua
local n=tonumber(arg[1])
local function mix(x) return x*17 end
local function calculate(n)
local i,total=0,0.0
local values={1,2,3,4}
while i<n do total=total+0.5;i=i+1 end
return total end
calculate(n)
local start=os.clock()
local result=calculate(n)
print((os.clock()-start)*1000,result)
```

### C++

```cpp
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
static volatile double sink;
static std::int64_t mix(std::int64_t x) { return x*17; }
__attribute__((noinline)) static double calculate(std::int64_t n) {
    std::int64_t i=0; double total=0;
    while(i<n) { total+=0.5; ++i; }
    return total;
}
int main() {
    std::int64_t n; if(!(std::cin>>n)) return 2;
    sink=calculate(n);
    if(!(std::cin>>n)) return 2;
    const auto start=std::chrono::steady_clock::now();
    const auto value=calculate(n);
    const auto end=std::chrono::steady_clock::now();
    sink=value;
    std::cout<<std::setprecision(17)<<std::chrono::duration<double,std::milli>(end-start).count()<<' '<<value<<'\n';
}
```

## calls

### Magel VM

```cpp
int64 mix(int64 x){return x*17;}
int64 main(){int64 n=inputInt(),i=0;int64 total=0;while(i<n){total+=mix(i);i+=1;}println("{}",total);return 0;}
```

### Magel AOT 0.18.3 / 0.18.2

```cpp
import std::clock;
int64 mix(int64 x){return x*17;}
int64 calculate(int64 n){int64 i=0;int64 total=0;while(i<n){total+=mix(i);i+=1;}return total;}
int64 main(){int64 first=inputInt();int64 warmup=calculate(first);int64 n=inputInt();int64 start=std::clock::nowNanos();int64 value=calculate(n);int64 elapsed=std::clock::nowNanos()-start;if(warmup!=value){return 1;}println("{} {}",elapsed/1000000.0,value);return 0;}
```

### Java / JVM

```java
import java.util.Scanner;
public final class Case_calls {
    private static volatile long sink;
    private static long mix(long x) { return x*17; }
    private static long calculate(long n) {
        long i=0; long total=0;
        while(i<n) { total+=mix(i); ++i; }
        return total;
    }
    public static void main(String[] args) {
        Scanner input=new Scanner(System.in);
        int warmups=Integer.parseInt(args[0]);
        for(int i=0;i<warmups;++i) sink=calculate(input.nextLong());
        long n=input.nextLong();
        long start=System.nanoTime();
        long value=calculate(n);
        long elapsed=System.nanoTime()-start;
        sink=value;
        System.out.println((elapsed/1e6)+" "+value);
    }
}
```

### Python

```python
import sys,time
n=int(sys.argv[1])
def mix(x): return x*17
def calculate(n):
    i,total=0,0
    values=[1,2,3,4]
    while i<n:
        total += mix(i)
        i+=1
    return total
calculate(n)
start=time.perf_counter_ns()
result=calculate(n)
print((time.perf_counter_ns()-start)/1e6,result)
```

### Lua

```lua
local n=tonumber(arg[1])
local function mix(x) return x*17 end
local function calculate(n)
local i,total=0,0
local values={1,2,3,4}
while i<n do total=total+mix(i);i=i+1 end
return total end
calculate(n)
local start=os.clock()
local result=calculate(n)
print((os.clock()-start)*1000,result)
```

### C++

```cpp
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
static volatile std::int64_t sink;
static std::int64_t mix(std::int64_t x) { return x*17; }
__attribute__((noinline)) static std::int64_t calculate(std::int64_t n) {
    std::int64_t i=0; std::int64_t total=0;
    while(i<n) { total+=mix(i); ++i; }
    return total;
}
int main() {
    std::int64_t n; if(!(std::cin>>n)) return 2;
    sink=calculate(n);
    if(!(std::cin>>n)) return 2;
    const auto start=std::chrono::steady_clock::now();
    const auto value=calculate(n);
    const auto end=std::chrono::steady_clock::now();
    sink=value;
    std::cout<<std::setprecision(17)<<std::chrono::duration<double,std::milli>(end-start).count()<<' '<<value<<'\n';
}
```

## array_reads

### Magel VM

```cpp
int64 mix(int64 x){return x*17;}
int64 main(){int64 n=inputInt(),i=0;int64 total=0;int64 values[4];values[0]=1;values[1]=2;values[2]=3;values[3]=4;while(i<n){total+=values[i%4];i+=1;}println("{}",total);return 0;}
```

### Magel AOT 0.18.3 / 0.18.2

```cpp
import std::clock;
int64 mix(int64 x){return x*17;}
int64 calculate(int64 n){int64 i=0;int64 total=0;int64 values[4];values[0]=1;values[1]=2;values[2]=3;values[3]=4;while(i<n){total+=values[i%4];i+=1;}return total;}
int64 main(){int64 first=inputInt();int64 warmup=calculate(first);int64 n=inputInt();int64 start=std::clock::nowNanos();int64 value=calculate(n);int64 elapsed=std::clock::nowNanos()-start;if(warmup!=value){return 1;}println("{} {}",elapsed/1000000.0,value);return 0;}
```

### Java / JVM

```java
import java.util.Scanner;
public final class Case_array_reads {
    private static volatile long sink;
    private static long mix(long x) { return x*17; }
    private static long calculate(long n) {
        long i=0; long total=0; long[] values={1,2,3,4};
        while(i<n) { total+=values[(int)(i%4)]; ++i; }
        return total;
    }
    public static void main(String[] args) {
        Scanner input=new Scanner(System.in);
        int warmups=Integer.parseInt(args[0]);
        for(int i=0;i<warmups;++i) sink=calculate(input.nextLong());
        long n=input.nextLong();
        long start=System.nanoTime();
        long value=calculate(n);
        long elapsed=System.nanoTime()-start;
        sink=value;
        System.out.println((elapsed/1e6)+" "+value);
    }
}
```

### Python

```python
import sys,time
n=int(sys.argv[1])
def mix(x): return x*17
def calculate(n):
    i,total=0,0
    values=[1,2,3,4]
    while i<n:
        total += values[i%4]
        i+=1
    return total
calculate(n)
start=time.perf_counter_ns()
result=calculate(n)
print((time.perf_counter_ns()-start)/1e6,result)
```

### Lua

```lua
local n=tonumber(arg[1])
local function mix(x) return x*17 end
local function calculate(n)
local i,total=0,0
local values={1,2,3,4}
while i<n do total=total+values[(i%4)+1];i=i+1 end
return total end
calculate(n)
local start=os.clock()
local result=calculate(n)
print((os.clock()-start)*1000,result)
```

### C++

```cpp
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
static volatile std::int64_t sink;
static std::int64_t mix(std::int64_t x) { return x*17; }
__attribute__((noinline)) static std::int64_t calculate(std::int64_t n) {
    std::int64_t i=0; std::int64_t total=0; std::int64_t values[4]={1,2,3,4};
    while(i<n) { total+=values[i%4]; ++i; }
    return total;
}
int main() {
    std::int64_t n; if(!(std::cin>>n)) return 2;
    sink=calculate(n);
    if(!(std::cin>>n)) return 2;
    const auto start=std::chrono::steady_clock::now();
    const auto value=calculate(n);
    const auto end=std::chrono::steady_clock::now();
    sink=value;
    std::cout<<std::setprecision(17)<<std::chrono::duration<double,std::milli>(end-start).count()<<' '<<value<<'\n';
}
```
