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



#ifndef HE_TRACKER_HPP
#define HE_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"

#define MAX_HE		16

template<class T> class HETracker: public BaseTracker<T>{
private:
	int task_num;
	int he_num;
	int epochFreq;
	int freq;
	bool collect;

	
public:
	struct HEInfo {
		struct HEInfo* next;
		uint64_t birth_epoch;
		uint64_t retire_epoch;
	};

	struct HESlot {
		std::atomic<uint64_t> entry[MAX_HE];
		alignas(128) char pad[0];
	};

private:
	HESlot* reservations;
	HESlot* local_reservations;
	padded<uint64_t>* retire_counters;
	padded<uint64_t>* alloc_counters;
	padded<HEInfo*>* retired;

	paddedAtomic<uint64_t> epoch;

public:
	~HETracker(){};
	HETracker(int task_num, int he_num, int epochFreq, int emptyFreq, bool collect): 
	 BaseTracker<T>(task_num),task_num(task_num),he_num(he_num),epochFreq(epochFreq),freq(emptyFreq),collect(collect){
		retired = new padded<HEInfo*>[task_num];
		reservations = (HESlot *) memalign(alignof(HESlot), sizeof(HESlot) * task_num);
		local_reservations = (HESlot*) memalign(alignof(HESlot), sizeof(HESlot) * task_num * task_num);
		for (int i = 0; i<task_num; i++){
			retired[i].ui = nullptr;
			for (int j = 0; j<he_num; j++){
				reservations[i].entry[j].store(0, std::memory_order_release);
			}
		}
		retire_counters = new padded<uint64_t>[task_num];
		alloc_counters = new padded<uint64_t>[task_num];
		epoch.ui.store(1, std::memory_order_release);
	}
	HETracker(int task_num, int emptyFreq) : HETracker(task_num,emptyFreq,true){}

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return reserve(tid);
	}
	uint64_t getEpoch(){
		return epoch.ui.load(std::memory_order_acquire);
	}

	void* alloc(int tid){
		alloc_counters[tid] = alloc_counters[tid]+1;
		if(alloc_counters[tid]%(epochFreq*task_num)==0){
			epoch.ui.fetch_add(1,std::memory_order_acq_rel);
		}
		char* block = (char*) malloc(sizeof(HEInfo) + sizeof(T));
		HEInfo* info = (HEInfo*) (block + sizeof(T));
		info->birth_epoch = getEpoch();
		return (void*)block;
	}

	void reclaim(T* obj){
		obj->~T();
		free ((char*)obj);
	}

	T* read(std::atomic<T*>& obj, int index, int tid){
		uint64_t prev_epoch = reservations[tid].entry[index].load(std::memory_order_acquire);
		while(true){
			T* ptr = obj.load(std::memory_order_acquire);
			uint64_t curr_epoch = getEpoch();
			if (curr_epoch == prev_epoch){
				return ptr;
			} else {
				// reservations[tid].entry[index].store(curr_epoch, std::memory_order_release);
				reservations[tid].entry[index].store(curr_epoch, std::memory_order_seq_cst);
				prev_epoch = curr_epoch;
			}
		}
	}

	void reserve_slot(T* obj, int index, int tid){
		uint64_t prev_epoch = reservations[tid].entry[index].load(std::memory_order_acquire);
		while(true){
			uint64_t curr_epoch = getEpoch();
			if (curr_epoch == prev_epoch){
				return;
			} else {
				reservations[tid].entry[index].store(curr_epoch, std::memory_order_seq_cst);
				prev_epoch = curr_epoch;
			}
		}
	}
	

	void clear_all(int tid){
		//reservations[tid].entry.store(UINT64_MAX,std::memory_order_release);
		for (int i = 0; i < he_num; i++){
			reservations[tid].entry[i].store(0, std::memory_order_seq_cst);
		}
	}

	inline void incrementEpoch(){
		epoch.ui.fetch_add(1,std::memory_order_acq_rel);
	}
	
	void retire(T* obj, int tid){
		if(obj==NULL){return;}
		HEInfo** field = &(retired[tid].ui);
		HEInfo* info = (HEInfo*) (obj + 1);
		info->retire_epoch = epoch.ui.load(std::memory_order_acquire);
		info->next = *field;
		*field = info;
		if(collect && retire_counters[tid]%freq==0){
			empty(tid);
		}
		retire_counters[tid]=retire_counters[tid]+1;
	}

	bool can_delete(HESlot* local, uint64_t birth_epoch, uint64_t retire_epoch) {
		for (int i = 0; i < task_num; i++){
			for (int j = 0; j < he_num; j++){
				const uint64_t epo = local[i].entry[j].load(std::memory_order_acquire);
				if (epo < birth_epoch || epo > retire_epoch || epo == 0){
					continue;
				} else {
					return false;
				}
			}
		}
		return true;
	}

	void empty(int tid) {
		// erase safe objects
		HESlot* local = local_reservations + tid * task_num;
		HEInfo** field = &(retired[tid].ui);
		HEInfo* info = *field;
		if (info == nullptr) return;
		for (int i = 0; i < task_num; i++) {
			for (int j = 0; j < he_num; j++) {
				local[i].entry[j].store(reservations[i].entry[j].load(std::memory_order_acquire), std::memory_order_relaxed);
			}
		}
		do {
			HEInfo* curr = info;
			info = curr->next;
			if (can_delete(local, curr->birth_epoch, curr->retire_epoch)) {
				*field = info;
				reclaim((T*)curr - 1);
				this->dec_retired(tid);
				continue;
			}
			field = &curr->next;
		} while (info != nullptr);
	}
		
	bool collecting(){return collect;}
	
};


#endif
