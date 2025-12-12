// Timer implementation for IC canisters
// Implements timer scheduling and execution using IC global timer API

#include "ic_timer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ic0.h"
#include "idl/cdk_alloc.h"

// =============================================================================
// Internal Data Structures
// =============================================================================

// Timer task type
typedef enum {
    IC_TIMER_TASK_ONCE,     // One-time timer
    IC_TIMER_TASK_INTERVAL, // Periodic timer
} ic_timer_task_type_t;

// Timer task structure
typedef struct {
    ic_timer_id_t        id;
    ic_timer_task_type_t type;
    int64_t              scheduled_time; // Nanoseconds since epoch
    int64_t              interval_ns;    // For interval timers
    ic_timer_callback_t  callback;
    void                *user_data;
    uint64_t counter; // Insertion order counter for deterministic ordering
} ic_timer_task_t;

// Timer heap structure (min-heap for earliest timer first)
#define IC_TIMER_HEAP_INITIAL_CAPACITY 16

typedef struct {
    ic_timer_task_t **tasks;
    size_t            size;
    size_t            capacity;
} ic_timer_heap_t;

// Global state
static ic_timer_heap_t g_timer_heap = {NULL, 0, 0};
static ic_timer_id_t   g_next_timer_id = 1;
static uint64_t        g_timer_counter = 0;
static int64_t         g_most_recent_set = -1; // -1 means not set

// =============================================================================
// Heap Operations
// =============================================================================

static int compare_timers(const ic_timer_task_t *a, const ic_timer_task_t *b) {
    // Compare by scheduled time first
    if (a->scheduled_time < b->scheduled_time) {
        return -1;
    }
    if (a->scheduled_time > b->scheduled_time) {
        return 1;
    }
    // Then by insertion order (counter)
    if (a->counter < b->counter) {
        return -1;
    }
    if (a->counter > b->counter) {
        return 1;
    }
    return 0;
}

static void heap_swap(ic_timer_heap_t *heap, size_t i, size_t j) {
    ic_timer_task_t *tmp = heap->tasks[i];
    heap->tasks[i] = heap->tasks[j];
    heap->tasks[j] = tmp;
}

static void heap_up(ic_timer_heap_t *heap, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (compare_timers(heap->tasks[idx], heap->tasks[parent]) >= 0) {
            break;
        }
        heap_swap(heap, idx, parent);
        idx = parent;
    }
}

static void heap_down(ic_timer_heap_t *heap, size_t idx) {
    while (1) {
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;
        size_t smallest = idx;

        if (left < heap->size &&
            compare_timers(heap->tasks[left], heap->tasks[smallest]) < 0) {
            smallest = left;
        }
        if (right < heap->size &&
            compare_timers(heap->tasks[right], heap->tasks[smallest]) < 0) {
            smallest = right;
        }

        if (smallest == idx) {
            break;
        }

        heap_swap(heap, idx, smallest);
        idx = smallest;
    }
}

static int heap_reserve(ic_timer_heap_t *heap, size_t new_capacity) {
    if (new_capacity <= heap->capacity) {
        return 0; // Success
    }

    ic_timer_task_t **new_tasks = (ic_timer_task_t **)cdk_realloc(
        heap->tasks, new_capacity * sizeof(ic_timer_task_t *));
    if (new_tasks == NULL) {
        return -1; // Failure
    }

    heap->tasks = new_tasks;
    heap->capacity = new_capacity;
    return 0; // Success
}

static int heap_push(ic_timer_heap_t *heap, ic_timer_task_t *task) {
    if (heap_reserve(heap, heap->size + 1) != 0) {
        return -1; // Failure
    }

    heap->tasks[heap->size] = task;
    heap_up(heap, heap->size);
    heap->size++;
    return 0; // Success
}

static ic_timer_task_t *heap_peek(const ic_timer_heap_t *heap) {
    if (heap->size == 0) {
        return NULL;
    }
    return heap->tasks[0];
}

static ic_timer_task_t *heap_pop(ic_timer_heap_t *heap) {
    if (heap->size == 0) {
        return NULL;
    }

    ic_timer_task_t *top = heap->tasks[0];
    heap->size--;

    if (heap->size > 0) {
        heap->tasks[0] = heap->tasks[heap->size];
        heap_down(heap, 0);
    }

    return top;
}

static ic_timer_task_t *heap_remove_by_id(ic_timer_heap_t *heap,
                                          ic_timer_id_t    id) {
    // Find the task
    size_t idx = SIZE_MAX;
    for (size_t i = 0; i < heap->size; i++) {
        if (heap->tasks[i]->id == id) {
            idx = i;
            break;
        }
    }

    if (idx == SIZE_MAX) {
        return NULL;
    }

    ic_timer_task_t *task = heap->tasks[idx];
    heap->size--;

    if (idx < heap->size) {
        heap->tasks[idx] = heap->tasks[heap->size];
        heap_down(heap, idx);
        heap_up(heap, idx);
    }

    return task;
}

