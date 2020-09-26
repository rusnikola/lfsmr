/*

Copyright 2017 University of Rochester

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



#ifndef RANGE_TRACKER_NEW_HPP
#define RANGE_TRACKER_NEW_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"


template<class T> 
class RangeTrackerNew:public BaseTracker<T>{
private:
	int task_num;
	int freq;
	int epochFreq;
	bool collect;
public:
	struct IntervalInfo {
		struct IntervalInfo* next;
		uint64_t birth_epoch;
		uint64_t retire_epoch;
	};
	
private:
	paddedAtomic<uint64_t>* upper_reservs;
	paddedAtomic<uint64_t>* lower_reservs;
	padded<uint64_t>* retire_counters;
	padded<uint64_t>* alloc_counters;
	padded<IntervalInfo*>* retired;

	paddedAtomic<uint64_t> epoch;

public:
	~RangeTrackerNew(){};
	RangeTrackerNew(int task_num, int epochFreq, int emptyFreq, bool collect): 
	 BaseTracker<T>(task_num),task_num(task_num),freq(emptyFreq),epochFreq(epochFreq),collect(collect){
		retired = new padded<IntervalInfo*>[task_num];
		upper_reservs = new paddedAtomic<uint64_t>[task_num];
		lower_reservs = new paddedAtomic<uint64_t>[task_num];
		for (int i = 0; i < task_num; i++){
			retired[i].ui = nullptr;
			upper_reservs[i].ui.store(UINT64_MAX, std::memory_order_release);
			lower_reservs[i].ui.store(UINT64_MAX, std::memory_order_release);
		}
		retire_counters = new padded<uint64_t>[task_num];
		alloc_counters = new padded<uint64_t>[task_num];
		epoch.ui.store(0,std::memory_order_release);
	}
	RangeTrackerNew(int task_num, int epochFreq, int emptyFreq) : RangeTrackerNew(task_num,epochFreq,emptyFreq,true){}

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return reserve(tid);
	}
	uint64_t get_epoch(){
		return epoch.ui.load(std::memory_order_acquire);
	}

	void* alloc(int tid){
		alloc_counters[tid] = alloc_counters[tid]+1;
		if(alloc_counters[tid]%(epochFreq*task_num)==0){
			epoch.ui.fetch_add(1,std::memory_order_acq_rel);
		}
		char* block = (char*) malloc(sizeof(IntervalInfo) + sizeof(T));
		IntervalInfo* info = (IntervalInfo*) (block + sizeof(T));
		info->birth_epoch = get_epoch();
		return (void*)block;
	}

	static uint64_t read_birth(T* obj){
		IntervalInfo* info = (IntervalInfo*) (obj + 1);
		return info->birth_epoch;
	}

	void reclaim(T* obj){
		obj->~T();
		free ((char*)obj);
	}

	T* read(std::atomic<T*>& obj, int idx, int tid){
		return read(obj, tid);
	}
    T* read(std::atomic<T*>& obj, int tid){
        uint64_t prev_epoch = upper_reservs[tid].ui.load(std::memory_order_acquire);
		while(true){
			T* ptr = obj.load(std::memory_order_acquire);
			uint64_t curr_epoch = get_epoch();
			if (curr_epoch == prev_epoch){
				return ptr;
			} else {
				// upper_reservs[tid].ui.store(curr_epoch, std::memory_order_release);
				upper_reservs[tid].ui.store(curr_epoch, std::memory_order_seq_cst);
				prev_epoch = curr_epoch;
			}
		}
    }

    void reserve_slot(T* ptr, int idx, int tid, T* node){
        uint64_t prev_epoch = upper_reservs[tid].ui.load(std::memory_order_acquire);
               while(true){
                       uint64_t curr_epoch = get_epoch();
                       if (curr_epoch == prev_epoch){
                               return;
                       } else {
                               // upper_reservs[tid].ui.store(curr_epoch, std::memory_order_release);
                               upper_reservs[tid].ui.store(curr_epoch, std::memory_order_seq_cst);
                               prev_epoch = curr_epoch;
                        }
                }
	}


	void start_op(int tid){
		uint64_t e = epoch.ui.load(std::memory_order_acquire);
		lower_reservs[tid].ui.store(e,std::memory_order_seq_cst);
		upper_reservs[tid].ui.store(e,std::memory_order_seq_cst);
		// lower_reservs[tid].ui.store(e,std::memory_order_release);
		// upper_reservs[tid].ui.store(e,std::memory_order_release);
	}
	void end_op(int tid){
		upper_reservs[tid].ui.store(UINT64_MAX,std::memory_order_release);
		lower_reservs[tid].ui.store(UINT64_MAX,std::memory_order_release);
	}
	void reserve(int tid){
		start_op(tid);
	}
	void clear(int tid){
		end_op(tid);
	}

	
	inline void incrementEpoch(){
		epoch.ui.fetch_add(1,std::memory_order_acq_rel);
	}
	
	void retire(T* obj, uint64_t birth_epoch, int tid){
		if(obj==NULL){return;}
		IntervalInfo** field = &(retired[tid].ui);
		IntervalInfo* info = (IntervalInfo*) (obj + 1);
		info->retire_epoch = epoch.ui.load(std::memory_order_acquire);
		info->next = *field;
		*field = info;
		if(collect && retire_counters[tid]%freq==0){
			empty(tid);
		}
		retire_counters[tid]=retire_counters[tid]+1;
	}

	void retire(T* obj, int tid){
		retire(obj, read_birth(obj), tid);
	}
	
	bool conflict(uint64_t* lower_epochs, uint64_t* upper_epochs, uint64_t birth_epoch, uint64_t retire_epoch){
		for (int i = 0; i < task_num; i++){
			if (upper_epochs[i] >= birth_epoch && lower_epochs[i] <= retire_epoch){
				return true;
			}
		}
		return false;
	}

	void empty(int tid){
		//read all epochs
		uint64_t upper_epochs_arr[task_num];
		uint64_t lower_epochs_arr[task_num];
		for (int i = 0; i < task_num; i++){
			//sequence matters.
			lower_epochs_arr[i] = lower_reservs[i].ui.load(std::memory_order_acquire);
			upper_epochs_arr[i] = upper_reservs[i].ui.load(std::memory_order_acquire);
		}

		// erase safe objects
		IntervalInfo** field = &(retired[tid].ui);
		IntervalInfo* info = *field;
		while (info != nullptr) {
			IntervalInfo* curr = info;
			info = curr->next;
			if(!conflict(lower_epochs_arr, upper_epochs_arr, curr->birth_epoch, curr->retire_epoch)){
				*field = info;
				reclaim((T*)curr - 1);
				this->dec_retired(tid);
				continue;
			}
			field = &curr->next;
		}
	}

	bool collecting(){return collect;}
};


#endif
