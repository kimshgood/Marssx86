
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
 * Copyright 2009 Furat Afram <fafram@cs.binghamton.edu>
 *
 */

#ifndef _MEMORYSYSTEM_H_
#define _MEMORYSYSTEM_H_

#include <globals.h>
#include <superstl.h>

#include <ptlsim.h>
#include <machine.h>
#include <memoryRequest.h>
#include <controller.h>
#include <interconnect.h>

#include <statsBuilder.h>

#include <ooo-const.h> //by vteori
#include <cpuController.h>

#define DEBUG_MEMORY
//#define DEBUG_WITH_FILE_NAME
#define ENABLE_CHECKS

#ifdef DEBUG_MEMORY
#ifdef DEBUG_WITH_FILE_NAME
#define memdebug(...) if(logable(5)) {			\
    ptl_logfile << __FILE__, ":", __LINE__,":\t",	\
      __VA_ARGS__ ; ptl_logfile.flush(); }
#else
#define memdebug(...) if(logable(5)) {				\
    ptl_logfile << __VA_ARGS__ ; } //ptl_logfile.flush();
#endif
#else
#define memdebug(...) (0)
#endif

#define ENABLE_MEM_REQUEST_HISTORY
#ifdef ENABLE_MEM_REQUEST_HISTORY
#define ADD_HISTORY(req, ...) req->get_history() << __VA_ARGS__
#define ADD_HISTORY_ADD(req) ADD_HISTORY(req, "{+", get_name(), "} ")
#define ADD_HISTORY_REM(req) ADD_HISTORY(req, "{-", get_name(), "} ")
#else
#define ADD_HISTORY(req, ...) (0)
#define ADD_HISTORY_ADD(req) (0)
#define ADD_HISTORY_REM(req) (0)
#endif
#ifndef ENABLE_CHECKS
#undef assert
#define assert(x) (x)
#endif

#define GET_STRINGBUF_PTR(var_name, ...)	\
  stringbuf *var_name = new stringbuf();	\
  *var_name << __VA_ARGS__;			\

#define SET_SIGNAL_CB(name, name_postfix, signal, cb)	\
  {							\
    stringbuf *sg_n = new stringbuf();			\
    *sg_n << name, name_postfix;			\
    signal.set_name(sg_n->buf);				\
    signal.connect(signal_mem_ptr(*this, cb));		\
  }

#define ADD_CACHELINE_INFO(req) req->get_cacheline().push_back(get_line_address(req));
#define ADD_CACHELINE_RESET(req) req->get_cacheline().clear();

namespace Memory {

  class Event : public FixStateListObject
  {
  private:
    Signal *signal_;
    W64    clock_;
    void   *arg_;

  public:
    void init() {
      signal_ = NULL;
      clock_ = -1;
      arg_ = NULL;
    }

    void setup(Signal *signal, W64 clock, void *arg) {
      signal_ = signal;
      clock_ = clock;
      arg_ = arg;
    }

    bool execute() {
      return signal_->emit(arg_);
    }

    W64 get_clock() {
      return clock_;
    }

    ostream& print(ostream& os) const {
      os << "Event< ";
      if(signal_)
		os << "Signal:" << signal_->get_name() << " ";
      os << "Clock:" << clock_ << " ";
      os << "arg:" << arg_ ;
      os << ">" << endl, flush;
      return os;
    }

    bool operator ==(Event &event) {
      if(clock_ == event.clock_)
	return true;
      return false;
    }

    bool operator >(Event &event) {
      if(clock_ > event.clock_)
	return true;
      return false;
    }

    bool operator <(Event &event) {
      if(clock_ < event.clock_)
	return true;
      return false;
    }

    bool operator >=(Event &event) {
      if (clock_ >= event.clock_)
	return true;
      return false;
    }
  };

  static inline ostream& operator <<(ostream& os, const Event& event) {
    return event.print(os);
  }

  struct MemoryInterlockEntry {
    W8 ctx_id;

    void reset() {ctx_id = -1;}

    ostream& print(ostream& os, W64 physaddr) const {
      os << "phys " << (void*)physaddr << ": vcpu " << (int)ctx_id;
      return os;
    }
  };

  struct MemoryInterlockBuffer: public LockableAssociativeArray<W64, MemoryInterlockEntry, 16, 4, 8> { };

  extern MemoryInterlockBuffer interlocks;

  //
  // MemoryHierarchy provides interface with core
  //

  class MemoryHierarchy {
  public:
    MemoryHierarchy(BaseMachine& machine);
    ~MemoryHierarchy(); // release memory for pool

    // check L1 availability
    bool is_cache_available(W8 coreid, W8 threadid, bool is_icache);

    // interface to memory hierarchy
    bool access_cache(MemoryRequest *request);

    // New Core wakeup function that uses Signal of MemoryRequest
    // if Signal is not setup, it uses old wrapper functions
    void core_wakeup(MemoryRequest *request) {
      if(request->get_coreSignal()) {
		request->get_coreSignal()->emit((void*)request);
		return;
      }
    }

    // to remove the requests if rob eviction has occured
    void annul_request(W8 coreid,
		       W8 threadid,
		       int robid,
		       W64 physaddr,
		       bool is_icache,
		       bool is_write);

    void clock();

    void reset();

    // return the number of cycle used to flush the caches
    int flush(uint8_t coreid);

    // for debugging
    void dump_info(ostream& os);
    void print_map(ostream& os);

    // Add event into event queue
    void add_event(Signal *signal, int delay, void *arg);

    MemoryRequest* get_free_request(int id) {
      return requestPool_[id]->get_free_request();
    }

