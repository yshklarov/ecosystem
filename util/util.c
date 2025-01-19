#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>


/**** Typedefs ****/

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;


/**** Clamping ****/

#ifndef MIN
#define MIN(a,b) ((a < b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a < b) ? (b) : (a))
#endif

#define CLAMP(x, lower, upper) ((x < lower) ? (lower) : ((x > upper) ? (upper) : (x)))
u8 clamp_i64_u8(i64 x) {
    return (u8)CLAMP(x, 0, 0xFF);
}
u16 clamp_i32_u16(i32 x) {
    return (u16)CLAMP(x, 0, 0xFFFF);
}
u16 clamp_i64_u16(i64 x) {
    return (u16)CLAMP(x, 0, 0xFFFF);
}
u16 clamp_size_t_u16(size_t x) {
    return (u16)MIN(x, 0xFFFF);
}
u32 clamp_i32_u32(i32 x) {
    return (u32)MAX(0, x);
}
u32 clamp_i64_u32(i64 x) {
    return (u32)MAX(0, x);
}


/**** Random number generator ****/

// JSF (Jenkins Small Fast) random number generator
// https://burtleburtle.net/bob/rand/smallprng.html
typedef struct rand_state { u64 a; u64 b; u64 c; u64 d; } rand_state;
#define ROT32(x,k) (((x)<<(k))|((x)>>(32-(k))))
u64 rand_raw_s(rand_state* x) {
    u64 e = x->a - ROT32(x->b, 27);
    x->a = x->b ^ ROT32(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}
void rand_init_from_seed(rand_state* x, u64 seed) {
    u64 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i=0; i<20; ++i) {
        rand_raw_s(x);
    }
}
void rand_init_from_time(rand_state* x) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    u64 seed = (u64)tv.tv_usec;
    rand_init_from_seed(x, seed);
}
rand_state rand_state_global;

u64 rand_raw(void) {
    return rand_raw_s(&rand_state_global);
}

// Generate a random integer in the closed interval [min, max].
// Parameters:
//   min <= max.
u32 rand_unif(u32 min, u32 max) {
    if (min < max) {
        // For uniformity, it's necessary that maximum delta (2^32 - 1) be much smaller than the maximum value of
        // rand_raw() (2^64 - 1).
        u32 raw = (u32)(rand_raw() % (u64)(max - min + 1));
        return min + raw;
    } else {
        return min;
    }
}

u32 rand_bool() {
    return rand_raw() % 2;
}


// Randomly pick a combination uniformly from the (n choose k) possibilities. Store the result in combination.
// Implements Robert Floyd's algorithm.
// Parameters:
//    n, k: Will choose k random elements of combination to be true, and n - k to be false.
//    combination: Must point to contiguous array of n bools.
void rand_combination(u32 n, u32 k, bool combination[n]) {
    assert(n >= k);
    for (u32 i = 0; i < n; ++i) {
        combination[i] = false;
    }
    for (u32 j = n - k; j < n; ++j) {
        u32 r = rand_unif(0, j);
        if (combination[r]) {
            combination[j] = true;
        } else {
            combination[r] = true;
        }
    }
    /*
    // DEBUG
    std::cerr << "Selected random combination (" << n << ", " << k << "): ";
    for (int i {0}; i < n; ++i) {
        std::cerr << (combination[i] ? "1" : "0") << " ";
    }
    std::cerr << std::endl;
    */
}


/**** Buffer ****/

typedef struct buffer {
    // Invariants:
    //   0 <= len <= len_max
    //   If len_max != 0, then p != nullptr and p points to a valid object of length len_max bytes.
    //   If len_max == 0, then p == nullptr.
    size_t len;
    size_t len_max;
    char* p;
} buffer;


bool buffer_valid(buffer const buf) {
    if (buf.len > buf.len_max) {
        return false;
    }
    if ((buf.len_max == 0) != (buf.p == nullptr)) {
        return false;
    }
    return true;
}

// Print contents of buf to stream.
// Return: true on success; false on failure.
bool buffer_printf(buffer buf, FILE* stream) {
    for (size_t i = 0; i < buf.len; ++i) {
        if (putc(buf.p[i], stream) == EOF) {
            return false;
        }
    }
    return true;
}

