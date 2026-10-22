# 全部库函数符号

[返回库目录](README.md)

| 限定名 | 签名 |
| --- | --- |
| [std::algorithms::allValue](std-algorithms.md#api-18) | `bool allValue<T>(List<T> items,bool fn(T) predicate)` |
| [std::algorithms::anyValue](std-algorithms.md#api-19) | `bool anyValue<T>(List<T> items,bool fn(T) predicate)` |
| [std::algorithms::binarySearchReference](std-algorithms.md#api-7) | `int64 binarySearchReference<T>(T const& data, int64 count, T value)` |
| [std::algorithms::binarySearchValue](std-algorithms.md#api-14) | `int64 binarySearchValue<T>(List<T> items,T value)` |
| [std::algorithms::choiceValue](std-algorithms.md#api-0) | `T choiceValue<T>(std::lang::List<T> items, uint64 seed)` |
| [std::algorithms::clampValue](std-algorithms.md#api-17) | `T clampValue<T>(T value,T low,T high)` |
| [std::algorithms::countReference](std-algorithms.md#api-10) | `int64 countReference<T>(T const& data, int64 count, T value)` |
| [std::algorithms::countValue](std-algorithms.md#api-26) | `int64 countValue<T>(List<T> items,T value)` |
| [std::algorithms::countValue](std-algorithms.md#api-27) | `int64 countValue<T>(List<T> items,bool fn(T) predicate)` |
| [std::algorithms::filterValue](std-algorithms.md#api-22) | `List<T> filterValue<T>(List<T> items,bool fn(T) predicate)` |
| [std::algorithms::findReference](std-algorithms.md#api-6) | `int64 findReference<T>(T const& data, int64 count, T value)` |
| [std::algorithms::findValue](std-algorithms.md#api-13) | `int64 findValue<T>(List<T> items,T value)` |
| [std::algorithms::foldValue](std-algorithms.md#api-20) | `T foldValue<T>(List<T> items,T initial,T fn(T,T) folder)` |
| [std::algorithms::mapValue](std-algorithms.md#api-21) | `List<R> mapValue<T>(List<T> items,R fn(T) mapper)` |
| [std::algorithms::maxReference](std-algorithms.md#api-9) | `Option<T mut&> maxReference<T>(T mut& data, int64 count)` |
| [std::algorithms::maxValue](std-algorithms.md#api-16) | `T maxValue<T>(T first,T rest...)` |
| [std::algorithms::minReference](std-algorithms.md#api-8) | `Option<T mut&> minReference<T>(T mut& data, int64 count)` |
| [std::algorithms::minValue](std-algorithms.md#api-15) | `T minValue<T>(T first,T rest...)` |
| [std::algorithms::reduceValue](std-algorithms.md#api-23) | `T reduceValue<T>(List<T> items,T fn(T,T) folder)` |
| [std::algorithms::reduceValue](std-algorithms.md#api-24) | `T reduceValue<T>(List<T> items,T initial,T fn(T,T) folder)` |
| [std::algorithms::reverseReference](std-algorithms.md#api-5) | `void reverseReference<T>(T mut& data, int64 count)` |
| [std::algorithms::shuffleReference](std-algorithms.md#api-2) | `void shuffleReference<T>(T mut& data, int64 count, uint64 seed)` |
| [std::algorithms::shuffleValue](std-algorithms.md#api-1) | `std::lang::List<T> shuffleValue<T>(std::lang::List<T> items, uint64 seed)` |
| [std::algorithms::sortReference](std-algorithms.md#api-4) | `void sortReference<T>(T mut& data, int64 count)` |
| [std::algorithms::sortValue](std-algorithms.md#api-12) | `List<T> sortValue<T>(List<T> items)` |
| [std::algorithms::sumReference](std-algorithms.md#api-11) | `T sumReference<T>(T mut& data, int64 count)` |
| [std::algorithms::sumValue](std-algorithms.md#api-25) | `T sumValue<T>(List<T> items)` |
| [std::algorithms::zipValue](std-algorithms.md#api-3) | `std::lang::List<(T, U)> zipValue<T, U>(std::lang::List<T> left, std::lang::List<U> right)` |
| [std::binary::BinaryReader::drop](std-binary.md#api-0) | `void drop()` |
| [std::binary::BinaryReader::fromArray](std-binary.md#api-3) | `static BinaryReader fromArray(uint8 bytes[])` |
| [std::binary::BinaryReader::fromArray](std-binary.md#api-2) | `static BinaryReader fromArray(uint8 const& bytes, int64 count)` |
| [std::binary::BinaryReader::fromBytes](std-binary.md#api-1) | `static BinaryReader fromBytes(std::lang::List<uint8> bytes)` |
| [std::binary::BinaryReader::position](std-binary.md#api-4) | `int64 position()` |
| [std::binary::BinaryReader::readBool](std-binary.md#api-30) | `bool readBool()` |
| [std::binary::BinaryReader::readBoolArray](std-binary.md#api-31) | `std::lang::List<bool> readBoolArray(int64 count)` |
| [std::binary::BinaryReader::readBytes](std-binary.md#api-32) | `std::lang::List<uint8> readBytes(int64 count)` |
| [std::binary::BinaryReader::readFloat32](std-binary.md#api-26) | `float32 readFloat32()` |
| [std::binary::BinaryReader::readFloat32Array](std-binary.md#api-27) | `std::lang::List<float32> readFloat32Array(int64 count)` |
| [std::binary::BinaryReader::readFloat64](std-binary.md#api-28) | `float64 readFloat64()` |
| [std::binary::BinaryReader::readFloat64Array](std-binary.md#api-29) | `std::lang::List<float64> readFloat64Array(int64 count)` |
| [std::binary::BinaryReader::readInt16](std-binary.md#api-14) | `int16 readInt16()` |
| [std::binary::BinaryReader::readInt16Array](std-binary.md#api-15) | `std::lang::List<int16> readInt16Array(int64 count)` |
| [std::binary::BinaryReader::readInt32](std-binary.md#api-18) | `int32 readInt32()` |
| [std::binary::BinaryReader::readInt32Array](std-binary.md#api-19) | `std::lang::List<int32> readInt32Array(int64 count)` |
| [std::binary::BinaryReader::readInt64](std-binary.md#api-22) | `int64 readInt64()` |
| [std::binary::BinaryReader::readInt64Array](std-binary.md#api-23) | `std::lang::List<int64> readInt64Array(int64 count)` |
| [std::binary::BinaryReader::readInt8](std-binary.md#api-10) | `int8 readInt8()` |
| [std::binary::BinaryReader::readInt8Array](std-binary.md#api-11) | `std::lang::List<int8> readInt8Array(int64 count)` |
| [std::binary::BinaryReader::readString](std-binary.md#api-33) | `std::lang::String readString(int64 length)` |
| [std::binary::BinaryReader::readStringArray](std-binary.md#api-34) | `std::lang::List<std::lang::String> readStringArray(int64 count, int64 width)` |
| [std::binary::BinaryReader::readUint16](std-binary.md#api-16) | `uint16 readUint16()` |
| [std::binary::BinaryReader::readUint16Array](std-binary.md#api-17) | `std::lang::List<uint16> readUint16Array(int64 count)` |
| [std::binary::BinaryReader::readUint32](std-binary.md#api-20) | `uint32 readUint32()` |
| [std::binary::BinaryReader::readUint32Array](std-binary.md#api-21) | `std::lang::List<uint32> readUint32Array(int64 count)` |
| [std::binary::BinaryReader::readUint64](std-binary.md#api-24) | `uint64 readUint64()` |
| [std::binary::BinaryReader::readUint64Array](std-binary.md#api-25) | `std::lang::List<uint64> readUint64Array(int64 count)` |
| [std::binary::BinaryReader::readUint8](std-binary.md#api-12) | `uint8 readUint8()` |
| [std::binary::BinaryReader::readUint8Array](std-binary.md#api-13) | `std::lang::List<uint8> readUint8Array(int64 count)` |
| [std::binary::BinaryReader::remaining](std-binary.md#api-5) | `int64 remaining()` |
| [std::binary::BinaryReader::seek](std-binary.md#api-8) | `bool seek(int64 position)` |
| [std::binary::BinaryReader::setBigEndian](std-binary.md#api-7) | `void setBigEndian(bool enabled)` |
| [std::binary::BinaryReader::size](std-binary.md#api-6) | `int64 size()` |
| [std::binary::BinaryReader::skip](std-binary.md#api-9) | `bool skip(int64 count)` |
| [std::cas::Expr::acos](std-cas.md#api-38) | `Expr acos()` |
| [std::cas::Expr::asin](std-cas.md#api-37) | `Expr asin()` |
| [std::cas::Expr::ast](std-cas.md#api-4) | `Expr ast()` |
| [std::cas::Expr::atan](std-cas.md#api-39) | `Expr atan()` |
| [std::cas::Expr::ceil](std-cas.md#api-45) | `Expr ceil()` |
| [std::cas::Expr::child](std-cas.md#api-11) | `Expr child(int64 index)` |
| [std::cas::Expr::childCount](std-cas.md#api-10) | `int64 childCount()` |
| [std::cas::Expr::children](std-cas.md#api-12) | `List<Expr> children()` |
| [std::cas::Expr::cos](std-cas.md#api-35) | `Expr cos()` |
| [std::cas::Expr::decimal](std-cas.md#api-15) | `BigDecimal decimal(int64 digits = 50)` |
| [std::cas::Expr::denominator](std-cas.md#api-14) | `BigInteger denominator()` |
| [std::cas::Expr::differentiate](std-cas.md#api-28) | `Expr differentiate(String variable = "x", int64 order = 1)` |
| [std::cas::Expr::exp](std-cas.md#api-40) | `Expr exp()` |
| [std::cas::Expr::expand](std-cas.md#api-25) | `Expr expand()` |
| [std::cas::Expr::factor](std-cas.md#api-26) | `Expr factor()` |
| [std::cas::Expr::floor](std-cas.md#api-44) | `Expr floor()` |
| [std::cas::Expr::fmod](std-cas.md#api-48) | `Expr fmod(Expr other)` |
| [std::cas::Expr::fromDecimal](std-cas.md#api-2) | `static Expr fromDecimal(BigDecimal number)` |
| [std::cas::Expr::fromInteger](std-cas.md#api-1) | `static Expr fromInteger(BigInteger number)` |
| [std::cas::Expr::integrate](std-cas.md#api-29) | `Expr integrate(String variable = "x")` |
| [std::cas::Expr::integrateDefinite](std-cas.md#api-33) | `Expr integrateDefinite(String variable, Expr lower, Expr upper)` |
| [std::cas::Expr::kind](std-cas.md#api-8) | `NodeKind kind()` |
| [std::cas::Expr::latex](std-cas.md#api-7) | `String latex()` |
| [std::cas::Expr::leftAdd](std-cas.md#api-16) | `Expr leftAdd(Expr other)` |
| [std::cas::Expr::leftDiv](std-cas.md#api-19) | `Expr leftDiv(Expr other)` |
| [std::cas::Expr::leftEqual](std-cas.md#api-20) | `bool leftEqual(Expr other)` |
| [std::cas::Expr::leftMul](std-cas.md#api-18) | `Expr leftMul(Expr other)` |
| [std::cas::Expr::leftSub](std-cas.md#api-17) | `Expr leftSub(Expr other)` |
| [std::cas::Expr::limit](std-cas.md#api-31) | `Expr limit(String variable, Expr point)` |
| [std::cas::Expr::ln](std-cas.md#api-41) | `Expr ln()` |
| [std::cas::Expr::log10](std-cas.md#api-42) | `Expr log10()` |
| [std::cas::Expr::name](std-cas.md#api-9) | `String name()` |
| [std::cas::Expr::negate](std-cas.md#api-21) | `Expr negate(Expr operand)` |
| [std::cas::Expr::numerator](std-cas.md#api-13) | `BigInteger numerator()` |
| [std::cas::Expr::parse](std-cas.md#api-0) | `static Expr parse(String text)` |
| [std::cas::Expr::pow](std-cas.md#api-22) | `Expr pow(Expr exponent)` |
| [std::cas::Expr::pow](std-cas.md#api-23) | `Expr pow(int64 exponent)` |
| [std::cas::Expr::round](std-cas.md#api-46) | `Expr round()` |
| [std::cas::Expr::simplify](std-cas.md#api-24) | `Expr simplify()` |
| [std::cas::Expr::sin](std-cas.md#api-34) | `Expr sin()` |
| [std::cas::Expr::sqrt](std-cas.md#api-43) | `Expr sqrt()` |
| [std::cas::Expr::str](std-cas.md#api-5) | `String str()` |
| [std::cas::Expr::substitute](std-cas.md#api-30) | `Expr substitute(String variable, Expr replacement)` |
| [std::cas::Expr::symbol](std-cas.md#api-3) | `static Expr symbol(String name)` |
| [std::cas::Expr::tan](std-cas.md#api-36) | `Expr tan()` |
| [std::cas::Expr::taylor](std-cas.md#api-32) | `Expr taylor(String variable, Expr point, int64 order)` |
| [std::cas::Expr::toString](std-cas.md#api-6) | `String toString()` |
| [std::cas::Expr::together](std-cas.md#api-27) | `Expr together()` |
| [std::cas::Expr::trunc](std-cas.md#api-47) | `Expr trunc()` |
| [std::cas::parse](std-cas.md#api-49) | `Expr parse(String text)` |
| [std::cli::CliArguments::get](std-cli.md#api-0) | `Option<String> get(String name)` |
| [std::cli::CliArguments::getInt](std-cli.md#api-1) | `Result<int64,CliError> getInt(String name)` |
| [std::cli::CliParser::add](std-cli.md#api-4) | `bool add(CliOption option)` |
| [std::cli::CliParser::help](std-cli.md#api-5) | `String help()` |
| [std::cli::CliParser::parse](std-cli.md#api-6) | `Result<CliArguments,CliError> parse(List<String> arguments)` |
| [std::cli::cliError](std-cli.md#api-2) | `CliError cliError(String argument, String message)` |
| [std::cli::cliTail](std-cli.md#api-3) | `String cliTail(String text,int64 start)` |
| [std::clock::nowMs](std-clock.md#api-0) | `int64 nowMs()` |
| [std::clock::nowNanos](std-clock.md#api-1) | `int64 nowNanos()` |
| [std::collections::Channel::create](std-collections.md#api-34) | `static Channel<T> create()` |
| [std::collections::Channel::drop](std-collections.md#api-39) | `void drop()` |
| [std::collections::Channel::iterator](std-collections.md#api-37) | `Option<T> iterator(int64 index)` |
| [std::collections::Channel::len](std-collections.md#api-38) | `int64 len()` |
| [std::collections::Channel::receive](std-collections.md#api-36) | `Option<T> receive()` |
| [std::collections::Channel::send](std-collections.md#api-35) | `void send(T value)` |
| [std::collections::Deque::clear](std-collections.md#api-9) | `void clear()` |
| [std::collections::Deque::create](std-collections.md#api-0) | `static Deque<T> create()` |
| [std::collections::Deque::drop](std-collections.md#api-10) | `void drop()` |
| [std::collections::Deque::index](std-collections.md#api-5) | `T index(int64 key)` |
| [std::collections::Deque::isEmpty](std-collections.md#api-8) | `bool isEmpty()` |
| [std::collections::Deque::iterator](std-collections.md#api-6) | `Option<T> iterator(int64 index)` |
| [std::collections::Deque::len](std-collections.md#api-7) | `int64 len()` |
| [std::collections::Deque::popBack](std-collections.md#api-3) | `Option<T> popBack()` |
| [std::collections::Deque::popFront](std-collections.md#api-4) | `Option<T> popFront()` |
| [std::collections::Deque::pushBack](std-collections.md#api-1) | `void pushBack(T value)` |
| [std::collections::Deque::pushFront](std-collections.md#api-2) | `void pushFront(T value)` |
| [std::collections::FixedVector::capacityValue](std-collections.md#api-32) | `int64 capacityValue()` |
| [std::collections::FixedVector::drop](std-collections.md#api-33) | `void drop()` |
| [std::collections::FixedVector::index](std-collections.md#api-29) | `T index(int64 key)` |
| [std::collections::FixedVector::iterator](std-collections.md#api-30) | `Option<T> iterator(int64 index)` |
| [std::collections::FixedVector::len](std-collections.md#api-31) | `int64 len()` |
| [std::collections::FixedVector::pop](std-collections.md#api-28) | `Option<T> pop()` |
| [std::collections::FixedVector::push](std-collections.md#api-27) | `bool push(T value)` |
| [std::collections::FixedVector::withCapacity](std-collections.md#api-26) | `static FixedVector<T> withCapacity(int64 capacity)` |
| [std::collections::Queue::create](std-collections.md#api-11) | `static Queue<T> create()` |
| [std::collections::Queue::drop](std-collections.md#api-18) | `void drop()` |
| [std::collections::Queue::isEmpty](std-collections.md#api-17) | `bool isEmpty()` |
| [std::collections::Queue::iterator](std-collections.md#api-15) | `Option<T> iterator(int64 index)` |
| [std::collections::Queue::len](std-collections.md#api-16) | `int64 len()` |
| [std::collections::Queue::peek](std-collections.md#api-14) | `Option<T> peek()` |
| [std::collections::Queue::pop](std-collections.md#api-13) | `Option<T> pop()` |
| [std::collections::Queue::push](std-collections.md#api-12) | `void push(T value)` |
| [std::collections::RingBuffer::available](std-collections.md#api-24) | `int64 available()` |
| [std::collections::RingBuffer::drop](std-collections.md#api-25) | `void drop()` |
| [std::collections::RingBuffer::iterator](std-collections.md#api-22) | `Option<T> iterator(int64 index)` |
| [std::collections::RingBuffer::len](std-collections.md#api-23) | `int64 len()` |
| [std::collections::RingBuffer::pop](std-collections.md#api-21) | `Option<T> pop()` |
| [std::collections::RingBuffer::push](std-collections.md#api-20) | `bool push(T value)` |
| [std::collections::RingBuffer::withCapacity](std-collections.md#api-19) | `static RingBuffer<T> withCapacity(int64 capacity)` |
| [std::complex::Complex::conjugate](std-complex.md#api-1) | `Complex<T> conjugate()` |
| [std::complex::Complex::leftAdd](std-complex.md#api-4) | `Complex<T> leftAdd(Complex<T> right)` |
| [std::complex::Complex::leftDiv](std-complex.md#api-7) | `Complex<T> leftDiv(Complex<T> right)` |
| [std::complex::Complex::leftMul](std-complex.md#api-6) | `Complex<T> leftMul(Complex<T> right)` |
| [std::complex::Complex::leftSub](std-complex.md#api-5) | `Complex<T> leftSub(Complex<T> right)` |
| [std::complex::Complex::normSquared](std-complex.md#api-2) | `T normSquared()` |
| [std::complex::Complex::of](std-complex.md#api-0) | `static Complex<T> of(T real, T imaginary)` |
| [std::complex::Complex::scale](std-complex.md#api-3) | `Complex<T> scale(T factor)` |
| [std::compress::compress](std-compress.md#api-0) | `std::lang::List<uint8> compress(std::lang::String text)` |
| [std::compress::decompress](std-compress.md#api-1) | `std::lang::String decompress(std::lang::List<uint8> bytes)` |
| [std::concurrency::AtomicInt64::create](std-concurrency.md#api-6) | `static AtomicInt64 create(int64 value)` |
| [std::concurrency::AtomicInt64::fetchAdd](std-concurrency.md#api-9) | `int64 fetchAdd(int64 delta)` |
| [std::concurrency::AtomicInt64::get](std-concurrency.md#api-7) | `int64 get()` |
| [std::concurrency::AtomicInt64::set](std-concurrency.md#api-8) | `void set(int64 value)` |
| [std::concurrency::Channel::close](std-concurrency.md#api-17) | `void close()` |
| [std::concurrency::Channel::create](std-concurrency.md#api-10) | `static Channel<T> create(int64 capacity)` |
| [std::concurrency::Channel::receive](std-concurrency.md#api-15) | `Receive<T> receive()` |
| [std::concurrency::Channel::receiveFor](std-concurrency.md#api-14) | `Receive<T> receiveFor(int64 millis)` |
| [std::concurrency::Channel::send](std-concurrency.md#api-12) | `int64 send(T value)` |
| [std::concurrency::Channel::sendFor](std-concurrency.md#api-11) | `int64 sendFor(T value, int64 millis)` |
| [std::concurrency::Channel::tryReceive](std-concurrency.md#api-16) | `Receive<T> tryReceive()` |
| [std::concurrency::Channel::trySend](std-concurrency.md#api-13) | `int64 trySend(T value)` |
| [std::concurrency::Receive::isReady](std-concurrency.md#api-0) | `bool isReady()` |
| [std::concurrency::Shared::create](std-concurrency.md#api-1) | `static Shared<T> create(T value)` |
| [std::concurrency::Shared::get](std-concurrency.md#api-2) | `T get()` |
| [std::concurrency::Shared::id](std-concurrency.md#api-5) | `int64 id()` |
| [std::concurrency::Shared::set](std-concurrency.md#api-3) | `void set(T value)` |
| [std::concurrency::Shared::swap](std-concurrency.md#api-4) | `T swap(T value)` |
| [std::csv::csvError](std-csv.md#api-0) | `CsvError csvError(int64 line, int64 column, String message)` |
| [std::csv::parseCsv](std-csv.md#api-1) | `Result<List<List<String>>,CsvError> parseCsv(String text, char delimiter=',')` |
| [std::csv::writeCsv](std-csv.md#api-2) | `Result<String,CsvError> writeCsv(List<List<String>> rows, char delimiter=',')` |
| [std::datetime::addDays](std-datetime.md#api-8) | `Result<DateTime,DateTimeError> addDays(DateTime value, int64 days)` |
| [std::datetime::addSeconds](std-datetime.md#api-7) | `Result<DateTime,DateTimeError> addSeconds(DateTime value, int64 delta)` |
| [std::datetime::dateDigits](std-datetime.md#api-4) | `String dateDigits(int64 value, int64 width)` |
| [std::datetime::daysInMonth](std-datetime.md#api-1) | `int64 daysInMonth(int64 year, int64 month)` |
| [std::datetime::formatUtc](std-datetime.md#api-5) | `Result<String,DateTimeError> formatUtc(DateTime value)` |
| [std::datetime::fromUnixSeconds](std-datetime.md#api-3) | `Result<DateTime,DateTimeError> fromUnixSeconds(int64 seconds)` |
| [std::datetime::isLeapYear](std-datetime.md#api-0) | `bool isLeapYear(int64 year)` |
| [std::datetime::nowUtc](std-datetime.md#api-9) | `Result<DateTime,DateTimeError> nowUtc()` |
| [std::datetime::parseUtc](std-datetime.md#api-6) | `Result<DateTime,DateTimeError> parseUtc(String text)` |
| [std::datetime::toUnixSeconds](std-datetime.md#api-2) | `Result<int64,DateTimeError> toUnixSeconds(DateTime value)` |
| [std::encoding::decodeBase64](std-encoding.md#api-5) | `Result<List<uint8>,EncodingError> decodeBase64(String text, bool url=false)` |
| [std::encoding::decodeBase64Url](std-encoding.md#api-6) | `Result<List<uint8>,EncodingError> decodeBase64Url(String text)` |
| [std::encoding::decodeHex](std-encoding.md#api-2) | `Result<List<uint8>,EncodingError> decodeHex(String text)` |
| [std::encoding::encodeBase64](std-encoding.md#api-3) | `String encodeBase64(List<uint8> bytes, bool url=false, bool padding=true)` |
| [std::encoding::encodeBase64Url](std-encoding.md#api-4) | `String encodeBase64Url(List<uint8> bytes, bool padding=false)` |
| [std::encoding::encodeHex](std-encoding.md#api-0) | `String encodeHex(List<uint8> bytes, bool uppercase=false)` |
| [std::encoding::hexDigit](std-encoding.md#api-1) | `int64 hexDigit(char value)` |
| [std::event::Timer::afterMillis](std-event.md#api-8) | `static Timer afterMillis(int64 millis)` |
| [std::event::Timer::afterNanos](std-event.md#api-6) | `static Timer afterNanos(int64 nanos)` |
| [std::event::Timer::everyMillis](std-event.md#api-7) | `static Timer everyMillis(int64 millis)` |
| [std::event::Timer::everyNanos](std-event.md#api-5) | `static Timer everyNanos(int64 nanos)` |
| [std::event::Timer::id](std-event.md#api-10) | `int64 id()` |
| [std::event::Timer::isValid](std-event.md#api-9) | `bool isValid()` |
| [std::event::Timer::stop](std-event.md#api-11) | `void stop()` |
| [std::event::close](std-event.md#api-4) | `void close()` |
| [std::event::post](std-event.md#api-3) | `int64 post(int64 tag, int64 source)` |
| [std::event::receive](std-event.md#api-1) | `std::concurrency::Receive<Event> receive()` |
| [std::event::receiveFor](std-event.md#api-0) | `std::concurrency::Receive<Event> receiveFor(int64 millis)` |
| [std::event::tryReceive](std-event.md#api-2) | `std::concurrency::Receive<Event> tryReceive()` |
| [std::graph::Graph::addEdge](std-graph.md#api-1) | `void addEdge(int64 from, int64 to, int64 weight)` |
| [std::graph::Graph::bfs](std-graph.md#api-5) | `List<int64> bfs(int64 start)` |
| [std::graph::Graph::connectedComponents](std-graph.md#api-7) | `int64 connectedComponents()` |
| [std::graph::Graph::create](std-graph.md#api-0) | `static Graph create(int64 vertexCount, bool directed)` |
| [std::graph::Graph::dfs](std-graph.md#api-6) | `List<int64> dfs(int64 start)` |
| [std::graph::Graph::dijkstra](std-graph.md#api-9) | `List<int64> dijkstra(int64 start)` |
| [std::graph::Graph::hasCycle](std-graph.md#api-10) | `bool hasCycle()` |
| [std::graph::Graph::isDirected](std-graph.md#api-3) | `bool isDirected()` |
| [std::graph::Graph::len](std-graph.md#api-4) | `int64 len()` |
| [std::graph::Graph::minimumSpanningTreeWeight](std-graph.md#api-11) | `int64 minimumSpanningTreeWeight()` |
| [std::graph::Graph::snapshot](std-graph.md#api-12) | `GraphSnapshot snapshot()` |
| [std::graph::Graph::topologicalSort](std-graph.md#api-8) | `List<int64> topologicalSort()` |
| [std::graph::Graph::vertexCount](std-graph.md#api-2) | `int64 vertexCount()` |
| [std::graph::UnionFind::connected](std-graph.md#api-16) | `bool connected(int64 a, int64 b)` |
| [std::graph::UnionFind::count](std-graph.md#api-18) | `int64 count()` |
| [std::graph::UnionFind::create](std-graph.md#api-13) | `static UnionFind create(int64 n)` |
| [std::graph::UnionFind::find](std-graph.md#api-14) | `int64 find(int64 x)` |
| [std::graph::UnionFind::len](std-graph.md#api-19) | `int64 len()` |
| [std::graph::UnionFind::parents](std-graph.md#api-20) | `List<int64> parents()` |
| [std::graph::UnionFind::sizeOf](std-graph.md#api-17) | `int64 sizeOf(int64 x)` |
| [std::graph::UnionFind::sizes](std-graph.md#api-21) | `List<int64> sizes()` |
| [std::graph::UnionFind::unionSets](std-graph.md#api-15) | `bool unionSets(int64 a, int64 b)` |
| [std::image::Image::binarize](std-image.md#api-23) | `static Result<List<uint8>, String> binarize(int64 width, int64 height, List<uint8> gray, int64 threshold)` |
| [std::image::Image::convolve](std-image.md#api-14) | `Result<Image, String> convolve(List<float64> kernel, int64 kw, int64 kh)` |
| [std::image::Image::decode](std-image.md#api-4) | `static Result<Image, String> decode(List<uint8> bytes)` |
| [std::image::Image::encodeBmp](std-image.md#api-6) | `Result<List<uint8>, String> encodeBmp()` |
| [std::image::Image::encodePgm](std-image.md#api-8) | `Result<List<uint8>, String> encodePgm()` |
| [std::image::Image::encodePpm](std-image.md#api-7) | `Result<List<uint8>, String> encodePpm()` |
| [std::image::Image::fromARGB](std-image.md#api-19) | `static Result<Image, String> fromARGB(int64 width, int64 height, List<uint32> argb)` |
| [std::image::Image::fromGray](std-image.md#api-17) | `static Result<Image, String> fromGray(int64 width, int64 height, List<uint8> gray)` |
| [std::image::Image::fromRGB565](std-image.md#api-21) | `static Result<Image, String> fromRGB565(int64 width, int64 height, List<uint16> rgb565)` |
| [std::image::Image::getPixel](std-image.md#api-12) | `Result<Pixel, String> getPixel(int64 x, int64 y)` |
| [std::image::Image::height](std-image.md#api-1) | `int64 height()` |
| [std::image::Image::make](std-image.md#api-3) | `static Result<Image, String> make(int64 width, int64 height)` |
| [std::image::Image::open](std-image.md#api-5) | `static Result<Image, String> open(String path)` |
| [std::image::Image::otsuBinarize](std-image.md#api-24) | `Result<List<uint8>, String> otsuBinarize()` |
| [std::image::Image::otsuThreshold](std-image.md#api-22) | `Result<int64, String> otsuThreshold()` |
| [std::image::Image::pixels](std-image.md#api-2) | `List<uint8> pixels()` |
| [std::image::Image::pool](std-image.md#api-15) | `Result<Image, String> pool(int64 mode, int64 kw, int64 kh)` |
| [std::image::Image::saveBmp](std-image.md#api-9) | `Result<int64, String> saveBmp(String path)` |
| [std::image::Image::savePgm](std-image.md#api-11) | `Result<int64, String> savePgm(String path)` |
| [std::image::Image::savePpm](std-image.md#api-10) | `Result<int64, String> savePpm(String path)` |
| [std::image::Image::setPixel](std-image.md#api-13) | `Result<int64, String> setPixel(int64 x, int64 y, int64 r, int64 g, int64 b, int64 a)` |
| [std::image::Image::toARGB](std-image.md#api-18) | `Result<List<uint32>, String> toARGB()` |
| [std::image::Image::toGray](std-image.md#api-16) | `Result<List<uint8>, String> toGray()` |
| [std::image::Image::toRGB565](std-image.md#api-20) | `Result<List<uint16>, String> toRGB565()` |
| [std::image::Image::width](std-image.md#api-0) | `int64 width()` |
| [std::io::FileStream::close](std-io.md#api-18) | `int64 close()` |
| [std::io::FileStream::flush](std-io.md#api-16) | `Result<int64, IoError> flush()` |
| [std::io::FileStream::open](std-io.md#api-6) | `static Result<FileStream, IoError> open(std::lang::String path, std::lang::String mode)` |
| [std::io::FileStream::print](std-io.md#api-12) | `Result<int64, IoError> print(String text)` |
| [std::io::FileStream::println](std-io.md#api-13) | `Result<int64, IoError> println(String text = "")` |
| [std::io::FileStream::read](std-io.md#api-7) | `Result<std::lang::String, IoError> read()` |
| [std::io::FileStream::readAllString](std-io.md#api-10) | `Result<String,IoError> readAllString(int64 maxBytes=16777216)` |
| [std::io::FileStream::readBytes](std-io.md#api-14) | `Result<List<uint8>,IoError> readBytes(int64 count)` |
| [std::io::FileStream::readLine](std-io.md#api-8) | `Result<String,IoError> readLine()` |
| [std::io::FileStream::readString](std-io.md#api-9) | `Result<String,IoError> readString(int64 count)` |
| [std::io::FileStream::seek](std-io.md#api-17) | `Result<int64, IoError> seek(int64 offset, SeekOrigin origin)` |
| [std::io::FileStream::write](std-io.md#api-11) | `Result<int64, IoError> write(std::lang::String text)` |
| [std::io::FileStream::writeBytes](std-io.md#api-15) | `Result<int64,IoError> writeBytes(List<uint8> bytes)` |
| [std::io::Formattable::toString](std-io.md#api-5) | `std::lang::String toString(std::lang::String style)` |
| [std::io::Reader::read](std-io.md#api-0) | `Result<std::lang::String, IoError> read()` |
| [std::io::Seek::seek](std-io.md#api-3) | `Result<int64, IoError> seek(int64 offset, SeekOrigin origin)` |
| [std::io::ToString::toString](std-io.md#api-4) | `std::lang::String toString()` |
| [std::io::Writer::flush](std-io.md#api-2) | `Result<int64, IoError> flush()` |
| [std::io::Writer::write](std-io.md#api-1) | `Result<int64, IoError> write(std::lang::String text)` |
| [std::io::createDirectories](std-io.md#api-35) | `Result<int64,IoError> createDirectories(String path)` |
| [std::io::fileClose](std-io.md#api-28) | `int64 fileClose(file const& handle)` |
| [std::io::fileEof](std-io.md#api-27) | `int64 fileEof(file const& handle)` |
| [std::io::fileInfo](std-io.md#api-34) | `Result<FileInfo,IoError> fileInfo(String path)` |
| [std::io::fileOpen](std-io.md#api-19) | `Result<std::lang::File, std::lang::String> fileOpen(std::lang::String path, std::lang::String mode)` |
| [std::io::filePrint](std-io.md#api-25) | `int64 filePrint(file const& handle, String text)` |
| [std::io::filePrintln](std-io.md#api-26) | `int64 filePrintln(file const& handle, String text = "")` |
| [std::io::fileRead](std-io.md#api-20) | `std::lang::String fileRead(file const& handle)` |
| [std::io::fileReadAllString](std-io.md#api-23) | `String fileReadAllString(File const& handle, int64 maxBytes = 16777216)` |
| [std::io::fileReadLine](std-io.md#api-21) | `String fileReadLine(File const& handle)` |
| [std::io::fileReadString](std-io.md#api-22) | `String fileReadString(File const& handle, int64 count)` |
| [std::io::fileWrite](std-io.md#api-24) | `int64 fileWrite(file const& handle, std::lang::String text)` |
| [std::io::listDirectory](std-io.md#api-33) | `Result<List<String>,IoError> listDirectory(String path)` |
| [std::io::readAllBytes](std-io.md#api-29) | `Result<List<uint8>,IoError> readAllBytes(String path, int64 maxBytes=16777216)` |
| [std::io::readAllText](std-io.md#api-31) | `Result<String,IoError> readAllText(String path, int64 maxBytes=16777216)` |
| [std::io::renamePath](std-io.md#api-36) | `Result<int64,IoError> renamePath(String from, String to)` |
| [std::io::writeAllBytes](std-io.md#api-30) | `Result<int64,IoError> writeAllBytes(String path, List<uint8> bytes)` |
| [std::io::writeAllText](std-io.md#api-32) | `Result<int64,IoError> writeAllText(String path, String text)` |
| [std::iter::Iter::all](std-iter.md#api-9) | `bool all(bool fn(T) predicate)` |
| [std::iter::Iter::any](std-iter.md#api-10) | `bool any(bool fn(T) predicate)` |
| [std::iter::Iter::count](std-iter.md#api-6) | `int64 count()` |
| [std::iter::Iter::find](std-iter.md#api-11) | `Option<T> find(bool fn(T) predicate)` |
| [std::iter::Iter::forEach](std-iter.md#api-12) | `void forEach(void fn(T) action)` |
| [std::iter::Iter::iterator](std-iter.md#api-5) | `Option<T> iterator(int64 index)` |
| [std::iter::Iter::last](std-iter.md#api-8) | `Option<T> last()` |
| [std::iter::Iter::next](std-iter.md#api-3) | `Option<T> next()` |
| [std::iter::Iter::nth](std-iter.md#api-7) | `Option<T> nth(int64 index)` |
| [std::iter::Iter::peek](std-iter.md#api-2) | `Option<T> peek()` |
| [std::iter::Iter::sizeHint](std-iter.md#api-4) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterBufferState::next](std-iter.md#api-77) | `Option<T> next()` |
| [std::iter::IterBufferState::sizeHint](std-iter.md#api-78) | `(int64,Option<int64>) sizeHint()` |
| [std::iter::IterChainState::next](std-iter.md#api-48) | `Option<T> next()` |
| [std::iter::IterChainState::sizeHint](std-iter.md#api-49) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterCharsState::next](std-iter.md#api-103) | `Option<char> next()` |
| [std::iter::IterCharsState::sizeHint](std-iter.md#api-104) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterConstListState::next](std-iter.md#api-90) | `Option<T const&> next()` |
| [std::iter::IterConstListState::sizeHint](std-iter.md#api-91) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterCycleState::next](std-iter.md#api-66) | `Option<T> next()` |
| [std::iter::IterCycleState::sizeHint](std-iter.md#api-67) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterEnumerateState::next](std-iter.md#api-54) | `Option<(int64,T)> next()` |
| [std::iter::IterEnumerateState::sizeHint](std-iter.md#api-55) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterFilterMapState::next](std-iter.md#api-30) | `Option<U> next()` |
| [std::iter::IterFilterMapState::sizeHint](std-iter.md#api-31) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterFilterState::next](std-iter.md#api-27) | `Option<T> next()` |
| [std::iter::IterFilterState::sizeHint](std-iter.md#api-28) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterFuseState::next](std-iter.md#api-60) | `Option<T> next()` |
| [std::iter::IterFuseState::sizeHint](std-iter.md#api-61) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterInspectState::next](std-iter.md#api-63) | `Option<T> next()` |
| [std::iter::IterInspectState::sizeHint](std-iter.md#api-64) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterListState::next](std-iter.md#api-17) | `Option<T> next()` |
| [std::iter::IterListState::sizeHint](std-iter.md#api-18) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterMapState::next](std-iter.md#api-24) | `Option<U> next()` |
| [std::iter::IterMapState::sizeHint](std-iter.md#api-25) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterMapWhileState::next](std-iter.md#api-33) | `Option<U> next()` |
| [std::iter::IterMapWhileState::sizeHint](std-iter.md#api-34) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterMutListState::next](std-iter.md#api-92) | `Option<T mut&> next()` |
| [std::iter::IterMutListState::sizeHint](std-iter.md#api-93) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterNext::next](std-iter.md#api-0) | `Option<T> next()` |
| [std::iter::IterNext::sizeHint](std-iter.md#api-1) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterRangeState::next](std-iter.md#api-20) | `Option<int64> next()` |
| [std::iter::IterRangeState::sizeHint](std-iter.md#api-21) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterRepeatState::next](std-iter.md#api-106) | `Option<T> next()` |
| [std::iter::IterRepeatState::sizeHint](std-iter.md#api-107) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterScanState::next](std-iter.md#api-69) | `Option<U> next()` |
| [std::iter::IterScanState::sizeHint](std-iter.md#api-70) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterSkipState::next](std-iter.md#api-45) | `Option<T> next()` |
| [std::iter::IterSkipState::sizeHint](std-iter.md#api-46) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterSkipWhileState::next](std-iter.md#api-39) | `Option<T> next()` |
| [std::iter::IterSkipWhileState::sizeHint](std-iter.md#api-40) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterSpanState::next](std-iter.md#api-100) | `Option<T const&> next()` |
| [std::iter::IterSpanState::sizeHint](std-iter.md#api-101) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterStepState::next](std-iter.md#api-57) | `Option<T> next()` |
| [std::iter::IterStepState::sizeHint](std-iter.md#api-58) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterTakeState::next](std-iter.md#api-42) | `Option<T> next()` |
| [std::iter::IterTakeState::sizeHint](std-iter.md#api-43) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterTakeWhileState::next](std-iter.md#api-36) | `Option<T> next()` |
| [std::iter::IterTakeWhileState::sizeHint](std-iter.md#api-37) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IterZipState::next](std-iter.md#api-51) | `Option<(T,U)> next()` |
| [std::iter::IterZipState::sizeHint](std-iter.md#api-52) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::IteratorValueState::next](std-iter.md#api-14) | `Option<T> next()` |
| [std::iter::IteratorValueState::sizeHint](std-iter.md#api-15) | `(int64, Option<int64>) sizeHint()` |
| [std::iter::all](std-iter.md#api-84) | `bool all<T>(Iter<T> mut& source, bool fn(T) predicate)` |
| [std::iter::any](std-iter.md#api-85) | `bool any<T>(Iter<T> mut& source, bool fn(T) predicate)` |
| [std::iter::chain](std-iter.md#api-50) | `Iter<T> chain<T>(Iter<T> left, Iter<T> right)` |
| [std::iter::chars](std-iter.md#api-105) | `Iter<char> chars(std::lang::String source)` |
| [std::iter::collect](std-iter.md#api-72) | `std::lang::List<T> collect<T>(Iter<T> source)` |
| [std::iter::countIter](std-iter.md#api-81) | `int64 countIter<T>(Iter<T> mut& source)` |
| [std::iter::cycle](std-iter.md#api-68) | `Iter<T> cycle<T>(Iter<T> source)` |
| [std::iter::enumerate](std-iter.md#api-56) | `Iter<(int64,T)> enumerate<T>(Iter<T> source)` |
| [std::iter::filter](std-iter.md#api-29) | `Iter<T> filter<T>(Iter<T> source, bool fn(T) predicate)` |
| [std::iter::filterMap](std-iter.md#api-32) | `Iter<U> filterMap<T,U>(Iter<T> source, Option<U> fn(T) callback)` |
| [std::iter::find](std-iter.md#api-86) | `Option<T> find<T>(Iter<T> mut& source, bool fn(T) predicate)` |
| [std::iter::findMap](std-iter.md#api-75) | `Option<U> findMap<T,U>(Iter<T> source, Option<U> fn(T) callback)` |
| [std::iter::fold](std-iter.md#api-73) | `A fold<T,A>(Iter<T> source, A initial, A fn(A, T) callback)` |
| [std::iter::forEach](std-iter.md#api-87) | `void forEach<T>(Iter<T> mut& source, void fn(T) action)` |
| [std::iter::fromIterator](std-iter.md#api-16) | `Iter<T> fromIterator<T,S : Iterator<T>>(S source)` |
| [std::iter::fromList](std-iter.md#api-19) | `Iter<T> fromList<T>(std::lang::List<T> values)` |
| [std::iter::fromRange](std-iter.md#api-89) | `Iter<int64> fromRange(std::lang::__Range source)` |
| [std::iter::fromSpan](std-iter.md#api-102) | `Iter<T const&> fromSpan<T>(T const& source, int64 length)` |
| [std::iter::fromState](std-iter.md#api-13) | `Iter<T> fromState<T,S>(S state)` |
| [std::iter::fuse](std-iter.md#api-62) | `Iter<T> fuse<T>(Iter<T> source)` |
| [std::iter::inspect](std-iter.md#api-65) | `Iter<T> inspect<T>(Iter<T> source, void fn(T) action)` |
| [std::iter::last](std-iter.md#api-83) | `Option<T> last<T>(Iter<T> mut& source)` |
| [std::iter::listIntoIter](std-iter.md#api-88) | `Iter<T> listIntoIter<T>(std::lang::List<T> mut& source)` |
| [std::iter::listIter](std-iter.md#api-94) | `Iter<T const&> listIter<T>(std::lang::List<T> const& source)` |
| [std::iter::listIterMut](std-iter.md#api-95) | `Iter<T mut&> listIterMut<T>(std::lang::List<T> mut& source)` |
| [std::iter::map](std-iter.md#api-26) | `Iter<U> map<T,U>(Iter<T> source, U fn(T) callback)` |
| [std::iter::mapIntoIter](std-iter.md#api-98) | `Iter<Pair<K,V>> mapIntoIter<K,V>(std::lang::Map<K,V> mut& source)` |
| [std::iter::mapIter](std-iter.md#api-99) | `Iter<Pair<K,V>> mapIter<K,V>(std::lang::Map<K,V> const& source)` |
| [std::iter::mapWhile](std-iter.md#api-35) | `Iter<U> mapWhile<T,U>(Iter<T> source, Option<U> fn(T) callback)` |
| [std::iter::nth](std-iter.md#api-82) | `Option<T> nth<T>(Iter<T> mut& source, int64 index)` |
| [std::iter::peekingTakeWhile](std-iter.md#api-80) | `(Iter<T>,Iter<T>) peekingTakeWhile<T>(Iter<T> source, bool fn(T) predicate)` |
| [std::iter::range](std-iter.md#api-23) | `Iter<int64> range(int64 start, int64 stop)` |
| [std::iter::range](std-iter.md#api-22) | `Iter<int64> range(int64 start, int64 stop, int64 step)` |
| [std::iter::repeat](std-iter.md#api-108) | `Iter<T> repeat<T>(T value)` |
| [std::iter::rev](std-iter.md#api-79) | `Iter<T> rev<T>(Iter<T> source)` |
| [std::iter::sameItems](std-iter.md#api-76) | `bool sameItems<T>(Iter<T> left, Iter<T> right)` |
| [std::iter::scan](std-iter.md#api-71) | `Iter<U> scan<T,A,U>(Iter<T> source, A initial, Option<U> fn(A mut&, T) callback)` |
| [std::iter::setIntoIter](std-iter.md#api-96) | `Iter<T> setIntoIter<T>(std::lang::Set<T> mut& source)` |
| [std::iter::setIter](std-iter.md#api-97) | `Iter<T> setIter<T>(std::lang::Set<T> const& source)` |
| [std::iter::skip](std-iter.md#api-47) | `Iter<T> skip<T>(Iter<T> source, int64 amount)` |
| [std::iter::skipWhile](std-iter.md#api-41) | `Iter<T> skipWhile<T>(Iter<T> source, bool fn(T) predicate)` |
| [std::iter::stepBy](std-iter.md#api-59) | `Iter<T> stepBy<T>(Iter<T> source, int64 step)` |
| [std::iter::take](std-iter.md#api-44) | `Iter<T> take<T>(Iter<T> source, int64 amount)` |
| [std::iter::takeWhile](std-iter.md#api-38) | `Iter<T> takeWhile<T>(Iter<T> source, bool fn(T) predicate)` |
| [std::iter::tryFold](std-iter.md#api-74) | `Result<A,E> tryFold<T,A,E>(Iter<T> source, A initial, Result<A,E> fn(A, T) callback)` |
| [std::iter::zip](std-iter.md#api-53) | `Iter<(T,U)> zip<T,U>(Iter<T> left, Iter<U> right)` |
| [std::json::JsonValue::asBool](std-json.md#api-17) | `Option<bool> asBool()` |
| [std::json::JsonValue::asBytes](std-json.md#api-19) | `Option<std::lang::List<uint8>> asBytes()` |
| [std::json::JsonValue::asDateTime](std-json.md#api-20) | `Option<std::lang::String> asDateTime()` |
| [std::json::JsonValue::asFloat64](std-json.md#api-16) | `Option<float64> asFloat64()` |
| [std::json::JsonValue::asInt](std-json.md#api-15) | `Option<int64> asInt()` |
| [std::json::JsonValue::asString](std-json.md#api-18) | `Option<std::lang::String> asString()` |
| [std::json::JsonValue::at](std-json.md#api-23) | `Option<JsonValue> at(int64 index)` |
| [std::json::JsonValue::close](std-json.md#api-3) | `bool close()` |
| [std::json::JsonValue::dump](std-json.md#api-27) | `std::lang::String dump()` |
| [std::json::JsonValue::find](std-json.md#api-21) | `Option<JsonValue> find(std::lang::String key)` |
| [std::json::JsonValue::findBytes](std-json.md#api-22) | `Option<JsonValue> findBytes(std::lang::List<uint8> key)` |
| [std::json::JsonValue::isArray](std-json.md#api-10) | `bool isArray()` |
| [std::json::JsonValue::isBool](std-json.md#api-6) | `bool isBool()` |
| [std::json::JsonValue::isDateTime](std-json.md#api-13) | `bool isDateTime()` |
| [std::json::JsonValue::isFloat](std-json.md#api-8) | `bool isFloat()` |
| [std::json::JsonValue::isInt](std-json.md#api-7) | `bool isInt()` |
| [std::json::JsonValue::isNull](std-json.md#api-5) | `bool isNull()` |
| [std::json::JsonValue::isObject](std-json.md#api-11) | `bool isObject()` |
| [std::json::JsonValue::isString](std-json.md#api-9) | `bool isString()` |
| [std::json::JsonValue::isTable](std-json.md#api-12) | `bool isTable()` |
| [std::json::JsonValue::isValid](std-json.md#api-2) | `bool isValid()` |
| [std::json::JsonValue::iterator](std-json.md#api-24) | `Option<JsonValue> iterator(int64 index)` |
| [std::json::JsonValue::keyAt](std-json.md#api-25) | `Option<std::lang::String> keyAt(int64 index)` |
| [std::json::JsonValue::keyBytesAt](std-json.md#api-26) | `Option<std::lang::List<uint8>> keyBytesAt(int64 index)` |
| [std::json::JsonValue::kind](std-json.md#api-4) | `int64 kind()` |
| [std::json::JsonValue::len](std-json.md#api-14) | `int64 len()` |
| [std::json::deserialize](std-json.md#api-1) | `Result<T,String> deserialize<T>(String text)` |
| [std::json::parse](std-json.md#api-28) | `Result<JsonValue, std::lang::String> parse(std::lang::String text)` |
| [std::json::serialize](std-json.md#api-0) | `Result<String,String> serialize<T>(T const& value)` |
| [std::lang::Add::leftAdd](std-lang.md#api-222) | `R leftAdd(T value)` |
| [std::lang::And::leftBitAnd](std-lang.md#api-244) | `R leftBitAnd(T value)` |
| [std::lang::Any::clear](std-lang.md#api-205) | `void clear()` |
| [std::lang::Any::copy](std-lang.md#api-203) | `Any copy()` |
| [std::lang::Any::getOptional](std-lang.md#api-209) | `Option<T mut&> getOptional()` |
| [std::lang::Any::getReference](std-lang.md#api-208) | `T mut& getReference()` |
| [std::lang::Any::getValue](std-lang.md#api-207) | `T getValue()` |
| [std::lang::Any::isEmpty](std-lang.md#api-204) | `bool isEmpty()` |
| [std::lang::Any::set](std-lang.md#api-206) | `int64 set(T value)` |
| [std::lang::BigDecimal::abs](std-lang.md#api-196) | `BigDecimal abs()` |
| [std::lang::BigDecimal::ceil](std-lang.md#api-199) | `BigInteger ceil()` |
| [std::lang::BigDecimal::copy](std-lang.md#api-193) | `BigDecimal copy()` |
| [std::lang::BigDecimal::floor](std-lang.md#api-198) | `BigInteger floor()` |
| [std::lang::BigDecimal::isInteger](std-lang.md#api-197) | `bool isInteger()` |
| [std::lang::BigDecimal::pow](std-lang.md#api-195) | `BigDecimal pow(int64 exponent)` |
| [std::lang::BigDecimal::root](std-lang.md#api-202) | `BigDecimal root(int64 k, int64 digits)` |
| [std::lang::BigDecimal::setPrecision](std-lang.md#api-194) | `int64 setPrecision(int64 digits)` |
| [std::lang::BigDecimal::sqrt](std-lang.md#api-201) | `BigDecimal sqrt(int64 digits)` |
| [std::lang::BigDecimal::str](std-lang.md#api-192) | `String str()` |
| [std::lang::BigDecimal::trunc](std-lang.md#api-200) | `BigInteger trunc()` |
| [std::lang::BigInteger::bitAnd](std-lang.md#api-187) | `BigInteger bitAnd(BigInteger other)` |
| [std::lang::BigInteger::bitLength](std-lang.md#api-186) | `int64 bitLength()` |
| [std::lang::BigInteger::bitOr](std-lang.md#api-188) | `BigInteger bitOr(BigInteger other)` |
| [std::lang::BigInteger::bitXor](std-lang.md#api-189) | `BigInteger bitXor(BigInteger other)` |
| [std::lang::BigInteger::copy](std-lang.md#api-177) | `BigInteger copy()` |
| [std::lang::BigInteger::gcd](std-lang.md#api-180) | `BigInteger gcd(BigInteger other)` |
| [std::lang::BigInteger::iroot](std-lang.md#api-191) | `BigInteger iroot(int64 k)` |
| [std::lang::BigInteger::isProbablePrime](std-lang.md#api-184) | `bool isProbablePrime(int64 rounds)` |
| [std::lang::BigInteger::lcm](std-lang.md#api-181) | `BigInteger lcm(BigInteger other)` |
| [std::lang::BigInteger::mod](std-lang.md#api-182) | `BigInteger mod(BigInteger modulus)` |
| [std::lang::BigInteger::modInverse](std-lang.md#api-183) | `BigInteger modInverse(BigInteger modulus)` |
| [std::lang::BigInteger::nextProbablePrime](std-lang.md#api-185) | `BigInteger nextProbablePrime()` |
| [std::lang::BigInteger::pow](std-lang.md#api-178) | `BigInteger pow(int64 exponent)` |
| [std::lang::BigInteger::powmod](std-lang.md#api-179) | `BigInteger powmod(BigInteger exponent, BigInteger modulus)` |
| [std::lang::BigInteger::str](std-lang.md#api-176) | `String str()` |
| [std::lang::BigInteger::strRadix](std-lang.md#api-190) | `String strRadix(int64 base)` |
| [std::lang::BitNot::bitNot](std-lang.md#api-255) | `R bitNot(T value)` |
| [std::lang::Closeable::close](std-lang.md#api-125) | `void close()` |
| [std::lang::Divide::leftDiv](std-lang.md#api-228) | `R leftDiv(T value)` |
| [std::lang::Drop::drop](std-lang.md#api-124) | `void drop()` |
| [std::lang::Equal::leftEqual](std-lang.md#api-232) | `R leftEqual(T value)` |
| [std::lang::ExplicitConvert::explicitConvert](std-lang.md#api-258) | `T explicitConvert()` |
| [std::lang::File::print](std-lang.md#api-48) | `int64 print(String text)` |
| [std::lang::File::println](std-lang.md#api-49) | `int64 println(String text = "")` |
| [std::lang::FromResidual::fromResidual](std-lang.md#api-87) | `static F fromResidual(R residual)` |
| [std::lang::Greater::leftGreater](std-lang.md#api-240) | `R leftGreater(T value)` |
| [std::lang::GreaterOrEqual::leftGreaterEqual](std-lang.md#api-242) | `R leftGreaterEqual(T value)` |
| [std::lang::Hash::hash](std-lang.md#api-257) | `uint64 hash(T value)` |
| [std::lang::ImplicitConvert::implicitConvert](std-lang.md#api-259) | `T implicitConvert()` |
| [std::lang::Index::index](std-lang.md#api-260) | `R index(T key)` |
| [std::lang::Iterator::iterator](std-lang.md#api-261) | `Option<R> iterator(int64 index)` |
| [std::lang::Less::leftLess](std-lang.md#api-236) | `R leftLess(T value)` |
| [std::lang::LessOrEqual::leftLessEqual](std-lang.md#api-238) | `R leftLessEqual(T value)` |
| [std::lang::List::at](std-lang.md#api-151) | `T at(int64 index)` |
| [std::lang::List::clear](std-lang.md#api-157) | `int64 clear()` |
| [std::lang::List::contains](std-lang.md#api-155) | `bool contains(T value)` |
| [std::lang::List::copy](std-lang.md#api-148) | `List<T> copy()` |
| [std::lang::List::has](std-lang.md#api-156) | `bool has(T value)` |
| [std::lang::List::indexOf](std-lang.md#api-154) | `int64 indexOf(T value)` |
| [std::lang::List::insert](std-lang.md#api-150) | `int64 insert(int64 index, T value)` |
| [std::lang::List::len](std-lang.md#api-158) | `int64 len()` |
| [std::lang::List::pop](std-lang.md#api-152) | `T pop()` |
| [std::lang::List::push](std-lang.md#api-149) | `int64 push(T value)` |
| [std::lang::List::remove](std-lang.md#api-153) | `int64 remove(int64 index)` |
| [std::lang::Map::clear](std-lang.md#api-174) | `int64 clear()` |
| [std::lang::Map::contains](std-lang.md#api-170) | `int64 contains(K key)` |
| [std::lang::Map::copy](std-lang.md#api-167) | `Map<K,V> copy()` |
| [std::lang::Map::has](std-lang.md#api-169) | `int64 has(K key)` |
| [std::lang::Map::keyAt](std-lang.md#api-172) | `K keyAt(int64 index)` |
| [std::lang::Map::len](std-lang.md#api-175) | `int64 len()` |
| [std::lang::Map::put](std-lang.md#api-168) | `int64 put(K key,V value)` |
| [std::lang::Map::remove](std-lang.md#api-171) | `int64 remove(K key)` |
| [std::lang::Map::valAt](std-lang.md#api-173) | `V valAt(int64 index)` |
| [std::lang::Modulo::leftMod](std-lang.md#api-230) | `R leftMod(T value)` |
| [std::lang::Multiply::leftMul](std-lang.md#api-226) | `R leftMul(T value)` |
| [std::lang::Negate::negate](std-lang.md#api-254) | `R negate(T value)` |
| [std::lang::NextValue::nextValue](std-lang.md#api-113) | `Option<T> nextValue(T end, bool inclusive)` |
| [std::lang::NotEqual::leftNotEqual](std-lang.md#api-234) | `R leftNotEqual(T value)` |
| [std::lang::Option::andThen](std-lang.md#api-98) | `Option<T> andThen(Option<T> fn(T) mapper)` |
| [std::lang::Option::branch](std-lang.md#api-88) | `TryBranch<T, NoneResidual> branch()` |
| [std::lang::Option::expect](std-lang.md#api-96) | `T expect(std::lang::String message)` |
| [std::lang::Option::fromResidual](std-lang.md#api-90) | `static Option<T> fromResidual(NoneResidual residual)` |
| [std::lang::Option::intoBranch](std-lang.md#api-89) | `TryBranch<T, NoneResidual> intoBranch()` |
| [std::lang::Option::isNone](std-lang.md#api-92) | `bool isNone()` |
| [std::lang::Option::isValue](std-lang.md#api-91) | `bool isValue()` |
| [std::lang::Option::okOr](std-lang.md#api-263) | `Result<T,E> okOr<E>(E error)` |
| [std::lang::Option::okOrElse](std-lang.md#api-264) | `Result<T,E> okOrElse<E>(E fn() factory)` |
| [std::lang::Option::take](std-lang.md#api-93) | `T take()` |
| [std::lang::Option::transform](std-lang.md#api-97) | `Option<T> transform(T fn(T) mapper)` |
| [std::lang::Option::unwrap](std-lang.md#api-94) | `T unwrap()` |
| [std::lang::Option::unwrapOr](std-lang.md#api-95) | `T unwrapOr(T fallback)` |
| [std::lang::Or::leftBitOr](std-lang.md#api-246) | `R leftBitOr(T value)` |
| [std::lang::Pair::create](std-lang.md#api-115) | `static Pair<K, V> create((K, V) value)` |
| [std::lang::Pair::create](std-lang.md#api-114) | `static Pair<K, V> create(K first, V second)` |
| [std::lang::Pair::toTuple](std-lang.md#api-116) | `(K, V) toTuple()` |
| [std::lang::Positive::positive](std-lang.md#api-256) | `R positive(T value)` |
| [std::lang::Result::andThen](std-lang.md#api-112) | `Result<T, E> andThen(Result<T, E> fn(T) mapper)` |
| [std::lang::Result::branch](std-lang.md#api-99) | `TryBranch<T, ErrorResidual<E>> branch()` |
| [std::lang::Result::error](std-lang.md#api-110) | `E error()` |
| [std::lang::Result::expect](std-lang.md#api-108) | `T expect(std::lang::String message)` |
| [std::lang::Result::fromResidual](std-lang.md#api-101) | `static Result<T,E> fromResidual(ErrorResidual<E> residual)` |
| [std::lang::Result::intoBranch](std-lang.md#api-100) | `TryBranch<T, ErrorResidual<E>> intoBranch()` |
| [std::lang::Result::isErr](std-lang.md#api-104) | `bool isErr()` |
| [std::lang::Result::isOk](std-lang.md#api-103) | `bool isOk()` |
| [std::lang::Result::mapErr](std-lang.md#api-262) | `Result<T,F> mapErr<F>(F fn(E) mapper)` |
| [std::lang::Result::ok](std-lang.md#api-102) | `Option<T> ok()` |
| [std::lang::Result::take](std-lang.md#api-105) | `T take()` |
| [std::lang::Result::takeError](std-lang.md#api-109) | `E takeError()` |
| [std::lang::Result::transform](std-lang.md#api-111) | `Result<T, E> transform(T fn(T) mapper)` |
| [std::lang::Result::unwrap](std-lang.md#api-106) | `T unwrap()` |
| [std::lang::Result::unwrapOr](std-lang.md#api-107) | `T unwrapOr(T fallback)` |
| [std::lang::RightAdd::rightAdd](std-lang.md#api-223) | `R rightAdd(T value)` |
| [std::lang::RightAnd::rightBitAnd](std-lang.md#api-245) | `R rightBitAnd(T value)` |
| [std::lang::RightDivide::rightDiv](std-lang.md#api-229) | `R rightDiv(T value)` |
| [std::lang::RightEqual::rightEqual](std-lang.md#api-233) | `R rightEqual(T value)` |
| [std::lang::RightGreater::rightGreater](std-lang.md#api-241) | `R rightGreater(T value)` |
| [std::lang::RightGreaterOrEqual::rightGreaterEqual](std-lang.md#api-243) | `R rightGreaterEqual(T value)` |
| [std::lang::RightLess::rightLess](std-lang.md#api-237) | `R rightLess(T value)` |
| [std::lang::RightLessOrEqual::rightLessEqual](std-lang.md#api-239) | `R rightLessEqual(T value)` |
| [std::lang::RightModulo::rightMod](std-lang.md#api-231) | `R rightMod(T value)` |
| [std::lang::RightMultiply::rightMul](std-lang.md#api-227) | `R rightMul(T value)` |
| [std::lang::RightNotEqual::rightNotEqual](std-lang.md#api-235) | `R rightNotEqual(T value)` |
| [std::lang::RightOr::rightBitOr](std-lang.md#api-247) | `R rightBitOr(T value)` |
| [std::lang::RightShiftLeft::rightShiftLeft](std-lang.md#api-251) | `R rightShiftLeft(T value)` |
| [std::lang::RightShiftRight::rightShiftRight](std-lang.md#api-253) | `R rightShiftRight(T value)` |
| [std::lang::RightSubtract::rightSub](std-lang.md#api-225) | `R rightSub(T value)` |
| [std::lang::RightXor::rightBitXor](std-lang.md#api-249) | `R rightBitXor(T value)` |
| [std::lang::Set::add](std-lang.md#api-160) | `int64 add(T value)` |
| [std::lang::Set::at](std-lang.md#api-164) | `T at(int64 index)` |
| [std::lang::Set::clear](std-lang.md#api-165) | `int64 clear()` |
| [std::lang::Set::contains](std-lang.md#api-162) | `int64 contains(T value)` |
| [std::lang::Set::copy](std-lang.md#api-159) | `Set<T> copy()` |
| [std::lang::Set::has](std-lang.md#api-161) | `int64 has(T value)` |
| [std::lang::Set::len](std-lang.md#api-166) | `int64 len()` |
| [std::lang::Set::remove](std-lang.md#api-163) | `int64 remove(T value)` |
| [std::lang::ShiftLeft::leftShiftLeft](std-lang.md#api-250) | `R leftShiftLeft(T value)` |
| [std::lang::ShiftRight::leftShiftRight](std-lang.md#api-252) | `R leftShiftRight(T value)` |
| [std::lang::String::byteAt](std-lang.md#api-17) | `Option<uint8> byteAt(int64 index)` |
| [std::lang::String::byteLength](std-lang.md#api-2) | `int64 byteLength()` |
| [std::lang::String::bytes](std-lang.md#api-44) | `List<uint8> bytes()` |
| [std::lang::String::charAt](std-lang.md#api-39) | `Option<String> charAt(int64 index)` |
| [std::lang::String::charLength](std-lang.md#api-9) | `int64 charLength()` |
| [std::lang::String::compare](std-lang.md#api-16) | `int64 compare(String text)` |
| [std::lang::String::concat](std-lang.md#api-46) | `String concat(String text)` |
| [std::lang::String::contains](std-lang.md#api-12) | `bool contains(String text)` |
| [std::lang::String::count](std-lang.md#api-15) | `int64 count(String text)` |
| [std::lang::String::endsWith](std-lang.md#api-14) | `bool endsWith(String text)` |
| [std::lang::String::indexOf](std-lang.md#api-10) | `int64 indexOf(String text, int64 start = 0)` |
| [std::lang::String::insert](std-lang.md#api-36) | `String insert(int64 index, String text)` |
| [std::lang::String::isAlnum](std-lang.md#api-7) | `bool isAlnum()` |
| [std::lang::String::isAlpha](std-lang.md#api-6) | `bool isAlpha()` |
| [std::lang::String::isAscii](std-lang.md#api-8) | `bool isAscii()` |
| [std::lang::String::isBlank](std-lang.md#api-4) | `bool isBlank()` |
| [std::lang::String::isDigit](std-lang.md#api-5) | `bool isDigit()` |
| [std::lang::String::isEmpty](std-lang.md#api-3) | `bool isEmpty()` |
| [std::lang::String::join](std-lang.md#api-45) | `String join(List<String> items)` |
| [std::lang::String::lastIndexOf](std-lang.md#api-11) | `int64 lastIndexOf(String text, int64 start = 0 - 1)` |
| [std::lang::String::left](std-lang.md#api-20) | `String left(int64 count)` |
| [std::lang::String::length](std-lang.md#api-0) | `int64 length()` |
| [std::lang::String::lines](std-lang.md#api-42) | `List<String> lines(bool keepEmpty = true)` |
| [std::lang::String::padEnd](std-lang.md#api-33) | `String padEnd(int64 length, String fill = " ")` |
| [std::lang::String::padStart](std-lang.md#api-32) | `String padStart(int64 length, String fill = " ")` |
| [std::lang::String::prepend](std-lang.md#api-47) | `String prepend(String text)` |
| [std::lang::String::remove](std-lang.md#api-37) | `String remove(int64 start, int64 count = 0 - 1)` |
| [std::lang::String::removePrefix](std-lang.md#api-34) | `String removePrefix(String prefix)` |
| [std::lang::String::removeSuffix](std-lang.md#api-35) | `String removeSuffix(String suffix)` |
| [std::lang::String::repeat](std-lang.md#api-30) | `String repeat(int64 count)` |
| [std::lang::String::replace](std-lang.md#api-29) | `String replace(String before, String after)` |
| [std::lang::String::replaceAll](std-lang.md#api-28) | `String replaceAll(String before, String after)` |
| [std::lang::String::replaceFirst](std-lang.md#api-27) | `String replaceFirst(String before, String after)` |
| [std::lang::String::reverse](std-lang.md#api-31) | `String reverse()` |
| [std::lang::String::right](std-lang.md#api-21) | `String right(int64 count)` |
| [std::lang::String::size](std-lang.md#api-1) | `int64 size()` |
| [std::lang::String::slice](std-lang.md#api-19) | `String slice(int64 start, int64 end)` |
| [std::lang::String::split](std-lang.md#api-40) | `List<String> split(String separator, bool keepEmpty = true)` |
| [std::lang::String::splitWhitespace](std-lang.md#api-41) | `List<String> splitWhitespace()` |
| [std::lang::String::startsWith](std-lang.md#api-13) | `bool startsWith(String text)` |
| [std::lang::String::substring](std-lang.md#api-18) | `String substring(int64 start, int64 count = 0 - 1)` |
| [std::lang::String::toLower](std-lang.md#api-25) | `String toLower()` |
| [std::lang::String::toUpper](std-lang.md#api-26) | `String toUpper()` |
| [std::lang::String::trim](std-lang.md#api-22) | `String trim()` |
| [std::lang::String::trimEnd](std-lang.md#api-24) | `String trimEnd()` |
| [std::lang::String::trimStart](std-lang.md#api-23) | `String trimStart()` |
| [std::lang::String::unicodeChars](std-lang.md#api-43) | `List<String> unicodeChars()` |
| [std::lang::String::utf8Substring](std-lang.md#api-38) | `String utf8Substring(int64 start, int64 count = 0 - 1)` |
| [std::lang::Subtract::leftSub](std-lang.md#api-224) | `R leftSub(T value)` |
| [std::lang::Try::branch](std-lang.md#api-85) | `TryBranch<T, R> branch()` |
| [std::lang::TryMove::intoBranch](std-lang.md#api-86) | `TryBranch<T, R> intoBranch()` |
| [std::lang::Xor::leftBitXor](std-lang.md#api-248) | `R leftBitXor(T value)` |
| [std::lang::add](std-lang.md#api-216) | `R add(T left,U right)` |
| [std::lang::assert](std-lang.md#api-81) | `void assert(bool condition)` |
| [std::lang::assert](std-lang.md#api-83) | `void assert(bool condition, int64 line)` |
| [std::lang::assert](std-lang.md#api-82) | `void assert(bool condition, std::lang::String message)` |
| [std::lang::assert](std-lang.md#api-84) | `void assert(bool condition, std::lang::String message, int64 line)` |
| [std::lang::bytesToFloat32](std-lang.md#api-60) | `Option<float32> bytesToFloat32(std::lang::List<uint8> bytes)` |
| [std::lang::bytesToFloat32](std-lang.md#api-59) | `Option<float32> bytesToFloat32(std::lang::List<uint8> bytes, bool bigEndian)` |
| [std::lang::bytesToFloat32](std-lang.md#api-64) | `Option<float32> bytesToFloat32(uint8 bytes[])` |
| [std::lang::bytesToFloat32](std-lang.md#api-63) | `Option<float32> bytesToFloat32(uint8 bytes[], bool bigEndian)` |
| [std::lang::bytesToFloat32](std-lang.md#api-62) | `Option<float32> bytesToFloat32(uint8 const& bytes, int64 count)` |
| [std::lang::bytesToFloat32](std-lang.md#api-61) | `Option<float32> bytesToFloat32(uint8 const& bytes, int64 count, bool bigEndian)` |
| [std::lang::bytesToFloat64](std-lang.md#api-72) | `Option<float64> bytesToFloat64(std::lang::List<uint8> bytes)` |
| [std::lang::bytesToFloat64](std-lang.md#api-71) | `Option<float64> bytesToFloat64(std::lang::List<uint8> bytes, bool bigEndian)` |
| [std::lang::bytesToFloat64](std-lang.md#api-76) | `Option<float64> bytesToFloat64(uint8 bytes[])` |
| [std::lang::bytesToFloat64](std-lang.md#api-75) | `Option<float64> bytesToFloat64(uint8 bytes[], bool bigEndian)` |
| [std::lang::bytesToFloat64](std-lang.md#api-74) | `Option<float64> bytesToFloat64(uint8 const& bytes, int64 count)` |
| [std::lang::bytesToFloat64](std-lang.md#api-73) | `Option<float64> bytesToFloat64(uint8 const& bytes, int64 count, bool bigEndian)` |
| [std::lang::div](std-lang.md#api-219) | `R div(T left,U right)` |
| [std::lang::eprint](std-lang.md#api-128) | `int64 eprint(String format, Any args...)` |
| [std::lang::eprintln](std-lang.md#api-129) | `int64 eprintln(String format, Any args...)` |
| [std::lang::eq](std-lang.md#api-210) | `bool eq(T left,T right)` |
| [std::lang::float32ToBytes](std-lang.md#api-66) | `bool float32ToBytes(float32 value, uint8 mut& bytes, int64 count)` |
| [std::lang::float32ToBytes](std-lang.md#api-65) | `bool float32ToBytes(float32 value, uint8 mut& bytes, int64 count, bool bigEndian)` |
| [std::lang::float32ToBytes](std-lang.md#api-68) | `bool float32ToBytes(float32 value, uint8[] mut& bytes)` |
| [std::lang::float32ToBytes](std-lang.md#api-67) | `bool float32ToBytes(float32 value, uint8[] mut& bytes, bool bigEndian)` |
| [std::lang::float32ToBytes](std-lang.md#api-58) | `std::lang::List<uint8> float32ToBytes(float32 value)` |
| [std::lang::float32ToBytes](std-lang.md#api-57) | `std::lang::List<uint8> float32ToBytes(float32 value, bool bigEndian)` |
| [std::lang::float64ToBytes](std-lang.md#api-78) | `bool float64ToBytes(float64 value, uint8 mut& bytes, int64 count)` |
| [std::lang::float64ToBytes](std-lang.md#api-77) | `bool float64ToBytes(float64 value, uint8 mut& bytes, int64 count, bool bigEndian)` |
| [std::lang::float64ToBytes](std-lang.md#api-80) | `bool float64ToBytes(float64 value, uint8[] mut& bytes)` |
| [std::lang::float64ToBytes](std-lang.md#api-79) | `bool float64ToBytes(float64 value, uint8[] mut& bytes, bool bigEndian)` |
| [std::lang::float64ToBytes](std-lang.md#api-70) | `std::lang::List<uint8> float64ToBytes(float64 value)` |
| [std::lang::float64ToBytes](std-lang.md#api-69) | `std::lang::List<uint8> float64ToBytes(float64 value, bool bigEndian)` |
| [std::lang::format](std-lang.md#api-130) | `String format(String format, Any args...)` |
| [std::lang::ge](std-lang.md#api-215) | `bool ge(T left,T right)` |
| [std::lang::gt](std-lang.md#api-214) | `bool gt(T left,T right)` |
| [std::lang::index](std-lang.md#api-221) | `T index(T const& data,int64 index)` |
| [std::lang::inputBigDecimal](std-lang.md#api-140) | `BigDecimal inputBigDecimal()` |
| [std::lang::inputBigInteger](std-lang.md#api-139) | `BigInteger inputBigInteger()` |
| [std::lang::inputBool](std-lang.md#api-137) | `bool inputBool()` |
| [std::lang::inputChar](std-lang.md#api-138) | `char inputChar()` |
| [std::lang::inputFloat](std-lang.md#api-136) | `float64 inputFloat()` |
| [std::lang::inputInt](std-lang.md#api-134) | `int64 inputInt()` |
| [std::lang::inputLine](std-lang.md#api-133) | `String inputLine()` |
| [std::lang::inputUInt](std-lang.md#api-135) | `uint64 inputUInt()` |
| [std::lang::le](std-lang.md#api-213) | `bool le(T left,T right)` |
| [std::lang::len](std-lang.md#api-142) | `int64 len(Any value)` |
| [std::lang::lt](std-lang.md#api-212) | `bool lt(T left,T right)` |
| [std::lang::memcpy](std-lang.md#api-146) | `int64 memcpy(char mut& destination, char const& source, int64 bytes)` |
| [std::lang::memset](std-lang.md#api-147) | `int64 memset(char mut& destination, int64 value, int64 bytes)` |
| [std::lang::mod](std-lang.md#api-220) | `R mod(T left,U right)` |
| [std::lang::move](std-lang.md#api-143) | `T move<T>(T mut& value)` |
| [std::lang::mul](std-lang.md#api-218) | `R mul(T left,U right)` |
| [std::lang::ne](std-lang.md#api-211) | `bool ne(T left,T right)` |
| [std::lang::panic](std-lang.md#api-141) | `int64 panic(String message)` |
| [std::lang::pause](std-lang.md#api-132) | `int64 pause()` |
| [std::lang::print](std-lang.md#api-126) | `int64 print(String format, Any args...)` |
| [std::lang::println](std-lang.md#api-127) | `int64 println(String format, Any args...)` |
| [std::lang::putChar](std-lang.md#api-131) | `int64 putChar(char value)` |
| [std::lang::range](std-lang.md#api-121) | `__Range range(int64 start, int64 stop)` |
| [std::lang::range](std-lang.md#api-122) | `__Range range(int64 start, int64 stop, int64 step)` |
| [std::lang::rangeEq](std-lang.md#api-123) | `__Interval_int64 rangeEq(int64 start, int64 stop)` |
| [std::lang::readLine](std-lang.md#api-50) | `String readLine()` |
| [std::lang::readLines](std-lang.md#api-51) | `String readLines(int32 N)` |
| [std::lang::readLines](std-lang.md#api-52) | `String readLines(int64 N)` |
| [std::lang::stringBytes](std-lang.md#api-120) | `List<uint8> stringBytes(String text)` |
| [std::lang::sub](std-lang.md#api-217) | `R sub(T left,U right)` |
| [std::lang::toInt32](std-lang.md#api-54) | `Option<int32> toInt32(float32 value)` |
| [std::lang::toInt32](std-lang.md#api-53) | `Option<int32> toInt32(float64 value)` |
| [std::lang::toInt64](std-lang.md#api-56) | `int64 toInt64(float32 value)` |
| [std::lang::toInt64](std-lang.md#api-55) | `int64 toInt64(float64 value)` |
| [std::lang::toOptional](std-lang.md#api-145) | `Option<T mut&> toOptional<T>(T mut& value)` |
| [std::lang::toReference](std-lang.md#api-144) | `T mut& toReference<T>(Option<T mut&> value)` |
| [std::lang::tryGet](std-lang.md#api-117) | `Option<T> tryGet<T>(Any const& value)` |
| [std::lang::tryGetMut](std-lang.md#api-119) | `Option<T mut&> tryGetMut<T>(Any mut& value)` |
| [std::lang::tryGetReference](std-lang.md#api-118) | `Option<T const&> tryGetReference<T>(Any const& value)` |
| [std::linalg::Matrix::add](std-linalg.md#api-39) | `Matrix<T> add(Matrix<T> other)` |
| [std::linalg::Matrix::apply](std-linalg.md#api-43) | `Vector<T> apply(Vector<T> vector)` |
| [std::linalg::Matrix::at](std-linalg.md#api-36) | `T at(int64 row, int64 col)` |
| [std::linalg::Matrix::column](std-linalg.md#api-32) | `Vector<T> column(int64 key)` |
| [std::linalg::Matrix::drop](std-linalg.md#api-27) | `void drop()` |
| [std::linalg::Matrix::fromArray](std-linalg.md#api-29) | `static Matrix<T> fromArray(T const& data, int64 count, int64 rows, int64 cols)` |
| [std::linalg::Matrix::fromArray](std-linalg.md#api-30) | `static Matrix<T> fromArray(T data[], int64 rows, int64 cols)` |
| [std::linalg::Matrix::identity](std-linalg.md#api-35) | `static Matrix<T> identity(int64 n)` |
| [std::linalg::Matrix::index](std-linalg.md#api-33) | `Vector<T> index(int64 key)` |
| [std::linalg::Matrix::iterator](std-linalg.md#api-34) | `Option<Vector<T>> iterator(int64 position)` |
| [std::linalg::Matrix::leftAdd](std-linalg.md#api-45) | `Matrix<T> leftAdd(Matrix<T> right)` |
| [std::linalg::Matrix::leftMul](std-linalg.md#api-47) | `Matrix<T> leftMul(Matrix<T> right)` |
| [std::linalg::Matrix::leftMul](std-linalg.md#api-49) | `Matrix<T> leftMul(T right)` |
| [std::linalg::Matrix::leftMul](std-linalg.md#api-48) | `Vector<T> leftMul(Vector<T> right)` |
| [std::linalg::Matrix::leftSub](std-linalg.md#api-46) | `Matrix<T> leftSub(Matrix<T> right)` |
| [std::linalg::Matrix::multiply](std-linalg.md#api-42) | `Matrix<T> multiply(Matrix<T> other)` |
| [std::linalg::Matrix::scale](std-linalg.md#api-41) | `Matrix<T> scale(T factor)` |
| [std::linalg::Matrix::set](std-linalg.md#api-37) | `void set(int64 row, int64 col, T value)` |
| [std::linalg::Matrix::solve](std-linalg.md#api-44) | `Vector<T> solve(Vector<T> rhs)` |
| [std::linalg::Matrix::subtract](std-linalg.md#api-40) | `Matrix<T> subtract(Matrix<T> other)` |
| [std::linalg::Matrix::toArray](std-linalg.md#api-31) | `T[] toArray()` |
| [std::linalg::Matrix::transpose](std-linalg.md#api-38) | `Matrix<T> transpose()` |
| [std::linalg::Matrix::zeros](std-linalg.md#api-28) | `static Matrix<T> zeros(int64 rows, int64 cols)` |
| [std::linalg::ScalarArray::at](std-linalg.md#api-2) | `T at(int64 index)` |
| [std::linalg::ScalarArray::clear](std-linalg.md#api-1) | `void clear()` |
| [std::linalg::ScalarArray::copy](std-linalg.md#api-6) | `ScalarArray<T> copy()` |
| [std::linalg::ScalarArray::index](std-linalg.md#api-3) | `T index(int64 key)` |
| [std::linalg::ScalarArray::iterator](std-linalg.md#api-7) | `Option<T> iterator(int64 position)` |
| [std::linalg::ScalarArray::len](std-linalg.md#api-0) | `int64 len()` |
| [std::linalg::ScalarArray::push](std-linalg.md#api-5) | `void push(T value)` |
| [std::linalg::ScalarArray::set](std-linalg.md#api-4) | `void set(int64 index, T value)` |
| [std::linalg::Vector::add](std-linalg.md#api-21) | `Vector<T> add(Vector<T> other)` |
| [std::linalg::Vector::at](std-linalg.md#api-16) | `T at(int64 i)` |
| [std::linalg::Vector::dot](std-linalg.md#api-18) | `T dot(Vector<T> other)` |
| [std::linalg::Vector::drop](std-linalg.md#api-8) | `void drop()` |
| [std::linalg::Vector::fromArray](std-linalg.md#api-10) | `static Vector<T> fromArray(T const& data, int64 count)` |
| [std::linalg::Vector::fromArray](std-linalg.md#api-11) | `static Vector<T> fromArray(T data[])` |
| [std::linalg::Vector::index](std-linalg.md#api-13) | `T index(int64 key)` |
| [std::linalg::Vector::iterator](std-linalg.md#api-14) | `Option<T> iterator(int64 position)` |
| [std::linalg::Vector::leftAdd](std-linalg.md#api-24) | `Vector<T> leftAdd(Vector<T> right)` |
| [std::linalg::Vector::leftMul](std-linalg.md#api-26) | `Vector<T> leftMul(T right)` |
| [std::linalg::Vector::leftSub](std-linalg.md#api-25) | `Vector<T> leftSub(Vector<T> right)` |
| [std::linalg::Vector::len](std-linalg.md#api-15) | `int64 len()` |
| [std::linalg::Vector::norm](std-linalg.md#api-19) | `float64 norm()` |
| [std::linalg::Vector::normalized](std-linalg.md#api-20) | `Vector<T> normalized()` |
| [std::linalg::Vector::scale](std-linalg.md#api-23) | `Vector<T> scale(T amount)` |
| [std::linalg::Vector::set](std-linalg.md#api-17) | `void set(int64 i, T value)` |
| [std::linalg::Vector::subtract](std-linalg.md#api-22) | `Vector<T> subtract(Vector<T> other)` |
| [std::linalg::Vector::toArray](std-linalg.md#api-12) | `T[] toArray()` |
| [std::linalg::Vector::zeros](std-linalg.md#api-9) | `static Vector<T> zeros(int64 n)` |
| [std::linalg::diagonalize2](std-linalg.md#api-55) | `JordanDecomposition diagonalize2(Matrix<float64> input)` |
| [std::linalg::jordan2](std-linalg.md#api-54) | `JordanDecomposition jordan2(Matrix<float64> input)` |
| [std::linalg::luDecompose](std-linalg.md#api-50) | `LuFactorization luDecompose(Matrix<float64> input)` |
| [std::linalg::lurDecompose](std-linalg.md#api-51) | `LuFactorization lurDecompose(Matrix<float64> input)` |
| [std::linalg::svd](std-linalg.md#api-53) | `SingularValueDecomposition svd(Matrix<float64> input)` |
| [std::linalg::symmetricEigen](std-linalg.md#api-52) | `EigenDecomposition symmetricEigen(Matrix<float64> input)` |
| [std::linkedlist::LinkedList::back](std-linkedlist.md#api-14) | `Option<int64> back()` |
| [std::linkedlist::LinkedList::clear](std-linkedlist.md#api-18) | `void clear()` |
| [std::linkedlist::LinkedList::contains](std-linkedlist.md#api-12) | `bool contains(int64 value)` |
| [std::linkedlist::LinkedList::create](std-linkedlist.md#api-0) | `static LinkedList create()` |
| [std::linkedlist::LinkedList::find](std-linkedlist.md#api-11) | `int64 find(int64 value)` |
| [std::linkedlist::LinkedList::fromArray](std-linkedlist.md#api-10) | `static LinkedList fromArray(List<int64> items)` |
| [std::linkedlist::LinkedList::front](std-linkedlist.md#api-13) | `Option<int64> front()` |
| [std::linkedlist::LinkedList::insertAfter](std-linkedlist.md#api-4) | `int64 insertAfter(int64 cursor, int64 value)` |
| [std::linkedlist::LinkedList::insertBefore](std-linkedlist.md#api-3) | `int64 insertBefore(int64 cursor, int64 value)` |
| [std::linkedlist::LinkedList::isEmpty](std-linkedlist.md#api-16) | `bool isEmpty()` |
| [std::linkedlist::LinkedList::iterator](std-linkedlist.md#api-17) | `Option<int64> iterator(int64 index)` |
| [std::linkedlist::LinkedList::len](std-linkedlist.md#api-15) | `int64 len()` |
| [std::linkedlist::LinkedList::popBack](std-linkedlist.md#api-7) | `Option<int64> popBack()` |
| [std::linkedlist::LinkedList::popFront](std-linkedlist.md#api-6) | `Option<int64> popFront()` |
| [std::linkedlist::LinkedList::pushBack](std-linkedlist.md#api-2) | `int64 pushBack(int64 value)` |
| [std::linkedlist::LinkedList::pushFront](std-linkedlist.md#api-1) | `int64 pushFront(int64 value)` |
| [std::linkedlist::LinkedList::removeAt](std-linkedlist.md#api-5) | `Option<int64> removeAt(int64 cursor)` |
| [std::linkedlist::LinkedList::reverse](std-linkedlist.md#api-8) | `void reverse()` |
| [std::linkedlist::LinkedList::toArray](std-linkedlist.md#api-9) | `List<int64> toArray()` |
| [std::math::abs](std-math.md#api-19) | `T abs<T>(T value)` |
| [std::math::acos](std-math.md#api-5) | `let acos<T>(T value)` |
| [std::math::asin](std-math.md#api-4) | `let asin<T>(T value)` |
| [std::math::atan](std-math.md#api-6) | `let atan<T>(T value)` |
| [std::math::ceil](std-math.md#api-12) | `let ceil<T>(T value)` |
| [std::math::clamp](std-math.md#api-23) | `float64 clamp(float64 value, float64 low, float64 high)` |
| [std::math::clamp](std-math.md#api-22) | `int64 clamp(int64 value, int64 low, int64 high)` |
| [std::math::cos](std-math.md#api-2) | `let cos<T>(T value)` |
| [std::math::exp](std-math.md#api-7) | `let exp<T>(T value)` |
| [std::math::floor](std-math.md#api-11) | `let floor<T>(T value)` |
| [std::math::fmod](std-math.md#api-16) | `let fmod<T, U>(T value, U other)` |
| [std::math::isInf](std-math.md#api-18) | `bool isInf<T>(T value)` |
| [std::math::isNaN](std-math.md#api-17) | `bool isNaN<T>(T value)` |
| [std::math::ln](std-math.md#api-8) | `let ln<T>(T value)` |
| [std::math::log10](std-math.md#api-9) | `let log10<T>(T value)` |
| [std::math::max](std-math.md#api-21) | `T max<T>(T... values)` |
| [std::math::min](std-math.md#api-20) | `T min<T>(T... values)` |
| [std::math::pi](std-math.md#api-0) | `BigDecimal pi(int64 digits = 50)` |
| [std::math::pow](std-math.md#api-15) | `let pow<T, U>(T value, U other)` |
| [std::math::round](std-math.md#api-13) | `let round<T>(T value)` |
| [std::math::sin](std-math.md#api-1) | `let sin<T>(T value)` |
| [std::math::sqrt](std-math.md#api-10) | `let sqrt<T>(T value)` |
| [std::math::tan](std-math.md#api-3) | `let tan<T>(T value)` |
| [std::math::trunc](std-math.md#api-14) | `let trunc<T>(T value)` |
| [std::memory::Arena::allocate](std-memory.md#api-6) | `Span<char> allocate(int64 bytes)` |
| [std::memory::Arena::create](std-memory.md#api-5) | `static Arena create()` |
| [std::memory::Arena::reset](std-memory.md#api-7) | `void reset()` |
| [std::memory::Memory::stats](std-memory.md#api-4) | `static MemoryStats stats()` |
| [std::memory::Span::from](std-memory.md#api-0) | `static Span<T> from(T mut& data, int64 length)` |
| [std::memory::Span::index](std-memory.md#api-1) | `T index(int64 key)` |
| [std::memory::Span::iterator](std-memory.md#api-2) | `Option<T mut&> iterator(int64 index)` |
| [std::memory::Span::len](std-memory.md#api-3) | `int64 len()` |
| [std::message::receiveFor](std-message.md#api-4) | `std::concurrency::Receive<T> receiveFor<T>(int64 millis)` |
| [std::message::receiveThread](std-message.md#api-1) | `T receiveThread<T>()` |
| [std::message::sendFor](std-message.md#api-2) | `int64 sendFor<T>(Thread thread, T value, int64 millis)` |
| [std::message::sendThread](std-message.md#api-0) | `void sendThread<T>(Thread thread, T value)` |
| [std::message::tryReceive](std-message.md#api-5) | `std::concurrency::Receive<T> tryReceive<T>()` |
| [std::message::trySend](std-message.md#api-3) | `int64 trySend<T>(Thread thread, T value)` |
| [std::net::HttpClient::close](std-net.md#api-34) | `void close()` |
| [std::net::HttpClient::connect](std-net.md#api-31) | `bool connect(std::lang::String host, int32 port, int32 timeoutMs)` |
| [std::net::HttpClient::exchange](std-net.md#api-33) | `HttpResponse exchange(std::lang::String method, std::lang::String url, std::lang::String body, std::lang::String contentType)` |
| [std::net::HttpClient::fetch](std-net.md#api-35) | `static HttpResponse fetch(std::lang::String url)` |
| [std::net::HttpClient::readAll](std-net.md#api-32) | `std::lang::String readAll()` |
| [std::net::HttpClient::submit](std-net.md#api-36) | `static HttpResponse submit(std::lang::String url, std::lang::String body, std::lang::String contentType)` |
| [std::net::HttpResponse::header](std-net.md#api-26) | `std::lang::String header(std::lang::String name)` |
| [std::net::HttpResponse::isSuccess](std-net.md#api-25) | `bool isSuccess()` |
| [std::net::TcpListener::accept](std-net.md#api-22) | `TcpSocket accept()` |
| [std::net::TcpListener::accept](std-net.md#api-21) | `TcpSocket accept(int32 timeoutMs)` |
| [std::net::TcpListener::acceptAsync](std-net.md#api-23) | `TcpSocket acceptAsync()` |
| [std::net::TcpListener::close](std-net.md#api-24) | `void close()` |
| [std::net::TcpListener::isOpen](std-net.md#api-18) | `bool isOpen()` |
| [std::net::TcpListener::lastError](std-net.md#api-19) | `String lastError()` |
| [std::net::TcpListener::listen](std-net.md#api-17) | `static TcpListener listen(String host, int32 port)` |
| [std::net::TcpListener::listen](std-net.md#api-16) | `static TcpListener listen(String host, int32 port, int32 backlog)` |
| [std::net::TcpListener::localPort](std-net.md#api-20) | `int64 localPort()` |
| [std::net::TcpSocket::close](std-net.md#api-12) | `void close()` |
| [std::net::TcpSocket::connect](std-net.md#api-1) | `static TcpSocket connect(String host, int32 port)` |
| [std::net::TcpSocket::connect](std-net.md#api-0) | `static TcpSocket connect(String host, int32 port, int32 timeoutMs)` |
| [std::net::TcpSocket::connectAsync](std-net.md#api-3) | `static TcpSocket connectAsync(String host, int32 port)` |
| [std::net::TcpSocket::connectAsync](std-net.md#api-2) | `static TcpSocket connectAsync(String host, int32 port, int32 timeoutMs)` |
| [std::net::TcpSocket::isOpen](std-net.md#api-9) | `bool isOpen()` |
| [std::net::TcpSocket::lastError](std-net.md#api-10) | `String lastError()` |
| [std::net::TcpSocket::pollConnect](std-net.md#api-4) | `int64 pollConnect()` |
| [std::net::TcpSocket::receive](std-net.md#api-14) | `String receive(int32 maxBytes)` |
| [std::net::TcpSocket::receiveAsync](std-net.md#api-7) | `String receiveAsync(int32 maxBytes)` |
| [std::net::TcpSocket::receiveLine](std-net.md#api-15) | `String receiveLine(int32 timeoutMs)` |
| [std::net::TcpSocket::receiveLineAsync](std-net.md#api-8) | `String receiveLineAsync()` |
| [std::net::TcpSocket::send](std-net.md#api-13) | `int64 send(String data)` |
| [std::net::TcpSocket::sendAsync](std-net.md#api-6) | `int64 sendAsync(String data)` |
| [std::net::TcpSocket::sendAsync](std-net.md#api-5) | `int64 sendAsync(String data, int64 offset)` |
| [std::net::TcpSocket::status](std-net.md#api-11) | `int64 status()` |
| [std::net::buildRequest](std-net.md#api-29) | `std::lang::String buildRequest(std::lang::String method, std::lang::String url, std::lang::String body, std::lang::String contentType)` |
| [std::net::httpScheme](std-net.md#api-27) | `std::lang::String httpScheme()` |
| [std::net::parseResponse](std-net.md#api-30) | `HttpResponse parseResponse(std::lang::String raw)` |
| [std::net::parseUrl](std-net.md#api-28) | `Url parseUrl(std::lang::String url)` |
| [std::numerics::Numerics::bisect](std-numerics.md#api-2) | `static float64 bisect(float64 fn(float64) function, float64 left, float64 right, float64 tolerance, int64 maxIterations)` |
| [std::numerics::Numerics::derivative](std-numerics.md#api-0) | `static float64 derivative(float64 fn(float64) function, float64 x, float64 step)` |
| [std::numerics::Numerics::integrateSimpson](std-numerics.md#api-1) | `static float64 integrateSimpson(float64 fn(float64) function, float64 begin, float64 end, int64 intervals)` |
| [std::numerics::Numerics::newton](std-numerics.md#api-4) | `static float64 newton(float64 fn(float64) function, float64 initial, float64 tolerance, int64 maxIterations)` |
| [std::numerics::Numerics::rk4Step](std-numerics.md#api-3) | `static float64 rk4Step(float64 fn(float64, float64) derivative, float64 time, float64 value, float64 step)` |
| [std::numerics::Numerics::solveOdeEuler](std-numerics.md#api-5) | `static std::lang::List<float64> solveOdeEuler( float64 fn(float64, float64) rate, float64 initialTime, float64 initialValue, float64 step, int64 steps)` |
| [std::numerics::Numerics::solveOdeRK4](std-numerics.md#api-6) | `static std::lang::List<float64> solveOdeRK4( float64 fn(float64, float64) rate, float64 initialTime, float64 initialValue, float64 step, int64 steps)` |
| [std::numtheory::addMod](std-numtheory.md#api-2) | `T addMod<T>(T left, T right, T modulus)` |
| [std::numtheory::gcd](std-numtheory.md#api-0) | `T gcd<T>(T... values)` |
| [std::numtheory::isPrime](std-numtheory.md#api-6) | `bool isPrime<T>(T value)` |
| [std::numtheory::lcm](std-numtheory.md#api-1) | `T lcm<T>(T... values)` |
| [std::numtheory::modInverse](std-numtheory.md#api-5) | `T modInverse<T>(T value, T modulus)` |
| [std::numtheory::mulMod](std-numtheory.md#api-3) | `T mulMod<T>(T left, T right, T modulus)` |
| [std::numtheory::powMod](std-numtheory.md#api-4) | `T powMod<T>(T base, T exponent, T modulus)` |
| [std::parse::Parse::parseBool](std-parse.md#api-3) | `static Result<bool, ParseError> parseBool(std::lang::String text)` |
| [std::parse::Parse::parseFloat64](std-parse.md#api-2) | `static Result<float64, ParseError> parseFloat64(std::lang::String text)` |
| [std::parse::Parse::parseInt64](std-parse.md#api-0) | `static Result<int64, ParseError> parseInt64(std::lang::String text)` |
| [std::parse::Parse::parseUInt64](std-parse.md#api-1) | `static Result<uint64, ParseError> parseUInt64(std::lang::String text)` |
| [std::path::PathString::extension](std-path.md#api-2) | `static std::lang::String extension(std::lang::String path)` |
| [std::path::PathString::fileName](std-path.md#api-1) | `static std::lang::String fileName(std::lang::String path)` |
| [std::path::PathString::join](std-path.md#api-0) | `static std::lang::String join(std::lang::String left, std::lang::String right)` |
| [std::path::PathString::normalize](std-path.md#api-3) | `static std::lang::String normalize(std::lang::String path)` |
| [std::probe::twice](std-probe.md#api-0) | `int64 twice(int64 x)` |
| [std::process::processClose](std-process.md#api-2) | `int64 processClose(Process const& process)` |
| [std::process::processRead](std-process.md#api-1) | `std::lang::String processRead(Process const& process)` |
| [std::process::processRun](std-process.md#api-0) | `Process processRun(std::lang::String command)` |
| [std::process::system](std-process.md#api-3) | `int64 system(std::lang::String command)` |
| [std::random::Random::fromEntropy](std-random.md#api-9) | `static Random fromEntropy()` |
| [std::random::Random::nextBool](std-random.md#api-14) | `bool nextBool()` |
| [std::random::Random::nextFloat64](std-random.md#api-12) | `float64 nextFloat64()` |
| [std::random::Random::nextInt](std-random.md#api-11) | `int64 nextInt(int64 minimum, int64 maximum)` |
| [std::random::Random::nextRange](std-random.md#api-13) | `float64 nextRange(float64 minimum, float64 maximum)` |
| [std::random::Random::nextUInt64](std-random.md#api-10) | `uint64 nextUInt64()` |
| [std::random::Random::withSeed](std-random.md#api-8) | `static Random withSeed(uint64 seed)` |
| [std::random::randomDrop](std-random.md#api-7) | `void randomDrop(uint64 handle)` |
| [std::random::randomFromEntropy](std-random.md#api-1) | `Random randomFromEntropy()` |
| [std::random::randomNextBool](std-random.md#api-6) | `bool randomNextBool(uint64 handle)` |
| [std::random::randomNextFloat64](std-random.md#api-4) | `float64 randomNextFloat64(uint64 handle)` |
| [std::random::randomNextInt](std-random.md#api-3) | `int64 randomNextInt(uint64 handle, int64 minimum, int64 maximum)` |
| [std::random::randomNextRange](std-random.md#api-5) | `float64 randomNextRange(uint64 handle, float64 minimum, float64 maximum)` |
| [std::random::randomNextUInt64](std-random.md#api-2) | `uint64 randomNextUInt64(uint64 handle)` |
| [std::random::randomWithSeed](std-random.md#api-0) | `Random randomWithSeed(uint64 seed)` |
| [std::reflect::ReflectValue::asBool](std-reflect.md#api-2) | `Result<bool,std::reflect::ReflectError> asBool()` |
| [std::reflect::ReflectValue::asChar](std-reflect.md#api-3) | `Result<char,std::reflect::ReflectError> asChar()` |
| [std::reflect::ReflectValue::asF32](std-reflect.md#api-12) | `Result<float32,std::reflect::ReflectError> asF32()` |
| [std::reflect::ReflectValue::asF64](std-reflect.md#api-13) | `Result<float64,std::reflect::ReflectError> asF64()` |
| [std::reflect::ReflectValue::asI16](std-reflect.md#api-5) | `Result<int16,std::reflect::ReflectError> asI16()` |
| [std::reflect::ReflectValue::asI32](std-reflect.md#api-6) | `Result<int32,std::reflect::ReflectError> asI32()` |
| [std::reflect::ReflectValue::asI64](std-reflect.md#api-7) | `Result<int64,std::reflect::ReflectError> asI64()` |
| [std::reflect::ReflectValue::asI8](std-reflect.md#api-4) | `Result<int8,std::reflect::ReflectError> asI8()` |
| [std::reflect::ReflectValue::asText](std-reflect.md#api-14) | `Result<std::lang::String,std::reflect::ReflectError> asText()` |
| [std::reflect::ReflectValue::asU16](std-reflect.md#api-9) | `Result<uint16,std::reflect::ReflectError> asU16()` |
| [std::reflect::ReflectValue::asU32](std-reflect.md#api-10) | `Result<uint32,std::reflect::ReflectError> asU32()` |
| [std::reflect::ReflectValue::asU64](std-reflect.md#api-11) | `Result<uint64,std::reflect::ReflectError> asU64()` |
| [std::reflect::ReflectValue::asU8](std-reflect.md#api-8) | `Result<uint8,std::reflect::ReflectError> asU8()` |
| [std::reflect::ReflectValue::typeId](std-reflect.md#api-1) | `uint64 typeId()` |
| [std::reflect::call](std-reflect.md#api-28) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> call(std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [std::reflect::callEnumMethod](std-reflect.md#api-32) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callEnumMethod<T>(T mut& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [std::reflect::callEnumMethodConst](std-reflect.md#api-33) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callEnumMethodConst<T>(T const& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [std::reflect::callEnumStatic](std-reflect.md#api-34) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callEnumStatic<T>(std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [std::reflect::callInterface](std-reflect.md#api-38) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callInterface<I>(I mut& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [std::reflect::callInterfaceConst](std-reflect.md#api-39) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callInterfaceConst<I>(I const& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [std::reflect::callMethod](std-reflect.md#api-29) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callMethod<T>(T mut& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [std::reflect::callMethodConst](std-reflect.md#api-30) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callMethodConst<T>(T const& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [std::reflect::callStatic](std-reflect.md#api-31) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callStatic<T>(std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [std::reflect::construct](std-reflect.md#api-35) | `Result<T,std::reflect::ReflectError> construct<T>(List<std::reflect::ReflectValue> args)` |
| [std::reflect::getAnnotationInfo](std-reflect.md#api-16) | `Option<std::reflect::AnnotationInfo> getAnnotationInfo(std::lang::String name)` |
| [std::reflect::getAnnotations](std-reflect.md#api-23) | `List<std::reflect::Annotation> getAnnotations<T>()` |
| [std::reflect::getEnumInfo](std-reflect.md#api-20) | `Option<std::reflect::EnumInfo> getEnumInfo<T>()` |
| [std::reflect::getField](std-reflect.md#api-36) | `Result<F,std::reflect::ReflectError> getField<T,F>(T const& obj, std::lang::String name)` |
| [std::reflect::getFieldAnnotations](std-reflect.md#api-24) | `List<std::reflect::Annotation> getFieldAnnotations<T>(std::lang::String name)` |
| [std::reflect::getFuncAnnotations](std-reflect.md#api-25) | `List<std::reflect::Annotation> getFuncAnnotations(std::lang::String name)` |
| [std::reflect::getFuncInfo](std-reflect.md#api-15) | `Option<std::reflect::FuncInfo> getFuncInfo(std::lang::String name)` |
| [std::reflect::getInterfaceId](std-reflect.md#api-18) | `uint64 getInterfaceId<I>()` |
| [std::reflect::getInterfaceInfo](std-reflect.md#api-21) | `Option<std::reflect::InterfaceInfo> getInterfaceInfo<I>()` |
| [std::reflect::getMethodAnnotations](std-reflect.md#api-26) | `List<std::reflect::Annotation> getMethodAnnotations<T>(std::lang::String name)` |
| [std::reflect::getStaticMemberAnnotations](std-reflect.md#api-27) | `List<std::reflect::Annotation> getStaticMemberAnnotations<T>(std::lang::String name)` |
| [std::reflect::getStructInfo](std-reflect.md#api-19) | `Option<std::reflect::StructInfo> getStructInfo<T>()` |
| [std::reflect::getTypeId](std-reflect.md#api-17) | `uint64 getTypeId<T>()` |
| [std::reflect::implementsInterface](std-reflect.md#api-22) | `bool implementsInterface<T>(uint64 interfaceId)` |
| [std::reflect::requireReflection](std-reflect.md#api-0) | `void requireReflection()` |
| [std::reflect::setField](std-reflect.md#api-37) | `Result<bool,std::reflect::ReflectError> setField<T,F>(T mut& obj, std::lang::String name, F value)` |
| [std::regex::Regex::capture](std-regex.md#api-4) | `std::lang::String capture(std::lang::String text, int64 group)` |
| [std::regex::Regex::compile](std-regex.md#api-0) | `static Result<Regex, RegexError> compile(std::lang::String pattern)` |
| [std::regex::Regex::contains](std-regex.md#api-2) | `bool contains(std::lang::String text)` |
| [std::regex::Regex::find](std-regex.md#api-3) | `Option<int64> find(std::lang::String text)` |
| [std::regex::Regex::fullMatch](std-regex.md#api-1) | `bool fullMatch(std::lang::String text)` |
| [std::regex::Regex::replaceAll](std-regex.md#api-6) | `std::lang::String replaceAll(std::lang::String text, std::lang::String replacement)` |
| [std::regex::Regex::replaceFirst](std-regex.md#api-5) | `std::lang::String replaceFirst(std::lang::String text, std::lang::String replacement)` |
| [std::serialization::deserialize](std-serialization.md#api-1) | `Result<T,String> deserialize<T>(List<uint8> bytes)` |
| [std::serialization::serialize](std-serialization.md#api-0) | `Result<List<uint8>,String> serialize<T>(T const& value)` |
| [std::signal::dft](std-signal.md#api-4) | `std::lang::List<Complex<float64>> dft(std::lang::List<Complex<float64>> input)` |
| [std::signal::fft](std-signal.md#api-6) | `std::lang::List<Complex<float64>> fft(std::lang::List<Complex<float64>> input)` |
| [std::signal::idft](std-signal.md#api-5) | `std::lang::List<Complex<float64>> idft(std::lang::List<Complex<float64>> input)` |
| [std::signal::ifft](std-signal.md#api-7) | `std::lang::List<Complex<float64>> ifft(std::lang::List<Complex<float64>> input)` |
| [std::signal::signalDft](std-signal.md#api-0) | `std::lang::List<Complex<float64>> signalDft(std::lang::List<Complex<float64>> input)` |
| [std::signal::signalFft](std-signal.md#api-2) | `std::lang::List<Complex<float64>> signalFft(std::lang::List<Complex<float64>> input)` |
| [std::signal::signalIdft](std-signal.md#api-1) | `std::lang::List<Complex<float64>> signalIdft(std::lang::List<Complex<float64>> input)` |
| [std::signal::signalIfft](std-signal.md#api-3) | `std::lang::List<Complex<float64>> signalIfft(std::lang::List<Complex<float64>> input)` |
| [std::sql::DataCell::asInt](std-sql.md#api-7) | `int64 asInt()` |
| [std::sql::DataCell::asReal](std-sql.md#api-8) | `float64 asReal()` |
| [std::sql::DataCell::isNull](std-sql.md#api-4) | `bool isNull()` |
| [std::sql::DataCell::isNumber](std-sql.md#api-5) | `bool isNumber()` |
| [std::sql::DataCell::text](std-sql.md#api-6) | `std::lang::String text()` |
| [std::sql::DataRow::at](std-sql.md#api-10) | `DataCell at(int64 index)` |
| [std::sql::DataRow::size](std-sql.md#api-9) | `int64 size()` |
| [std::sql::Database::affectedRows](std-sql.md#api-37) | `int64 affectedRows()` |
| [std::sql::Database::backendName](std-sql.md#api-31) | `std::lang::String backendName()` |
| [std::sql::Database::close](std-sql.md#api-38) | `void close()` |
| [std::sql::Database::count](std-sql.md#api-43) | `int64 count(std::lang::String sql)` |
| [std::sql::Database::driver](std-sql.md#api-29) | `Driver driver()` |
| [std::sql::Database::exec](std-sql.md#api-40) | `ResultSet exec(SqlStatement statement)` |
| [std::sql::Database::exec](std-sql.md#api-39) | `ResultSet exec(std::lang::String sql)` |
| [std::sql::Database::execOk](std-sql.md#api-41) | `bool execOk(std::lang::String sql)` |
| [std::sql::Database::hasTable](std-sql.md#api-44) | `bool hasTable(std::lang::String tableName)` |
| [std::sql::Database::isOpen](std-sql.md#api-35) | `bool isOpen()` |
| [std::sql::Database::lastError](std-sql.md#api-36) | `std::lang::String lastError()` |
| [std::sql::Database::local](std-sql.md#api-34) | `static Database local(std::lang::String database)` |
| [std::sql::Database::maria](std-sql.md#api-33) | `static Database maria(String host, String user, String password, String database, int32 port = (int32)3306)` |
| [std::sql::Database::mysql](std-sql.md#api-24) | `static Database mysql(String host, String user, String password, String database, int32 port)` |
| [std::sql::Database::mysql](std-sql.md#api-32) | `static Database mysql(std::lang::String host, std::lang::String user, std::lang::String password, std::lang::String database)` |
| [std::sql::Database::odbc](std-sql.md#api-27) | `static Database odbc(String options)` |
| [std::sql::Database::postgres](std-sql.md#api-26) | `static Database postgres(String options)` |
| [std::sql::Database::quote](std-sql.md#api-30) | `std::lang::String quote(std::lang::String text)` |
| [std::sql::Database::scalar](std-sql.md#api-42) | `std::lang::String scalar(std::lang::String sql)` |
| [std::sql::Database::sqlServer](std-sql.md#api-28) | `static Database sqlServer(String options)` |
| [std::sql::Database::sqlite](std-sql.md#api-25) | `static Database sqlite(String path)` |
| [std::sql::Query::build](std-sql.md#api-3) | `Result<SqlStatement,String> build(String operation = "select", String values = "{}")` |
| [std::sql::Query::orderBy](std-sql.md#api-0) | `void orderBy(String field, bool descending = false)` |
| [std::sql::Query::page](std-sql.md#api-2) | `void page(int64 limit, int64 offset = 0)` |
| [std::sql::Query::select](std-sql.md#api-1) | `void select(String field)` |
| [std::sql::ResultSet::at](std-sql.md#api-16) | `DataCell at(int64 row, int64 column)` |
| [std::sql::ResultSet::colCount](std-sql.md#api-12) | `int64 colCount()` |
| [std::sql::ResultSet::columnIndex](std-sql.md#api-15) | `int64 columnIndex(std::lang::String name)` |
| [std::sql::ResultSet::columnName](std-sql.md#api-14) | `std::lang::String columnName(int64 index)` |
| [std::sql::ResultSet::count](std-sql.md#api-21) | `int64 count()` |
| [std::sql::ResultSet::intAt](std-sql.md#api-18) | `int64 intAt(int64 row, int64 column)` |
| [std::sql::ResultSet::isEmpty](std-sql.md#api-13) | `bool isEmpty()` |
| [std::sql::ResultSet::pad](std-sql.md#api-22) | `std::lang::String pad(std::lang::String text, int64 width)` |
| [std::sql::ResultSet::realAt](std-sql.md#api-19) | `float64 realAt(int64 row, int64 column)` |
| [std::sql::ResultSet::render](std-sql.md#api-23) | `std::lang::String render()` |
| [std::sql::ResultSet::rowCount](std-sql.md#api-11) | `int64 rowCount()` |
| [std::sql::ResultSet::scalar](std-sql.md#api-20) | `std::lang::String scalar()` |
| [std::sql::ResultSet::textAt](std-sql.md#api-17) | `std::lang::String textAt(int64 row, int64 column)` |
| [std::sql::displayWidth](std-sql.md#api-49) | `int64 displayWidth(std::lang::String text)` |
| [std::sql::filter](std-sql.md#api-51) | `Result<bool,String> filter<V>(Query mut& query, String field, V value, int64 operation = 0)` |
| [std::sql::filterMany](std-sql.md#api-52) | `Result<bool,String> filterMany<V>(Query mut& query, String field, List<V> values, int64 operation = 11)` |
| [std::sql::insert](std-sql.md#api-53) | `Result<SqlStatement,String> insert<T>(Query mut& query, T const& value)` |
| [std::sql::intLiteral](std-sql.md#api-47) | `std::lang::String intLiteral(int64 value)` |
| [std::sql::query](std-sql.md#api-50) | `Result<Query,String> query<T>()` |
| [std::sql::quote](std-sql.md#api-46) | `std::lang::String quote(std::lang::String text)` |
| [std::sql::quote](std-sql.md#api-45) | `std::lang::String quote(std::lang::String text, bool escapeBackslash)` |
| [std::sql::realLiteral](std-sql.md#api-48) | `std::lang::String realLiteral(float64 value)` |
| [std::sql::update](std-sql.md#api-54) | `Result<SqlStatement,String> update<T>(Query mut& query, T const& value)` |
| [std::text::StringBuilder::append](std-text.md#api-25) | `void append(std::lang::String text)` |
| [std::text::StringBuilder::appendBool](std-text.md#api-30) | `void appendBool(bool value)` |
| [std::text::StringBuilder::appendChar](std-text.md#api-27) | `void appendChar(char value)` |
| [std::text::StringBuilder::appendFloat](std-text.md#api-29) | `void appendFloat(float64 value)` |
| [std::text::StringBuilder::appendInt](std-text.md#api-28) | `void appendInt(int64 value)` |
| [std::text::StringBuilder::appendLine](std-text.md#api-26) | `void appendLine(std::lang::String text)` |
| [std::text::StringBuilder::clear](std-text.md#api-31) | `void clear()` |
| [std::text::StringBuilder::create](std-text.md#api-22) | `static StringBuilder create()` |
| [std::text::StringBuilder::drop](std-text.md#api-21) | `void drop()` |
| [std::text::StringBuilder::isEmpty](std-text.md#api-24) | `bool isEmpty()` |
| [std::text::StringBuilder::len](std-text.md#api-23) | `int64 len()` |
| [std::text::StringBuilder::toString](std-text.md#api-32) | `std::lang::String toString()` |
| [std::text::Text::charAt](std-text.md#api-15) | `static char charAt(std::lang::String text, int64 index)` |
| [std::text::Text::compare](std-text.md#api-5) | `static int64 compare(std::lang::String left, std::lang::String right)` |
| [std::text::Text::contains](std-text.md#api-2) | `static bool contains(std::lang::String text, std::lang::String needle)` |
| [std::text::Text::endsWith](std-text.md#api-4) | `static bool endsWith(std::lang::String text, std::lang::String suffix)` |
| [std::text::Text::find](std-text.md#api-0) | `static int64 find(std::lang::String text, std::lang::String needle)` |
| [std::text::Text::join](std-text.md#api-17) | `static std::lang::String join(std::lang::List<std::lang::String> parts, std::lang::String separator)` |
| [std::text::Text::kmpFailureTable](std-text.md#api-20) | `static std::lang::List<int64> kmpFailureTable(std::lang::String needle)` |
| [std::text::Text::kmpFind](std-text.md#api-18) | `static int64 kmpFind(std::lang::String text, std::lang::String needle)` |
| [std::text::Text::kmpFindAll](std-text.md#api-19) | `static std::lang::List<int64> kmpFindAll(std::lang::String text, std::lang::String needle)` |
| [std::text::Text::ltrim](std-text.md#api-9) | `static std::lang::String ltrim(std::lang::String text)` |
| [std::text::Text::padEnd](std-text.md#api-14) | `static std::lang::String padEnd(std::lang::String text, int64 width, char fill)` |
| [std::text::Text::padStart](std-text.md#api-13) | `static std::lang::String padStart(std::lang::String text, int64 width, char fill)` |
| [std::text::Text::repeat](std-text.md#api-12) | `static std::lang::String repeat(std::lang::String text, int64 count)` |
| [std::text::Text::replace](std-text.md#api-11) | `static std::lang::String replace(std::lang::String text, std::lang::String from, std::lang::String to)` |
| [std::text::Text::rfind](std-text.md#api-1) | `static int64 rfind(std::lang::String text, std::lang::String needle)` |
| [std::text::Text::rtrim](std-text.md#api-10) | `static std::lang::String rtrim(std::lang::String text)` |
| [std::text::Text::split](std-text.md#api-16) | `static std::lang::List<std::lang::String> split(std::lang::String text, std::lang::String separator)` |
| [std::text::Text::startsWith](std-text.md#api-3) | `static bool startsWith(std::lang::String text, std::lang::String prefix)` |
| [std::text::Text::toLower](std-text.md#api-7) | `static std::lang::String toLower(std::lang::String text)` |
| [std::text::Text::toUpper](std-text.md#api-6) | `static std::lang::String toUpper(std::lang::String text)` |
| [std::text::Text::trim](std-text.md#api-8) | `static std::lang::String trim(std::lang::String text)` |
| [std::thread::ThreadGroup::add](std-thread.md#api-21) | `void add(Thread task)` |
| [std::thread::ThreadGroup::cancelAll](std-thread.md#api-22) | `void cancelAll()` |
| [std::thread::ThreadGroup::create](std-thread.md#api-19) | `static ThreadGroup create()` |
| [std::thread::ThreadGroup::drop](std-thread.md#api-24) | `void drop()` |
| [std::thread::ThreadGroup::start](std-thread.md#api-20) | `Thread start(void fn() task)` |
| [std::thread::ThreadGroup::waitAll](std-thread.md#api-23) | `bool waitAll()` |
| [std::thread::ThreadPool::cancelAll](std-thread.md#api-29) | `void cancelAll()` |
| [std::thread::ThreadPool::closeAndWait](std-thread.md#api-28) | `bool closeAndWait()` |
| [std::thread::ThreadPool::create](std-thread.md#api-25) | `static ThreadPool create(int64 workers, int64 capacity)` |
| [std::thread::ThreadPool::drop](std-thread.md#api-30) | `void drop()` |
| [std::thread::ThreadPool::submit](std-thread.md#api-27) | `int64 submit(void fn() task)` |
| [std::thread::ThreadPool::submitFor](std-thread.md#api-26) | `int64 submitFor(void fn() task, int64 millis)` |
| [std::thread::ThreadResult::isReady](std-thread.md#api-9) | `bool isReady()` |
| [std::thread::closeInbox](std-thread.md#api-18) | `void closeInbox()` |
| [std::thread::create](std-thread.md#api-0) | `Thread create(void fn() task)` |
| [std::thread::createWith](std-thread.md#api-1) | `Thread createWith<T>(void fn(T) task, T value)` |
| [std::thread::current](std-thread.md#api-14) | `Thread current()` |
| [std::thread::parent](std-thread.md#api-15) | `Thread parent()` |
| [std::thread::receive](std-thread.md#api-13) | `String receive()` |
| [std::thread::receiveFor](std-thread.md#api-16) | `std::concurrency::Receive<String> receiveFor(int64 millis)` |
| [std::thread::requestStop](std-thread.md#api-5) | `void requestStop(Thread thread)` |
| [std::thread::send](std-thread.md#api-12) | `void send(Thread thread, String message)` |
| [std::thread::sleepFor](std-thread.md#api-8) | `int64 sleepFor(int64 millis)` |
| [std::thread::state](std-thread.md#api-7) | `int64 state(Thread thread)` |
| [std::thread::stopRequested](std-thread.md#api-6) | `bool stopRequested()` |
| [std::thread::tryReceive](std-thread.md#api-17) | `std::concurrency::Receive<String> tryReceive()` |
| [std::thread::tryWait](std-thread.md#api-4) | `bool tryWait(Thread thread)` |
| [std::thread::wait](std-thread.md#api-2) | `void wait(Thread thread)` |
| [std::thread::waitFor](std-thread.md#api-3) | `bool waitFor(Thread thread, int64 millis)` |
| [std::thread::waitResult](std-thread.md#api-11) | `ThreadResult waitResult(Thread thread)` |
| [std::thread::waitResultFor](std-thread.md#api-10) | `ThreadResult waitResultFor(Thread thread, int64 millis)` |
| [std::toml::TomlValue::asBool](std-toml.md#api-17) | `Option<bool> asBool()` |
| [std::toml::TomlValue::asBytes](std-toml.md#api-19) | `Option<std::lang::List<uint8>> asBytes()` |
| [std::toml::TomlValue::asDateTime](std-toml.md#api-20) | `Option<std::lang::String> asDateTime()` |
| [std::toml::TomlValue::asFloat64](std-toml.md#api-16) | `Option<float64> asFloat64()` |
| [std::toml::TomlValue::asInt](std-toml.md#api-15) | `Option<int64> asInt()` |
| [std::toml::TomlValue::asString](std-toml.md#api-18) | `Option<std::lang::String> asString()` |
| [std::toml::TomlValue::at](std-toml.md#api-23) | `Option<TomlValue> at(int64 index)` |
| [std::toml::TomlValue::close](std-toml.md#api-3) | `bool close()` |
| [std::toml::TomlValue::dump](std-toml.md#api-27) | `std::lang::String dump()` |
| [std::toml::TomlValue::find](std-toml.md#api-21) | `Option<TomlValue> find(std::lang::String key)` |
| [std::toml::TomlValue::findBytes](std-toml.md#api-22) | `Option<TomlValue> findBytes(std::lang::List<uint8> key)` |
| [std::toml::TomlValue::isArray](std-toml.md#api-10) | `bool isArray()` |
| [std::toml::TomlValue::isBool](std-toml.md#api-6) | `bool isBool()` |
| [std::toml::TomlValue::isDateTime](std-toml.md#api-13) | `bool isDateTime()` |
| [std::toml::TomlValue::isFloat](std-toml.md#api-8) | `bool isFloat()` |
| [std::toml::TomlValue::isInt](std-toml.md#api-7) | `bool isInt()` |
| [std::toml::TomlValue::isNull](std-toml.md#api-5) | `bool isNull()` |
| [std::toml::TomlValue::isObject](std-toml.md#api-11) | `bool isObject()` |
| [std::toml::TomlValue::isString](std-toml.md#api-9) | `bool isString()` |
| [std::toml::TomlValue::isTable](std-toml.md#api-12) | `bool isTable()` |
| [std::toml::TomlValue::isValid](std-toml.md#api-2) | `bool isValid()` |
| [std::toml::TomlValue::iterator](std-toml.md#api-24) | `Option<TomlValue> iterator(int64 index)` |
| [std::toml::TomlValue::keyAt](std-toml.md#api-25) | `Option<std::lang::String> keyAt(int64 index)` |
| [std::toml::TomlValue::keyBytesAt](std-toml.md#api-26) | `Option<std::lang::List<uint8>> keyBytesAt(int64 index)` |
| [std::toml::TomlValue::kind](std-toml.md#api-4) | `int64 kind()` |
| [std::toml::TomlValue::len](std-toml.md#api-14) | `int64 len()` |
| [std::toml::deserialize](std-toml.md#api-1) | `Result<T,String> deserialize<T>(String text)` |
| [std::toml::parse](std-toml.md#api-28) | `Result<TomlValue, std::lang::String> parse(std::lang::String text)` |
| [std::toml::serialize](std-toml.md#api-0) | `Result<String,String> serialize<T>(T const& value)` |
| [std::tree::BinarySearchTree::clear](std-tree.md#api-2) | `void clear()` |
| [std::tree::BinarySearchTree::contains](std-tree.md#api-5) | `bool contains(int64 value)` |
| [std::tree::BinarySearchTree::create](std-tree.md#api-0) | `static BinarySearchTree create()` |
| [std::tree::BinarySearchTree::height](std-tree.md#api-8) | `int64 height()` |
| [std::tree::BinarySearchTree::inorder](std-tree.md#api-9) | `List<int64> inorder()` |
| [std::tree::BinarySearchTree::insert](std-tree.md#api-3) | `bool insert(int64 value)` |
| [std::tree::BinarySearchTree::len](std-tree.md#api-1) | `int64 len()` |
| [std::tree::BinarySearchTree::max](std-tree.md#api-7) | `Option<int64> max()` |
| [std::tree::BinarySearchTree::min](std-tree.md#api-6) | `Option<int64> min()` |
| [std::tree::BinarySearchTree::postorder](std-tree.md#api-11) | `List<int64> postorder()` |
| [std::tree::BinarySearchTree::preorder](std-tree.md#api-10) | `List<int64> preorder()` |
| [std::tree::BinarySearchTree::remove](std-tree.md#api-4) | `bool remove(int64 value)` |
| [std::tree::BinarySearchTree::snapshot](std-tree.md#api-12) | `TreeSnapshot snapshot()` |
| [std::tree::GeneralTree::addChild](std-tree.md#api-36) | `int64 addChild(int64 cursor, int64 value)` |
| [std::tree::GeneralTree::addRoot](std-tree.md#api-35) | `int64 addRoot(int64 value)` |
| [std::tree::GeneralTree::children](std-tree.md#api-39) | `List<int64> children(int64 cursor)` |
| [std::tree::GeneralTree::clear](std-tree.md#api-34) | `void clear()` |
| [std::tree::GeneralTree::create](std-tree.md#api-32) | `static GeneralTree create()` |
| [std::tree::GeneralTree::fromBinary](std-tree.md#api-44) | `static GeneralTree fromBinary(BinarySearchTree tree)` |
| [std::tree::GeneralTree::height](std-tree.md#api-42) | `int64 height()` |
| [std::tree::GeneralTree::len](std-tree.md#api-33) | `int64 len()` |
| [std::tree::GeneralTree::levelOrder](std-tree.md#api-41) | `List<int64> levelOrder()` |
| [std::tree::GeneralTree::preorder](std-tree.md#api-40) | `List<int64> preorder()` |
| [std::tree::GeneralTree::rootNode](std-tree.md#api-38) | `int64 rootNode()` |
| [std::tree::GeneralTree::snapshot](std-tree.md#api-45) | `GeneralTreeSnapshot snapshot()` |
| [std::tree::GeneralTree::toBinary](std-tree.md#api-43) | `BinarySearchTree toBinary()` |
| [std::tree::GeneralTree::valueAt](std-tree.md#api-37) | `int64 valueAt(int64 cursor)` |
| [std::tree::MinHeap::clear](std-tree.md#api-26) | `void clear()` |
| [std::tree::MinHeap::create](std-tree.md#api-24) | `static MinHeap create()` |
| [std::tree::MinHeap::isEmpty](std-tree.md#api-30) | `bool isEmpty()` |
| [std::tree::MinHeap::len](std-tree.md#api-25) | `int64 len()` |
| [std::tree::MinHeap::peek](std-tree.md#api-29) | `Option<int64> peek()` |
| [std::tree::MinHeap::pop](std-tree.md#api-28) | `Option<int64> pop()` |
| [std::tree::MinHeap::push](std-tree.md#api-27) | `void push(int64 value)` |
| [std::tree::MinHeap::snapshot](std-tree.md#api-31) | `List<int64> snapshot()` |
| [std::tree::RedBlackTree::clear](std-tree.md#api-19) | `void clear()` |
| [std::tree::RedBlackTree::contains](std-tree.md#api-16) | `bool contains(int64 value)` |
| [std::tree::RedBlackTree::create](std-tree.md#api-13) | `static RedBlackTree create()` |
| [std::tree::RedBlackTree::height](std-tree.md#api-18) | `int64 height()` |
| [std::tree::RedBlackTree::inorder](std-tree.md#api-23) | `List<int64> inorder()` |
| [std::tree::RedBlackTree::insert](std-tree.md#api-14) | `bool insert(int64 value)` |
| [std::tree::RedBlackTree::len](std-tree.md#api-17) | `int64 len()` |
| [std::tree::RedBlackTree::max](std-tree.md#api-21) | `Option<int64> max()` |
| [std::tree::RedBlackTree::min](std-tree.md#api-20) | `Option<int64> min()` |
| [std::tree::RedBlackTree::remove](std-tree.md#api-15) | `bool remove(int64 value)` |
| [std::tree::RedBlackTree::validate](std-tree.md#api-22) | `bool validate()` |
| [std::utf8::codePointCount](std-utf8.md#api-5) | `Result<int64,Utf8Error> codePointCount(String text)` |
| [std::utf8::decodeUtf8](std-utf8.md#api-1) | `Result<List<uint32>,Utf8Error> decodeUtf8(String text)` |
| [std::utf8::decodeUtf8Bytes](std-utf8.md#api-0) | `Result<List<uint32>,Utf8Error> decodeUtf8Bytes(List<uint8> text)` |
| [std::utf8::encodeUtf8](std-utf8.md#api-4) | `Result<String,Utf8Error> encodeUtf8(List<uint32> points)` |
| [std::utf8::encodeUtf8Bytes](std-utf8.md#api-3) | `Result<List<uint8>,Utf8Error> encodeUtf8Bytes(List<uint32> points)` |
| [std::utf8::isValidUtf8](std-utf8.md#api-2) | `bool isValidUtf8(String text)` |
| [std::utf8::sliceCodePoints](std-utf8.md#api-6) | `Result<String,Utf8Error> sliceCodePoints(String text, int64 start, int64 count)` |
| [std::watchdog::disableTimeout](std-watchdog.md#api-5) | `void disableTimeout()` |
| [std::watchdog::enable](std-watchdog.md#api-0) | `Result<int64, WatchdogError> enable(int64 watermarkBytes)` |
| [std::watchdog::enableTimeout](std-watchdog.md#api-2) | `Result<int64, WatchdogError> enableTimeout(int64 units)` |
| [std::watchdog::enableTimeoutMillis](std-watchdog.md#api-3) | `Result<int64, WatchdogError> enableTimeoutMillis(int64 millis)` |
| [std::watchdog::kick](std-watchdog.md#api-4) | `Result<bool, WatchdogError> kick()` |
| [std::watchdog::requestRestart](std-watchdog.md#api-1) | `Result<int64, WatchdogError> requestRestart()` |
