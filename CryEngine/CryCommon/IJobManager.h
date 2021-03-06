////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  File name:   IJobManager.h
//  Version:     v1.00
//  Created:     1/8/2011 by Christopher Bolte
//  Description: JobManager interface (developed from the SPU only JobManager)
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#if !defined(__SPU__) // don't devirtualize on SPU
#include DEVIRTUALIZE_HEADER_FIX(IJobManager.h)
#endif

#ifndef __IJOBMAN_SPU_H
#define __IJOBMAN_SPU_H
#pragma once

#if !defined(__SPU__)
#include <ITimer.h>
#endif











// include global variables which are shared with SPUBackend
#include <JobManager_SPUDriver.h>

#if !defined(_SPU_DRIVER_PERSISTENT) && !defined(_LIB_DRIVER)
#include <CryThread.h>
#endif


#if !defined(_ALIGN)
// needed for SPU compilation (should be removed as soon as the spus doesn't depend on this header anymore
#define _ALIGN(x) __attribute__((aligned(x)))
#endif

// for some spu/crycg compile path, STATIC_CHECK is not defined, but for those it doesn't really matter
#if !defined(STATIC_CHECK)
	#define STATIC_CHECK(a, b)
#endif












	#define SPU_DRIVER_INT_PTR INT_PTR

// temp workaround for SPU include













// ==============================================================================
// forward declarations
// ==============================================================================
struct ILog;
struct CellGcmContextData;
struct CellSpurs;


class SJobFinishedConditionVariable;

#if !defined(__SPU__)
// implementation of mutex/condition variable
// used in the job manager for yield waiting in corner cases
// like waiting for a job to finish/jobqueue full and so on
class SJobFinishedConditionVariable
{
public:
	SJobFinishedConditionVariable()
	{
		m_nFinished = 0;
	}

	void Acquire()
	{
		//wait for completion of the condition
		m_Notify.Lock();
		while(m_nFinished == 0)
			m_CondNotify.Wait(m_Notify);
		m_Notify.Unlock();
	}

	void Release(  )
	{		
		m_Notify.Lock();
		m_nFinished = 1;
		m_CondNotify.Notify();
		m_Notify.Unlock();	
	}

	void SetRunning()
	{
		m_nFinished = 0;
	}

private:
	CryMutex							m_Notify;
	CryConditionVariable	m_CondNotify;	
	volatile uint32				m_nFinished;
};
#endif

namespace NSPU{ namespace NDriver{ struct SJobPerfStats; } }

namespace JobManager { class CJobBase; }
namespace JobManager { struct ISPUBackend; }

// for friend declaration
namespace NSPU { namespace NDriver { void ProcessJob(const uint32, const uint32, const uint32); } }

// Wrapper for Vector4 Uint









struct SVEC4_UINT
{
	uint32 v[4];
};




// ==============================================================================
// global values
// ==============================================================================
// front end values
namespace JobManager
{	
	enum { INVALID_JOB_HANDLE = ((unsigned int)-1) };

	//return results for AddJob
	enum EAddJobRes
	{
		eAJR_Success,						//success of adding job
		eAJR_NoElf,							//spu job is no elf file
		eAJR_NoSPUElf,					//spu job is no spu elf file
		eAJR_ElfTooBig,					//spu job elf is too big
		eAJR_NoQWAddress,				//spu job image start is not on a quadword address
		eAJR_EnqueueTimeOut,		//spu job was not added due to timeout (SPU were occupied for too long)
		eAJR_EnqueueTimeOutPushJob,		//spu job was not added due to timeout of a push job slot (SPU was occupied by one particular job for too long)
		eAJR_NotInitialized,	//SPU were not initialized
		eAJR_JobTooLarge,				//spu job cannot fit into a single SPU local store
		eAJR_JobSetupViolation,	//spu job has some invalid setup for packets/depending jobs
		eAJR_InvalidJobHandle,	//spu job was invoked with an invalid job handle (job has not been found in spu repository)		
		eAJR_UnknownError,			//unknown error
	};

	// Functions to use to allocation, deallocation
	typedef void (*TSPUFreeFunc)(void*);
	typedef void* (*TSPUMallocFunc)(unsigned int, unsigned int);

// SPUBackEnd values
namespace SPUBackend
{	

	enum { PAGE_MODE_MASK = 3 };

	enum { CACHE_MODE_MASK = ~7 };
	enum { JOBTYPE_MODE_MASK = 4 };
	enum { CACHE_MODE_SIZE_SHIFT = 3 };

	enum { PROF_ID_RESERVED = 8 };
	enum { MAX_PROF_ID = 1024 };						//max num of supported cache lookup id's
	enum { SPU_PRINTF_BUF_SIZE = 512	 };

	enum { scMaxWorkerSPU	= 4 };									//maximum number of SPUs to manage as worker threads
	enum { scMaxSPU	= 5 };												//maximum number of SPUs to manage (one extra SPU dedicated for DXPS)

	//maximum cache size, 
	enum ECacheMode
	{
		eCM_None = 0,			//no cache, bypassing
		eCM_4		 = 8,			//max cache size 4 KB
		eCM_8		 = 16,		//max cache size 8 KB
		eCM_16	 = 32,		//max cache size 16 KB
		eCM_32	 = 64,		//max cache size 32 KB
		eCM_64	 = 128		//max cache size 64 KB, default
	};

	// type of job
	enum EJobType
	{
		eGeneralJob = 0,	// general job, put into worker thread
		eOffLoadJob = 4	// offload job, only put into worker, if worker is a different cpu, like SPUs
	};

	#define CACHE_MODE_DEFAULT JobManager::SPUBackend::eCM_64
	#define PAGE_MODE_DEFAULT  JobManager::SPUBackend::ePM_Single
	
} // namespace JobManager::SPUBackend

namespace Fiber
{
	// The alignment of the fibertask stack (currently set to 128 kb)
	enum {FIBERTASK_ALIGNMENT = 128U<<10 };
	// The number of switches the fibertask records (default: 32) 
	enum {FIBERTASK_RECORD_SWITCHES = 32U };

} // namespace JobManager::Fiber

} // namespace JobManager


// ==============================================================================
// Used structures of the JobManager
// ==============================================================================
namespace JobManager
{	
	struct SJobStringHandle
	{
		const char *cpString;			//points into repository, must be first element
		unsigned int strLen;			//string length
		uint32 jobHandle;					//job address (corresponding to NBinPage::SJob)
		int jobId;								//index (also acts as id) of job
		uint32 nJobInvokerIdx;		//index of the jobInvoker (used to job switching in the prod/con queue)

		inline bool operator==(const SJobStringHandle& crOther) const;		
		inline bool operator<(const SJobStringHandle& crOther) const;
		
	};

	//handle retrieved by name for job invocation
	typedef SJobStringHandle* TJobHandle;	

	inline bool IsValidJobHandle(const JobManager::TJobHandle cJobHandle)
	{
		return cJobHandle->jobHandle != JobManager::INVALID_JOB_HANDLE;
	}

	bool SJobStringHandle::operator==(const SJobStringHandle& crOther) const
	{
#if !defined(__SPU__)
		return strcmp(cpString,crOther.cpString) == 0;



#endif
	}

	bool SJobStringHandle::operator<(const SJobStringHandle& crOther) const
	{
#if !defined(__SPU__)
		return strcmp(cpString,crOther.cpString) < 0;



#endif
	}

	// struct to collect profiling informations about job invocations and sync times
	struct SJobProfilingData
	{	



		typedef CTimeValue TimeValueT;


		TimeValueT nStartTime;
		TimeValueT nEndTime;

		uint32 nWorkerThread;
		uint32 nTimeCacheLookups;		// only used on PS3
		uint32 nTimeCodePaging;			// only used on PS3
		uint32 nTimeFnResolve;			// only used on PS3
		uint32 nTimeMemtransferSync; // only used on PS3
		uint32 pad[3];

		// first 48 bytes are written by SPU
		TimeValueT nDispatchTime;		
		TimeValueT nWaitBegin;
		TimeValueT nWaitEnd;

		TJobHandle jobHandle;		
		uint32 nThreadId;
	} _ALIGN(16); // 48 bytes


	// special combination of a volatile spinning variable combined with a semaphore
	// which is used if the running state is not yet set to finish during waiting
	struct SJobSyncVariable
	{		
#if !defined(_SPU_DRIVER_PERSISTENT) // no constructor for SPU driver, to spare code size
		SJobSyncVariable();
#endif
		bool IsRunning() const volatile;
	
