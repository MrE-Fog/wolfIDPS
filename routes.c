#include "wolfidps_internal.h"

int wolfidps_route_key_cmp(struct wolfidps_table_ent_generic *left, struct wolfidps_table_ent_generic *right) {
    struct wolfidps_route *left_route = ((struct wolfidps_route_table_ent *)left)->route;
    struct wolfidps_route *right_route = ((struct wolfidps_route_table_ent *)right)->route;
    struct wolfidps_route_endpoint *left_endpoint, *right_endpoint;
    u_char *left_addr, *right_addr;
    int left_addr_len, right_addr_len;
    int cmp;

    /* { family, address/block, proto, port } */

    if (left_route->sa_family != right_route->sa_family) {
        if (left_route->sa_family < right_route->sa_family)
            return -1;
        else
            return 1;
    }

    left_endpoint =
        (((struct wolfidps_route_table_ent *)left)->ent_type == WOLFIDPS_ROUTE_TABLE_SRC_ENT) ?
        &left_route->src :
        &left_route->dst;
    right_endpoint =
        (((struct wolfidps_route_table_ent *)right)->ent_type == WOLFIDPS_ROUTE_TABLE_SRC_ENT) ?
        &right_route->src :
        &right_route->dst;
    left_addr =
        (((struct wolfidps_route_table_ent *)left)->ent_type == WOLFIDPS_ROUTE_TABLE_SRC_ENT) ?
        &left_route->addr_buf[0] :
        &left_route->addr_buf[WOLFIDPS_ADDR_BITS_TO_BYTES(left_route->src.addr_len)];
    left_addr_len = WOLFIDPS_ADDR_BITS_TO_BYTES(left_endpoint->addr_len);
    right_addr =
        (((struct wolfidps_route_table_ent *)right)->ent_type == WOLFIDPS_ROUTE_TABLE_SRC_ENT) ?
        &right_route->addr_buf[0] :
        &right_route->addr_buf[WOLFIDPS_ADDR_BITS_TO_BYTES(right_route->src.addr_len)];
    right_addr_len = WOLFIDPS_ADDR_BITS_TO_BYTES(right_endpoint->addr_len);

    if (left_addr_len != right_addr_len) {
        int min_addr_len = (left_addr_len < right_addr_len) ? left_addr_len : right_addr_len;
        if ((cmp = memcmp(left_addr, right_addr, min_addr_len)))
            return cmp;
        else if (left_addr_len < right_addr_len)
            return -1;
        else
            return 1;
    }

    if ((cmp = memcmp(left_addr, right_addr, left_addr_len)))
        return cmp;

    if (left_route->sa_proto != right_route->sa_proto) {
        if (left_route->sa_proto < right_route->sa_proto)
            return -1;
        else
            return 1;
    }

    if (left_endpoint->sa_port != right_endpoint->sa_port) {
        if (left_endpoint->sa_port < right_endpoint->sa_port)
            return -1;
        else
            return 1;
    }

    return 0;
}