/* Deallocate buf. After the call, buf will point to a valid (but empty, of length 0) buffer. */
void buffer_destroy(buffer* buf) {
    free(buf->p);
    buf->p = nullptr;
    buf->len = 0;
}

/* Allocate and return a new buffer whose content is a copy of the given string. If content == nullptr, or if content
   == "", return an empty buffer.  If content != nullptr, then content must point to a null-terminated (possibly
   empty) string.
*/
buffer buffer_create(char const* content) {
    if (!content || !content[0]) {
        return (buffer){0, 0, nullptr};
    }
    size_t len = strlen(content);
    buffer buf = { len, len, malloc(sizeof(char) * len) };
    char const* src = content;
    char* dst = buf.p;
    for (size_t i = 0; i < len; ++i) {
        *(dst++) = *(src++);
    }
    return buf;
}

/* Attempt to expand the buffer's maximum length. Do not modify contents. Return true on success. */
bool buffer_expand(buffer* buf) {
    size_t new_len = (size_t)(buf->len_max + buf->len_max / 2);
    if (new_len == 0) {
        // This is the first allocation for this buffer.
        new_len = 256;
    }
    char* new_p = (char*)realloc(buf->p, new_len);
    if (new_p == nullptr) {
        fprintf(stderr, "[ERROR] Failed to allocate memory: %s\n", strerror(errno));
        return false;
    }
    buf->p = new_p;
    buf->len_max = new_len;
    return true;
}

/* Reclaim unused/unneeded memory at end of buffer. Does not change buf's contents or length. */
void buffer_compress(buffer* buf) {
    char* new_p = (char*)realloc(buf->p, buf->len);
    if (new_p) {
        //fprintf(stderr, "[DEBUG] Compressed buffer from %zu to %zu.\n", buf->len_max, buf->len);
        buf->p = new_p;
        buf->len_max = buf->len;
    } else {
        fprintf(stderr, "[WARNING] Failed to compress buffer.\n");
    }
}

buffer buffer_create_from_file(char const* filename) {
    buffer buf = {0, 0, nullptr};
    FILE *f = fopen(filename, "r");
    if (nullptr == f) {
        fprintf(stderr, "[ERROR] Failed to open file %s: %s\n", filename, strerror(errno));
        return buf;
    }
    int c = {};
    while ((c = getc(f)) != EOF) {
        if (buf.len == buf.len_max) {
            if (!buffer_expand(&buf)) {
                buffer_destroy(&buf);
                return buf;
            }
        }
        buf.p[buf.len++] = (char)c;
    }
    fclose(f);
    buffer_compress(&buf);
    if (!buffer_valid(buf)) {
        fprintf(stderr, "[ERROR] Failed to create valid buffer from file %s.\n", filename);
        buffer_destroy(&buf);
    }
    //fprintf(stderr, "[DEBUG] Created buffer from file: Length %zu, maximum %zu. Content:\n", buf.len, buf.len_max);
    //buffer_printf(buf, stderr);  // DEBUG
    return buf;
}

buffer buffer_clone(buffer const* buf) {
    if (buf->len == 0) {
        return (buffer){0, 0, nullptr};
    }
    buffer buf2 = {
        .len = buf->len,
        .len_max = buf->len,
        .p = malloc(sizeof(char) * buf->len)
    };
    memcpy(buf2.p, buf->p, buf->len);
    return buf2;
}


// Check whether buf's contents equal the null-terminated string str.
bool buffer_eq(buffer const* buf, char const* str) {
    for (size_t i = 0; i < buf->len; ++i) {
        if (!str) {
            return false;
        }
        if (*str == buf->p[i]) {
            ++str;
        } else {
            return false;
        }
    }
    return true;
}


/**** JSON ****/


typedef enum json_type : u8 {
    JSON_TYPE_OBJECT,
    JSON_TYPE_ARRAY,
    JSON_TYPE_STRING,
    JSON_TYPE_INTEGER,
    JSON_TYPE_FLOATING,
    JSON_TYPE_BOOLEAN,
    JSON_TYPE_NULL,
    JSON_TYPES_COUNT
} json_type;

