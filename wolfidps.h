#ifndef WOLFIDPS_H
#define WOLFIDPS_H

#include <inttypes.h>
#include <semaphore.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/error-crypt.h>

typedef uint16_t wolfidps_family_t;
typedef uint16_t wolfidps_proto_t;
typedef uint16_t wolfidps_port_t;
typedef uint16_t wolfidps_ent_id_t;
typedef uint64_t wolfidps_count_t;
typedef uint64_t wolfidps_time_t;

struct wolfidps_table_ent_header {
    struct wolfidps_table_ent_generic *prev, *next; /* these will be replaced by red-black table elements later. */
};

struct wolfidps_list_ent_header {
    struct wolfidps_list_ent_generic *prev, *next;
};

struct wolfidps_table_ent_generic;
struct wolfidps_list_ent_generic;
struct wolfidps_ent_generic;

typedef int (*wolfidps_ent_cmp_fn_t)(struct wolfidps_ent_generic *left, struct wolfidps_ent_generic *right);

struct wolfidps_table_header {
    struct wolfidps_table_ent_generic *head, *tail; /* these will be replaced by red-black table elements later. */
    wolfidps_ent_cmp_fn_t cmp_fn;
};

struct wolfidps_list_header {
    struct wolfidps_list_ent_generic *head, *tail;
    /* note no cmp_fn slot */
};

typedef enum wolfidps_disposition {
    WOLFIDPS_UNSPEC = 0,
    WOLFIDPS_ACCEPT,
    WOLFIDPS_REJECT,
    WOLFIDPS_DROP
} wolfidps_disposition_t;

struct wolfidps_route;
struct wolfidps_event;

typedef wolfidps_disposition_t *(wolfidps_action_callback_t)(void *handler_context, void *caller_context, const struct wolfidps_event *event, const struct wolfidps_route *route);

struct wolfidps_action {
    struct wolfidps_table_ent_header header;
    int refcount;
    wolfidps_ent_id_t id;

    wolfidps_count_t hitcount;
    void *handler_context;
    wolfidps_action_callback_t *handler;
    byte label_len;
    char label[];
};

struct wolfidps_action_table {
    struct wolfidps_table_header header;
};

struct wolfidps_route_table_ent {
    struct wolfidps_table_ent_header header;
    struct wolfidps_route *route;
    enum {
        WOLFIDPS_ROUTE_TABLE_SRC_ENT, /* this ent is keyed on wolfidps_route.src. */
        WOLFIDPS_ROUTE_TABLE_DST_ENT /* this ent is keyed on wolfidps_route.dst. */
    } ent_type;
};

typedef struct wolfidps_route_flags {
    union {
        uint32_t flags;
        struct {
            uint32_t src_if_id_wildcard : 1;
            uint32_t dst_if_id_wildcard : 1;
            uint32_t sa_family_wildcard : 1;
            uint32_t sa_src_addr_wildcard : 1;
            uint32_t sa_dst_addr_wildcard : 1;
            uint32_t sa_proto_wildcard : 1;
            uint32_t tcplike_port_numbers : 1;
            uint32_t sa_src_port_wildcard : 1;
            uint32_t sa_dst_port_wildcard : 1;
            uint32_t dont_count : 1;
        };
    };
} wolfidps_route_flags_t;

#define WOLFIDPS_ADDR_BITS_TO_BYTES(x) ((x+7)>>3)

typedef struct {
    u_char address[256/8];
} wolfidps_address_mask_t;

#define WOLFIDPS_TIME_NEVER ((wolfidps_time_t)0)

struct wolfidps_route_endpoint {
    wolfidps_port_t sa_port;
    u_char addr_len; /* in bits */
    u_char extra_port_count;
    u_char if_id;
};

struct wolfidps_event;

struct wolfidps_route {
    struct wolfidps_route_table_ent src_ent, dst_ent;
    wolfidps_ent_id_t id;

    struct wolfidps_event *parent_event;
    struct wolfidps_action *action;

    struct wolfidps_route_flags flags;

    wolfidps_family_t sa_family;
    wolfidps_proto_t sa_proto;
    struct wolfidps_route_endpoint src, dst;

    uint16_t buf_alloced;

