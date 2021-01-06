#ifndef WOLFIDPS_INTERNAL_H
#define WOLFIDPS_INTERNAL_H

#include "wolfidps.h"

int wolfidps_lock_readonly(struct wolfidps_rwlock *lock);
int wolfidps_lock_readwrite(struct wolfidps_rwlock *lock);
int wolfidps_lock_unlock(struct wolfidps_rwlock *lock);
int wolfidps_lock_write2read(struct wolfidps_rwlock *lock);

int wolfidps_rule_key_cmp(struct wolfidps_table_ent_generic *left, struct wolfidps_table_ent_generic *right);
int wolfidps_action_key_cmp(struct wolfidps_table_ent_generic *left, struct wolfidps_table_ent_generic *right);
int wolfidps_route_key_cmp(struct wolfidps_table_ent_generic *left, struct wolfidps_table_ent_generic *right);

int wolfidps_table_ent_insert(struct wolfidps_table_ent_generic *ent, struct wolfidps_table_generic *table);
int wolfidps_table_ent_get(struct wolfidps_table_generic *table, struct wolfidps_table_ent_generic **ent);
int wolfidps_table_ent_delete(struct wolfidps_table_generic *table, struct wolfidps_table_ent_generic **ent);
void wolfidps_table_ent_delete_1(struct wolfidps_table_generic *table, struct wolfidps_table_ent_generic *ent);

struct wolfidps_cursor {
    struct wolfidps_table_ent_generic *point;
};

int wolfidps_table_cursor_init(struct wolfidps_context *wolfidps, struct wolfidps_cursor **cursor);
int wolfidps_table_cursor_set(struct wolfidps_table_generic *table, struct wolfidps_table_ent_generic *ent, struct wolfidps_cursor *cursor, int *cursor_position);
int wolfidps_table_cursor_current(struct wolfidps_cursor *cursor, struct wolfidps_table_ent_generic **ent);
int wolfidps_table_cursor_prev(struct wolfidps_cursor *cursor, struct wolfidps_table_ent_generic **ent);
int wolfidps_table_cursor_next(struct wolfidps_cursor *cursor, struct wolfidps_table_ent_generic **ent);

#endif /* WOLFIDPS_INTERNAL_H */
