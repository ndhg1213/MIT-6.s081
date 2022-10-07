// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  uint8 count;
  struct spinlock lock;
}refcnt[(PHYSTOP >> 12)];    //PHYSTOP >> 12是总物理页数

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    inrefcnt((uint64)p); //先将引用计数加1再调用free减为0
    kfree(p);
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
  
  if(derefcnt((uint64)pa) == 0){ //引用计数为0时才真正清理

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  inrefcnt((uint64)r); //分配内存页时引用计数加1

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
inrefcnt(uint64 pa)
{
  pa = pa >> 12;    //获得pa对应物理页数作为数组索引

  acquire(&refcnt[pa].lock);
  refcnt[pa].count++;
  release(&refcnt[pa].lock);
}

uint8
derefcnt(uint64 pa)
{
  pa = pa >> 12;

  acquire(&refcnt[pa].lock);
  refcnt[pa].count--;
  release(&refcnt[pa].lock);

  return refcnt[pa].count;
}
