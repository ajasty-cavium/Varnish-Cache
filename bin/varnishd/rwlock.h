
#ifndef __RWLOCK_H__
#define __RWLOCK_H__

static inline void wmb()
{
    asm volatile ("dmb ish\n" ::: "memory");
}

static inline uint64_t fetch_add(uint64_t *u64, int i)
{
    uint64_t tmp;
    asm volatile (".cpu generic+lse\n"
		  "ldaddal %x[i], %x[tmp], %[v]\n"
		  : [tmp]"=&r"(tmp), [i]"+r"(i), [v]"+Q"(u64)
		  : : "memory");
    return tmp + i;
}

typedef union rwlock_t {
    uint64_t u64;
    struct {
	uint32_t rlock;
	uint32_t wlock;
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
	while (lck->wlock);

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
    while (__atomic_exchange_n(&lck->wlock, 1, __ATOMIC_SEQ_CST) == 1);

    /* Wait for readers to drain */
    while (lck->rlock);
}

static void
RWLck_WUnlock(rwlock *lck)
{
    __atomic_store_n(&lck->wlock, 0, __ATOMIC_SEQ_CST);
}

static void
RWLck_WPromote(rwlock *lck)
{
    while (__atomic_exchange_n(&lck->wlock, 1, __ATOMIC_SEQ_CST) == 1);

    fetch_add(&lck->u64, -1);

    while (lck->rlock);
}

#endif