int wolfidps_route_insert_1(
    struct wolfidps_context *wolfidps,
    struct wolfidps_sockaddr *src,
    struct wolfidps_sockaddr *dst,
    wolfidps_route_flags_t flags,
    struct wolfidps_event *parent_event,
    woldidps_time_t ttl
    ) {
    size_t new_size;
    struct wolfidps_route *new;
    int ret;

    if ((src->sa_family != dst->sa_family) ||
        (src->sa_proto != dst->sa_proto))
        return -1;

    if ((flags.src_if_id_wildcard && (src->if_id != 0)) ||
        (flags.dst_if_id_wildcard && (dst->if_id != 0)) ||
        (flags.sa_family_wildcard && (src->sa_family != 0)) ||
        (flags.sa_src_addr_wildcard && (src->addr_len != 0)) ||
        (flags.sa_dst_addr_wildcard && (dst->addr_len != 0)) ||
        (flags.sa_src_port_wildcard && (src->sa_port != 0)) ||
        (flags.sa_dst_port_wildcard && (dst->sa_port != 0)))
        return -1;

    new_size = sizeof *new + WOLFIDPS_BITS_TO_BYTES(src->addr_len) + WOLFIDPS_BITS_TO_BYTES(dst->addr_len);
    if (new_size >= (size_t)(uint16_t)~0UL)
        return -1;

    new = wolfidps->allocator.malloc(wolfidps->allocator.context, new_size);
    memset(new, 0, new_size);
    new->buf_alloced = (uint16_t)new_size;

    if (wolfidps->timecbs.get_time(wolfidps->timecbs.context, &new->last_transition_time) < 0) {
        wolfidps->allocator.free(wolfidps->allocator.context, new);
        return -1;
    }
    new->ttl = ttl;
    new->parent_event = parent_event;
    new->flags = flags;
    new->sa_family = src->sa_family;
    new->sa_proto = src->sa_proto;
    new->src.sa_port = src->sa_port;
    new->src.addr_len = src->addr_len;
    new->src.if_id = src->if_id;
    new->dst.sa_port = dst->sa_port;
    new->dst.addr_len = dst->addr_len;
    new->dst.if_id = dst->if_id;

    new->src_ent.route = new->dst_ent.route = new;
    new->src_ent.ent_type = WOLFIDPS_ROUTE_TABLE_SRC_ENT;
    new->dst_ent.ent_type = WOLFIDPS_ROUTE_TABLE_DST_ENT;

    ret = wolfidps_table_ent_insert((struct wolfidps_table_ent_generic *)&new->src_ent, (struct wolfidps_table_generic *)&wolfidps->routes);
    if (ret < 0) {
        wolfidps->allocator.free(wolfidps->allocator.context, new);
        return ret;
    }
    ret = wolfidps_table_ent_insert((struct wolfidps_table_ent_generic *)&new->dst_ent, (struct wolfidps_table_generic *)&wolfidps->routes);
    if (ret < 0) {
        wolfidps_table_ent_delete_1((struct wolfidps_table_generic *)&wolfidps->routes, (struct wolfidps_table_ent_generic *)&new->src_ent);
        wolfidps->allocator.free(wolfidps->allocator.context, new);
        return ret;
    }

    return 0;
}

int wolfidps_route_insert(
    struct wolfidps_context *wolfidps,
    struct wolfidps_sockaddr *src,
    struct wolfidps_sockaddr *dst,
    wolfidps_route_flags_t flags,
    int event_label_len,
    const char *event_label,
    woldidps_time_t ttl
    ) {
    int ret;
    struct wolfidps_event *event = NULL;
    if (event_label) {
        if ((ret = wolfidps_event_getreference(wolfidps, event_label_len, event_label, &event)) < 0)
            return ret;
    }
    ret = wolfidps_route_insert_1(wolfidps, src, dst, flags, event, ttl);
    if (ret < 0)
        wolfidps_event_dropreference(wolfidps, event);
    return ret;
}

int wolfidps_route_lookup(
    struct wolfidps_context *wolfidps,
    struct wolfidps_sockaddr *src,
    struct wolfidps_sockaddr *dst,
    wolfidps_route_flags_t dst_flags,
    int event_label_len,
    const char *event_label,
    struct wolfidps_route **route) {

}

int wolfidps_route_delete_1(
    struct wolfidps_context *wolfidps,
    struct wolfidps_route *route) {

}

int wolfidps_route_delete(
    struct wolfidps_context *wolfidps,
    struct wolfidps_sockaddr *src,
    struct wolfidps_sockaddr *dst,
    wolfidps_route_flags_t dst_flags,
    int event_label_len,
    const char *event_label
    )
{
    struct wolfidps_route *route;
    int ret;
    if ((ret = wolfidps_route_lookup(wolfidps, src, dst, dst_flags, event_label_len, event_label)) < 0)
        return ret;
    return wolfidps_route_delete_1(wolfidps, route);
}

int wolfidps_route_dispatch(
    struct wolfidps_context *wolfidps,
    struct wolfidps_sockaddr *src,
    struct wolfidps_sockaddr *dst,
    wolfidps_disposition_t *disposition,
    wolfidps_time_t *ttl
    );