//char const* json_type_name[JSON_TYPES_COUNT] = {
//    "object", "array", "string", "integer", "floating", "boolean", "null" };
static const char json_type_name[JSON_TYPES_COUNT][10] = {
    [JSON_TYPE_OBJECT] = "object\0",
    [JSON_TYPE_ARRAY] = "array\0",
    [JSON_TYPE_STRING] = "string\0",
    [JSON_TYPE_INTEGER] = "integer\0",
    [JSON_TYPE_FLOATING] = "floating\0",
    [JSON_TYPE_BOOLEAN] = "boolean\0",
    [JSON_TYPE_NULL] = "null\0",
};

typedef union json_datum {
    buffer string;
    i64 integer;
    f64 floating;
    bool boolean;
} json_datum;

// JSON is stored as a tree with dynamically-allocated values.
// The root value is a JSON_TYPE_OBJECT with unspecified name.
typedef struct json_value {
    json_type type;
    json_datum datum;
    // A value is permitted to have an empty name (e.g., array members, and the root object);
    // in that case, it will have name == nullptr. If the name is nonempty, then
    // name->type == JSON_TYPE_STRING and name->name == name->next == name->child == nullptr.
    struct json_value* name;
    struct json_value* next;
    struct json_value* child;
} json_value;

typedef json_value* json_data;

void repeated_printf(char const* item, u32 repeat) {
    for (u32 i = 0; i < repeat; ++i) {
        printf("%s", item);
    }
}

void json_value_printf(json_value const* v, u8 indent_level) {
    if (!v) {
        return;
    }
    printf("%c| ", toupper(json_type_name[v->type][0]));
    repeated_printf("    ", indent_level);
    if (v->name) {
        printf("\"");
        buffer_printf(v->name->datum.string, stdout);
        printf("\": ");
    }
    switch (v->type) {
    case JSON_TYPE_OBJECT:
        printf("{\n");
        if (v->child) {
            json_value_printf(v->child, indent_level+1);
        }
        printf("%c| ", ' ');
        repeated_printf("    ", indent_level);
        printf("}");
        break;
    case JSON_TYPE_ARRAY:
        printf("[\n");
        if (v->child) {
            json_value_printf(v->child, indent_level+1);
        }
        printf("%c| ", ' ');
        repeated_printf("    ", indent_level);
        printf("]");
        break;
    case JSON_TYPE_STRING:
        printf("\"");
        buffer_printf(v->datum.string, stdout);
        printf("\"");
        break;
    case JSON_TYPE_INTEGER:
        printf("%ld", v->datum.integer);
        break;
    case JSON_TYPE_FLOATING:
        printf("%.15lg", v->datum.floating);
        break;
    case JSON_TYPE_BOOLEAN:
        printf(v->datum.boolean ? "true" : "false");
        break;
    case JSON_TYPE_NULL:
        printf("null");
        break;
    default:
        fprintf(stderr, "[ERROR] Cannot print: Unhandled type.\n");
        exit(1);
        break;
    }
    if (v->next) {
        printf(",\n");
        json_value_printf(v->next, indent_level);
    } else {
        printf("\n");
    }
}

// Print JSON data to standard output.
void json_data_printf(json_data* data) {
    if (data) {
        json_value_printf(*data, 0);
    }
}

json_value* json_value_create(void) {
    return (json_value*)calloc(1, sizeof(json_value));
}

void json_value_append_child(json_value* value, json_value* child) {
    if (!value->child) {
        value->child = child;
    } else {
        json_value* v = value->child;
        while (v->next) {
            v = v->next;
        }
        v->next = child;
    }
}

// Free (and invalidate) value, including all siblings and children.
void json_value_destroy(json_value** value) {
    if (value == nullptr || *value == nullptr) {
        return;
    }
    if ((*value)->type == JSON_TYPE_STRING) {
        buffer_destroy(&((*value)->datum.string));
    }
    if ((*value)->name) {
        json_value_destroy(&(*value)->name);
        (*value)->name = nullptr;
    }
    if ((*value)->child) {
        json_value_destroy(&(*value)->child);
        (*value)->child = nullptr;
    }
    if ((*value)->next) {
        json_value_destroy(&(*value)->next);
        (*value)->next = nullptr;
    }
    free(*value);
    *value = nullptr;
}

