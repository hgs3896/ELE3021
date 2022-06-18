#pragma once

struct ts_guard;
typedef struct ts_guard thread_safe_guard;

thread_safe_guard *thread_safe_guard_init(int fd);
int thread_safe_pread(thread_safe_guard *file_guard, void *addr, int n, int off);
int thread_safe_pwrite(thread_safe_guard *file_guard, void *addr, int n, int off);
void thread_safe_guard_destroy(thread_safe_guard *file_guard);