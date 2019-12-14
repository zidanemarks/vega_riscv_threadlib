#pragma once
#include "../../q530_com/base_def.h"
#include "../../q530_com/mem_lib.h"
#include "dev_hal.h"

#ifdef PROBE_LOG
extern uint32 readin;
extern uint32 readoutflash;
extern uint32 readoutmem;
extern uint32 currenttick;
extern uint32 writein;
extern uint32 writeoutwritexor;
extern uint32 writeoutwrite;
extern uint32 readingc;
extern uint32 writeoutdiscard;
extern uint32 accidletime;

void disp_diagnoisisinfo();
#endif

typedef void (*thread_fn)(void *arg);
#ifdef PROBE_LOG
#define CREATE_THREAD(func,arg,priority)	thread_lib::create_thread(#func,func,arg,priority)
#else
#define CREATE_THREAD(func,arg,priority)	thread_lib::create_thread(func,arg,priority)
#endif

class threadext_dt;
class thread_dt
{
public:
#ifdef WIN32
	LPVOID thread_fiber;
#else
	uint32 sp;
	union
	{
		uint32 reg[15];
		struct
		{
			uint32 r15;
			uint32 r17;
			uint32 r19;
			uint32 r20;
			uint32 r21;
			uint32 r22;
			uint32 r23;
			uint32 r24;
			uint32 r25;
			uint32 r26;
			uint32 r27;
			uint32 r28;
			uint32 r29;
			uint32 r30;
			uint32 r31;
		};
	};
#endif
	threadext_dt *extdat;
	thread_dt *next;
   	uint32 priority;
#ifdef PROBE_LOG
	uint32 count;
	char funcname[12];
#endif
};
class threadext_dt
{
public:
	static const uint32 thread_stack_size = 520284; //Near 512k. Not exactly 512k to avoid cache collision.
#ifndef WIN32
	uint8 astack[thread_stack_size];
#endif
	thread_fn thread_start;
	void *thread_arg;
};

class contextqueue
{
private:
	thread_dt* volatile head;
	thread_dt* volatile * volatile tail;

public:
	inline void init()
	{
		head = NULL;
		tail = &head;
	}

	inline thread_dt* volatile current()
	{
		return head;
	}

	inline void insert(thread_dt* c)
	{
		c->next = NULL;
		*tail = c;
		tail = &c->next;
	}

	inline void inserthead(thread_dt* c)
	{
		c->next = head;
		if(head == NULL)
		{
			tail = &c->next;
		}
		head = c;
	}

	inline void batchinsert(contextqueue &addqueue)
	{
		ASSERT(addqueue.head != NULL,LEVEL_NORMAL);
		*tail = addqueue.head;
		tail = addqueue.tail;
	}

	inline void remove()
	{
		if ((head = head->next) == NULL)
		{
			tail = &head;
		}
	}

	inline void removeall()
	{
		init();
	}

	inline void rotate()
	{
		ASSERT(head != NULL,LEVEL_NORMAL);
		*tail = head;
		tail = &head->next;
		head = head->next;
		*tail = NULL;
	}
};
class thread_lib
{
public:
	static const uint32 high_priority = 0;
	static const uint32 normal_priority = 1;
	static const uint32 low_priority = 2;
	static const uint32 childthreadintetris=lunsizeintetris*threadcountinlun;
#ifdef PROBE_LOG
	uint32 lasttick;
#endif
private:
	static const uint32 maxthread = tetrissizelimitinsystem*(1u+childthreadintetris);
	static thread_lib instance;
	static threadext_dt extcontext[maxthread];
	thread_dt availablecontext[maxthread];
	contextqueue ready_queue[3];//0:high priority   1:normal priority  2:low priority
	thread_dt main_thread;
	contextqueue spare_queue;
	thread_dt *current;
public:
	static contextqueue& get_readyqueue(uint32 priority)
	{
		return instance.ready_queue[priority];
	}
	static contextqueue& get_currentreadyqueue()
	{
		return instance.ready_queue[instance.current->priority];
	}

	static void reschedule();
	static inline void init()
	{
		instance.reset_threadpool();

#ifdef WIN32
		instance.main_thread.thread_fiber = ConvertThreadToFiber(NULL);
		ASSERT(instance.main_thread.thread_fiber != NULL,LEVEL_NORMAL);
#endif

		instance.ready_queue[low_priority].insert(&(instance.main_thread));
		instance.current = &(instance.main_thread);
#ifdef PROBE_LOG
		instance.lasttick = reg_ops::gettickcount();
		instance.current->count = 0;
#endif
	instance.current->priority = low_priority;

	}