void json_value_destroy_all_children(json_value* value) {
    // Subsequent children are siblings of first child, so it suffices to destroy the first child.
    if (value) {
        json_value_destroy(&(value->child));
    }
}

void json_data_destroy(json_data* data) {
    json_value_destroy(data);
}

size_t json_count_children(json_value const* jv) {
    json_value *child = jv->child;
    size_t result = 0;
    while (child) {
        ++result;
        child = child->next;
    }
    return result;
}

json_value* json_find_child(json_value const* jv, char const* name) {
    if (!jv) {
        return nullptr;
    }
    json_value *child = jv->child;
    while (child) {
        if (buffer_eq(&child->name->datum.string, name)) {
            return child;
        } else {
            child = child->next;
        }
    }
    return nullptr;
}

json_value* json_find_child_of_type(json_value const* jv, char const* name, json_type type) {
    if (!jv) {
        return nullptr;
    }
    json_value *child = jv->child;
    while (child) {
        if (buffer_eq(&child->name->datum.string, name) && child->type == type) {
            return child;
        } else {
            child = child->next;
        }
    }
    return nullptr;
}


// Consume a single character ch, if it is at offset. If the incorrect character is found, or if offset is the end of
// the buffer, return false. Otherwise, increment offset and return true.
bool json_parse_eat_char(
    buffer buf,
    size_t* offset,
    char ch
    ) {
    if (*offset >= buf.len) {
        fprintf(stderr,
                "[ERROR] Bad parse at offset %zu: Expected '%c' (0x%X); observed end of buffer.",
                *offset, ch, ch);
        return false;
    }
    if (buf.p[*offset] != ch) {
        fprintf(stderr,
                "[ERROR] Bad parse at offset %zu: Expected '%c' (0x%X), observed '%c' (0x%X).\n",
                *offset, ch, ch, buf.p[*offset], buf.p[*offset]);
        return false;
    }
    ++(*offset);
    return true;
}

// Attempt to consume the entire string (excluding null terminator). If the string is consumed, increment offset
// accordingly. Otherwise, do not modify offset.
// Parameters:
//   string: null-terminated string.
// Return:
//   true: The string was consumed.
//   false: The string was not consumed.
bool json_parse_eat_string(
    buffer buf,
    size_t* offset,
    char const* string
    ){
    size_t offset_new = *offset;
    char const* ch = string;
    while (*ch) {
        //fprintf(stderr, "[DEBUG] %zu ?= %s\n", offset_new, ch);
        if (!json_parse_eat_char(buf, &offset_new, *ch)) {
            fprintf(stderr, "[ERROR] Bad parse: Expected string \"%s\" at offset %zu.\n", string, *offset);
            return false;
        }
        ++ch;
    }
    *offset = offset_new;
    return true;
}

// Eat all leading whitespace at offset. Will never eat past end-of-buffer, but it's possible that
// *new_offset == buf.len after the call (if only whitespace is found).
void json_parse_eat_whitespaces(
    buffer buf,
    size_t* offset
    ) {
    while (*offset < buf.len && isspace(buf.p[*offset])) {
        ++(*offset);
    }
}

bool json_parse_value(
    buffer buf,
    size_t* offset,
    json_value* value,
    bool expect_name);

bool json_parse_value_null(
    buffer buf,
    size_t* offset,
    json_value* value
    ) {
    json_parse_eat_whitespaces(buf, offset);
    if (!json_parse_eat_string(buf, offset, "null")) {
        fprintf(stderr, "[ERROR] Bad parse: Invalid 'null' at offset %zu.\n", *offset);
        return false;
    }
    value->type = JSON_TYPE_NULL;
    return true;
}