    wolfidps_time_t last_transition_time;
    wolfidps_count_t n_hits;
    wolfidps_time_t ttl;
    u_char addr_buf[]; /* first the src addr in big endian padded up to nearest byte, then dst addr, then src_extra_ports, then dst_extra_ports. */
};

#define WOLFIDPS_BITS_TO_BYTES(x) (((x) + 7) >> 3)
#define WOLFIDPS_ROUTE_SRC_ADDR(r) r->addr_buf
#define WOLFIDPS_ROUTE_SRC_ADDR_BYTES(r) WOLFIDPS_BITS_TO_BYTES(r->src_addr_len)
#define WOLFIDPS_ROUTE_DST_ADDR(r) r->addr_buf + WOLFIDPS_ROUTE_SRC_ADDR_BYTES(r)
#define WOLFIDPS_ROUTE_DST_ADDR_BYTES(r) WOLFIDPS_BITS_TO_BYTES(r->dst_addr_len)
#define WOLFIDPS_ROUTE_SRC_PORT_COUNT(r) (1 + r->src_extra_port_count)
#define WOLFIDPS_ROUTE_DST_PORT_COUNT(r) (1 + r->dst_extra_port_count)
#define WOLFIDPS_ROUTE_SRC_EXTRA_PORTS(r) ((wolfidps_port_t *)(WOLFIDPS_ROUTE_DST_ADDR(r) + WOLFIDPS_ROUTE_DST_ADDR_BYTES(r) + ((WOLFIDPS_ROUTE_SRC_ADDR_BYTES(r) + WOLFIDPS_ROUTE_DST_ADDR_BYTES(r)) & 1)))
#define WOLFIDPS_ROUTE_DST_EXTRA_PORTS(r) (WOLFIDPS_ROUTE_SRC_EXTRA_PORTS(r) + WOLFIDPS_ROUTE_SRC_PORT_COUNT(r))
#define WOLFIDPS_ROUTE_BUF_SIZE(r) (WOLFIDPS_ROUTE_SRC_ADDR_BYTES(r) + WOLFIDPS_ROUTE_DST_ADDR_BYTES(r) + ((WOLFIDPS_ROUTE_SRC_ADDR_BYTES(r) + WOLFIDPS_ROUTE_DST_ADDR_BYTES(r)) & 1) + (WOLFIDPS_ROUTE_SRC_PORT_COUNT(r) * sizeof(wolfidps_port_t)) + (WOLFIDPS_ROUTE_DST_PORT_COUNT(r) * sizeof(wolfidps_port_t)))

#define WOLFIDPS_ROUTE_SRC_PORT_GET(r, i) (i ? WOLFIDPS_ROUTE_SRC_EXTRA_PORTS(r)[i-1] : r->sa_src_port)
#define WOLFIDPS_ROUTE_DST_PORT_GET(r, i) (i ? WOLFIDPS_ROUTE_DST_EXTRA_PORTS(r)[i-1] : r->sa_dst_port)

struct wolfidps_route_table {
    struct wolfidps_table_header header;
};

struct wolfidps_action_list_ent {
    struct wolfidps_list_ent_header header;
    struct wolfidps_action *action;
};

struct wolfidps_action_list {
    struct wolfidps_list_header header;
};

struct wolfidps_event {
    struct wolfidps_table_ent_header header;
    int refcount;
    wolfidps_ent_id_t id;

    struct wolfidps_action_list action_list;

    wolfidps_count_t hitcount;
    byte keyword_len;
    char keyword[];
};

struct wolfidps_event_table {
    struct wolfidps_table_header header;
};

struct wolfidps_table_ent_generic {
    union {
        struct wolfidps_table_ent_header generic;
        struct wolfidps_event event;
        struct wolfidps_action action;
        struct wolfidps_route_table_ent route;
    };
};

struct wolfidps_table_generic {
    union {
        struct wolfidps_table_header generic;
        struct wolfidps_event_table events;
        struct wolfidps_action_table actions;
        struct wolfidps_route_table routes;
    };
};

struct wolfidps_sockaddr {
    wolfidps_family_t sa_family;
    wolfidps_proto_t sa_proto;
    wolfidps_port_t sa_port;
    u_char if_id;
    u_char addr_len; /* in bits */
    u_char addr[];
};

