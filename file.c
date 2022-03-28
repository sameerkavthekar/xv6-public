//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

#define FILESIZE sizeof(struct file)
#define PGSIZE    4096

struct next_file {
  struct next_file *next;
};

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  char *cache;
} ftable;

void
cache_init(char *cache)
{
  memset(cache, 0, PGSIZE);
  ((struct next_file *)cache)->next = 0;
  return;
}

struct file*
get_file(char *cache)
{
  while(cache != 0) {
    char *file_start = cache + sizeof(struct next_file);
    for(char *p = file_start; p + FILESIZE < cache + PGSIZE; p += FILESIZE) {
      struct file *f = (struct file *)p;
      if(f->ref == 0){
        f->ref = 1;
        return f;
      }
    }
    cache = (char *)((struct next_file *)cache)->next;
  }

  char *new_cache = kalloc();
  if(new_cache == 0)
    return 0;
  
  cache_init(new_cache);
  ((struct next_file *)cache)->next = (struct next_file *)new_cache;
  return get_file(new_cache);
}

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
  ftable.cache = kalloc();
  cache_init(ftable.cache);
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  f = get_file(ftable.cache);
  release(&ftable.lock);
  if(f != 0)
    return f;
  else
    panic("out of kernel memory");
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

