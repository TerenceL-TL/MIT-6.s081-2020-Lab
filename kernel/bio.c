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

#define NBUCKET 13

struct {
  struct spinlock lock[NBUCKET];
  struct spinlock exlock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
  struct buf hashbuc[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  int i;
  for(i = 0; i < NBUCKET; i++)
  {
    initlock(&bcache.lock[i], "bcache");
  }
  initlock(&bcache.exlock, "bcache_ex");
  
  for(i = 0; i < NBUCKET; i++)
  {
    bcache.hashbuc[i].prev = &bcache.hashbuc[i];
    bcache.hashbuc[i].next = &bcache.hashbuc[i];
  }
  // Create linked list of buffers
  i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hashbuc[i].next;
    b->prev = &bcache.hashbuc[i];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbuc[i].next->prev = b;
    bcache.hashbuc[i].next = b;
    i++;
    i%=NBUCKET;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint hash_idx = blockno % NBUCKET;

  acquire(&bcache.lock[hash_idx]);

  // Is the block already cached?
  for(b = bcache.hashbuc[hash_idx].next; b != &bcache.hashbuc[hash_idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[hash_idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.hashbuc[hash_idx].prev; b != &bcache.hashbuc[hash_idx]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[hash_idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[hash_idx]);

  acquire(&bcache.exlock);
  for(int i = 1; i < NBUCKET; i++)
  {
    uint c_buc = (hash_idx + i) % NBUCKET;
    acquire(&bcache.lock[c_buc]);
    for(b = bcache.hashbuc[c_buc].prev; b != &bcache.hashbuc[c_buc]; b = b->prev){
      if(b->refcnt == 0)
      {
        acquire(&bcache.lock[hash_idx]);
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->prev = &bcache.hashbuc[hash_idx];
        b->next = b->prev->next;
        b->prev->next = b;
        b->next->prev = b;

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[hash_idx]);
        release(&bcache.lock[c_buc]);
        release(&bcache.exlock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[c_buc]);
  }
  release(&bcache.exlock);
  

  // for(b = bcache.hashbuc[hash_idx].prev; b != &bcache.hashbuc[hash_idx]; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock[hash_idx]);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
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
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  uint hash_idx = (b->blockno) % NBUCKET;
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock[hash_idx]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbuc[hash_idx].next;
    b->prev = &bcache.hashbuc[hash_idx];
    bcache.hashbuc[hash_idx].next->prev = b;
    bcache.hashbuc[hash_idx].next = b;
  }
  
  release(&bcache.lock[hash_idx]);
}

void
bpin(struct buf *b) {
  uint hash_idx = (b->blockno) % NBUCKET;
  acquire(&bcache.lock[hash_idx]);
  b->refcnt++;
  release(&bcache.lock[hash_idx]);
}

void
bunpin(struct buf *b) {
  uint hash_idx = (b->blockno) % NBUCKET;
  acquire(&bcache.lock[hash_idx]);
  b->refcnt--;
  release(&bcache.lock[hash_idx]);
}