		// interface, should only be used by the job manager or the job state classes
		void Wait() volatile;		
		void SetRunning() volatile;
		void SetStopped() volatile;				
	private:
		friend class CJobManager;		
		friend void NSPU::NDriver::ProcessJob(const uint32, const uint32, const uint32);

		// union used to combine the semaphore and the running state in a single word
		union SyncVar
		{
			volatile SPU_DRIVER_INT_PTR running;
			volatile SJobFinishedConditionVariable *pSemaphore;
#if defined(WIN64) || defined(LINUX64) 
			volatile int64 exchangeValue;
#else
			volatile long exchangeValue;
#endif
		};

		// util function to the C-A-S operation
		bool CompareExchange( SyncVar newValue, SyncVar oldValue, SyncVar &rResValue ) volatile;		
		 
		// members
		SyncVar syncVar;		// sync-variable which contain the running state or the used semaphore

	};


	//condition variable like struct to be used for polling if a job has been finished
	//since it is set by the SPU by DMA, it has to be on a 16 byte boundary (alignment ensured by padding in SJobData)
	struct SJobStateBase
	{
	public:		
		ILINE bool IsRunning() const		{	return syncVar.IsRunning(); }
		ILINE void SetRunning( )				{ syncVar.SetRunning(); }
		ILINE void SetStopped()					
		{ 



			syncVar.SetStopped(); 

		}			
		ILINE void Wait()								{ syncVar.Wait(); }

	private:
		friend class CJobManager;	

		SJobSyncVariable syncVar;
	};

	// for speed use 16 byte aligned job state
	struct SJobState : SJobStateBase 
	{ 
		void InitProfilingData();

		// when doing profiling, intercept the SetRunning() and SetStopped() functions for profiling informations

		ILINE SJobState() 		
#if defined(JOBMANAGER_SUPPORT_PROFILING)
		: pJobProfilingData(NULL),
			lock(0)
#endif
		{}

		ILINE void SetRunning();		
		ILINE void SetStopped();	
#if !defined(__SPU__) && defined(JOBMANAGER_SUPPORT_PROFILING)
		ILINE void LockProfilingData() { CrySpinLock( &lock, 0, 1 ); }
		ILINE void UnLockProfilingData() { CrySpinLock( &lock, 1, 0 ); }
#endif

#if defined(JOBMANAGER_SUPPORT_PROFILING)	
		SJobProfilingData *pJobProfilingData;
		volatile int lock;
#endif

	} _ALIGN(16);


	struct SSPUFrameStats
	{
		float spuStatsPerc[JobManager::SPUBackend::scMaxSPU];	//0.f..100.f
		
		inline SSPUFrameStats();
		inline void Reset();				
	};

	SSPUFrameStats::SSPUFrameStats()
	{
		Reset();
	}

	void SSPUFrameStats::Reset()
	{
		for(int i=0; i<JobManager::SPUBackend::scMaxSPU; ++i)
			spuStatsPerc[i] = 0.f;
	}


	//per frame rsx data emitted by DXPS thread
	struct SFrameProfileRSXData
	{
		unsigned int frameTime;
		unsigned int rsxWaitTime;
		unsigned int psTime;
		unsigned int vsTime;
		unsigned int flushTime;
		unsigned int inputLayoutTime;
		unsigned int pad[2];
	} _ALIGN(16);


	//per frame and job specific 
	struct SFrameProfileData
	{
		unsigned int usec;				//last frames SPU time in microseconds (written and accumulated by SPU)
		unsigned int count;				//number of calls this frame (written by PPU and SPU)
		const char* cpName;				//job name (written by PPU)

		unsigned int usecLast;		//last but one frames SPU time in microseconds (written and accumulated by SPU)

		inline SFrameProfileData();
		inline SFrameProfileData(const char* cpJobName);		

		inline void Reset();
		inline void operator=(const SFrameProfileData& crFrom);						
		inline bool operator<(const SFrameProfileData& crOther) const;
		
	} _ALIGN(16);

	SFrameProfileData::SFrameProfileData(const char* cpJobName) : usec(0), count(0), cpName(cpJobName), usecLast(0)
	{}

	SFrameProfileData::SFrameProfileData() : cpName("Uninitialized"), usec(0), count(0), usecLast(0)
	{}

	void SFrameProfileData::operator=(const SFrameProfileData& crFrom)
	{
		usec = crFrom.usec;
		count = crFrom.count;
		cpName = crFrom.cpName;
		usecLast = crFrom.usecLast;
	}

	void SFrameProfileData::Reset()
	{
		usecLast = usec;
		usec = count = 0;
	}

	bool SFrameProfileData::operator<(const SFrameProfileData& crOther) const
	{
		//sort from large to small
		return usec > crOther.usec;
	}

	//binary representation of a job
	struct SJob
	{
		unsigned short totalJobSize;		//total size of job in 4 bytes (header plus executable code and function table size) (size >> 2)
		short initialPages[4];					//id of first 4 pages, entry point points into initialPages[0], other might be -1
		unsigned short destPageOff;			//relative offset of the entry point into the first page (in 4 byte units)
		unsigned short branchOff;				//offset of the branch (relative to the job start (not text start)) (in 4 byte units)
		unsigned short bhOff;						//offset of the branch hint (relative to the job start (not text start)) (in 4 byte units)

		unsigned short funcTableOffset;	//offset of the function table relative to SJob, 0 if funcTableEntryCount = 0
		unsigned short funcTableEntryCount;//number of entries in function table
		unsigned short funcTableDebugOffset;//offset of the function table PPU addresses relative to SJob, will be same as funcTableOffset if not present
		unsigned short pageMode;				//page mode set by JobGen, encoded in upper 2 bite, lower bits contain max page size
		unsigned short bssOffset;				//offset of bss relative to job start (in multiple of 16 bytes)
		unsigned short bssSize;					//size of bss(in multiple of 16 bytes)
		unsigned short funcTableBinOff;	//binary offset of function table (without bss), in multiple of 4 bytes
		unsigned short funcTableSize;		//function table size, in multiple of 4 bytes

		unsigned short funcProfCount;		//number of function profile sections
		unsigned short funcProfIndStart;//index string table starts for function profile sections
		unsigned short funcProfTimingOff128;//offset for job timing area in cacheline units(allocated at startup)
		unsigned short pad0;

		unsigned int	 jobNameEA;				//set up during loading to map from SJob to its name
		unsigned short pad1[2];

		static const unsigned int scPageModeMask	= (1<<15 | 1<<14);
		static const unsigned int scPageModeShift = (14);

		unsigned int GetMaxPageSize()
		{
			return (pageMode & ~scPageModeMask) * 128;
		}
		unsigned int GetPageMode()
		{
			return (pageMode & scPageModeMask) >> scPageModeShift;
		}
		void SetMaxPageSize(const unsigned int cMaxSize)
		{
			pageMode &= scPageModeMask; pageMode |= (unsigned short)(cMaxSize / 128);
		}
		void SetPageMode(const unsigned int cPageMode)
		{
			pageMode &= ~scPageModeMask; pageMode |= (unsigned short)((unsigned int)cPageMode << scPageModeShift);
		}

		//here follows the executable text
		//	followed by the function pointer table
	} _ALIGN(16);	


	// struct to represent a packet for the consumer producer queue
	struct SAddPacketData
	{
		unsigned char opMode;
		unsigned short nInvokerIndex; // to keep the struct 16 byte big, we use a index into the info block
		unsigned char pad[1];
		SPU_DRIVER_INT_PTR eaJobState;
		unsigned int binJobEA;
		unsigned int jobEA;
		ILINE void SetPageMode(const JobManager::SPUBackend::EPageMode cPageMode)
		{
			opMode = (opMode & ~JobManager::SPUBackend::PAGE_MODE_MASK) | (unsigned char)cPageMode;
		}
		ILINE void SetCacheMode(const JobManager::SPUBackend::ECacheMode cCacheMode)
		{
			opMode = (opMode & ~JobManager::SPUBackend::CACHE_MODE_MASK) | (unsigned char)cCacheMode;
		}
		ILINE JobManager::SPUBackend::EPageMode GetPageMode() const
		{
			return (JobManager::SPUBackend::EPageMode)(opMode & JobManager::SPUBackend::PAGE_MODE_MASK);
		}
		ILINE JobManager::SPUBackend::ECacheMode GetCacheMode() const
		{
			return (JobManager::SPUBackend::ECacheMode)(opMode & JobManager::SPUBackend::CACHE_MODE_MASK);
		}
	} _ALIGN(16);

	//callback information
	struct SCallback
	{
		typedef void (*TSPUCallbackFunc)(void*);