struct wolfidps_rwlock {
    sem_t sem;
    sem_t sem_read_waiters;
    sem_t sem_write_waiters;
    volatile int shared_count;
    volatile int read_waiter_count;
    volatile int write_waiter_count;
    volatile enum {
        WOLFIDPS_LOCK_UNLOCKED = 0,
        WOLFIDPS_LOCK_SHARED,
        WOLFIDPS_LOCK_EXCLUSIVE
    } state;
};

typedef int64_t woldidps_time_t;

typedef int (*wolfidps_get_time_cb_t)(void *context, woldidps_time_t *ts);
typedef woldidps_time_t (*wolfidps_diff_time_cb_t)(woldidps_time_t earlier, woldidps_time_t later);
typedef woldidps_time_t (*wolfidps_add_time_cb_t)(woldidps_time_t start_time, woldidps_time_t time_interval);
typedef int (*wolfidps_epoch_time_cb_t)(woldidps_time_t when, long *epoch_secs, long *epoch_nsecs);

typedef void *(*wolfidps_malloc_cb_t)(void *context, size_t size);
typedef void (*wolfidps_free_cb_t)(void *context, void *ptr);
typedef void *(*wolfidps_realloc_cb_t)(void *context, void *ptr, size_t size);
typedef void *(*wolfidps_memalign_cb_t)(void *context, size_t alignment, size_t size);

struct wolfidps_allocator {
    void *context;
    wolfidps_malloc_cb_t malloc;
    wolfidps_free_cb_t free;
    wolfidps_realloc_cb_t realloc;
    wolfidps_memalign_cb_t memalign;
};

struct wolfidps_timecbs {
    void *context;
    wolfidps_get_time_cb_t get_time;
    wolfidps_diff_time_cb_t diff_time;
    wolfidps_add_time_cb_t add_time;
    wolfidps_epoch_time_cb_t epoch_time;
};

struct wolfidps_context {
    struct wolfidps_rwlock lock;
    struct wolfidps_allocator allocator;
    struct wolfidps_timecbs timecbs;
    struct wolfidps_event_table events;
    struct wolfidps_action_table actions;
    struct wolfidps_route_table routes;
};

int wolfidps_init(struct wolfidps_allocator *allocator, struct wolfidps_context **wolfidps);
int wolfidps_shutdown(struct wolfidps_context **wolfidps);

int wolfidps_set_callback_get_time(struct wolfidps_context *wolfidps, wolfidps_get_time_cb_t handler, void *context);
int wolfidps_set_callback_diff_time(struct wolfidps_context *wolfidps, wolfidps_diff_time_cb_t handler);
int wolfidps_set_callback_add_time(struct wolfidps_context *wolfidps, wolfidps_add_time_cb_t handler);
int wolfidps_set_callback_epoch_time(struct wolfidps_context *wolfidps, wolfidps_epoch_time_cb_t handler);

int wolfidps_route_insert(
    struct wolfidps_context *wolfidps,
    struct wolfidps_sockaddr *src,
    struct wolfidps_sockaddr *dst,
    wolfidps_route_flags_t flags,
    int event_label_len,
    const char *event_label,
    wolfidps_time_t ttl
    );

int wolfidps_route_delete(
    struct wolfidps_context *wolfidps,
    struct wolfidps_sockaddr *src,
    struct wolfidps_sockaddr *dst,
    wolfidps_route_flags_t dst_flags,
    int event_label_len,
    const char *event_label
    );

int wolfidps_route_dispatch(
    struct wolfidps_context *wolfidps,
    struct wolfidps_sockaddr *src,
    struct wolfidps_sockaddr *dst,
    void *context, /* passed to action callback(s) as the caller_context. */
    wolfidps_disposition_t *disposition,
    wolfidps_time_t *ttl
    );


//    * manually get, set, increment, or decrement the current hit count for a family-address-keyword tuple.
//    * manually remove a family-address or family-address-keyword tuple from the table, optionally qualified by associated disposition
//    * manually insert an family-address tuple into the table, with a given status (disposition, time-to-live)


#endif /* WOLFIDPS_H */
