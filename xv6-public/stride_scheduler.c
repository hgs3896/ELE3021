#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "schedulers.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct spinlock stride_lock;
StrideQueue stride_queue;
uint stride_ticks;

void
stride_item_init(StrideItem *item, uint share, void *proc, uint isMLFQ)
{
  item->share = share;
  item->pass.num = stride_queue.max_pass.num;
  item->pass.denom = stride_queue.max_pass.denom;
  item->proc = proc;
  item->isMLFQ = isMLFQ;
}

int
stride_item_copy(StrideItem *dest, const StrideItem *src)
{
  if(dest == 0 || src == 0)
    return -1;
  if(dest == src)
    return 0;
  dest->share = src->share;
  dest->isMLFQ = src->isMLFQ;
  dest->proc = src->proc;
  dest->pass.denom = src->pass.denom;
  dest->pass.num = src->pass.num;
  return 0;
}

#define HEAP_LESS_THAN(a, b) !frac_is_less_than((a), (b))

static void
heap_init(StrideHeap *heap)
{
  for(StrideItem *it = heap->items; it != heap->items + NELEM(heap->items);
      it++)
    stride_item_init(it, 0, 0, 0);
  heap->size = 0;
}

static int
heap_full(const StrideHeap *heap)
{
  return heap->size >= NELEM(heap->items) - 1;
}

static int
heap_empty(const StrideHeap *heap)
{
  return heap->size == 0;
}

static int
heap_push_item(StrideHeap *heap, const StrideItem *item)
{
  int idx;
  if(heap_full(heap)) {
    return -1; // The heap is full.
  }
  idx = ++heap->size;
  while((idx != 1) &&
        !HEAP_LESS_THAN(&item->pass, &heap->items[idx / 2].pass)) {
    stride_item_copy(&heap->items[idx], &heap->items[idx >> 1]);
    idx = idx >> 1;
  }
  stride_item_copy(&heap->items[idx], item);
  return 0;
}

static int
heap_pop_item(StrideHeap *heap)
{
  StrideItem *temp;
  uint parent = 1;
  uint child = 2;
  if(heap_empty(heap)) {
    return -1; // The heap is empty
  }
  temp = &heap->items[heap->size--];
  while(child <= heap->size) {
    /* compare left and right child’s key passs */
    if((child < heap->size) &&
       HEAP_LESS_THAN(&heap->items[child].pass, &heap->items[child + 1].pass))
      child++;
    if(!HEAP_LESS_THAN(&temp->pass, &heap->items[child].pass))
      break; /* move to the next lower level */
    stride_item_copy(&heap->items[parent], &heap->items[child]);
    parent = child;
    child = child << 1;
  }
  stride_item_copy(&heap->items[parent], temp);
  return 0;
}

static StrideItem *
heap_top_item(StrideHeap *heap)
{
  if(heap_empty(heap))
    return 0;
  return &heap->items[1];
}

void
stride_init(void)
{
  stride_queue.all_share = 0;
  frac_zero(&stride_queue.max_pass);
  heap_init(&stride_queue.q);
}

int
stride_push(const StrideItem *item)
{
  if(!item)
    return 0;
  stride_queue.all_share += item->share;
  return heap_push_item(&stride_queue.q, item);
}

int
stride_pop(void)
{
  StrideItem *item = stride_top();
  if(item == 0)
    return -1;
  stride_queue.all_share -= item->share;
  return heap_pop_item(&stride_queue.q);
}

StrideItem *
stride_top(void)
{
  return heap_top_item(&stride_queue.q);
}

int
stride_can_change_share(int old_share, int new_share)
{
  return stride_queue.all_share - old_share + new_share <= STRIDE_MAX_SHARE;
}

int
stride_adjust(StrideItem *item, int new_share)
{
  if(!item || heap_empty(&stride_queue.q))
    return -1;
  if(!stride_can_change_share(item->share, new_share))
    return -1;
  stride_queue.all_share -= item->share;
  item->share = new_share;
  stride_queue.all_share += item->share;
  return 0;
}

int
is_stride(struct proc *p)
{
  return p && p->lev == STRIDE_PROC_LEVEL;
}