		TSPUCallbackFunc pCallbackFnct;				//callback function, null if no
		void* __restrict pArg;							//argument to pCallbackFnct

		ILINE SCallback() : pCallbackFnct(0), pArg(0){}
	};

	// delegator function, takes a pointer to a params structure
	// and does the decomposition of the parameters, then calls the
	// job entry function
	typedef void (*Invoker)(void*);

	//info block transferred first for each job, size if passed to in CJobManager::EnqueueSPUJob
	//we have to transfer the info block, parameter block, input memory needed for execution, data and text of job
	//first 16 bytes gets overwritten for job state from SPU, dont store some post job persistent data there
	//parameter data, packet chain ptr and dma list data are transferred at once since they got allocated consecutively
	struct SInfoBlock
	{
		// data needed to run the job and it's functionality
		JobManager::SJobStateBase jobState;				//job state variable, set via DMA from SPU, 4 bytes (16 byte aligned)
		JobManager::SCallback callBackData;				// struct to contain information about a possible callback when the job finished( 8 bytes)
		Invoker jobInvoker;		
		SPU_DRIVER_INT_PTR eaExtJobStateAddress;	//external job state address /shared with address of prod-cons queue
		SPU_DRIVER_INT_PTR eaJobPerfAddress;			//source address of profiling stats for this job
		SPU_DRIVER_INT_PTR eaDMAJobAddress;				//source address of job program data
		
		// per job settings like cache size and so on
		unsigned char firstPageIndex;							//index of first page job branches into, added to fetch page fast
		unsigned char frameProfIndex;							//index of SFrameProfileData*				
		unsigned char nflags;											
		unsigned char opMode;											//page mode and cache mode		
		unsigned char paramSize;									//size in total of parameter block in 16 byte units
		unsigned char jobId;											//corresponding job ID, needs to track jobs		

		//	SPU-setting to know how to initialize the job	
		unsigned short bssSize;										//size of bss segment in 16 bytes
		unsigned short bssOff;										//offset of bss segment relativ to job start in 16 bytes
		unsigned short funcTableBinOff;						//binary offset of function table (without bss), in multiple of 4 bytes
		unsigned short funcTableSize;							//function table size, in multiple of 4 bytes
		unsigned short maxPageSize;								//max page size in 128 bytes
		unsigned short funcProfTimingCount;				//size of function profiler area to add to bss
		unsigned short jobSize;										//size of job to transfer (contains .data and .text section) (multiple of 4 bytes)			

		//if adding more data here, please adjust copying inside AddJob
#if defined(WIN64) || defined(LINUX64)
		static const unsigned int scAvailParamSize = 256 - 72;//size to keep it 128 byte aligned
#else
		static const unsigned int scAvailParamSize = 128 - 48;//size to keep it 128 byte aligned
#endif
		static const unsigned int scNoThreadId		 = 0xFFFFFFFF;//indicates that spu index acts as thread id
		
		//bits used for flags
		static const unsigned int scDedicatedThreadOnly		= 0x1;
		static const unsigned int scHasQueue							= 0x4;
		static const unsigned int scDebugEnabled					= 0x8;		
		static const unsigned int scTransferProfDataBack	= 0x10;
		static const unsigned int scKeepQueueCache				= 0x20;
		
		//sizes for accessing the SQueueNodeSPU array
#if defined(WIN64) || defined(LINUX64)
		static const unsigned int scSizeOfSJobQueueEntry			= 256;	//sizeof SInfoBlock (extra coded because shift)
		static const unsigned int scSizeOfSJobQueueEntryShift = 8;		//SInfoBlock in shifts
#else
		static const unsigned int scSizeOfSJobQueueEntry			= 128;	//sizeof SInfoBlock (extra coded because shift)
		static const unsigned int scSizeOfSJobQueueEntryShift = 7;		//SInfoBlock in shifts
#endif
		

		//parameter data are enclosed within to save a cache miss
		unsigned char paramData[scAvailParamSize];//is 32 byte aligned, make sure it is kept aligned
		
		ILINE void AssignMembersTo(SInfoBlock* const pDest) const
		{
			SVEC4_UINT *const __restrict pDst = (SVEC4_UINT*)pDest;
			const SVEC4_UINT *const __restrict cpSrc = (const SVEC4_UINT*)this;
			pDst[0] = cpSrc[0];
			pDst[1] = cpSrc[1];
			pDst[2] = cpSrc[2];		
			pDst[3] = cpSrc[3];		
#if defined(WIN64) || defined(LINUX64)
			pDst[4] = cpSrc[4];
			pDst[5] = cpSrc[5];
			pDst[6] = cpSrc[6];		
			pDst[7] = cpSrc[7];		

#endif
		}

		ILINE unsigned char GetFirstPageIndex() const
		{
			return firstPageIndex;
		}

		ILINE void SetPageMode(const JobManager::SPUBackend::EPageMode cPageMode)
		{
			opMode = (opMode & ~JobManager::SPUBackend::PAGE_MODE_MASK) | (unsigned char)cPageMode;
		}

		ILINE JobManager::SPUBackend::EPageMode GetPageMode() const
		{
			return (JobManager::SPUBackend::EPageMode)(opMode & JobManager::SPUBackend::PAGE_MODE_MASK);
		}

		ILINE void SetPageMaxSize(const unsigned int cMaxPageSize)
		{
			assert(cMaxPageSize < 230*1024);
			assert((cMaxPageSize & 127) == 0);
			maxPageSize = (unsigned short)(cMaxPageSize / 128);//store in 128 byte amounts
		}

		ILINE unsigned int GetMaxPageSize() const
		{
			return (unsigned int)(maxPageSize * 128);
		}

		ILINE void SetCacheMode(const JobManager::SPUBackend::ECacheMode cCacheMode)
		{
			opMode = (opMode & ~JobManager::SPUBackend::CACHE_MODE_MASK) | (unsigned char)cCacheMode;
		}
				
		ILINE bool IsOffLoadJob() const 
		{
			return (opMode & JobManager::SPUBackend::eOffLoadJob) != 0;
		}
		ILINE void SetOpMode(const unsigned int cMode)
		{
			assert(cMode < 255);
			opMode = (unsigned char)cMode;
		}

		ILINE unsigned int GetMaxCacheSize() const
		{
			return ((unsigned int)(opMode & JobManager::SPUBackend::CACHE_MODE_MASK) >> JobManager::SPUBackend::CACHE_MODE_SIZE_SHIFT) << 12;//in KB
		}

		ILINE bool HasQueue() const
		{
			return (nflags & (unsigned char)scHasQueue) != 0;
		}
		

		ILINE void EnableDebug()
		{
			nflags |= scDebugEnabled;
		}

		ILINE int IsDebugEnabled() const
		{
			return (int)((nflags & (unsigned char)scDebugEnabled) != 0);
		}

		ILINE void SetDedicatedThreadOnly(const bool cDedicatedThreadOnly)
		{
			if(cDedicatedThreadOnly)
				nflags |= scDedicatedThreadOnly;
			else
				nflags &= ~scDedicatedThreadOnly;
		}

		ILINE bool IsDedicatedThreadOnly() const
		{
			return (nflags & (unsigned char)scDedicatedThreadOnly) != 0;
		}

		ILINE void SetTransferProfDataBack(const bool cTransfer)
		{
			if(cTransfer)
				nflags |= scTransferProfDataBack;
			else
				nflags &= ~scTransferProfDataBack;
		}

		ILINE bool TransferProfDataBack() const
		{
			return (nflags & (unsigned char)scTransferProfDataBack) != 0;
		}

		ILINE int KeepCache() const
		{
			return (nflags & (unsigned char)scKeepQueueCache);
		}
		
		ILINE void SetExtJobStateAddress(const SPU_DRIVER_INT_PTR cAddr)
		{
			assert(!HasQueue());
			eaExtJobStateAddress = cAddr;
		}

		ILINE SPU_DRIVER_INT_PTR GetExtJobStateAddress() const
		{
			assert(!HasQueue());
			return eaExtJobStateAddress;
		}

		ILINE SPU_DRIVER_INT_PTR GetQueue() const
		{
			assert(HasQueue());
			return eaExtJobStateAddress;
		}

		ILINE unsigned char* const __restrict GetParamAddress()
		{
			assert(!HasQueue());
			return &paramData[0];
		}
	} _ALIGN(128);


	//state information for the fill and empty pointers of the job queue
	//this is usable as shared memory with concurrent accesses from several SPUs at once
	struct SJobQueuePos
	{
		union	// lockObtained is only use for pull pointer, while fetchIndex is only use for the push pointer
		{
			unsigned int									lockObtained;		// keeps track if the consumer state is locked (and by which SPUid, only pull and on SPU only)
			volatile unsigned int					fetchIndex;			// index use for job slot fetching (is mapped into the job queue)
		};
		
