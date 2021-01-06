#include "wolfidps_internal.h"

int wolfidps_lock_readonly(struct wolfidps_rwlock *lock) {
    int waited = 0;
  again:
    if (sem_wait(&lock->sem) < 0)
        return -1;
    if (waited)
        --lock->read_waiter_count;
    if (lock->state == WOLFIDPS_LOCK_EXCLUSIVE) {
        ++lock->read_waiter_count;
        if (sem_post(&lock->sem) < 0)
            return -1;
        if (sem_wait(&lock->sem_read_waiters) < 0)
            return -1;
        goto again;
    } else if (lock->state == WOLFIDPS_LOCK_UNLOCKED)
        lock->state = WOLFIDPS_LOCK_SHARED;
    ++lock->shared_count;
    if (sem_post(&lock->sem) < 0)
        return -1;
    return 0;
}

int wolfidps_lock_readwrite(struct wolfidps_rwlock *lock) {
    int waited = 0;
  again:
    if (sem_wait(&lock->sem) < 0)
        return -1;
    if (waited)
        --lock->write_waiter_count;
    if (lock->state != WOLFIDPS_LOCK_UNLOCKED) {
        ++lock->write_waiter_count;
        if (sem_post(&lock->sem) < 0)
            return -1;
        if (sem_wait(&lock->sem_write_waiters) < 0)
            return -1;
        goto again;
    }
    lock->state = WOLFIDPS_LOCK_EXCLUSIVE;
    if (sem_post(&lock->sem) < 0)
        return -1;
    return 0;
}

int wolfidps_lock_unlock(struct wolfidps_rwlock *lock) {
    if (sem_wait(&lock->sem) < 0)
        return -1;
    if (lock->state == WOLFIDPS_LOCK_SHARED) {
        if (--lock->shared_count)
            lock->state == WOLFIDPS_LOCK_UNLOCKED;
    } else if (lock->state == WOLFIDPS_LOCK_EXCLUSIVE)
        lock->state == WOLFIDPS_LOCK_UNLOCKED;
    else {
        (void)sem_post(&lock->sem);
        return -1;
    }
    if (lock->write_waiter_count > 0) {
        if (sem_post(&lock->sem_write_waiters) < 0)
            return -1;
    } else if (lock->read_waiter_count > 0) {
        int i;
        for (i = 0; i < lock->read_waiter_count; ++i) {
            if (sem_post(&lock->sem_read_waiters) < 0)
                return -1;
        }
    }
    if (sem_post(&lock->sem) < 0)
        return -1;
    return 0;
}

int wolfidps_lock_write2read(struct wolfidps_rwlock *lock) {
    if (sem_wait(&lock->sem) < 0)
        return -1;
    if (lock->state != WOLFIDPS_LOCK_EXCLUSIVE) {
        if (sem_post(&lock->sem) < 0)
            return -1;
        return -1;
    }
    lock->state == WOLFIDPS_LOCK_SHARED;
    lock->shared_count = 1;
    if ((lock->write_waiter_count == 0) &&
        (lock->read_waiter_count > 0)) {
        int i;
        for (i = 0; i < lock->read_waiter_count; ++i) {
            if (sem_post(&lock->sem_read_waiters) < 0)
                return -1;
        }
    }
    if (sem_post(&lock->sem) < 0)
        return -1;
    return 0;
}

int wolfidps_table_ent_insert(struct wolfidps_table_ent_generic *ent, struct wolfidps_table_generic *table) {
    struct wolfidps_table_ent_generic *i = table->generic.head;
    while (i) {
        if (table->generic.cmp_fn((struct wolfidps_ent_generic *)ent, (struct wolfidps_ent_generic *)i) > 0)
            break;
        i = i->generic.next;
    }
    if (i) {
        ent->generic.prev = i->generic.prev;
        ent->generic.next = i;
        if (i->generic.prev) {
            i->generic.prev->generic.next = ent;
        } else {
            table->generic.head = ent;
        }
        i->generic.prev = ent;
    } else if (table->generic.tail) {
        table->generic.tail->generic.next = i;
        ent->generic.prev = table->generic.tail;
        ent->generic.next = NULL;
        table->generic.tail = ent;
    } else {
        table->generic.head = table->generic.tail = ent;
        ent->generic.prev = ent->generic.next = NULL;
    }
}

