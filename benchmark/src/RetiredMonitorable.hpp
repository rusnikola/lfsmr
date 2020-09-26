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


#ifndef RETIREDMONITORABLE_HPP
#define RETIREDMONITORABLE_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"
#include "MemoryTracker.hpp"

class RetiredMonitorable{
private:
	padded<int64_t>* retired_cnt;
	BaseMT* mem_tracker = NULL;
public:
	RetiredMonitorable(GlobalTestConfig* gtc){
		retired_cnt = new padded<int64_t>[gtc->task_num+gtc->task_stall];
		for (int i=0; i<gtc->task_num+gtc->task_stall; i++){
			retired_cnt[i].ui = 0;
		}
	}

	void setBaseMT(BaseMT* base) {
		mem_tracker = base;
	}

	void collect_retired_size(int64_t size, int tid){
		retired_cnt[tid].ui += size;
	}
	int64_t report_retired(int tid){
		//calling this function at the end of the benchmark
		if (mem_tracker != NULL)
			mem_tracker->lastExit(tid);
		return retired_cnt[tid].ui;
	}
};

#endif