		SPU_DRIVER_INT_PTR							baseAddr;				// base of job queue
		SPU_DRIVER_INT_PTR							topAddr;				// top of job queue
		volatile unsigned int						publishIndex;		// index use for job slot publishing (is mapped into the job queue and checked by SPU)
	} _ALIGN(16);

	//struct covers DMA data setup common to all jobs and packets
	class CCommonDMABase
	{
	public:
		ILINE CCommonDMABase(){}

		ILINE void SetJobParamData(void* pParamData)
		{
			m_pParamData = pParamData;
		}

		ILINE const void* GetJobParamData() const
		{
			return m_pParamData;
		}

	protected:
		void*	m_pParamData;					//parameter struct, not initialized to 0 for performance reason
	};
		
	//delegation class for each job
	class CJobDelegator : public CCommonDMABase
	{
	public:	

		typedef volatile CJobBase* TDepJob;

		CJobDelegator();
		const JobManager::EAddJobRes RunJob( const unsigned int cOpMode, const JobManager::TJobHandle cJobHandle);
		void RegisterQueue(const void* const cpQueue, const bool cKeepCache = false);		
		const void* GetQueue() const;
		const bool KeepCache() const;		
		void RegisterCallback(void (*pCallbackFnct)(void*), void *pArg);
		void RegisterJobState(JobManager::SJobState* __restrict pJobState);
		
		const SCallback& RESTRICT_REFERENCE GetCallbackdata() const;		
		//return a copy since it will get overwritten
		void SetJobPerfStats(volatile NSPU::NDriver::SJobPerfStats* pPerfStats);		
		void SetParamDataSize(const unsigned int cParamDataSize);
		const unsigned int GetParamDataSize() const;
		void SetCurrentThreadId(const uint32 cID);		
		const uint32 GetCurrentThreadId() const;
		
		JobManager::SJobState*GetJobState() const;		
		void SetRunning();
								
		ILINE void SetDelegator( Invoker pGenericDelecator )
		{			
			m_pGenericDelecator = pGenericDelecator;		
		}

		Invoker GetGenericDelegator()
		{
			return m_pGenericDelecator;
		}

		bool IsHighPriority() const { return m_bIsHighPriority; }
		void SetHighPriority() { m_bIsHighPriority = true; }
		bool IsDedicatedThreadOnly() const { return m_bDedicatedThreadOnly; }
		void SetDedicatedThreadOnly() { m_bDedicatedThreadOnly = true; }

	protected:
		volatile NSPU::NDriver::SJobPerfStats* m_pJobPerfData;	//job performance data pointer
		JobManager::SJobState*m_pJobState;			//extern job state
		SCallback	m_Callbackdata;										//callback data
		const void*	m_pQueue;												//consumer/producer queue
		bool m_KeepCache;														//if true then the cache is kept between packets
		bool m_bIsHighPriority;												//if true then the job should use a high priority thread
		bool m_bDedicatedThreadOnly;												// if true, can only be executed on Raw SPUs
		unsigned int		m_ParamDataSize;						//sizeof parameter struct 		
		unsigned int		m_CurThreadID;							//current thread id
		Invoker m_pGenericDelecator;	//		
	};

	//base class for jobs
	class CJobBase
	{
	public:
		ILINE CJobBase() : m_OpMode(PAGE_MODE_DEFAULT | CACHE_MODE_DEFAULT)
		{
			m_pJobProgramData = 0;
		}

		ILINE void RegisterCallback(void (*pCallbackFnct)(void*), void *pArg) volatile
		{
			((CJobBase*)this)->m_JobDelegator.RegisterCallback(pCallbackFnct, pArg);
		}

		ILINE void RegisterJobState( JobManager::SJobState* __restrict pJobState) volatile
		{
			((CJobBase*)this)->m_JobDelegator.RegisterJobState(pJobState);
		}

		ILINE void RegisterQueue(const void* const cpQueue, const bool cKeepCache = false) volatile
		{
			((CJobBase*)this)->m_JobDelegator.RegisterQueue(cpQueue, cKeepCache);
		}
		ILINE const JobManager::EAddJobRes Run() volatile
		{
			return ((CJobBase*)this)->m_JobDelegator.RunJob(m_OpMode, m_pJobProgramData);
		}
		
		ILINE const JobManager::TJobHandle GetJobProgramData()
		{	
			return m_pJobProgramData;
		}

		ILINE const unsigned int GetOpMode() const
		{	
			return m_OpMode;
		}

		ILINE void SetCacheMode(const JobManager::SPUBackend::ECacheMode cCacheMode) volatile
		{	
			m_OpMode = (m_OpMode & (unsigned int)~JobManager::SPUBackend::CACHE_MODE_MASK) | (unsigned char)cCacheMode;
		}

		ILINE void SetCacheMode(const JobManager::SPUBackend::ECacheMode cCacheMode)
		{	
			m_OpMode = (m_OpMode & (unsigned int)~JobManager::SPUBackend::CACHE_MODE_MASK) | (unsigned char)cCacheMode;
		}
		
		ILINE void SetJobType(const JobManager::SPUBackend::EJobType cJobTope)
		{
			m_OpMode = (m_OpMode & ~JobManager::SPUBackend::JOBTYPE_MODE_MASK) | (unsigned char)cJobTope;
		}

		ILINE void SetCurrentThreadId(const uint32 cID)
		{
			((CJobBase*)this)->m_JobDelegator.SetCurrentThreadId(cID);
		}

		ILINE const uint32 GetCurrentThreadId() const
		{
			return ((CJobBase*)this)->m_JobDelegator.GetCurrentThreadId();
		}

		ILINE void SetJobPerfStats(volatile NSPU::NDriver::SJobPerfStats* pPerfStats) volatile
		{
			((CJobBase*)this)->m_JobDelegator.SetJobPerfStats(pPerfStats);
		}

	protected:
		CJobDelegator	m_JobDelegator;				//delegation implementation, all calls to job manager are going through it
		JobManager::TJobHandle				m_pJobProgramData;		//handle to program data to run
		unsigned int			m_OpMode;							//cache and code paging mode for the job

		//sets the job program data
		ILINE void SetJobProgramData(const JobManager::TJobHandle pJobProgramData, const JobManager::SPUBackend::ECacheMode cCacheMode = CACHE_MODE_DEFAULT)
		{
			assert(pJobProgramData != 0);
			m_pJobProgramData = pJobProgramData;
			m_OpMode					= cCacheMode;
		}

	};
		
} // namespace JobManager


// ==============================================================================
// Interface of the JobManager
// ==============================================================================

// Create a Producer consumer queue for a job type
#define PROD_CONS_QUEUE_TYPE(name, size) JobManager::CProdConsQueue<name, (size)>

// Declaration for the InvokeOnLinkedStacked util function
extern "C" 
{
	void InvokeOnLinkedStack(void (*proc)(void *), void *arg, void *stack, size_t stackSize);

} // extern "C"


namespace JobManager
{
	// base class for the various backends the jobmanager supports
	struct IBackend
	{
		virtual ~IBackend(){}

		virtual bool Init(uint32 nSysMaxWorker) = 0;
		virtual bool ShutDown() = 0;
		virtual void Update() = 0;