    void set_controller_full(Controller* controller, bool flag);
    void set_interconnect_full(Interconnect* interconnect, bool flag);
    bool is_controller_full(Controller* controller);

    Message* get_message();
    void free_message(Message* msg);

    int get_core_pending_offchip_miss(W8 coreid);

    BaseMachine& get_machine() { return machine_; }

    void add_cpu_controller(Controller* cont) {
      cpuControllers_.push(cont);
    }

    void add_cache_mem_controller(Controller* cont) {
      allControllers_.push(cont);
    }

    void add_interconnect(Interconnect* conn) {
      allInterconnects_.push(conn);
    }

    void setup_full_flags() {
      // Setup the full flags
      cpuFullFlags_.resize(cpuControllers_.count(), false);
      controllersFullFlags_.resize(allControllers_.count(), false);
      interconnectsFullFlags_.resize(allInterconnects_.count(), false);
    }

    bool grab_lock(W64 lockaddr, W8 ctx_id);
    bool probe_lock(W64 lockaddr, W8 ctx_id);
    void invalidate_lock(W64 lockaddr, W8 ctx_id);

    /***** by vteori *****/
    void set_l1_icache_miss(bool l1_icache_miss_) { l1_icache_miss = l1_icache_miss_; }
    void set_l2_icache_miss(bool l2_icache_miss_) { l2_icache_miss = l2_icache_miss_; }
    bool is_l1_icache_miss() { return l1_icache_miss; }
    bool is_l2_icache_miss() { return l2_icache_miss; }
    void set_l1_dcache_miss(int idx, bool l1_dcache_miss_) { l1_dcache_miss[idx] = l1_dcache_miss_; }
    void set_l2_dcache_miss(int idx, bool l2_dcache_miss_) { l2_dcache_miss[idx] = l2_dcache_miss_; }
    bool is_l1_dcache_miss(int idx) { return l1_dcache_miss[idx]; }
    bool is_l2_dcache_miss(int idx) { return l2_dcache_miss[idx]; }
    void set_itlb_miss(bool itlb_miss_) { itlb_miss = itlb_miss_; }
    void set_dtlb_miss(int idx, bool dtlb_miss_) { dtlb_miss[idx] = dtlb_miss_; }
    bool is_itlb_miss() { return itlb_miss; }
    bool is_dtlb_miss(int idx) { return dtlb_miss[idx]; }
	void set_cachelines(int idx, W64 cacheline) { cachelines[idx] = cacheline; }
	void set_l1cachelines(int idx, W64 cacheline) { l1cachelines[idx] = cacheline; }
	void set_l2cachelines(int idx, W64 cacheline) { l2cachelines[idx] = cacheline; }
	W64 get_cachelines(int idx) { return cachelines[idx]; }
	W64 get_l1cachelines(int idx) { return l1cachelines[idx]; }
	W64 get_l2cachelines(int idx) { return l2cachelines[idx]; }
	void set_cacheline_sharing(int idx, bool is_sharing) { cacheline_sharing[idx] = is_sharing; }
	void set_l1cacheline_sharing(int idx, bool is_sharing) { l1cacheline_sharing[idx] = is_sharing; }
	void set_l2cacheline_sharing(int idx, bool is_sharing) { l2cacheline_sharing[idx] = is_sharing; }
	bool get_cacheline_sharing(int idx) { return cacheline_sharing[idx]; }
	bool get_l1cacheline_sharing(int idx) { return l1cacheline_sharing[idx]; }
	bool get_l2cacheline_sharing(int idx) { return l2cacheline_sharing[idx]; }

    void flush_icache_buffer(W8 coreid){
      CPUController *cpuController = (CPUController *)cpuControllers_[coreid];
      assert(cpuController != NULL);
      cpuController->flush_icache_buffer();
    }

    // (Trace)
    W64 get_cacheline(W64 physaddr, W8 coreid){
      CPUController *cpuController = (CPUController *)cpuControllers_[coreid];
      assert(cpuController != NULL);
      return cpuController->get_cacheline(physaddr);
    }

  private:

    // machine
    BaseMachine &machine_;

    // array of caches and memory
    dynarray<Controller*> cpuControllers_;
    dynarray<Controller*> allControllers_;
    dynarray<Interconnect*> allInterconnects_;
    Controller* memoryController_;

    // array to indicate if controller or interconnect buffers
    // are full or not
    dynarray<bool> cpuFullFlags_;
    dynarray<bool> controllersFullFlags_;
    dynarray<bool> interconnectsFullFlags_;
    bool someStructIsFull_;

    // number of cores
    int coreNo_;

    // Request pool
    dynarray<RequestPool*> requestPool_;

    // Message pool
    FixStateList<Message, 128> messageQueue_;

    // Event Queue
    FixStateList<Event, 2048> eventQueue_;

    void sort_event_queue(Event *event);
    void sort_event_queue_tail(Event *event);

    // Temp Stats
    Stats *stats;
	
    /***** by vteori *****/
    // for FMTs
    bool l1_icache_miss;
    bool l2_icache_miss;
    bool l1_dcache_miss[OOO_ROB_SIZE];
    bool l2_dcache_miss[OOO_ROB_SIZE];
    bool itlb_miss;
    bool dtlb_miss[OOO_ROB_SIZE];
	// for traces
	W64 cachelines[OOO_ROB_SIZE];
	W64 l1cachelines[OOO_ROB_SIZE];
	W64 l2cachelines[OOO_ROB_SIZE];
	bool cacheline_sharing[OOO_ROB_SIZE];
	bool l1cacheline_sharing[OOO_ROB_SIZE];
	bool l2cacheline_sharing[OOO_ROB_SIZE];
  };
};

#endif //_MEMORYSYSTEM_H_
