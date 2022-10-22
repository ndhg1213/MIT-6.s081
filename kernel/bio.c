// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define NBUCKETS 13

struct {
  struct spinlock biglock; //在哈希表操纵不同的哈希桶时需要整体上锁
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // bache.hashbucket.next is most recently used.
  //struct bue.hashbucket;
  struct buf hashbucket[NBUCKETS]; //每个哈希队列一个linked list及一个lock
} bcache;

uint
hash(uint blockno){ 
  return blockno%NBUCKETS;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.biglock, "bcache_biglock"); //初始化大锁

  for (uint i = 0; i < NBUCKETS; i++) 
  {
    initlock(&bcache.lock[i], "bcache"); //初始化小锁
  }
  
  //为了在插入链表过程中操作更加便捷
  //同时任务并未要求空间利用率
  //因此在使用时间戳依赖的LRU算法时仍然保留双向链表结构
  for (uint i = 0; i < NBUCKETS; i++)
  {
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){ //初始化不知道blockno因此将所有缓存块装入0号桶中
    b->next = bcache.hashbucket[0].next;
    b->prev = &bcache.hashbucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[0].next->prev = b;
    bcache.hashbucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *temp = 0;

  uint id = hash(blockno), min_timeticks = 0;

  acquire(&bcache.lock[id]);

  //在桶内查找缓冲块并且命中
  for(b = bcache.hashbucket[id].next; b != &bcache.hashbucket[id]; b = b->next){ 
    if(b->dev == dev && b->blockno == blockno){ 
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.lock[id]);

  acquire(&bcache.biglock); //可能需要操纵相邻桶缓存块，申请大锁
  acquire(&bcache.lock[id]);
  
  //在归还锁后由于并发性其他进程可能在此让出缓存块
  //重新扫描一次
  for(b = bcache.hashbucket[id].next; b != &bcache.hashbucket[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[id]);
      release(&bcache.biglock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  //没有得到期望缓存块
  //首先在桶内依据时间戳查找是否有合适块
  for(b = bcache.hashbucket[id].next; b != &bcache.hashbucket[id]; b = b->next){
    if(b->refcnt == 0 && (b -> timeticks < min_timeticks || temp == 0)){
      min_timeticks = b -> timeticks; //查找最小时间戳即最远使用的缓冲块
      temp = b;
    }
  }
  //查找成功
  if(temp){
    temp->dev = dev; //替换属性
    temp->blockno = blockno;
    temp->refcnt++;
    temp->valid = 0;
    release(&bcache.lock[id]); //依次释放获得的锁，防止出现死锁
    release(&bcache.biglock);
    acquiresleep(&temp -> lock);
    return temp;
  }else{
    //桶内没有能够使用的块
    //在其他桶中偷取一块缓冲块
    for (uint i = hash(id + 1); i != id; i = hash(i + 1)){
      for(b = bcache.hashbucket[i].next; b != &bcache.hashbucket[i]; b = b->next){
        if(b->refcnt == 0 && ((b -> timeticks < min_timeticks) || temp == 0)) {
          min_timeticks = b -> timeticks;
          temp = b;
        }
      }
      //查找成功
      if(temp){
        temp->dev = dev;
        temp->blockno = blockno;
        temp->refcnt++;
        temp->valid = 0;
        temp -> next -> prev = temp -> prev; //仍然将链表插入head后
        temp -> prev -> next = temp -> next;
        temp -> next = bcache.hashbucket[id].next;
        temp -> prev = &bcache.hashbucket[id];
        bcache.hashbucket[id].next -> prev = temp;
        bcache.hashbucket[id].next = temp;
        release(&bcache.lock[id]); 
        release(&bcache.biglock);
        acquiresleep(&temp -> lock);
        return temp;
      }
    }
  }

  release(&bcache.lock[id]);
  release(&bcache.biglock);
  panic("bget: no buffers");

}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the.hashbucket of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint id = hash(b -> blockno);

  acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b -> timeticks = ticks; //LRU算法判定方式改为时间戳依赖
  }
  
  release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  uint id = hash(b -> blockno);
  acquire(&bcache.lock[id]); //使用哈希锁
  b->refcnt++;
  release(&bcache.lock[id]);
}

void
bunpin(struct buf *b) {
  uint id = hash(b -> blockno); 
  acquire(&bcache.lock[id]); //使用哈希锁
  b->refcnt--;
  release(&bcache.lock[id]);
}