int
stride_remove(StrideItem *s)
{
  if(!s)
    return -1;

  int idx = s - stride_queue.q.items;
  stride_adjust(&stride_queue.q.items[idx], 0);
  stride_item_copy(&stride_queue.q.items[idx],
                   &stride_queue.q.items[stride_queue.q.size--]);

  StrideItem t;
  StrideItem *child;
  for(int n = stride_queue.q.size / 2; n >= 1; n--) {
    child = &stride_queue.q.items[2 * n];
    if(2 * n + 1 <= stride_queue.q.size &&
       HEAP_LESS_THAN(&((child + 1)->pass), &child->pass)) {
      child = child + 1;
    }
    if(HEAP_LESS_THAN(&child->pass, &stride_queue.q.items[n].pass)) {
      stride_item_copy(&t, child);
      stride_item_copy(child, &stride_queue.q.items[n]);
      stride_item_copy(&stride_queue.q.items[n], &t);
    }
  }
  return 0;
}

// Called at trap
int
stride_has_to_yield(struct proc *p)
{
  static uint stride_ticks_elapsed;

  if(stride_ticks_elapsed >= STRIDE_MAX_TICKS)
    stride_ticks_elapsed = 0;

  acquire(&stride_lock);
  stride_ticks += 1; // Increase the Stride tick count
  release(&stride_lock);

  stride_ticks_elapsed++; // Increase the time quantom the current process can
                          // use

  acquire(&ptable.lock);
  p->cticks += 1; // Increase the process's tick count
  if(stride_ticks_elapsed >= STRIDE_MAX_TICKS)
    p->yield_by = 1;
  release(&ptable.lock);

  return stride_ticks_elapsed >= STRIDE_MAX_TICKS;
}

StrideItem *
stride_find_item(const struct proc *p)
{
  StrideItem *const begin = stride_queue.q.items, *const end =
                                                      begin +
                                                      stride_queue.q.size;
  for(StrideItem *it = begin; it != end; it++) {
    if(p == it->proc) {
      return it;
    }
  }
  return 0;
}

void
stride_print(void)
{
  static const char *state2str[] = {
      [UNUSED] "unused",   [EMBRYO] "embryo",  [SLEEPING] "sleep ",
      [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};

  StrideItem *s;
  int i;
  cprintf("Stride Info\n"
          "All Share = %d\n",
          stride_queue.all_share);

  for(i = 1; i <= stride_queue.q.size; i++) {
    s = &stride_queue.q.items[i];
    if(s->isMLFQ) {
      cprintf("[%d] MLFQ (%d)\n", i, s->share);
      mlfq_print();
    } else {
      cprintf("[%d] [%s] %s (%d)\n", i,
              state2str[((struct proc *)s->proc)->state],
              ((struct proc *)s->proc)->name, s->share);
    }
  }
}

void
stride_scheduler(struct cpu *c)
{
  StrideItem item, *top;
  frac stride;

  // Fetch a top element from the stride
  top = stride_top();
  if(stride_item_copy(&item, top)) {
    panic("Error occured during copying stride item");
  }

  if(item.isMLFQ) {
    // Do the MLFQ
    mlfq_scheduler(c);
  } else {
    struct proc *p = item.proc;

    if(p->state != RUNNABLE)
      goto stride_pass;

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), current_lwp(p)->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
  }

stride_pass:
  // Pop the item
  if(stride_pop())
    return;

  uint all_share = stride_queue.all_share + item.share;

  // Add the pass
  if(frac_init(&stride, all_share,
               item.share) || // stride = all_share / item.share
     frac_add(&item.pass, &item.pass, &stride)) { // item.pass += stride
    if(stride_push(&item))
      panic("Stride item cannot be pushed\n");
    return;
  }

  // Update the max pass
  if(frac_is_less_than(&stride_queue.max_pass, &item.pass)) {
    if(frac_init(&stride_queue.max_pass, item.pass.num, item.pass.denom))
      panic("cannot update the max pass\n");
  }

  // Put it back alive
  if(item.isMLFQ || ((struct proc *)item.proc)->state != ZOMBIE) {
    if(stride_push(&item))
      panic("Stride item cannot be pushed\n");
  }
}