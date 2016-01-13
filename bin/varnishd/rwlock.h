
#ifndef __RWLOCK_H__
#define __RWLOCK_H__

#include <sys/syscall.h>
#include <assert.h>

#define DUMPL(x) dumpl(x,__LINE__)

#define YIELD_THRESH 8

static inline void wmb()
{
    asm volatile ("dmb ish\n" ::: "memory");
}

static inline uint64_t fetch_add64(volatile uint64_t *u64, int64_t i)
{
    uint64_t tmp;
    asm volatile (".cpu generic+lse\n"
		  "ldaddal %x[i], %x[tmp], %[v]\n"
		  : [tmp]"=&r"(tmp), [v]"+Q"(*u64)
		  : [i]"r"(i)
		  : "memory");
    return tmp + i;
}

static inline uint32_t fetch_add32(volatile uint32_t *u32, int32_t i)
{
    uint32_t tmp;
    asm volatile (".cpu generic+lse\n"
		  "ldaddal %w[i], %w[tmp], %[v]\n"
		  : [tmp]"=&r"(tmp), [v]"+Q"(*u32)
		  : [i]"r"(i)
		  : "memory");
    return tmp + i;
}

static inline uint32_t swp(volatile uint32_t *u32, uint32_t v)
{
    register uint32_t tmp;
    asm volatile (".cpu generic+lse\n"
		  "swpal %w[v], %w[tmp], %[p]\n"
		  : [tmp]"=r"(tmp), [p]"+Q"(*u32)
		  : [v]"r"(v)
		  : "memory");
    return tmp;
}

static void
swait(int ival)
{
    register int w0 asm ("w0") = ival << 6;
    while(w0)
	asm volatile ("sub %w[i], %w[i], #1\n"
		: [i]"=&r"(w0));
}
typedef union wrlock_t {
    struct {
        volatile uint32_t wserving;
	volatile uint32_t wticket;
    };
    volatile uint64_t u64;
} wrlock;

typedef struct rwlock_t {
    wrlock w;
    volatile uint64_t readers;
    volatile pid_t owner;
} rwlock;

static void
RWLck_Init(rwlock *lck)
{
    __atomic_store_n(&lck->readers, 0ull, __ATOMIC_SEQ_CST);
    __atomic_store_n(&lck->w.u64, 0ull, __ATOMIC_SEQ_CST);
}

static uint32_t
RWLck_WCount(rwlock *lck)
{
    wrlock wr;

    wr.u64 = lck->w.u64;
    return wr.wticket - wr.wserving;
}

static void dumpl(rwlock *rwl, int line)
{
#if 0
    fprintf(stderr, "l %u= %lu-%u/%u %u.\n", line, rwl->readers, rwl->w.wserving, 
	    rwl->w.wticket, RWLck_WCount(rwl));
#endif
}

static pid_t gettid()
{
    return syscall(SYS_gettid);
}

static void
RWLck_RLock(rwlock *lck)
{
    while(1) {
	rwlock rl;
	int count;

	/* Wait for free */
	while ((count = RWLck_WCount(lck)) > 0)
	    if (count > YIELD_THRESH)
		pthread_yield();

	/* Take read-lock */
	rl.readers = fetch_add64(&lck->readers, 1);

	/* If wlock taken, back out and retry */
	if (RWLck_WCount(lck) == 0) return;

	fetch_add64(&lck->readers, -1);
    }
}

static void
RWLck_RUnlock(rwlock *lck)
{
    fetch_add64(&lck->readers, -1);
}

static void
RWLck_WLock(rwlock *lck)
{
    DUMPL(lck);
    /* Keep trying to get lock */
    while (RWLck_WCount(lck) > 0);
    uint32_t tick = fetch_add32(&lck->w.wticket, 1) - 1, tc;
    while ((tc = (tick - lck->w.wserving)) > 0) {
	DUMPL(lck);
	/* If we're too far behind, yield */
	if ((tick - lck->w.wserving) > YIELD_THRESH) pthread_yield();
    }

    /* Wait for readers to drain */
    while (lck->readers);
    lck->owner = gettid();
}

static void
RWLck_WUnlock(rwlock *lck)
{
    //assert(lck->w.wticket > lck->w.wserving);
    if (lck->owner != gettid()) abort();
    fetch_add32(&lck->w.wserving, 1);
}

static int
RWLck_WPromote(rwlock *lck)
{
    do {
	wrlock wr = lck->w, wr2 = lck->w;
	if (wr.wticket != wr.wserving) return 0;
	wr.wticket++;
	DUMPL(lck);
	if (__atomic_compare_exchange_n(&lck->w.u64, (uint64_t*)&wr2.u64, wr.u64, 0, 
		    __ATOMIC_SEQ_CST, __ATOMIC_RELAXED))
	    break;
    } while (1);

    fetch_add64(&lck->readers, -1);

    while (lck->readers);
    lck->owner = gettid();

    return 1;
}

#endif

