// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define MAX_LOCK_NAME_LENGTH 16 
static char lock_names[NCPU][MAX_LOCK_NAME_LENGTH];

void freerange(void *pa_start, void *pa_end);
void kfree_alloc(void *pa, int cpu_id);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void
kinit()
{
  int i;
  for (i = 0; i < NCPU; i++) {
    snprintf(lock_names[i], MAX_LOCK_NAME_LENGTH, "kmem_%d", i);  // 格式化锁名称
    initlock(&kmems[i].lock, lock_names[i]);  // 使用全局名称初始化锁
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  int cpu_id = 0;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    kfree_alloc(p, cpu_id);
    cpu_id = (cpu_id + 1) % NCPU;
  } 
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  push_off();
  int cpu_id = cpuid();
  r = (struct run*)pa;

  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);

  pop_off();
}


// used for kinit
void
kfree_alloc(void *pa, int cpu_id)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  memset(pa, 1, PGSIZE);
  
  r = (struct run*)pa;

  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int i;

  push_off();
  int cpu_id = cpuid();

  acquire(&kmems[cpu_id].lock);
  r = kmems[cpu_id].freelist;
  if(r)
    kmems[cpu_id].freelist = r->next;
  release(&kmems[cpu_id].lock);

  if(!r)
  {
    for(i = 0; i < NCPU; i++) {
      if (i == cpu_id)
        continue;  // Don't steal from ourselves
      acquire(&kmems[i].lock);  // Lock the other CPU's freelist
      r = kmems[i].freelist;
      if (r) {
        kmems[i].freelist = r->next;  // Remove from the other CPU's freelist
        release(&kmems[i].lock);  // Release the lock
        break;
      }
      release(&kmems[i].lock);  // Release the lock if no memory found
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  pop_off();

  return (void*)r;
}