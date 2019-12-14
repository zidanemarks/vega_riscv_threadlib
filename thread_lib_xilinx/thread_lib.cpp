#include "thread_lib.h"
#include "nand_ctrl.h"
#include "isr_hal.h"

thread_lib thread_lib::instance FASTDATA_SECTION;
threadext_dt thread_lib::extcontext[maxthread];

#ifdef WIN32
VOID CALLBACK thread_lib::run_thread(LPVOID pcontext)
#else
void thread_lib::run_thread()
#endif
{
	instance.current->extdat->thread_start(instance.current->extdat->thread_arg);
	threadexit();
}
#ifndef WIN32
#ifdef __cplusplus
extern "C" {
#endif
extern void __Yield(thread_dt* poldctx,thread_dt* pnewctx);
#ifdef __cplusplus
}
#endif
#endif // WIN32
#ifdef PROBE_LOG
void thread_lib::create_thread(const char* funcname,thread_fn thread_start,void* parg, uint32 priority)
#else
void thread_lib::create_thread(thread_fn thread_start,void* parg, uint32 priority)
#endif
{
	ASSERT(dpc_lib::getdpclevel() == dpc_lib::threadlevel, LEVEL_NORMAL);
	ASSERT(priority <= low_priority, LEVEL_NORMAL);
	thread_dt *pcontext = instance.spare_queue.current();
	ASSERT(pcontext!=NULL,LEVEL_NORMAL);
	instance.spare_queue.remove();
	pcontext->next = NULL;
	pcontext->priority = priority;
	pcontext->extdat->thread_start = thread_start;
	pcontext->extdat->thread_arg = parg;
#ifndef WIN32
	memset(pcontext->extdat->astack,0,threadext_dt::thread_stack_size);
#endif // WIN32
#ifdef PROBE_LOG
	pcontext->count = 0;
	memset(pcontext->funcname,' ',sizeof(pcontext->funcname));
	const char* funcnamebody=funcname;
	const char* current=funcname;
	while(*current!='\0')
	{
		if(*current==':')
		{
			funcnamebody=current+1;
		}
		++current;
	}
	for(uint32 i=0;i!=sizeof(pcontext->funcname);++i)
	{
		char deschar=funcnamebody[i];
		if(deschar=='\0')
			break;
		pcontext->funcname[i]=deschar;
	}
#endif
#ifdef WIN32
	pcontext->thread_fiber  = CreateFiber(threadext_dt::thread_stack_size, &run_thread, NULL);
	ASSERT(pcontext->thread_fiber != NULL,LEVEL_NORMAL);
#else
	pcontext->sp = ((uint32)pcontext->extdat->astack)+threadext_dt::thread_stack_size-60;
	pcontext->r15 = (uint32)run_thread-8;
#endif
	uint32 status = clearinterruptacquire();
	instance.ready_queue[priority].insert(pcontext);
	interruptrestore(status);
}
void thread_lib::reschedule()
{
	thread_dt *pnewctx;
#ifdef PROBE_LOG
	uint32 curtick=reg_ops::gettickcount();
	instance.current->count += curtick - instance.lasttick;
	do
	{
		communicator::overheat_delay();
		if(laterthan(reg_ops::gettickcount(),currenttick))
		{
			uint32 newtick=reg_ops::gettickcount();
			accidletime+=newtick-curtick;
			curtick=newtick;
			disp_diagnoisisinfo();
		}
	}
	while((pnewctx = instance.ready_queue[high_priority].current())==NULL &&
		(pnewctx = instance.ready_queue[normal_priority].current())==NULL &&
		(pnewctx = instance.ready_queue[low_priority].current())==NULL);
	{
		uint32 newtick=reg_ops::gettickcount();
		accidletime+=newtick-curtick;
		instance.lasttick = newtick;
	}
#else
	do
	{
		communicator::overheat_delay();
	}
	while((pnewctx = instance.ready_queue[high_priority].current())==NULL &&
		(pnewctx = instance.ready_queue[normal_priority].current())==NULL &&
		(pnewctx = instance.ready_queue[low_priority].current())==NULL);
#endif
#ifdef WIN32
	instance.current = pnewctx;
	LPVOID next_fiber =instance.current->thread_fiber;
	ASSERT(next_fiber != NULL,LEVEL_NORMAL);
	SwitchToFiber(next_fiber);
#else
	thread_dt *poldctx = instance.current;
	instance.current = pnewctx;
	__Yield(poldctx,pnewctx);
#endif
}