		virtual JobManager::EAddJobRes AddJob( JobManager::CJobDelegator& RESTRICT_REFERENCE crJob, const uint32 cOpMode, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock &rInfoBlock ) = 0;
		virtual uint32 GetNumWorkerThreads() const = 0;
	};

	
	// singleton managing the job queues and/for the SPUs
	UNIQUE_IFACE struct IJobManager
	{
		// === front end interface ===
		virtual ~IJobManager(){}

		virtual void Init( JobManager::TSPUFreeFunc FreeFunc, JobManager::TSPUMallocFunc MallocFunc, bool bEnablePrintf, uint32 nSysMaxWorker ) = 0;

		//adds a job
		virtual const JobManager::EAddJobRes AddJob( JobManager::CJobDelegator& RESTRICT_REFERENCE crJob, const unsigned int cOpMode, const JobManager::TJobHandle cJobHandle ) = 0;

		//polls for a spu job (do not use is a callback has been registered)
		//returns false if a time out has occurred
		virtual const bool WaitForJob( JobManager::SJobState& rJobState, const int cTimeOutMS=-1) const = 0;

		//obtain job handle from name
		virtual const JobManager::TJobHandle GetJobHandle(const char* cpJobName, const unsigned int cStrLen, JobManager::Invoker pInvoker)= 0;
		virtual const JobManager::TJobHandle GetJobHandle(const char* cpJobName, JobManager::Invoker pInvoker) = 0;

		virtual void* Allocate(uint32 size, uint32 alignment = 8) = 0;
		virtual void Free(void*) = 0;
		
		//shuts down spu job manager
		virtual void ShutDown() = 0;
	
		virtual const uint32 GetAllocatedMemory() const = 0;
		
		virtual JobManager::ISPUBackend* GetSPUBackEnd() = 0;

		virtual bool InvokeAsJob(const char *cJobHandle) const =0;
		virtual bool InvokeAsJob(const JobManager::TJobHandle cJobHandle) const =0;
		
		virtual bool InvokeAsSPUJob(const char *cJobHandle) const =0;
		virtual bool InvokeAsSPUJob(const JobManager::TJobHandle cJobHandle) const =0;

		virtual void SetJobFilter( const char *pFilter ) = 0;
		virtual void SetJobSystemEnabled( int nEnable ) = 0;

		virtual uint32 GetWorkerThreadId() const = 0;
		
		virtual JobManager::SJobProfilingData* GetProfilingData() = 0;

#if !defined(__SPU__)
		virtual void SetFrameStartTime( const CTimeValue &rFrameStartTime ) = 0;
#endif

		virtual void AddStartMarker( const char *pName,unsigned int id ) =0;
		virtual void AddStopMarker( unsigned int id ) =0;

		virtual void Update(int nJobSystemProfiler) = 0;

		virtual uint32 GetNumWorkerThreads() const = 0;

		// get a free semaphore from the Job Manager pool
		virtual SJobFinishedConditionVariable* GetSemaphore() = 0;

		// return a semaphore to the Job Manager pool
		virtual void FreeSemaphore(SJobFinishedConditionVariable*pSemaphore) =0;

		virtual void DumpJobList() = 0;
	};

	// specialized interface for PS3 with a SPU backend
	UNIQUE_IFACE struct ISPUBackend : public IBackend
	{		

		virtual bool Init(uint32 nSysMaxWorker) =0;
		virtual void Update() =0;

		//returns number of SPUs allowed for job scheduling
		virtual const unsigned int GetSPUsAllowed() const = 0;

		//returns spu driver size, all data must be placed behind it
		virtual const unsigned int GetDriverSize() const  = 0;

		//initializes all allowed SPUs
		virtual const bool InitSPUs(const char* cpSPURepo, const int cSPUWorkerCount ) = 0;

		
		//print performance stats
		virtual void PrintPerfStats(const volatile NSPU::NDriver::SJobPerfStats* pPerfStats, const char* cpJobName) const = 0;
							
		//enables spu debugging for a particular job
		virtual void EnableSPUJobDebugging(const void*) = 0;
		//registers a variable to check if the profiling data should be transferred back
		virtual void RegisterProfileStatVar(int *pVar) = 0;
		//enables/disables spu profiling
		virtual void EnableSPUProfiling(const uint8=0) = 0;
		//obtains and resets the SPU stats of the last frame
		virtual void GetAndResetSPUFrameStats(JobManager::SSPUFrameStats& rStats, const bool=true) = 0;
		virtual void GetAndResetSPUFrameStats(JobManager::SSPUFrameStats& rStats, const JobManager::SFrameProfileData*& rpCurFrameProfVec, uint32& rCount) = 0;
		//obtains and resets the SPU function profiling stats of the last frame
		virtual void GetAndResetSPUFuncProfStats(const JobManager::SFrameProfileData*& rpCurFuncProfStatVec, uint32& rCount, const uint32 cThresholdUSecs=100) = 0;
	
		//retrieve initialized spurs memory
		virtual CellSpurs* GetSPURS() = 0;

		//initialize values required for spu-libgcm usage
		virtual void GcmInit
		(
			const uint32,
			void *const __restrict,
			const uint32, 
			CellGcmContextData *const __restrict, 
			const uint32,
			const uint32,
			const uint32
		) = 0;
						
		
		// set the text filer, if a job matches this filer, it is not executed in a thread
		virtual void SetJobFilter( const char *) =0;

		// returns true in case a SPU repository was loaded succcessful
		virtual bool HasValidSPURepository() const = 0;
		
	};
	
	ILINE ISPUBackend* GetSPUBackEnd()
	{



		snPause();
		return NULL;

	}

	// util function to get the worker thread id in a job, returns 0xFFFFFFFF otherwise
	ILINE uint32 GetWorkerThreadId()
	{



		return gEnv->GetJobManager()->GetWorkerThreadId();

	}

	ILINE void SJobState::InitProfilingData()
	{
#if defined(JOBMANAGER_SUPPORT_PROFILING) && !defined(__SPU__)
		this->LockProfilingData();
		this->pJobProfilingData = gEnv->GetJobManager()->GetProfilingData();
		this->UnLockProfilingData();
#endif
	}

	ILINE void SJobState::SetRunning( )
	{			
#if defined(JOBMANAGER_SUPPORT_PROFILING) && !defined(__SPU__)
		LockProfilingData();
		IF(pJobProfilingData == NULL, 0)
			pJobProfilingData = gEnv->GetJobManager()->GetProfilingData();
		pJobProfilingData->nDispatchTime = gEnv->pTimer->GetAsyncTime();					
		UnLockProfilingData();
#endif
		SJobStateBase::SetRunning();
	}
	ILINE void SJobState::SetStopped()
	{		
#if defined(JOBMANAGER_SUPPORT_PROFILING) && !defined(__SPU__)
		LockProfilingData();
		if(pJobProfilingData)
		{
			pJobProfilingData->nEndTime = gEnv->pTimer->GetAsyncTime();	
			PIXEndNamedEvent();
		}
		UnLockProfilingData();
#endif
		SJobStateBase::SetStopped();
	}

// ==============================================================================
// Interface of the Producer/Consumer Queue for JobManager
// ==============================================================================
	//	producer (1 PPU thread) - consumer queue (1 SPU)
	//
	//	- all implemented ILINE using a template:
	//		- ring buffer size (num of elements)
	//		- instance type of job
	//		- param type of job
	//	- Factory with macro instantiating the queue (knowing the exact names for a job)
	//	- queue consists of:
	//		- ring buffer consists of 1 instance of param type of job
	//		- for atomic test on finished and push pointer update, the queue is 128 byte aligned and push, pull ptr and 
	//				DMA job state are lying within that first 128 byte
	//		- volatile push (only modified by PPU) /pull (only modified by SPU) pointer, point to ring buffer, both equal in the beginning
	//		- job instance (create with def. ctor)	
	//		- DMA job state (running, finished)
	//		- AddPacket - method to add a packet
	//			- wait on current push/pull if any space is available
	//			- need atomically test if a SPU is running and update the push pointer, if SPU is finished, start new job
	//		- Finished method, returns push==pull
	//		- WaitForSPU method doing a poll with nops
	//
	//	PPU job manager side:
	//		- provide RegisterProdConsumerQueue - method, set flags accordingly
	//	SPU side: 
	//		- check if it has  a prod/consumer queue
	//		- if it is part of queue, it must obtain DMA address for job state and of push/pull (static offset to pointer)
	//		- a flag tells if job state is job state or queue (eaExtJobStateAddress becomes the queue address)
	//		- lock is obtained when current push/pull ptr is obtained, snooping therefore enabled
	//		- before FlushCacheComplete, get next parameter packet if multiple packets needs to be processed
	//		- if all packets were processed, try to write updated pull pointer and set finished state
	//				if it fails, push pointer was updated during processing time, in that case get lock again and with it the new push pointer
	//		- loop till the lock and finished state was updated successfully
	//		- no HandleCallback method, only 1 SPU is permitted to run with queue
	//		- no write back to external job state, always just one inner packet loop
	struct SProdConsQueueBase
	{
		// ---- don't move the state and push ptr to another place in this struct ----
		// ---- they are updated together with an atomic operation
		SJobSyncVariable		m_QueueRunningState;				// waiting state of the queue
		void* m_pPush;																			//push pointer, current ptr to push packets into (written by PPU)
		// ---------------------------------------------------------------------------

		volatile void* m_pPull;															//pull pointer, current ptr to pull packets from (written by SPU)
		uint32 m_PullIncrement _ALIGN(16);					//increment of pull, also accessed by SPU
		uint32 m_AddPacketDataOffset;								//offset of additional data relative to push ptr		
		SPU_DRIVER_INT_PTR m_RingBufferStart;									//start of ring buffer (to swap properly by SPU), also accessed by SPU
		SPU_DRIVER_INT_PTR m_RingBufferEnd;										//end of ring buffer (to swap properly by SPU), also accessed by SPU
		SJobFinishedConditionVariable*				m_pQueueFullSemaphore;			// Semaphore used for yield-waiting when the queue is full
		
	};