	static inline thread_dt* getcurrentcontext()
	{
		return instance.current;
	}
	static inline void __yield()
	{
		instance.ready_queue[instance.current->priority].rotate();
		setinterrupt();
		reschedule();
		clearinterrupt();
	}
	static inline void yield()
	{
		uint32 status = clearinterruptacquire();
		instance.ready_queue[instance.current->priority].rotate();
		interruptrestore(status);
		reschedule();
	}
	static inline void __lowpriorityyield()
	{
		uint32 priority = instance.current->priority;
		instance.ready_queue[priority].remove();
		instance.ready_queue[low_priority].inserthead(instance.current);
		setinterrupt();
		reschedule();
		clearinterrupt();
		instance.ready_queue[low_priority].remove();
		instance.ready_queue[priority].inserthead(instance.current);
	}
	static inline void lowpriorityyield()
	{
		uint32 priority = instance.current->priority;
		uint32 status = clearinterruptacquire();
		instance.ready_queue[priority].remove();
		instance.ready_queue[low_priority].inserthead(instance.current);
		interruptrestore(status);
		reschedule();
		status = clearinterruptacquire();
		instance.ready_queue[low_priority].remove();
		instance.ready_queue[priority].inserthead(instance.current);
		interruptrestore(status);
	}
	static inline void sleep(uint32 Millisecond = 250)
	{
		uint32 RefTick = reg_ops::gettickcount()+Millisecond*reg_ops::tick_size;
		ASSERT((instance.current != NULL),LEVEL_NORMAL);
		do
		{
			yield();
		}while(beforethan(reg_ops::gettickcount(), RefTick));
	}

	static inline void threadexit()
	{
#ifdef PROBE_LOG
		instance.current->extdat->thread_start=NULL;
		instance.current->extdat->thread_arg=NULL;
#endif
		ASSERT((instance.current != NULL),LEVEL_NORMAL);
		uint32 status = clearinterruptacquire();

		instance.ready_queue[instance.current->priority].remove();

		interruptrestore(status);
		instance.spare_queue.insert(instance.current);
		instance.reschedule();
	}
	static inline void linkinterrupt(uint32 ISRID,ISRCallBack f)
	{
		aISRFunc[ISRID] = f;
	}

	static inline void reset_threadpool()
	{
#ifdef WIN32
		for (uint32 i=0; i!=maxthread; ++i)
		{
			LPVOID thread_fiber = instance.availablecontext[i].thread_fiber;
			if (thread_fiber != NULL)
			{
				DeleteFiber(thread_fiber);
			}
		}
#endif
		memset(&instance,0,sizeof instance);
		memset(extcontext,0,sizeof(extcontext));
		for(uint32 i = high_priority; i <= low_priority; ++i)
		{
			instance.ready_queue[i].init();
		}
		instance.spare_queue.init();
		for(uint32 i=0; i!=maxthread; ++i)
		{
			instance.availablecontext[i].extdat=&extcontext[i];
			instance.spare_queue.insert(&instance.availablecontext[i]);
		}
	}

	//create_thread should only be called in thread context. Not in interrupt/dpc context.
#ifdef PROBE_LOG
	static void create_thread(const char* funcname,thread_fn thread_start,void* parg, uint32 priority);
#else
	static void create_thread(thread_fn thread_start,void* parg, uint32 priority);
#endif


#ifdef WIN32
	static VOID CALLBACK run_thread(LPVOID pcontext);
#else
	static void run_thread();
#endif

#ifdef PROBE_LOG
	static inline void thread_printf(void)
	{
		//printf("main thread  count = %x\r\n", instance.main_thread.count);
		for(uint32 i = 0; i < maxthread; ++i)
		{
			thread_dt& thread=instance.availablecontext[i];
			if(thread.extdat->thread_start!=NULL)
			{
				printf("fun %c%c%c%c%c%c%c%c%c%c%c%c arg = %d tick = %d\r\n",
				thread.funcname[0],thread.funcname[1],thread.funcname[2],thread.funcname[3],
				thread.funcname[4],thread.funcname[5],thread.funcname[6],thread.funcname[7],
				thread.funcname[8],thread.funcname[9],thread.funcname[10],thread.funcname[11],
				(uint32)thread.extdat->thread_arg, thread.count);
			}
		}
	}
#endif

};

