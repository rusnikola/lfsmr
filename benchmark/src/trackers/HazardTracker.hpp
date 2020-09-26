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



#ifndef HAZARD_TRACKER_HPP
#define HAZARD_TRACKER_HPP

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"


#define MAX_HP		16

template<class T>
class HazardTracker: public BaseTracker<T>{
private:
	int task_num;
	int slotsPerThread;
	int freq;
	bool collect;

	RAllocator* mem;

public:
	struct HazardInfo {
		struct HazardInfo* next;
	};

	struct HazardSlot {
		std::atomic<T*> entry[MAX_HP];
		alignas(128) char pad[0];
	};

private:
	HazardSlot* slots;
	HazardSlot* local_slots;
	padded<HazardInfo*>* retired;
	padded<int>* cntrs;

	void empty(int tid) {
		HazardSlot* local = local_slots + tid * task_num;
		HazardInfo** field = &(retired[tid].ui);
		HazardInfo* info = *field;
		if (info == nullptr) return;
		for (int i = 0; i < task_num; i++) {
			for (int j = 0; j < slotsPerThread; j++) {
				local[i].entry[j].store(slots[i].entry[j], std::memory_order_relaxed);
			}
		}
		do {
			HazardInfo* curr = info;
			info = curr->next;
			bool danger = false;
			auto ptr = (T*)curr - 1;
			for (int i = 0; i < task_num; i++){
				for (int j = 0; j < slotsPerThread; j++){ 
					if (ptr == local[i].entry[j]){
						danger = true;
						break;
					}
				}
			}
			if (!danger) {
				*field = info;
				this->reclaim(ptr);
				this->dec_retired(tid);
				continue;
			}
			field = &curr->next;
		} while (info != nullptr);
		return;
	}

public:
	~HazardTracker(){};
	HazardTracker(int task_num, int slotsPerThread, int emptyFreq, bool collect):BaseTracker<T>(task_num){
		this->task_num = task_num;
		this->slotsPerThread = slotsPerThread;
		this->freq = emptyFreq;
		slots = (HazardSlot*) memalign(alignof(HazardSlot), sizeof(HazardSlot) * task_num);
		local_slots = (HazardSlot*) memalign(alignof(HazardSlot), sizeof(HazardSlot) * task_num * task_num);
		for (int i = 0; i < task_num; i++) {
			for (int j = 0; j < slotsPerThread; j++) {
				slots[i].entry[j]=NULL;
			}
		}
		retired = new padded<HazardInfo*>[task_num];
		cntrs = new padded<int>[task_num];
		for (int i = 0; i<task_num; i++){
			cntrs[i]=0;
			retired[i].ui = nullptr;
		}
		this->collect = collect;
	}
	HazardTracker(int task_num, int slotsPerThread, int emptyFreq): 
		HazardTracker(task_num, slotsPerThread, emptyFreq, true){}

	T* read(std::atomic<T*>& obj, int idx, int tid){
		T* ret;
		T* realptr;
		while(true){
			ret = obj.load(std::memory_order_acquire);
			realptr = (T*)((size_t)ret & 0xfffffffffffffffc);
			reserve_slot(realptr, idx, tid);
			if(ret == obj.load(std::memory_order_acquire)){
				return ret;
			}
		}
	}

	void reserve_slot(T* ptr, int slot, int tid){
		slots[tid].entry[slot] = ptr;
	}
	void clearSlot(int slot, int tid){
		slots[tid].entry[slot] = NULL;
	}
	void clearAll(int tid){
		for(int i = 0; i<slotsPerThread; i++){
			slots[tid].entry[i] = NULL;
		}
	}
	void clear_all(int tid){
		clearAll(tid);
	}

	void* alloc(int tid){
		return (void*)malloc(sizeof(T)+sizeof(HazardInfo));
	}

	void retire(T* ptr, int tid){
		if (ptr==NULL){return;}
		HazardInfo** field = &(retired[tid].ui);
		HazardInfo* info = (HazardInfo*) (ptr + 1);
		info->next = *field;
		*field = info;
		if (collect && cntrs[tid]==freq){
			cntrs[tid]=0;
			empty(tid);
		}
		cntrs[tid].ui++;
	}
	

	bool collecting(){return collect;}
	
};


#endif
