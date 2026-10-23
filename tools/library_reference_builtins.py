"""编译器识别的应用层内建 API 的等价调用签名，不作为可导入的语言源码。"""

BUILTINS = {
    'std::lang': ('source/semantics/boxing.cpp', '''
/** 输出格式化文本，返回过程状态；占位符和变长实参由编译器检查。 */
int64 print(String format, Any args...);
/** 输出格式化文本并换行。 */
int64 println(String format, Any args...);
/** 格式化写入标准错误流，不追加换行。 */
int64 eprint(String format, Any args...);
/** 格式化写入标准错误流并追加换行。 */
int64 eprintln(String format, Any args...);
/** 返回格式化的新字符串。 */
String format(String format, Any args...);
/** 输出一个字符。 */
int64 putChar(char value);
/** 调试暂停，提示写入标准错误；非交互输入立即返回。 */
int64 pause();
/** 读取下一项原文，输入项按空白分隔，不是整行。 */
String inputLine();
/** 读取下一项有符号整数。 */
int64 inputInt();
/** 读取下一项无符号整数。 */
uint64 inputUInt();
/** 读取下一项浮点数。 */
float64 inputFloat();
/** 读取下一项布尔值。 */
bool inputBool();
/** 读取下一项字符。 */
char inputChar();
/** 读取下一项任意精度整数。 */
BigInteger inputBigInteger();
/** 读取下一项任意精度小数。 */
BigDecimal inputBigDecimal();
/** 终止当前执行并报告 message。 */
int64 panic(String message);
/** 返回字符串字节长度、数组元素数量或容器长度。 */
int64 len(Any value);
/** 将可选可写引用转换成可写引用；None 无法转为有效引用。 */
T mut& toReference<T>(Option<T mut&> value);
/** 将可写引用封装成可选引用，不复制指向的对象。 */
Option<T mut&> toOptional<T>(T mut& value);
/** 按字节复制内存；调用者保证源与目标存储范围有效。 */
int64 memcpy(char mut& destination, char const& source, int64 bytes);
/** 按字节填充存储；调用者保证目标范围有效。 */
int64 memset(char mut& destination, int64 value, int64 bytes);
struct List<T> {
    List<T> copy(); int64 push(T value); int64 insert(int64 index, T value);
    T at(int64 index); T pop(); int64 remove(int64 index);
    int64 indexOf(T value); bool contains(T value); bool has(T value);
    int64 clear(); int64 len();
};
struct Set<T> {
    Set<T> copy(); int64 add(T value); int64 has(T value); int64 contains(T value);
    int64 remove(T value); T at(int64 index); int64 clear(); int64 len();
};
struct Map<K,V> {
    Map<K,V> copy(); int64 put(K key,V value); int64 has(K key); int64 contains(K key);
    int64 remove(K key); K keyAt(int64 index); V valAt(int64 index); int64 clear(); int64 len();
};
/** 任意精度整数：算术、数论、素性、位运算与进制互转，详见 example/samples/bigint_methods.chl。 */
struct BigInteger {
    String str(); BigInteger copy();
    /** 非负整数次幂（快速幂）。 */
    BigInteger pow(int64 exponent);
    /** 模幂：`this^exponent mod modulus`（指数可以是大整数）。 */
    BigInteger powmod(BigInteger exponent, BigInteger modulus);
    /** 最大公约数 / 最小公倍数（取绝对值）。 */
    BigInteger gcd(BigInteger other); BigInteger lcm(BigInteger other);
    /** 非负取模：结果落在 [0, |modulus|)。 */
    BigInteger mod(BigInteger modulus);
    /** 模逆元；两数不互素时运行期报错。 */
    BigInteger modInverse(BigInteger modulus);
    /** Miller-Rabin 概率素性测试（固定种子，可复现）。 */
    bool isProbablePrime(int64 rounds);
    /** 严格大于当前值的下一个（概率）素数。 */
    BigInteger nextProbablePrime();
    /** 二进制位长。 */
    int64 bitLength();
    /** 按位与 / 或 / 异或（仅非负数；负数运行期报错）。 */
    BigInteger bitAnd(BigInteger other); BigInteger bitOr(BigInteger other); BigInteger bitXor(BigInteger other);
    /** 2..36 进制字符串（数字用 0-9a-z，负数带 -）。 */
    String strRadix(int64 base);
    /** 整数 k 次方根（向下取整）。 */
    BigInteger iroot(int64 k);
};
/** 任意精度十进制浮点：尾数 × 10^指数。 */
struct BigDecimal { String str(); BigDecimal copy(); int64 setPrecision(int64 digits);
    /** 精确非负整数次幂。 */
    BigDecimal pow(int64 exponent);
    /** 绝对值。 */
    BigDecimal abs();
    /** 是不是整数。 */
    bool isInteger();
    /** 向负无穷 / 向正无穷 / 向零取整，返回大整数。 */
    BigInteger floor(); BigInteger ceil(); BigInteger trunc();
    /** 平方根，保留 digits 位有效数字。 */
    BigDecimal sqrt(int64 digits);
    /** k 次方根，保留 digits 位有效数字。 */
    BigDecimal root(int64 k, int64 digits);
};
struct Any {
    Any copy(); bool isEmpty(); void clear(); int64 set(T value);
    T getValue(); T mut& getReference(); Option<T mut&> getOptional();
};
struct Math { static const float64 PI; static const float64 E; };
/** 内建运算函数，等价于 left == right。 */
bool eq(T left,T right);
/** 内建运算函数，等价于 left != right。 */
bool ne(T left,T right);
/** 内建运算函数，等价于 left < right。 */
bool lt(T left,T right);
/** 内建运算函数，等价于 left <= right。 */
bool le(T left,T right);
/** 内建运算函数，等价于 left > right。 */
bool gt(T left,T right);
/** 内建运算函数，等价于 left >= right。 */
bool ge(T left,T right);
/** 内建运算函数，等价于 left + right，结果由运算符解析决定。 */
R add(T left,U right);
/** 内建运算函数，等价于 left - right。 */
R sub(T left,U right);
/** 内建运算函数，等价于 left * right。 */
R mul(T left,U right);
/** 内建运算函数，等价于 left / right。 */
R div(T left,U right);
/** 内建运算函数，等价于 left % right。 */
R mod(T left,U right);
/** 读取连续存储的第 index 项，相当于下标操作。 */
T index(T const& data,int64 index);
'''),
    'std::algorithms': ('source/semantics/boxing.cpp', '''
/** 返回升序排序副本，原列表保持不变。 */
List<T> sortValue<T>(List<T> items);
/** 线性查找，返回首个下标；未找到为 -1。 */
int64 findValue<T>(List<T> items,T value);
/** 在已升序排序的列表二分查找，未找到为 -1。 */
int64 binarySearchValue<T>(List<T> items,T value);
/** 返回参数中的最小值，至少一个实参。 */
T minValue<T>(T first,T rest...);
/** 返回参数中的最大值，至少一个实参。 */
T maxValue<T>(T first,T rest...);
/** 将 value 限制到闭区间 [low,high]。 */
T clampValue<T>(T value,T low,T high);
/** 判断全部元素是否满足谓词，空列表为 true。 */
bool allValue<T>(List<T> items,bool fn(T) predicate);
/** 判断是否有元素满足谓词，空列表为 false。 */
bool anyValue<T>(List<T> items,bool fn(T) predicate);
/** 从 initial 开始顺序累积。 */
T foldValue<T>(List<T> items,T initial,T fn(T,T) folder);
/** 对各元素调用 mapper，返回新列表；R 由回调结果类型推导。 */
List<R> mapValue<T>(List<T> items,R fn(T) mapper);
/** 返回满足 predicate 的元素列表副本。 */
List<T> filterValue<T>(List<T> items,bool fn(T) predicate);
/** 用首元素作初值累积；空列表报错。 */
T reduceValue<T>(List<T> items,T fn(T,T) folder);
/** 从显式 initial 累积，等价 foldValue。 */
T reduceValue<T>(List<T> items,T initial,T fn(T,T) folder);
/** 对数值列表求和。 */
T sumValue<T>(List<T> items);
/** 统计等于 value 的元素数量。 */
int64 countValue<T>(List<T> items,T value);
/** 统计满足 predicate 的元素数量。 */
int64 countValue<T>(List<T> items,bool fn(T) predicate);
'''),
}