class event_dt
{
private:
	volatile uint32 status;
	thread_dt* volatile thread;
public:
	inline void init(uint32 initialvalue)
	{
		status = initialvalue;
		thread=NULL;
	}
	inline bool isset() const
	{
		return status!=0;
	}
	inline void __set()
	{
		ASSERT(!isinterruptenabled(),LEVEL_NORMAL);
		status = 1;
		if(thread != NULL)
		{
			thread_lib::get_readyqueue(thread->priority).insert(thread);
			thread=NULL;
		}
	}
	inline void set()
	{
		uint32 oldstatus = clearinterruptacquire();
		ASSERT(oldstatus,LEVEL_INFO);
		__set();
		interruptrestore(oldstatus);
	}
	inline void reset()
	{
		status = 0;
	}
	inline void __wait()
	{
		ASSERT(!isinterruptenabled(),LEVEL_NORMAL);
		ASSERT(dpc_lib::getdpclevel() == dpc_lib::threadlevel, LEVEL_NORMAL);
		if(status == 0)
		{
			thread_lib::get_currentreadyqueue().remove();
			ASSERT(thread==NULL,LEVEL_NORMAL);
			thread=thread_lib::getcurrentcontext();
			setinterrupt();
			thread_lib::reschedule();
			clearinterrupt();
		}
	}
	inline void wait()
	{
		ASSERT(isinterruptenabled(),LEVEL_NORMAL);
		ASSERT(dpc_lib::getdpclevel() == dpc_lib::threadlevel, LEVEL_NORMAL);
		uint32 oldstatus = clearinterruptacquire();
		if(status == 0)
		{
			thread_lib::get_currentreadyqueue().remove();
			ASSERT(thread==NULL,LEVEL_NORMAL);
			thread=thread_lib::getcurrentcontext();
			interruptrestore(oldstatus);
			thread_lib::reschedule();
		}
		else
		{
			interruptrestore(oldstatus);
		}
	}
};
class semaphore_dt
{
private:
	volatile int32 status;
	thread_dt* volatile thread;

public:
	inline semaphore_dt()
	{
	}
	inline semaphore_dt(int32 initialvalue)
	{
		init(initialvalue);
	}
	inline void init(int32 initialvalue)
	{
		status = initialvalue;
		thread=NULL;
	}
	inline void __inc()
	{
		ASSERT(!isinterruptenabled(),LEVEL_NORMAL);
		if(status == 0)
		{
			if(thread == NULL)
			{
				++status;
			}
			else
			{
				thread_lib::get_readyqueue(thread->priority).insert(thread);
				thread=NULL;
			}
		}
		else
		{
			++status;
		}
	}
	inline void inc()
	{
		uint32 oldstatus = clearinterruptacquire();
		ASSERT(oldstatus,LEVEL_INFO);
		__inc();
		interruptrestore(oldstatus);
	}
	inline int32 getresourcecount() const
	{
		return status;
	}
	inline bool isneedwait() const
	{
		return status <=0;
	}
	inline bool __trywait()
	{
		ASSERT(!isinterruptenabled(),LEVEL_NORMAL);
		ASSERT(dpc_lib::getdpclevel() == dpc_lib::threadlevel, LEVEL_NORMAL);
		if(status <= 0)
		{
			return false;
		}
		--status;
		return true;
	}
	inline bool trywait()
	{
		uint32 oldstatus = clearinterruptacquire();
		ASSERT(oldstatus,LEVEL_INFO);
		bool ret=__trywait();
		interruptrestore(oldstatus);
		return ret;
	}
	inline void __wait()
	{
		ASSERT(!isinterruptenabled(),LEVEL_NORMAL);
		ASSERT(dpc_lib::getdpclevel() == dpc_lib::threadlevel, LEVEL_NORMAL);
		if(status <= 0)
		{
			thread_lib::get_currentreadyqueue().remove();
			ASSERT(thread==NULL,LEVEL_NORMAL);
			thread=thread_lib::getcurrentcontext();

			setinterrupt();
			thread_lib::reschedule();
			clearinterrupt();
		}
		else
		{
			--status;
		}
	}
	inline void wait()
	{
		ASSERT(isinterruptenabled(),LEVEL_NORMAL);
		ASSERT(dpc_lib::getdpclevel() == dpc_lib::threadlevel, LEVEL_NORMAL);
		uint32 oldstatus = clearinterruptacquire();
		if(status <= 0)
		{
			thread_lib::get_currentreadyqueue().remove();
			ASSERT(thread==NULL,LEVEL_NORMAL);
			thread=thread_lib::getcurrentcontext();
			interruptrestore(oldstatus);
			thread_lib::reschedule();
		}
		else
		{
			--status;
			interruptrestore(oldstatus);
		}
	}
};