	// util functions namespace for the producer consumer queue
	/////////////////////////////////////////////////////////////////////////////////
	namespace ProdConsQueueBase
	{	
		inline SPU_DRIVER_INT_PTR MarkPullPtrThatWeAreWaitingOnASemaphore( SPU_DRIVER_INT_PTR nOrgValue )	{ assert( (nOrgValue&1) == 0 ); return nOrgValue | 1; }		
		inline bool IsPullPtrMarked( SPU_DRIVER_INT_PTR nOrgValue ) { return (nOrgValue & 1) == 1; }
	}

	/////////////////////////////////////////////////////////////////////////////////
	template <class TJobType, unsigned int Size>
	class CProdConsQueue : public SProdConsQueueBase
	{
	public:
		CProdConsQueue(const bool cKeepCache = false);																//default ctor
		~CProdConsQueue();
		
		template <class TAnotherJobType>
		void AddPacket
		(
			const typename TJobType::packet& crPacket, 
			const JobManager::SPUBackend::ECacheMode cCacheMode,
			TAnotherJobType* pJobParam,
			bool differentJob = true
		);	//adds a new parameter packet with different job type (job invocation)
		void AddPacket
		(
			const typename TJobType::packet& crPacket, 
			const JobManager::SPUBackend::ECacheMode cCacheMode = JobManager::SPUBackend::eCM_64
		);	//adds a new parameter packet (job invocation)
		void WaitFinished();										//wait til all current jobs have been finished and been processed by a SPU
		bool IsEmpty();													//returns true if queue is empty
		uint32 PendingPackets() const;




	

	private:
		//initializes queue
		void Init(const unsigned int cPacketSize);		
		
		//get incremented ptr, takes care of wrapping
		void* const GetIncrementedPointer();
		
		void* m_pRingBuffer _ALIGN(128);																//the ring buffer
		TJobType m_JobInstance;															//job instance
		int	m_Initialized;
		int m_EncounteredDebug;

		static const unsigned int IDLE_NOPS = 32;				//number of nops performed in each wait loop iteration

		// compile tinme optinal profiling support




	} _ALIGN(128);//align for DMA speed and cache line reservation


// ==============================================================================
// Fiber Interface
// ==============================================================================


namespace Fiber {
	// Thread compatible fiber method
	typedef void (*FiberFnc)(void*); 

	enum FiberTaskState
	{ 
		FIBERTASK_STARTED = 0x1, 
		FIBERTASK_JOINED = 0x2, 
		FIBERTASK_FINALIZED = 0x4,
		FIBERTASK_SUSPENDED = 0x8,
	}; 

	// Fiber Task structure definition. 
	// 
	// Note: The beginning of the stack for the fiber task is always located
	// right after the structure instance in memory (aligned to quadword
	// boundary). This means it is easy for a given fiber to find out where the
	// info structure is and for the task info to determine where the task's
	// stack is.
	struct FiberState 
	{ 
		uintptr_t m_ReturnToc;			// The toc address of the return address  
		uintptr_t m_ReturnLR;				// The return link register of the containing thread
		uintptr_t m_ReturnStack;		// The active stack link address of the containing thread
		uintptr_t m_ReturnIP;				// The return address this fiber will jump back to 
	}; 

	// A fiber task context 
	struct FiberTask 
	{ 
		uint64 m_Magic;						// Magic header to ensure that only prepared stacks are switched back and forth
		uint32 m_Flags;						// The state flags of the task fiber 
		uint32 m_CurSwitch;       // The current switch index for rdtsc recording 
		uint64 m_SwitchTimes[JobManager::Fiber::FIBERTASK_RECORD_SWITCHES];   // The rdtsc at the time of the stack switches
		uint64 m_TimeYielded;     // The accumulated cycles outside of the fiber (must be reset outside of jobmanager)
		uint64 m_TimeInFiber;     // The accumulated cycles outside of the fiber (must be reset outside of jobmanager)
		uint64 m_FiberYields;     // The accumulated number of fiber switches 
		FiberState m_State;         // The saved context state of the fiber prior to it's last switch  
	}; 

	// Creates a fiber task and executes it till the first yield 
	FiberTask* CreateFiber(FiberFnc pFnc, void* pArgument, size_t stackSize); 

	// Destroys the fiber and frees all allocated memory 
	void DestroyFiber(FiberTask* pFiber); 

	// Switch the fiber directly (possible from within the fiber or outside)
	void SwitchFiberDirect();

	// Yield from within a fiber task back to the containing thread
	void YieldFiber(); 

	// Execute the fiber until the fiber has completed. 
	void JoinFiber(FiberTask* pFiber); 

	// Jump into the fiber task 
	void SwitchToFiber(FiberTask* pFiber); 

	// Returns true if the current thread is within the fiber 
	bool IsInFiberThread();	

	// Returns the time outside of the current fiberi (if present). If called
	// when currently within the fiber, it will return the number of cycles spent
	// outside of the fiber, else the number of cycles within the fiber. This
	// function can be used to obtain an accurate timing of cycles while
	// subtracting the amount of time spent in the 'other stack'. 
	uint64 FiberYieldTime(); 

} // namespace JobManager::Fiber
}// namespace JobManager


// interface to create the JobManager since it is needed before CrySystem on PS3
// needed here for calls from Producer Consumer queue
extern "C" 
{
#if !defined(__SPU__)
	DLL_EXPORT JobManager::IJobManager* GetJobManSPUInterface();
#endif
}

// ==============================================================================
// Implementation of Producer Consumer functions
// currently adding jobs to a producer/consumer queue is not supported
// ==============================================================================
#if !defined(__SPU__)
template <class TJobType, unsigned int Size>
ILINE JobManager::CProdConsQueue<TJobType, Size>::~CProdConsQueue()
{
}

///////////////////////////////////////////////////////////////////////////////
template <class TJobType, unsigned int Size>
ILINE JobManager::CProdConsQueue<TJobType, Size>::CProdConsQueue(const bool cKeepCache) : m_Initialized(0)
{
	assert(Size > 2);
	m_JobInstance.RegisterQueue((void*)this, cKeepCache);



}

///////////////////////////////////////////////////////////////////////////////
template <class TJobType, unsigned int Size>
ILINE void JobManager::CProdConsQueue<TJobType, Size>::Init(const uint32 cPacketSize)
{
	ScopedSwitchToGlobalHeap heapSwitch;
 	assert((cPacketSize & 15) == 0);
 	m_AddPacketDataOffset = cPacketSize;	
 	m_PullIncrement				= m_AddPacketDataOffset + sizeof(SAddPacketData);
 	m_pRingBuffer =  gEnv->GetJobManager()->Allocate( Size * m_PullIncrement,128);
 	assert(m_pRingBuffer);
 	m_pPush = m_pRingBuffer;
	m_pPull = m_pRingBuffer;
 	m_RingBufferStart	= (SPU_DRIVER_INT_PTR)m_pRingBuffer;
 	m_RingBufferEnd		= m_RingBufferStart + Size * m_PullIncrement;
 	m_Initialized			= 1;
 	((TJobType*)&m_JobInstance)->SetParamDataSize(cPacketSize);
 	m_EncounteredDebug = 0;
	m_pQueueFullSemaphore = NULL;
}

///////////////////////////////////////////////////////////////////////////////
template <class TJobType, unsigned int Size>
ILINE uint32 JobManager::CProdConsQueue<TJobType, Size>::PendingPackets() const
{
	//non atomic retrieval
	const SPU_DRIVER_INT_PTR push = (SPU_DRIVER_INT_PTR)m_pPush;
	const SPU_DRIVER_INT_PTR pull = (SPU_DRIVER_INT_PTR)m_pPull;
	const SPU_DRIVER_INT_PTR pullIncr = m_PullIncrement;
	if(push > pull)
		return (push - pull) / pullIncr;
	if(push < pull)
		return ((SPU_DRIVER_INT_PTR)m_RingBufferEnd-pull + push - (SPU_DRIVER_INT_PTR)m_RingBufferStart) / pullIncr;
	return 0;	
}

///////////////////////////////////////////////////////////////////////////////
template <class TJobType, unsigned int Size>
ILINE void JobManager::CProdConsQueue<TJobType, Size>::WaitFinished() 
{
	m_QueueRunningState.Wait();
	// ensure that the pull ptr is set right 
	assert(m_pPull == m_pPush);

}

