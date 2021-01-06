#include "wolfidps_internal.h"

#ifndef WOLFIDPS_NO_MALLOC_BUILTIN

static void *wolfidps_builtin_malloc(void *context, size_t size) {
    (void)context;
    return malloc(size);
}

static void wolfidps_builtin_free(void *context, void *ptr) {
    (void)context;
    free(ptr);
}

static void *wolfidps_builtin_realloc(void *context, void *ptr, size_t size) {
    (void)context;
    return realloc(ptr, size);
}

static void *wolfidps_builtin_memalign(void *context, size_t alignment, size_t size) {
    void *ret;
    (void)context;
    if (posix_memalign(&ret, alignment, size) < 0)
        return NULL;
    return ret;
}

static struct wolfidps_allocator default_allocator = {
#ifdef __GNUC__
    .context = NULL,
    .malloc = wolfidps_builtin_malloc,
    .free = wolfidps_builtin_free,
    .realloc = wolfidps_builtin_realloc,
    .memalign = wolfidps_builtin_memalign
#else
    NULL,
    wolfidps_builtin_malloc,
    wolfidps_builtin_free,
    wolfidps_builtin_realloc,
    wolfidps_builtin_memalign
#endif
};

#endif /* !WOLFIDPS_NO_MALLOC_BUILTIN */

#ifndef WOLFIDPS_NO_CLOCK_BUILTIN

static int wolfidps_builtin_get_time(void *context, woldidps_time_t *now) {
    struct timespec ts;
    (void)context;
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
        return -1;
    *now = ((woldidps_time_t)ts.tv_sec * (woldidps_time_t)1000000) + ((woldidps_time_t)ts.tv_nsec / (woldidps_time_t)1000);
    return 0;
}

static woldidps_time_t wolfidps_builtin_diff_time(woldidps_time_t earlier, woldidps_time_t later) {
    return later - earlier;
}

static woldidps_time_t wolfidps_builtin_add_time(woldidps_time_t start_time, woldidps_time_t time_interval) {
    return start_time + time_interval;
}

static int wolfidps_builtin_epoch_time(woldidps_time_t when, long *epoch_secs, long *epoch_nsecs) {
    *epoch_secs = when / (woldidps_time_t)1000000;
    *epoch_nsecs = (when % (woldidps_time_t)1000000) * (woldidps_time_t)1000;
    return 0;
}

#endif /* !WOLFIDPS_NO_CLOCK_BUILTIN */

int wolfidps_event_key_cmp(struct wolfidps_table_ent_generic *left, struct wolfidps_table_ent_generic *right) {
    return 0;
}

int wolfidps_action_key_cmp(struct wolfidps_table_ent_generic *left, struct wolfidps_table_ent_generic *right) {
    return 0;
}

int wolfidps_init(struct wolfidps_allocator *allocator, struct wolfidps_context **wolfidps) {
#ifndef WOLFIDPS_NO_MALLOC_BUILTIN
    if (allocator == NULL)
        allocator = &default_allocator;
#endif
    if (allocator == NULL)
        return BAD_FUNC_ARG;
    if ((*wolfidps = (struct wolfidps_context *)allocator->malloc(allocator->context, sizeof **wolfidps)) == NULL)
        return MEMORY_E;
    memset(*wolfidps, 0, sizeof **wolfidps);
    (*wolfidps)->allocator = *allocator;
#ifndef WOLFIDPS_NO_CLOCK_BUILTIN
    (*wolfidps)->timecbs.get_time = wolfidps_builtin_get_time;
    (*wolfidps)->timecbs.diff_time = wolfidps_builtin_diff_time;
    (*wolfidps)->timecbs.add_time = wolfidps_builtin_add_time;
    (*wolfidps)->timecbs.epoch_time = wolfidps_builtin_epoch_time;
#endif
    (*wolfidps)->events.header.cmp_fn = (wolfidps_ent_cmp_fn_t)wolfidps_event_key_cmp;
    (*wolfidps)->actions.header.cmp_fn = (wolfidps_ent_cmp_fn_t)wolfidps_action_key_cmp;
    (*wolfidps)->routes.header.cmp_fn = (wolfidps_ent_cmp_fn_t)wolfidps_route_key_cmp;
    return 0;
}

int wolfidps_shutdown(struct wolfidps_context **wolfidps) {
    wolfidps_free_cb_t free_cb = (*wolfidps)->allocator.free;
    free_cb((*wolfidps)->allocator.context, *wolfidps);
    *wolfidps = NULL;
    return 0;
}

int wolfidps_set_callback_get_time(struct wolfidps_context *wolfidps, wolfidps_get_time_cb_t handler, void *context) {
    wolfidps->timecbs.context = context;
    wolfidps->timecbs.get_time = handler;
    return 0;
}

int wolfidps_set_callback_diff_time(struct wolfidps_context *wolfidps, wolfidps_diff_time_cb_t handler) {
    wolfidps->timecbs.diff_time = handler;
    return 0;
}

int wolfidps_set_callback_add_time(struct wolfidps_context *wolfidps, wolfidps_add_time_cb_t handler) {
    wolfidps->timecbs.add_time = handler;
    return 0;
}

int wolfidps_set_callback_epoch_time(struct wolfidps_context *wolfidps, wolfidps_epoch_time_cb_t handler) {
    wolfidps->timecbs.epoch_time = handler;
    return 0;
}

#ifdef TEST

#include <stdlib.h>

int main(int argc, char **argv) {
    struct wolfidps_context *wolfidps;
    int ret = wolfidps_init(0 /* struct wolfidps_allocator *allocator */, &wolfidps);
    printf("wolfidps_init() returns %d\n",ret);
    ret = wolfidps_shutdown(&wolfidps);
    printf("wolfidps_shutdown() returns %d\n",ret);
    exit(0);
}

#endif