// =============================================================================
// Timer Management
// =============================================================================

static void update_ic0_timer(void) {
    ic_timer_task_t *soonest = heap_peek(&g_timer_heap);
    if (soonest == NULL) {
        // No timers, clear the global timer
        g_most_recent_set = -1;
        return;
    }

    int64_t soonest_time = soonest->scheduled_time;
    // Only update if the time is earlier than what we set before
    if (g_most_recent_set < 0 || soonest_time < g_most_recent_set) {
        ic0_global_timer_set(soonest_time);
        g_most_recent_set = soonest_time;
    }
}

static void update_ic0_timer_clean(void) {
    g_most_recent_set = -1;
    update_ic0_timer();
}

static int64_t get_scheduled_time(int64_t delay_ns) {
    int64_t now = ic0_time();
    // Check for overflow
    if (delay_ns > INT64_MAX - now) {
        return -1; // Overflow
    }
    return now + delay_ns;
}

ic_timer_id_t
ic_timer_set(int64_t delay_ns, ic_timer_callback_t callback, void *user_data) {
    if (callback == NULL || delay_ns < 0) {
        return IC_TIMER_ID_INVALID;
    }

    int64_t scheduled_time = get_scheduled_time(delay_ns);
    if (scheduled_time < 0) {
        return IC_TIMER_ID_INVALID;
    }

    ic_timer_task_t *task =
        (ic_timer_task_t *)cdk_malloc(sizeof(ic_timer_task_t));
    if (task == NULL) {
        return IC_TIMER_ID_INVALID;
    }

    task->id = g_next_timer_id++;
    task->type = IC_TIMER_TASK_ONCE;
    task->scheduled_time = scheduled_time;
    task->interval_ns = 0;
    task->callback = callback;
    task->user_data = user_data;
    task->counter = g_timer_counter++;

    if (heap_push(&g_timer_heap, task) != 0) {
        cdk_free(task);
        return IC_TIMER_ID_INVALID;
    }

    update_ic0_timer();
    return task->id;
}

ic_timer_id_t ic_timer_set_interval(int64_t             interval_ns,
                                    ic_timer_callback_t callback,
                                    void               *user_data) {
    if (callback == NULL || interval_ns <= 0) {
        return IC_TIMER_ID_INVALID;
    }

    int64_t scheduled_time = get_scheduled_time(interval_ns);
    if (scheduled_time < 0) {
        return IC_TIMER_ID_INVALID;
    }

    ic_timer_task_t *task =
        (ic_timer_task_t *)cdk_malloc(sizeof(ic_timer_task_t));
    if (task == NULL) {
        return IC_TIMER_ID_INVALID;
    }

    task->id = g_next_timer_id++;
    task->type = IC_TIMER_TASK_INTERVAL;
    task->scheduled_time = scheduled_time;
    task->interval_ns = interval_ns;
    task->callback = callback;
    task->user_data = user_data;
    task->counter = g_timer_counter++;

    if (heap_push(&g_timer_heap, task) != 0) {
        cdk_free(task);
        return IC_TIMER_ID_INVALID;
    }

    update_ic0_timer();
    return task->id;
}

ic_timer_result_t ic_timer_clear(ic_timer_id_t timer_id) {
    if (timer_id == IC_TIMER_ID_INVALID) {
        return IC_TIMER_ERR_INVALID_ARG;
    }

    ic_timer_task_t *task = heap_remove_by_id(&g_timer_heap, timer_id);
    if (task == NULL) {
        return IC_TIMER_ERR_NOT_FOUND;
    }

    cdk_free(task);
    update_ic0_timer_clean();
    return IC_TIMER_OK;
}

void ic_timer_process_expired(void) {
    int64_t now = ic0_time();

    // Process all expired timers
    while (1) {
        ic_timer_task_t *task = heap_peek(&g_timer_heap);
        if (task == NULL || task->scheduled_time > now) {
            break;
        }

        // Remove from heap
        task = heap_pop(&g_timer_heap);

        // Execute callback
        if (task->callback != NULL) {
            task->callback(task->user_data);
        }

        // Reschedule if it's an interval timer
        if (task->type == IC_TIMER_TASK_INTERVAL) {
            // Reschedule for next interval
            int64_t next_time = now + task->interval_ns;
            // Check for overflow
            if (next_time > now && next_time >= task->scheduled_time) {
                task->scheduled_time = next_time;
                task->counter = g_timer_counter++;
                heap_push(&g_timer_heap, task);
            } else {
                // Overflow or invalid, discard the timer
                cdk_free(task);
            }
        } else {
            // One-time timer, free it
            cdk_free(task);
        }
    }

    // Update the global timer for the next expiration
    update_ic0_timer_clean();
}
