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



#ifndef HyalineEL_TRACKER_HPP
#define HyalineEL_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include <mutex>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"
#include "../../../hyaline/lfsmr.h"
#include "log2.hpp"

#include <malloc.h>

template<class T> class HyalineELTracker: public BaseTracker<T>{
private:
	int task_num;
	bool collect;

public:

private:
	struct task_data {
		_Alignas(LF_CACHE_BYTES) lfsmr_handle_t handle;
		lfsmr_batch_t batch;
		unsigned long enter_num;
		_Alignas(LF_CACHE_BYTES) char _pad[0];
	};

	//All benchmarks use one DS so we can move the smr of the DS here
	struct lfsmr *smr;
	struct task_data *taskData;
	size_t SMR_ORDER;
	size_t SMR_BATCH;

	static HyalineELTracker *myself;
public:
	~HyalineELTracker(){};

	HyalineELTracker(int task_num, int epochFreq, int emptyFreq, int slots, bool collect):
	BaseTracker<T>(task_num), task_num(task_num), collect(collect) {
		myself = this;
		SMR_ORDER = calc_next_log2(task_num > slots ? slots : task_num);
		size_t SMR_NUM = (1U << SMR_ORDER);
		SMR_BATCH = ((unsigned)task_num < SMR_NUM ? SMR_NUM : SMR_NUM+1);

		smr = (struct lfsmr *) memalign(LFSMR_ALIGN, LFSMR_SIZE(SMR_NUM));
		taskData = (struct task_data *) memalign(LF_CACHE_BYTES,
				sizeof(struct task_data) * task_num);
		for (int i=0; i<task_num; i++) {
			taskData[i].enter_num = i & (SMR_NUM - 1);
			lfsmr_batch_init(&taskData[i].batch);
		}
		lfsmr_init(smr, SMR_ORDER);
	}

	static inline void free_node(struct lfsmr * hdr, struct lfsmr_node * node)
	{
		T * dnode = (T *) ((char *) node - sizeof(T));
		myself->reclaim(dnode);
		myself->dec_retired(0); // tid=0, it is not used anyway
	}

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return start_op(tid);
	}
	
	void* alloc(int tid){
		return (void*)malloc(sizeof(T) + sizeof(struct lfsmr_node));
	}

	void start_op(int tid){
		lfsmr_enter(smr, taskData[tid].enter_num, &taskData[tid].handle, 0, LF_DONTCHECK);
	}

	void end_op(int tid){
		lfsmr_leave(smr, taskData[tid].enter_num, SMR_ORDER, taskData[tid].handle, free_node, 0, LF_DONTCHECK);
	}

	void last_end_op(int tid){
#if 0
	    // Finalize retirement
	    while (!lfsmr_batch_empty(&taskData[tid].batch)) {
		T * retr = (T *) alloc(tid);
	        lfsmr_retire(smr, SMR_ORDER, (struct lfsmr_node *) ((char *) retr + sizeof(T)), free_node, 0, &taskData[tid].batch, SMR_BATCH);
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

		lfsmr_retire(smr, SMR_ORDER, (struct lfsmr_node *) ((char *) obj + sizeof(T)), free_node, 0, &taskData[tid].batch, SMR_BATCH);
	}
	
	void empty(int tid){
	}

	bool collecting(){return collect;}
	
};

template<class T>
HyalineELTracker<T>* HyalineELTracker<T>::myself;


#endif
