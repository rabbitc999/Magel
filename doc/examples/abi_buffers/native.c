#include <stdint.h>
#ifdef _WIN32
#define API __declspec(dllexport)
#else
#define API
#endif
typedef struct
{
    int8_t tag;
    float weight;
} Inner;
typedef struct
{
    Inner inner;
    int32_t count;
    double total;
} Packet;
API Packet transform(Packet p)
{
    p.inner.tag += 1;
    p.inner.weight += 0.5f;
    p.count += 2;
    p.total += 3;
    return p;
}
API void update(Packet *p)
{
    if (p)
    {
        p->count += 10;
        p->total += 20;
    }
}
API int32_t inspect(const Packet *p) { return p ? p->count : -1; }
API void bump(int32_t *p)
{
    if (p) ++*p;
}
API int32_t alias(Packet *a, const Packet *b)
{
    a->count += 1;
    return a == b && b->count == a->count;
}
static Packet saved = { { -5, 2.5f }, 9, 12.0 };
static int32_t buffer[4] = { 10, 20, 30, 40 };
API const Packet *snapshot(void) { return &saved; }
API Packet *nothing(void) { return 0; }
API int32_t *numbers(int32_t count) { return count == 4 ? buffer : 0; }
API void fill(int32_t *values, int32_t count)
{
    for (int32_t i = 0; i < count; ++i) values[i] += 2;
}
API void wipe(void)
{
    for (int i = 0; i < 4; ++i) buffer[i] = 0;
    saved.count = 0;
}
API int32_t *echo(int32_t *value) { return value; }
API int64_t wide(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e) { return a + b + c + d + e; }
API int32_t arrays_alias(int32_t *a, int32_t *b)
{
    a[0] += 1;
    return a == b && a[0] == b[0];
}
static Packet packets[2] = { { { 1, 1.5f }, 2, 3.0 }, { { 4, 4.5f }, 5, 6.0 } };
API Packet *get_packets(int32_t count) { return count == 2 ? packets : 0; }
API void fill_packets(Packet *p, int32_t count)
{
    for (int32_t i = 0; i < count; ++i) p[i].count += 10;
}
API int32_t identity(int32_t value) { return value; }

#include <stddef.h>
#pragma pack(push, 1)
typedef struct
{
    int8_t tag;
    double value;
    int32_t count;
} Packed;
typedef struct
{
    int8_t lead;
    Packet normal;
    Packed packed;
} PackedOuter;
#pragma pack(pop)
typedef struct
{
    int8_t lead;
    Packed packed;
    double tail;
} NormalOuter;
API int32_t packed_layout(void) { return sizeof(Packed) == 13 && offsetof(Packed, value) == 1 && offsetof(Packed, count) == 9 && offsetof(PackedOuter, normal) == 1 && offsetof(NormalOuter, tail) % 8 == 0; }
API Packed packed_transform(Packed p)
{
    p.tag += 1;
    p.value += 0.5;
    p.count += 2;
    return p;
}
API void packed_update(Packed *p)
{
    p->value += 10;
    p->count += 3;
}
static Packed packed_buffer[2] = { { -3, 1.25, 4 }, { 7, 2.5, 8 } };
API Packed *packed_snapshot(void) { return packed_buffer; }
API Packed *packed_numbers(int32_t count) { return count == 2 ? packed_buffer : 0; }
API void packed_fill(Packed *p, int32_t count)
{
    for (int32_t i = 0; i < count; ++i)
    {
        p[i].value += 1;
        p[i].count += i + 1;
    }
}
API PackedOuter packed_outer(PackedOuter p)
{
    p.normal.count += 5;
    p.packed.value += 2;
    return p;
}
API NormalOuter normal_outer(NormalOuter p)
{
    p.packed.count += 6;
    p.tail += 3;
    return p;
}