bool json_parse_value_boolean(
    buffer buf,
    size_t* offset,
    json_value* value
    ) {
    json_parse_eat_whitespaces(buf, offset);
    if (*offset >= buf.len) {
        fprintf(stderr, "[ERROR] Bad parse: Buffer overrun.\n");
        return false;
    }
    bool v = false;
    char initial = buf.p[*offset];
    switch (initial) {
    case 't':
        v = true;
        break;
    case 'f':
        v = false;
        break;
    default:
        fprintf(stderr, "[ERROR] Bad boolean value: begins with '%c' (0x%X).\n", initial, initial);
        break;
    }
    char const* const v_str = v ? "true" : "false";
    if (!json_parse_eat_string(buf, offset, v_str)) {
        fprintf(stderr, "[ERROR] Bad parse: Invalid boolean at offset %zu.\n", *offset);
        return false;
    }
    value->type = JSON_TYPE_BOOLEAN;
    value->datum.boolean = v;
    return true;
}

bool json_parse_value_string(
    buffer buf,
    size_t* offset,
    json_value* value
    ) {
    json_parse_eat_whitespaces(buf, offset);
    if (!json_parse_eat_char(buf, offset, '"')) {
        fprintf(stderr, "[ERROR] Bad parse: Invalid string at offset %zu.\n", *offset);
        return false;
    }

    size_t offset_new = *offset;
    char* ch = buf.p + offset_new;
    buffer str = buffer_create("");
    while (offset_new < buf.len && *ch != '"') {
        if (str.len == str.len_max) {
            if (!buffer_expand(&str)) {
                fprintf(stderr, "[ERROR] Bad parse: Failed to allocate memory at offset %zu.\n", *offset);
                buffer_destroy(&str);
                return false;
            }
        }
        str.p[str.len++] = *ch;
        ++ch;
        ++offset_new;
    }
    if (offset_new == buf.len) {
        fprintf(stderr, "[ERROR] Bad parse: Unterminated string at offset %zu.\n", *offset);
        return false;
    } else if (!json_parse_eat_char(buf, &offset_new, '"')) {
        fprintf(stderr, "[ERROR] Bad parse: Bad string terminator at offset %zu.\n", *offset);
        return false;
    }

    *offset = offset_new;
    value->datum.string = str;
    value->type = JSON_TYPE_STRING;
    return true;
}

bool json_parse_value_aggregate(
    buffer buf,
    size_t* offset,
    json_value* value,
    json_type type,
    char opener,
    char closer
    ) {
    json_parse_eat_whitespaces(buf, offset);
    if (!json_parse_eat_char(buf, offset, opener)) {
        fprintf(stderr,
                "[ERROR] Bad parse: Could not find correct aggregate opener '%c' at offzet %zu.\n",
                opener,
                *offset);
        return false;
    }
    size_t offset_new = *offset;
    bool got_first_child = false;
    bool got_comma = false;
    bool failure = false;
    while (true) {
        json_parse_eat_whitespaces(buf, &offset_new);
        if (offset_new >= buf.len) {
            fprintf(stderr, "[ERROR] Bad parse: Unexpected end-of-buffer.\n");
            failure = true;
            break;
        }
        char next_ch = buf.p[offset_new];
        if (next_ch == closer) {
            if (got_comma) {
                fprintf(stderr,
                        "[ERROR] Bad parse: Unexpected '%c' after comma at offset %zu.\n",
                        closer, offset_new);
                failure = true;
                break;
            }
            ++offset_new;  // Eat the closer.
            break;
        } else if (next_ch == ',') {
            if (got_first_child) {
                // All good; ready for the next child.
                got_comma = true;
                ++offset_new;  // Eat the comma.
                continue;
            } else {
                // It's too early for a comma -- we haven't seen any children yet.
                fprintf(stderr, "[ERROR] Bad parse: Unexpected comma at offset %zu.\n", offset_new);
                failure = true;
                break;
            }
        } else {
            if(got_first_child && !got_comma) {
                fprintf(stderr, "[ERROR] Bad parse: Missing comma at offset %zu.\n", offset_new);
                failure = true;
                break;
            }
            json_value *child = json_value_create();
            json_value_append_child(value, child);
            // Objects' ({...}) children are named; arrays' ([...]) children are not.
            bool expect_name = type == JSON_TYPE_OBJECT;
            if (!json_parse_value(buf, &offset_new, child, expect_name)) {
                fprintf(stderr, "[ERROR] Bad parse: Could not parse value at offset %zu.\n", offset_new);
                failure = true;
                break;
            }
            got_first_child = true;
            got_comma = false;
        }
    }
    if (failure) {
        json_value_destroy_all_children(value);
        return false;
    } else {
        *offset = offset_new;
        value->type = type;
        return true;
    }
}

