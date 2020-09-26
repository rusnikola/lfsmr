/*

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 

*/



#ifndef HyalineOSTR_TRACKER_HPP
#define HyalineOSTR_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include <mutex>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"
#include "../../../hyaline/lfbsmro.h"
#include "log2.hpp"

#include <malloc.h>

#define FLUSH_THRESHOLD 5
#define SMR_OFREQ 500

template<class T> class HyalineOSTRTracker: public BaseTracker<T>{
private:
	int task_num;
	bool collect;

public:
	
private:
	struct task_data {
		_Alignas(LF_CACHE_BYTES) lfbsmro_handle_t handle;
		lfbsmro_batch_t batch;
		unsigned long enter_num;
		size_t counter;
		bool firstTime;
		_Alignas(LF_CACHE_BYTES) char _pad[0];
	};

	// All benchmarks use one DS so we can move the smr of the DS here
	struct lfbsmro *smr;
	struct task_data *taskData;
	size_t SMR_ORDER;
	size_t SMR_BATCH;

	static HyalineOSTRTracker *myself;
public:
	~HyalineOSTRTracker(){};

	HyalineOSTRTracker(int task_num, int epochFreq, int emptyFreq, bool collect):
	BaseTracker<T>(task_num), task_num(task_num), collect(collect) {
		myself = this;
		SMR_ORDER = calc_next_log2(task_num);
		size_t SMR_NUM = (1U << SMR_ORDER);
		SMR_BATCH = (task_num < 64 ? 64 : SMR_NUM+1);

		smr = (struct lfbsmro *) memalign(LFBSMRO_ALIGN, LFBSMRO_SIZE(SMR_NUM));
		taskData = (struct task_data *) memalign(LF_CACHE_BYTES,
				sizeof(struct task_data) * task_num);
		for (int i=0; i<task_num; i++) {
			taskData[i].counter = 0;
			taskData[i].firstTime = true;
			taskData[i].enter_num = i & (SMR_NUM - 1);
			lfbsmro_batch_init(&taskData[i].batch);
		}
		lfbsmro_init(smr, SMR_ORDER);
	}

	static inline void free_node(struct lfbsmro * hdr, struct lfbsmro_node * node)
	{
		T * dnode = (T *) ((char *) node - sizeof(T));
		myself->reclaim(dnode);
		myself->dec_retired(0); // tid=0, it is not used anyway
	}

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return start_op(tid);
	}
	
	void* alloc(int tid){
		char * node = (char *) malloc(sizeof(T) + sizeof(struct lfbsmro_node));
		lfbsmro_init_node(smr, (struct lfbsmro_node *) (node + sizeof(T)), &taskData[tid].counter, SMR_OFREQ);
		return node;
	}

	T* read(std::atomic<T*>& obj, int idx, int tid){
		return read(obj, tid);
	}

	T* read(std::atomic<T*>& obj, int tid){
		return (T *)lfbsmro_deref(smr, taskData[tid].enter_num, (uintptr_t *)&obj);
	}

	void start_op(int tid){
		if (taskData[tid].firstTime) {
			taskData[tid].firstTime = false;
			lfbsmro_enter(smr, &taskData[tid].enter_num, SMR_ORDER, &taskData[tid].handle, 0, LF_DONTCHECK);
		}
	}

	void end_op(int tid){
		lfbsmro_trim(smr, &taskData[tid].enter_num, SMR_ORDER, &taskData[tid].handle, free_node, 0, LF_DONTCHECK, FLUSH_THRESHOLD);
	}

	void last_end_op(int tid){
		lfbsmro_leave(smr, taskData[tid].enter_num, SMR_ORDER, taskData[tid].handle, free_node, 0, LF_DONTCHECK);

#if 0
	    // Finalize retirement
	    while (!lfbsmro_batch_empty(&taskData[tid].batch)) {
		T * retr = (T *) alloc(tid);
	        lfbsmro_retire(smr, SMR_ORDER,
			(struct lfbsmro_node *) ((char *) retr + sizeof(T)),
			free_node, 0, &taskData[tid].batch, SMR_BATCH);
	    }
#endif
	}

	void reserve(int tid){
		start_op(tid);
	}
	void clear(int tid){
		end_op(tid);
	}

	void __attribute__ ((deprecated)) retire(T* obj, uint64_t e, int tid){
		return retire(obj,tid);
	}
	
	void retire(T* obj, int tid){
		if(obj==NULL){return;}

		lfbsmro_retire(smr, SMR_ORDER,
			(struct lfbsmro_node *) ((char *) obj + sizeof(T)),
			free_node, 0, &taskData[tid].batch, SMR_BATCH);
	}
	
	void empty(int tid){
	}
		
	bool collecting(){return collect;}
	
};

template<class T>
HyalineOSTRTracker<T>* HyalineOSTRTracker<T>::myself;


#endif