///////////////////////////////////////////////////////////////////////////////
template <class TJobType, unsigned int Size>
ILINE bool JobManager::CProdConsQueue<TJobType, Size>::IsEmpty() 
{
	return (SPU_DRIVER_INT_PTR)m_pPush == (SPU_DRIVER_INT_PTR)m_pPull;
}

///////////////////////////////////////////////////////////////////////////////








///////////////////////////////////////////////////////////////////////////////
template <class TJobType, unsigned int Size>
ILINE void* const JobManager::CProdConsQueue<TJobType, Size>::GetIncrementedPointer() 
{
	//returns branch free the incremented wrapped aware param pointer
	SPU_DRIVER_INT_PTR cNextPtr = (SPU_DRIVER_INT_PTR)m_pPush + m_PullIncrement;
#if defined(WIN64) || defined(LINUX64)
	if((SPU_DRIVER_INT_PTR)cNextPtr >= (SPU_DRIVER_INT_PTR)m_RingBufferEnd)	cNextPtr = (SPU_DRIVER_INT_PTR)m_RingBufferStart;
	return (void*)cNextPtr;
#else
	const unsigned int cNextPtrMask = (unsigned int)(((int)(cNextPtr - m_RingBufferEnd)) >> 31);
	return (void*)(cNextPtr & cNextPtrMask | m_RingBufferStart & ~cNextPtrMask);
#endif
}

///////////////////////////////////////////////////////////////////////////////
template <class TJobType, unsigned int Size>
inline void JobManager::CProdConsQueue<TJobType, Size>::AddPacket
(
  const typename TJobType::packet& crPacket, 
  const JobManager::SPUBackend::ECacheMode cCacheMode
)
{
	AddPacket<TJobType>(crPacket, cCacheMode, (TJobType*)&m_JobInstance, false);
}

///////////////////////////////////////////////////////////////////////////////
template <class TJobType, unsigned int Size>
template <class TAnotherJobType>
inline void JobManager::CProdConsQueue<TJobType, Size>::AddPacket
(
	const typename TJobType::packet& crPacket, 
	const JobManager::SPUBackend::ECacheMode cCacheMode,
	TAnotherJobType* pJobParam,
	bool differentJob
)
{		
	const uint32 cPacketSize = crPacket.GetPacketSize();
	IF(m_Initialized == 0, 0)
		Init(cPacketSize);

	assert(m_RingBufferEnd == m_RingBufferStart + Size * (cPacketSize + sizeof(SAddPacketData)));

	const void* const cpCurPush = m_pPush;

	// don't overtake the pull ptr
	
	SJobSyncVariable curQueueRunningState = m_QueueRunningState;
	IF( (cpCurPush == m_pPull) && (curQueueRunningState.IsRunning()), 0)
	{		
			
		SPU_DRIVER_INT_PTR nPushPtr = (SPU_DRIVER_INT_PTR)cpCurPush;
		SPU_DRIVER_INT_PTR markedPushPtr = JobManager::ProdConsQueueBase::MarkPullPtrThatWeAreWaitingOnASemaphore(nPushPtr);
		m_pQueueFullSemaphore = gEnv->pJobManager->GetSemaphore();
		
		bool bNeedSemaphoreWait = false;
#if defined(WIN64) || defined(LINUX64)// for 64 bit, we need to atomicly swap 128 bit
		int64 compareValue[2] = { *alias_cast<int64*>(&curQueueRunningState),(int64)nPushPtr};		
#if defined(WIN64)
		_InterlockedCompareExchange128((volatile int64*)this, (int64)markedPushPtr, *alias_cast<int64*>(&curQueueRunningState), compareValue);
#else
		CryInterlockedCompareExchange128((volatile int64*)this, (int64)markedPushPtr, *alias_cast<int64*>(&curQueueRunningState), compareValue);
#endif
		// make sure nobody set the state to stopped in the meantime
		bNeedSemaphoreWait = ( compareValue[0] == *alias_cast<int64*>(&curQueueRunningState) && compareValue[1] == (int64)nPushPtr ); 
#else
		// union used to construct 64 bit value for atomic updates
		union T64BitValue
		{
			int64 doubleWord;
			struct  
	{
				uint32 word0;
				uint32 word1;
			};
		};

		// job system is process the queue right now, we need an atomic update
		T64BitValue compareValue;
		compareValue.word0 = *alias_cast<uint32*>(&curQueueRunningState);
		compareValue.word1 = (uint32)nPushPtr;

		T64BitValue exchangeValue;
		exchangeValue.word0 = *alias_cast<uint32*>(&curQueueRunningState);
		exchangeValue.word1 = (uint32)markedPushPtr;

		T64BitValue resultValue;
		resultValue.doubleWord = CryInterlockedCompareExchange64((volatile int64*)this, exchangeValue.doubleWord, compareValue.doubleWord);

		bNeedSemaphoreWait = ( resultValue.word0 == *alias_cast<uint32*>(&curQueueRunningState) && resultValue.word1 == nPushPtr);
#endif
		
		// C-A-S successful, now we need to do a yield-wait
		IF( bNeedSemaphoreWait,0)
			m_pQueueFullSemaphore->Acquire();

		gEnv->pJobManager->FreeSemaphore(m_pQueueFullSemaphore);
		m_pQueueFullSemaphore = NULL;
	}

#if defined(JOBMANAGER_SUPPORT_PROFILING) && !defined(__SPU__)
	if(crPacket.GetJobStateAddess())
	{
		JobManager::SJobState *pJobState = reinterpret_cast<JobManager::SJobState*>(crPacket.GetJobStateAddess());
		pJobState->SetRunning();
		pJobState->LockProfilingData();		
		pJobState->pJobProfilingData->jobHandle = pJobParam->GetJobProgramData();				
		pJobState->UnLockProfilingData();
	}
#endif

	//get incremented push pointer and check if there is a slot to push it into
	void* const cpNextPushPtr = GetIncrementedPointer();

	const SVEC4_UINT * __restrict pPacketCont = crPacket.GetPacketCont();
	SVEC4_UINT * __restrict pPushCont				 = (SVEC4_UINT*)cpCurPush;

	//copy packet data
	const uint32 cIters = cPacketSize >> 4;
	for(uint32 i=0;i<cIters; ++i)
		pPushCont[i] = pPacketCont[i];


	// setup addpacket data for SPUs
	SAddPacketData* const __restrict pAddPacketData = (SAddPacketData*)((unsigned char*)cpCurPush + m_AddPacketDataOffset);
	pAddPacketData->SetCacheMode(cCacheMode);
	pAddPacketData->jobEA = 0;
	pAddPacketData->eaJobState = crPacket.GetJobStateAddess();
	// used for non-spu backends during job switching
	pAddPacketData->nInvokerIndex = pJobParam->GetProgramHandle()->nJobInvokerIdx;

	if(differentJob)
	{
		SJob *pJob = NULL; 
		TJobHandle handle = pJobParam->GetProgramHandle(); 
		uint32 jobHandle = pJobParam->GetProgramHandle()->jobHandle; 

		pAddPacketData->jobEA =  pJobParam->GetJobAddress();
		pJob	= (SJob*)handle;
		pAddPacketData->binJobEA	= (uint32)pJob;
		if (IsValidJobHandle(handle))
			pAddPacketData->SetPageMode((JobManager::SPUBackend::EPageMode)pJob->GetPageMode());

	
	}

	// new job queue, or empty job queue
	if( !m_QueueRunningState.IsRunning() )
	{
		m_pPush = cpNextPushPtr;//make visible
		m_QueueRunningState.SetRunning();

		pJobParam->RegisterQueue((void*)this, false);
		pJobParam->SetParamDataSize(((TJobType*)&m_JobInstance)->GetParamDataSize());
		pJobParam->SetCacheMode(cCacheMode);
		if(crPacket.IsDedicatedThreadOnly())
			pJobParam->SetDedicatedThreadOnly();
		pJobParam->Run(); 
		
		return;
	}

	bool bAtomicSwapSuccessfull = false;  
	JobManager::SJobSyncVariable newSyncVar; newSyncVar.SetRunning(); 
#if defined(WIN64) || defined(LINUX64)// for 64 bit, we need to atomicly swap 128 bit
	int64 compareValue[2] = { *alias_cast<int64*>(&newSyncVar),(int64)cpCurPush};		
#if defined(WIN64)
	_InterlockedCompareExchange128((volatile int64*)this, (int64)cpNextPushPtr, *alias_cast<int64*>(&newSyncVar), compareValue);
#else
	CryInterlockedCompareExchange128((volatile int64*)this, (int64)cpNextPushPtr, *alias_cast<int64*>(&newSyncVar), compareValue);
#endif
	// make sure nobody set the state to stopped in the meantime
	bAtomicSwapSuccessfull = ( compareValue[0] > 0 ); 
#else
	// union used to construct 64 bit value for atomic updates
	union T64BitValue
	{
		int64 doubleWord;
		struct  
		{
			uint32 word0;
			uint32 word1;
		};
	};

	// job system is process the queue right now, we need an atomic update
	T64BitValue compareValue;
	compareValue.word0 = *alias_cast<uint32*>(&newSyncVar);
	compareValue.word1 = (uint32)cpCurPush;

	T64BitValue exchangeValue;
	exchangeValue.word0 = *alias_cast<uint32*>(&newSyncVar);
	exchangeValue.word1 = (uint32)cpNextPushPtr;

	T64BitValue resultValue;
	resultValue.doubleWord = CryInterlockedCompareExchange64((volatile int64*)this, exchangeValue.doubleWord, compareValue.doubleWord);

	bAtomicSwapSuccessfull = ( resultValue.word0 > 0 );
#endif

	// job system finished in the meantime, next to issue a new job
	if( !bAtomicSwapSuccessfull )
	{
		m_pPush = cpNextPushPtr;//make visible
		m_QueueRunningState.SetRunning();

		pJobParam->RegisterQueue((void*)this, false);
		pJobParam->SetParamDataSize(((TJobType*)&m_JobInstance)->GetParamDataSize());
		pJobParam->SetCacheMode(cCacheMode);
		if(crPacket.IsDedicatedThreadOnly())
			pJobParam->SetDedicatedThreadOnly();
		pJobParam->Run(); 		
	}
}
#endif