bool json_parse_value_object(
    buffer buf,
    size_t* offset,
    json_value* value
    ) {
    return json_parse_value_aggregate(buf, offset, value, JSON_TYPE_OBJECT, '{', '}');
}

bool json_parse_value_array(
    buffer buf,
    size_t* offset,
    json_value* value
    ) {
    return json_parse_value_aggregate(buf, offset, value, JSON_TYPE_ARRAY, '[', ']');
}

bool json_parse_value_number(
    buffer buf,
    size_t* offset,
    json_value* value
    ) {
    json_parse_eat_whitespaces(buf, offset);
    size_t offset_new = *offset;
    bool negate = false;
    bool got_digit = false;
    bool is_floating = false;
    bool got_digit_after_point = false;
    i64 integer = 0;
    u8 fractional_digits = 0;

    // Parse the number.
    while (offset_new < buf.len) {
        char ch = buf.p[offset_new];
        if (!got_digit && !negate && '-' == ch) {
            negate = true;
        } else if (got_digit && '.' == ch) {
            if (is_floating == true) {
                fprintf(stderr, "[ERROR] Bad parse: Too many decimal points at offset %zu.\n", offset_new);
                return false;
            }
            is_floating = true;
        } else if ('0' <= ch && ch <= '9') {
            // Non-conformant to spec: Sequences starting with '0' but not with '0.' will be accepted as integers.
            // Sloppy: A handful of almost-maximal integers can fit in an i64, but will be rejected.
            if (integer > (INT64_MAX - (i64)9) / (i64)10) {
                if (!is_floating) {
                    fprintf(stderr, "[ERROR] Bad parse: Integer overflow at offset %zu.\n", offset_new);
                    return false;
                } else {
                    fprintf(stderr, "[WARNING] Ignoring high-precision digits at offset %zu.\n", offset_new);
                }
            } else {
                integer = 10*integer + (ch - '0');
                got_digit = true;
                if (is_floating) {
                    ++fractional_digits;
                    got_digit_after_point = true;
                }
            }
        } else {
            // UNIMPLEMENTED: Exponential notation for JSON numbers.
            // This ch is not part of this number token (e.g., ch is whitespace, a comma, or a closing brace).
            break;
        }
        ++offset_new;
    }
    if (is_floating && !got_digit_after_point) {
        fprintf(stderr, "[ERROR] Bad parse: Decimal point without subsequent digit at offset %zu.\n", offset_new);
        return false;
    }

    // Construct the number.
    if (negate) {
        integer *= -1;
    }
    if (is_floating) {
        f64 floating = (f64)integer;
        for (int i = 0; i < fractional_digits; ++i) {
            floating *= (f64)0.1;
        }
        value->datum.floating = floating;
        value->type = JSON_TYPE_FLOATING;
    } else {
        value->datum.integer = integer;
        value->type = JSON_TYPE_INTEGER;
    }

    *offset = offset_new;
    return true;
}

/* Parse buf at offset, reading a single JSON value into value. Write into new_offset the location (within buf) one past
 * the end of the parsed value.
 *
 * The parsed text will not include any whitespace or possible comma just past the end of the value (so that
 * buf.p[new_offset] may be whitespace, or ',' if there is a subsequent sibling value).
 *
 * If this call is successful, then the caller is responsible for eventually calling json_value_destroy(value) to
 * release resources.
 *
 * If this call is successful, offset will be updated to point to after the value. If unsuccessful, offset will not be
 * unmodified.
 *
 * Return: true on success, false if there was an error.
 */