int wolfidps_table_ent_get(struct wolfidps_table_generic *table, struct wolfidps_table_ent_generic **ent) {
    struct wolfidps_table_ent_generic *i = table->generic.head;
    while (i) {
        int c = table->generic.cmp_fn((struct wolfidps_ent_generic *)*ent, (struct wolfidps_ent_generic *)i);
        if (c >= 0) {
            if (c == 0) {
                *ent = i;
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

void wolfidps_table_ent_delete_1(struct wolfidps_table_generic *table, struct wolfidps_table_ent_generic *ent) {
    if (ent->generic.prev)
        ent->generic.prev->generic.next = ent->generic.next;
    else
        table->generic.head = ent->generic.next;
    if (ent->generic.next)
        ent->generic.next->generic.prev = ent->generic.prev;
    else
        table->generic.tail = ent->generic.prev;
    ent->generic.prev = ent->generic.next = NULL;
}

int wolfidps_table_ent_delete(struct wolfidps_table_generic *table, struct wolfidps_table_ent_generic **ent) {
    struct wolfidps_table_ent_generic *i = table->generic.head;
    while (i) {
        int c = table->generic.cmp_fn((struct wolfidps_ent_generic *)*ent, (struct wolfidps_ent_generic *)i);
        if (c >= 0) {
            if (c == 0) {
                *ent = i;
                wolfidps_table_ent_delete_1(table, i);
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

int wolfidps_table_cursor_init(struct wolfidps_context *wolfidps, struct wolfidps_cursor **cursor) {
    if (*cursor == NULL)
        *cursor = (struct wolfidps_cursor *)wolfidps->allocator.malloc(wolfidps->allocator.context, sizeof *cursor);
    if (*cursor == NULL)
        return MEMORY_E;
    memset(*cursor, 0, sizeof **cursor);
    return 0;
}

/* in a fashion analogous to the values returned by comparison
 * functions, *cursor_position is set to -1, 0, or 1, depending on
 * whether cursor is initialized to point to the ent immediately
 * before where the search ent would be, right on the search ent, or
 * immediately after where the search ent would be.
 */
int wolfidps_table_cursor_set(struct wolfidps_table_generic *table, struct wolfidps_table_ent_generic *ent, struct wolfidps_cursor *cursor, int *cursor_position) {
    struct wolfidps_table_ent_generic *i = table->generic.head;
    while (i) {
        int c = table->generic.cmp_fn((struct wolfidps_ent_generic *)ent, (struct wolfidps_ent_generic *)i);
        if (c >= 0) {
            cursor->point = i;
            if (c == 0)
                *cursor_position = 0;
            else
                *cursor_position = 1;
            return 0;
        }
    }
    cursor->point = table->generic.tail;
    *cursor_position = -1;
    return 0;
}

int wolfidps_table_cursor_current(struct wolfidps_cursor *cursor, struct wolfidps_table_ent_generic **ent) {
    *ent = cursor->point;
    return 0;
}

int wolfidps_table_cursor_prev(struct wolfidps_cursor *cursor, struct wolfidps_table_ent_generic **ent) {
    if (cursor->point == NULL)
        return -1;
    cursor->point = cursor->point->generic.prev;
    *ent = cursor->point;
    return 0;
}

int wolfidps_table_cursor_next(struct wolfidps_cursor *cursor, struct wolfidps_table_ent_generic **ent) {
    if (cursor->point == NULL)
        return -1;
    cursor->point = cursor->point->generic.next;
    *ent = cursor->point;
    return 0;
}