// ==============================================================================
// Job Delegator Function implementations
// ==============================================================================
ILINE JobManager::CJobDelegator::CJobDelegator() : m_ParamDataSize(0), m_CurThreadID(JobManager::SInfoBlock::scNoThreadId)
{
	m_pJobPerfData = NULL;
	m_pQueue = NULL;
	m_KeepCache = false;
	m_pJobState = NULL;
	m_bIsHighPriority =false;
	m_bDedicatedThreadOnly = false;
}





///////////////////////////////////////////////////////////////////////////////
ILINE const JobManager::EAddJobRes JobManager::CJobDelegator::RunJob( const unsigned int cOpMode,	const JobManager::TJobHandle cJobHandle )
{	






	JobManager::IJobManager *const __restrict pJobMan = gEnv->GetJobManager();		
	const JobManager::EAddJobRes cRes =  pJobMan->AddJob((CJobDelegator& RESTRICT_REFERENCE)*this, cOpMode, cJobHandle );			

#if defined(USE_JOB_QUEUE_VERIFICATION)
	OutputJobError(cRes, cJobHandle->cpString, cJobHandle->strLen);
#endif
	return cRes;


}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::RegisterQueue(const void* const cpQueue, const bool cKeepCache)
{
	m_pQueue = cpQueue;
	m_KeepCache = cKeepCache;
}

///////////////////////////////////////////////////////////////////////////////
ILINE const void* JobManager::CJobDelegator::GetQueue() const
{
	return m_pQueue;
}

///////////////////////////////////////////////////////////////////////////////
ILINE const bool JobManager::CJobDelegator::KeepCache() const
{
	return m_KeepCache;
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::RegisterCallback(void (*pCallbackFnct)(void*), void *pArg)
{
	m_Callbackdata.pCallbackFnct = pCallbackFnct;
	m_Callbackdata.pArg = pArg;
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::RegisterJobState( JobManager::SJobState* __restrict pJobState)
{
	m_pJobState = pJobState;
	pJobState->InitProfilingData();
}

///////////////////////////////////////////////////////////////////////////////
ILINE const JobManager::SCallback& RESTRICT_REFERENCE JobManager::CJobDelegator::GetCallbackdata() const
{
	return m_Callbackdata;
}

//return a copy since it will get overwritten
ILINE void JobManager::CJobDelegator::SetJobPerfStats(volatile NSPU::NDriver::SJobPerfStats* pPerfStats)
{
	m_pJobPerfData = pPerfStats;
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::SetParamDataSize(const unsigned int cParamDataSize)
{
	m_ParamDataSize = cParamDataSize;
}

///////////////////////////////////////////////////////////////////////////////
ILINE const unsigned int JobManager::CJobDelegator::GetParamDataSize() const
{
	return m_ParamDataSize;
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::SetCurrentThreadId(const uint32 cID)
{
	m_CurThreadID = cID;
}

///////////////////////////////////////////////////////////////////////////////
ILINE const uint32 JobManager::CJobDelegator::GetCurrentThreadId() const
{
	return m_CurThreadID;
}

///////////////////////////////////////////////////////////////////////////////
ILINE JobManager::SJobState* JobManager::CJobDelegator::GetJobState() const
{
	return m_pJobState;
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::SetRunning()
{
	m_pJobState->SetRunning();
}


// ==============================================================================
// Implementation of SJObSyncVariable functions
// ==============================================================================
#if !defined(_SPU_DRIVER_PERSISTENT) // no constructor for SPU driver, to spare code size
inline JobManager::SJobSyncVariable::SJobSyncVariable()  
{ 
	syncVar.running = 0; 
}
#endif

/////////////////////////////////////////////////////////////////////////////////
inline bool JobManager::SJobSyncVariable::IsRunning() const volatile		
{
	return syncVar.running != 0;
}

/////////////////////////////////////////////////////////////////////////////////
inline void JobManager::SJobSyncVariable::Wait() volatile
{



	if( syncVar.running == 0 )
		return;
			
	SyncVar newValue;  newValue.pSemaphore = gEnv->GetJobManager()->GetSemaphore();
	SyncVar oldValue; oldValue.running = 1;
	SyncVar resValue; 
	// if this C-A-S failles, it means that the job has set the running state to 0
	if( CompareExchange(newValue,oldValue, resValue) )
	{				
		const_cast<SJobFinishedConditionVariable*>(newValue.pSemaphore)->Acquire();
	}

	// we already had another condition at this place so wait for the original one
	if(resValue.running > 1)
	{
		const_cast<SJobFinishedConditionVariable*>(resValue.pSemaphore)->Acquire();
	}

	gEnv->GetJobManager()->FreeSemaphore( const_cast<SJobFinishedConditionVariable*>(newValue.pSemaphore) );

}

/////////////////////////////////////////////////////////////////////////////////
inline void JobManager::SJobSyncVariable::SetRunning() volatile
{
	// thread-safe, since called before the job is started
	assert(syncVar.exchangeValue == 0);
	syncVar.running = 1;
}


// provide a special path for SPUs, since they use a different atomic operations
// and require a interrupt thread call to unlock the semaphore
// the SPU version is living in SPUDriver_spu.cpp due to dependency issues
/////////////////////////////////////////////////////////////////////////////////
#if !defined(__SPU__)
inline void JobManager::SJobSyncVariable::SetStopped() volatile
{	
	if( syncVar.running == 0 )
		return;

	SyncVar resValue; 
	SyncVar newValue;  newValue.running = 0;
	SyncVar oldValue;

	// first try to set the result value to 0 atomically
	do 
	{				
			oldValue.exchangeValue = syncVar.exchangeValue;
	} while (!CompareExchange(newValue,oldValue,resValue));

	// now we have two cases: running was 1, which means no semaphore
	// or we have a semaphore which we have to release
	if( resValue.exchangeValue > 1 )
	{			
		const_cast<SJobFinishedConditionVariable*>(resValue.pSemaphore)->Release();
	}			
}
#endif

/////////////////////////////////////////////////////////////////////////////////
inline bool JobManager::SJobSyncVariable::CompareExchange( SyncVar newValue, SyncVar oldValue, SyncVar &rResValue ) volatile
{
#if !defined(__SPU__)
#if defined(WIN64) || defined(LINUX64)			
	rResValue.exchangeValue = CryInterlockedCompareExchange64( (volatile int64*)&syncVar, newValue.exchangeValue, oldValue.exchangeValue );
	return rResValue.exchangeValue == oldValue.exchangeValue;
#else		
	rResValue.exchangeValue = CryInterlockedCompareExchange( (volatile long*)&syncVar, newValue.exchangeValue, oldValue.exchangeValue );
	return rResValue.exchangeValue == oldValue.exchangeValue;
#endif



#endif
}


		
#endif //__IJOBMAN_SPU_H
