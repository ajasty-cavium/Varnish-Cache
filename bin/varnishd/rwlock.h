
#ifndef __RWLOCK_H__
#define __RWLOCK_H__

static inline void wmb()
{
    asm volatile ("dmb ish\n" ::: "memory");
}

static inline uint64_t fetch_add(volatile uint64_t *u64, int64_t i)
{
    uint64_t tmp;
    asm volatile (".cpu generic+lse\n"
		  "ldaddal %x[i], %x[tmp], %[v]\n"
		  : [tmp]"=&r"(tmp), [v]"+Q"(*u64)
		  : [i]"r"(i)
		  : "memory");
    return tmp + i;
}

static inline uint32_t swp(uint32_t *u32, uint32_t v)
{
    register uint32_t tmp;
    asm volatile (".cpu generic+lse\n"
		  "swpal %w[i], %w[tmp], %[v]\n"
		  : [tmp]"=&r"(tmp), [v]"+Q"(*u32)
		  : [i]"r"(v)
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

typedef union rwlock_t {
    volatile uint64_t u64;
    struct {
	volatile uint32_t rlock;
	volatile uint32_t wlock;
    };
} rwlock;

static void
RWLck_Init(rwlock *lck)
{
    wmb();
    lck->u64 = 0;
    wmb();
}

static void
RWLck_RLock(rwlock *lck)
{
    while(1) {
	rwlock rl;

	/* Wait for free */
	while (lck->wlock) pthread_yield();

	/* Take read-lock */
	rl.u64 = fetch_add(&lck->u64, 1);

	/* If wlock taken, back out and retry */
	if (!rl.wlock) return;

	fetch_add(&lck->u64, -1);
    }
}

static void
RWLck_RUnlock(rwlock *lck)
{
    fetch_add(&lck->u64, -1);
}

static void
RWLck_WLock(rwlock *lck)
{
    /* Keep trying to get lock */
    while (__atomic_exchange_n(&lck->wlock, 1, __ATOMIC_SEQ_CST) == 1) pthread_yield();

    /* Wait for readers to drain */
    while (lck->rlock);
}

static void
RWLck_WUnlock(rwlock *lck)
{
    __atomic_store_n(&lck->wlock, 0, __ATOMIC_SEQ_CST);
}

static int
RWLck_WPromote(rwlock *lck)
{
    if (__atomic_exchange_n(&lck->wlock, 1, __ATOMIC_SEQ_CST) == 1) return 0;

    fetch_add(&lck->u64, -1);

    while (lck->rlock);

    return 1;
}

#endif