bool json_parse_value(
    buffer buf,
    size_t* offset,
    json_value* value,
    bool expect_name
    ) {
    size_t offset_new = *offset;

    json_parse_eat_whitespaces(buf, &offset_new);
    if (offset_new >= buf.len) {
        fprintf(stderr, "[ERROR] Bad parse: Unexpected end-of-buffer.\n");
        return false;
    }

    if (expect_name) {
        // Parse the name (a string).
        if (buf.p[offset_new] != '"') {
            fprintf(stderr, "[ERROR] Bad parse: Expected value name at offset %zu, but did not find a string.\n", offset_new);
            return false;
        }
        value->name = json_value_create();
        if(!json_parse_value_string(buf, &offset_new, value->name)) {
            return false;
        }
        json_parse_eat_whitespaces(buf, &offset_new);
        if (!json_parse_eat_char(buf, &offset_new, ':')) {
            fprintf(stderr, "[ERROR] Bad parse: Failed to find ':' after offset %zu.\n", offset_new);
            return false;
        }
        json_parse_eat_whitespaces(buf, &offset_new);
    }

    if (offset_new >= buf.len) {
        fprintf(stderr, "[ERROR] Bad parse: Unexpected end-of-buffer.\n");
        return false;
    }

    // Parse the JSON value.
    char initial = buf.p[offset_new];
    bool good_parse = true;
    if ('{' == initial) {
        good_parse = json_parse_value_object(buf, &offset_new, value);
    } else if ('[' == initial) {
        good_parse = json_parse_value_array(buf, &offset_new, value);
    } else if ('"' == initial) {
        good_parse = json_parse_value_string(buf, &offset_new, value);
    } else if ('n' == initial) {
        good_parse = json_parse_value_null(buf, &offset_new, value);
    } else if ('t' == initial || 'f' == initial) {
        good_parse = json_parse_value_boolean(buf, &offset_new, value);
    } else if ('-' == initial || ('0' <= initial && initial <= '9')) {
        good_parse = json_parse_value_number(buf, &offset_new, value);
    } else {
        fprintf(stderr,
                "[ERROR] Bad parse: Unexpected character '%c' (0x%X) at buffer offset %zu. Expected a JSON value, instead.\n",
                initial, initial, offset_new);
        good_parse = false;
    }
    if (!good_parse) {
        return false;
    }

    *offset = offset_new;
    return true;
}

bool json_parse_nothing_until_end(
    buffer buf,
    size_t* offset
    ) {
    json_parse_eat_whitespaces(buf, offset);

    if (*offset != buf.len) {
        fprintf(stderr,
                (*offset < buf.len
                 ? "[ERROR] Bad parse: Unexpected character '%c' (0x%X) found at offset %zu.\n"
                 : "[ERROR] Bad parse: Buffer overrun.\n"),
                buf.p[*offset], buf.p[*offset], *offset);
        return false;
    }
    return true;
}

/* Parse JSON string data from buf, storing it in data.
 * Parameters:
 *   buf: Stores textual data, to be parsed as JSON.
 *   data: Must be uninitialized and unallocated. Will be written to.
 * Return: true on success; false if there's an error.
 * Notes: If this call is successful, then the caller is responsible for calling json_data_destroy(data).
 */
bool json_read_from_buffer(buffer buf, json_data* data) {
    size_t offset = 0;
    *data = json_value_create();
    if (!json_parse_value(buf, &offset, *data, false)) {
        fprintf(stderr, "[ERROR] Bad parse at top level.\n");
        json_data_destroy(data);
        return false;
    }
    // Make sure there's nothing after the top-level JSON object.
    if (!json_parse_nothing_until_end(buf, &offset)) {
        fprintf(stderr, "[ERROR] Too many values at top level.\n");
        json_data_destroy(data);
        return false;
    }
    if ((**data).type != JSON_TYPE_OBJECT) {
        fprintf(stderr, "[ERROR] Did not find JSON object at top level.\n");
        json_data_destroy(data);
        return false;
    }
    return true;
}

// Read file into data.
// Return: true on success; false if there's an error.
bool json_read_from_file(char const filename[], json_data* data) {
    buffer buf = buffer_create_from_file(filename);
    if (buf.len == 0) {
        fprintf(stderr, "[ERROR] Failed to read JSON data from file %s.\n", filename);
        return false;
    }
    bool rtn = json_read_from_buffer(buf, data);
    buffer_destroy(&buf);
    return rtn;
}
