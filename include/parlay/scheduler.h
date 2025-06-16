
#ifndef PARLAY_SCHEDULER_H_
#define PARLAY_SCHEDULER_H_

#include <cassert>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <chrono>         // IWYU pragma: keep
#include <memory>
#include <thread>
#include <type_traits>    // IWYU pragma: keep
#include <utility>
#include <vector>
#include <fstream>

#include "internal/work_stealing_deque.h"         // IWYU pragma: keep
#include "internal/work_stealing_job.h"

// IWYU pragma: no_include <bits/chrono.h>
// IWYU pragma: no_include <bits/this_thread_sleep.h>

// #define _GNU_SOURCE
#include <sys/time.h>
#include <sys/resource.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"



// True if the scheduler should scale the number of awake workers
// proportional to the amount of work to be done. This saves CPU
// time if there is not any parallel work available, but may cause
// some startup lag when more parallelism becomes available.
//
// Default: true
#ifndef PARLAY_ELASTIC_PARALLELISM
#define PARLAY_ELASTIC_PARALLELISM true
#endif


// PARLAY_ELASTIC_STEAL_TIMEOUT sets the number of microseconds
// that a worker will attempt to steal jobs, such that if no
// jobs are successfully stolen, it will go to sleep.
//
// Default: 10000 (10 milliseconds)
#ifndef PARLAY_ELASTIC_STEAL_TIMEOUT
#define PARLAY_ELASTIC_STEAL_TIMEOUT 10000
#endif


#if PARLAY_ELASTIC_PARALLELISM
#include "internal/atomic_wait.h"
#endif

namespace parlay {

enum Proc_current_state
{
  WORKING,
  STEALING,
  SLEEPING
};

template <typename Job>
struct scheduler {

  using worker_id_type = unsigned int;

 private:
  static_assert(std::is_invocable_r_v<void, Job&>);

  struct workerInfo {
    static constexpr worker_id_type UNINITIALIZED = std::numeric_limits<worker_id_type>::max();

    worker_id_type worker_id;
    scheduler* my_scheduler;

    workerInfo() : worker_id(UNINITIALIZED), my_scheduler(nullptr) {}
    workerInfo(std::size_t worker_id_, scheduler* s) : worker_id(worker_id_), my_scheduler(s) {}

    workerInfo& operator=(const workerInfo&) = delete;
    workerInfo(const workerInfo&) = delete;

    workerInfo& operator=(workerInfo&& w) noexcept {
      if (this != &w) {
        worker_id = std::exchange(w.worker_id, UNINITIALIZED);
        my_scheduler = std::exchange(w.my_scheduler, nullptr);
      }
      return *this;
    }

    workerInfo(workerInfo&& w) noexcept { *this = std::move(w); }
  };

  // After YIELD_FACTOR * P unsuccessful steal attempts, a
  // a worker will sleep briefly for SLEEP_FACTOR * P nanoseconds
  // to give other threads a chance to work and save some cycles.
  constexpr static size_t YIELD_FACTOR = 200;
  constexpr static size_t SLEEP_FACTOR = 200;

  // The length of time that a worker must fail to steal anything
  // before it goes to sleep to save CPU time.
  constexpr static std::chrono::microseconds STEAL_TIMEOUT{PARLAY_ELASTIC_STEAL_TIMEOUT};

  static inline thread_local workerInfo worker_info{};

 public:

  const worker_id_type num_threads;

  // If the current thread is a worker of an existing scheduler, or the thread that spawned
  // a scheduler, return the most recent such scheduler.  Otherwise, returns null.
  static scheduler* get_current_scheduler() {
    return worker_info.my_scheduler;
  }

  explicit scheduler(size_t num_workers)
      : num_threads(num_workers),
        num_deques(num_threads),
        num_awake_workers(num_threads),
        parent_worker_info(std::exchange(worker_info, workerInfo{0, this})),
        deques(num_deques),
        attempts(num_deques),
        spawned_threads(),
        finished_flag(false){

    // logger_st_pair = spdlog::basic_logger_mt("st_pair_logger", "logs/st_pair.txt");
    logger_w_pair = spdlog::basic_logger_mt("w_pair_logger", "logs/w_pair.txt");    

    arr_proc_state_info = (proc_state_info*)malloc(num_workers*sizeof(proc_state_info));
    
    arr_proc_state_info[0].current_state = -1;
    arr_proc_state_info[0].tot_working_time = 0;
    arr_proc_state_info[0].tot_stealing_time = 0;
    arr_proc_state_info[0].tot_sleeping_time = 0;
    arr_proc_state_info[0].attempt_steals = 0;
    arr_proc_state_info[0].succ_steals = 0;
    arr_proc_state_info[0].min_st_pair = ULLONG_MAX;
    arr_proc_state_info[0].max_st_pair = 0;
    arr_proc_state_info[0].st_count = 0;
    arr_proc_state_info[0].first_working = true;
    
    start_working();

    // Spawn num_threads many threads on startup
    for (worker_id_type i = 1; i < num_threads; ++i) 
    {
      arr_proc_state_info[i].current_state = -1;
      arr_proc_state_info[i].tot_working_time = 0;
      arr_proc_state_info[i].tot_stealing_time = 0;
      arr_proc_state_info[i].tot_sleeping_time = 0;
      arr_proc_state_info[i].attempt_steals = 0;
      arr_proc_state_info[i].succ_steals = 0;
      arr_proc_state_info[i].min_st_pair = ULLONG_MAX;
      arr_proc_state_info[i].max_st_pair = 0;
      arr_proc_state_info[i].st_count = 0;
      arr_proc_state_info[i].first_working = true;

      spawned_threads.emplace_back([&, i]() {
        worker_info = {i, this};
        start_stealing();
        worker();
      });
    }
  }

  ~scheduler() {
    shutdown();
    stop_working();

    display_proc_times();

    worker_info = std::move(parent_worker_info);
    free(arr_proc_state_info);
  }

  // Push onto local stack.
  void spawn(Job* job) {
    int id = worker_id();
    [[maybe_unused]] bool first = deques[id].push_bottom(job);
#if PARLAY_ELASTIC_PARALLELISM
    if (first) wake_up_a_worker();
#endif
  }

  // Wait until the given condition is true.
  //
  // If conservative, this thread will simply busy wait. Otherwise,
  // it will look for work to steal and keep itself occupied. This
  // can deadlock if the stolen work wants a lock held by the code
  // that is waiting, so avoid that.
  template <typename F>
  void wait_until(F&& done, bool conservative = false) {
    // Conservative avoids deadlock if scheduler is used in conjunction
    // with user locks enclosing a wait.
    if (conservative) {

      while (!done()){}

      stop_stealing();
      start_sleeping();

      std::this_thread::yield();

      stop_sleeping();
      start_stealing();
    }
    // If not conservative, schedule within the wait.
    // Can deadlock if a stolen job uses same lock as encloses the wait.
    else {
      do_work_until(std::forward<F>(done));
    }
  }

  // Pop from local stack.
  Job* get_own_job() {
    auto id = worker_id();
    return deques[id].pop_bottom();
  }

  worker_id_type num_workers() { return num_threads; }
  worker_id_type worker_id() { return worker_info.worker_id; }

  bool finished() const noexcept {
    return finished_flag.load(std::memory_order_acquire);
  }

  int get_proc_current_state()
  {
    size_t id = worker_id();
    return arr_proc_state_info[id].current_state;
  }

  inline void start_working()
  {
    size_t id = worker_id();
    arr_proc_state_info[id].current_state = WORKING;
    arr_proc_state_info[id].curr_state_started_ts = std::chrono::high_resolution_clock::now();

    if(arr_proc_state_info[id].first_working)
    {
      arr_proc_state_info[id].first_working = false;
    }
    else
    {
      auto duration_log_1 = (std::chrono::time_point_cast<std::chrono::nanoseconds>(arr_proc_state_info[id].prev_stop_working_ts)).time_since_epoch();
      unsigned long long int duration_log_1_count = static_cast<unsigned long long int>(duration_log_1.count());
      auto duration_log_2 = (std::chrono::time_point_cast<std::chrono::nanoseconds>(arr_proc_state_info[id].curr_state_started_ts)).time_since_epoch();
      unsigned long long int duration_log_2_count = static_cast<unsigned long long int>(duration_log_2.count());
    
      logger_w_pair->info("{} {}", duration_log_1_count, duration_log_2_count);
    }
  }

  inline void stop_working()
  {
    auto stop_ts = std::chrono::high_resolution_clock::now();
    size_t id = worker_id();
    assert(arr_proc_state_info[id].current_state == WORKING);
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop_ts - arr_proc_state_info[id].curr_state_started_ts);
    arr_proc_state_info[id].tot_working_time += static_cast<unsigned long long int>(duration.count());
    arr_proc_state_info[id].prev_stop_working_ts = stop_ts;
  }

  inline void start_stealing()
  {
    size_t id = worker_id();
    arr_proc_state_info[id].current_state = STEALING;
    arr_proc_state_info[id].curr_state_started_ts = std::chrono::high_resolution_clock::now();
  }

  inline void stop_stealing()
  {
    auto stop_ts = std::chrono::high_resolution_clock::now();
    size_t id = worker_id();
    assert(arr_proc_state_info[id].current_state == STEALING);
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop_ts - arr_proc_state_info[id].curr_state_started_ts);
    unsigned long long int st_duration_count = static_cast<unsigned long long int>(duration.count());
    arr_proc_state_info[id].tot_stealing_time += st_duration_count;
    ++arr_proc_state_info[id].st_count;
    
    if(st_duration_count < arr_proc_state_info[id].min_st_pair)
    {
      arr_proc_state_info[id].min_st_pair = st_duration_count; 
    }

    if(st_duration_count > arr_proc_state_info[id].max_st_pair)
    {
      arr_proc_state_info[id].max_st_pair = st_duration_count; 
    }

    auto duration_log_1 = (std::chrono::time_point_cast<std::chrono::nanoseconds>(arr_proc_state_info[id].curr_state_started_ts)).time_since_epoch();
    unsigned long long int duration_log_1_count = static_cast<unsigned long long int>(duration_log_1.count());
    auto duration_log_2 = (std::chrono::time_point_cast<std::chrono::nanoseconds>(stop_ts)).time_since_epoch();
    unsigned long long int duration_log_2_count = static_cast<unsigned long long int>(duration_log_2.count());
    
    // logger_st_pair->info("{} {}", duration_log_1_count, duration_log_2_count);
  }

  inline void start_sleeping()
  {
    size_t id = worker_id();
    arr_proc_state_info[id].current_state = SLEEPING;
    arr_proc_state_info[id].curr_state_started_ts = std::chrono::high_resolution_clock::now();
  }

  inline void stop_sleeping()
  {
    auto stop_ts = std::chrono::high_resolution_clock::now();
    size_t id = worker_id();
    assert(arr_proc_state_info[id].current_state == SLEEPING);
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop_ts - arr_proc_state_info[id].curr_state_started_ts);
    arr_proc_state_info[id].tot_sleeping_time += static_cast<unsigned long long int>(duration.count());
  }

  void display_proc_times()
  {
    unsigned long long int acc_working_time = 0;
    unsigned long long int acc_stealing_time = 0;
    unsigned long long int acc_sleeping_time = 0;

    unsigned long long int acc_stealing_count = 0;
    unsigned long long int acc_succ_st = 0;
    unsigned long long int acc_attempt_st = 0;
    unsigned long long int min_st = ULLONG_MAX;
    unsigned long long int max_st = 0;

    std::ofstream results_all("Results_all.txt");
    std::ofstream results_metrics("Results_metrics.txt");
    std::ofstream par_w_time("Par_w_time.txt");

    // std::cout << "\n";
    for(int i = 0; i<num_threads; ++i)
    {
      acc_working_time += arr_proc_state_info[i].tot_working_time;
      acc_stealing_time += arr_proc_state_info[i].tot_stealing_time;
      acc_sleeping_time += arr_proc_state_info[i].tot_sleeping_time;

      acc_stealing_count += arr_proc_state_info[i].st_count;
      acc_succ_st += arr_proc_state_info[i].succ_steals;
      acc_attempt_st += arr_proc_state_info[i].attempt_steals;

      if(arr_proc_state_info[i].min_st_pair < min_st)
      {
        min_st = arr_proc_state_info[i].min_st_pair;
      }

      if(arr_proc_state_info[i].max_st_pair > max_st)
      {
        max_st = arr_proc_state_info[i].max_st_pair;
      }

      results_all << "Thread " << i << ": " << arr_proc_state_info[i].tot_working_time << ", " << arr_proc_state_info[i].tot_stealing_time << ", " << arr_proc_state_info[i].tot_sleeping_time << "\n";
    }

    double util = acc_working_time/(double)(acc_working_time + acc_stealing_time + acc_sleeping_time);
    double burn_r = (acc_working_time + acc_stealing_time)/(double)(acc_working_time);

    double succ_st_r = 0.0;
    double avg_st_pair = 0.0;

    if(acc_attempt_st != 0)
    {
      succ_st_r = acc_succ_st/(double)(acc_attempt_st);
    }

    if(acc_stealing_count != 0)
    {
      avg_st_pair = acc_stealing_time/(double)(acc_stealing_count);
    }
    else
    {
      min_st = 0;
      max_st = 0;
    }

    results_all << "\nNumber of threads: " << num_threads << "\n\n";
    results_all << "Parallel Working Time: " << acc_working_time << " nanoseconds\n";
    results_all << "Parallel Stealing Time: " << acc_stealing_time << " nanoseconds\n";
    results_all << "Parallel Sleeping Time: " << acc_sleeping_time << " nanoseconds\n";
    results_all << "Utilization: " << util << "\n";
    results_all << "Burn Ratio: " << burn_r << "\n";
    results_all << "Successful Steal Ratio: " << succ_st_r << "\n";
    results_all << "Steal Pair Min: " << min_st << "\n";
    results_all << "Steal Pair Max: " << max_st << "\n";
    results_all << "Steal Pair Avg: " << avg_st_pair << "\n";

    results_all << "num_th, W, St, Sl, Util, Succ_st_r, Burn_r, St_pair_min, St_pair_max, St_pair_avg: " << num_threads << ", " << acc_working_time << ", " << acc_stealing_time << ", " << acc_sleeping_time << ", " << util << ", " << succ_st_r << ", " << burn_r << ", " << min_st << ", " << max_st << ", " << avg_st_pair << "\n";
    results_all << "num_th, Util, Succ_st_r, Burn_r, St_pair_min, St_pair_max, St_pair_avg: " << num_threads << ", " << util << ", " << succ_st_r << ", " << burn_r << ", " << min_st << ", " << max_st << ", " << avg_st_pair << "\n\n";

    results_metrics << num_threads << "," << util << "," << succ_st_r << "," << burn_r << "," << min_st << "," << max_st << "," << avg_st_pair << "\n";
    par_w_time << acc_working_time << "\n";

    results_all.close();
    results_metrics.close();

  }

 private:
  // Align to avoid false sharing.
  struct alignas(128) attempt {
    size_t val;
  };

  int num_deques;
  std::atomic<size_t> num_awake_workers;
  workerInfo parent_worker_info;
  std::vector<internal::Deque<Job>> deques;
  std::vector<attempt> attempts;
  std::vector<std::thread> spawned_threads;
  std::atomic<int> finished_flag;

  std::atomic<size_t> wake_up_counter{0};
  std::atomic<size_t> num_finished_workers{0};

  // struct alignas(64) proc_state_info
  struct proc_state_info
  {
    int current_state; // WORKING, STEALING or SLEEPING
    unsigned long long int tot_working_time;
    unsigned long long int tot_stealing_time;
    unsigned long long int tot_sleeping_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> curr_state_started_ts;
    unsigned long long int succ_steals;
    unsigned long long int attempt_steals;
    unsigned long long int min_st_pair;
    unsigned long long int max_st_pair;
    unsigned long long int st_count;
    std::chrono::time_point<std::chrono::high_resolution_clock> prev_stop_working_ts;
    bool first_working;

    proc_state_info()
    {
      current_state = -1;
      tot_working_time = 0;
      tot_stealing_time = 0;
      tot_sleeping_time = 0;
      curr_state_started_ts = 0;
      succ_steals = 0;
      attempt_steals = 0;
      min_st_pair = ULLONG_MAX;
      max_st_pair = 0;
      st_count = 0;
      prev_stop_working_ts = 0;
      first_working = true;
      // std::cout << "proc_state_info ctor called\n";
    }
  };
  typedef struct proc_state_info proc_state_info;
  proc_state_info* arr_proc_state_info = NULL;

  std::shared_ptr<spdlog::logger> logger_st_pair;
  std::shared_ptr<spdlog::logger> logger_w_pair;

  // Start an individual worker task, stealing work if no local
  // work is available. May go to sleep if no work is available
  // for a long time, until woken up again when notified that
  // new work is available.
  void worker() {
#if PARLAY_ELASTIC_PARALLELISM
    wait_for_work();
#endif
    while (!finished()) {
      Job* job = get_job([&]() { return finished(); }, PARLAY_ELASTIC_PARALLELISM);
      if (job)
      {
        stop_stealing();
        start_working();

        (*job)();

        stop_working();
        start_stealing();
      }
#if PARLAY_ELASTIC_PARALLELISM
      else if (!finished()) {
        // If no job was stolen, the worker should go to
        // sleep and wait until more work is available
        wait_for_work();
      }
#endif
    }
    assert(finished());
    num_finished_workers.fetch_add(1);
  }

  // Runs tasks until done(), stealing work if necessary.
  //
  // Does not sleep or time out since this can be called
  // by the main thread and by join points, for which sleeping
  // would cause deadlock, and timing out could cause a join
  // point to resume execution before the job it was waiting
  // on has completed.
  template <typename F>
  void do_work_until(F&& done) {
    while (true) {
      Job* job = get_job(done, false);  // timeout MUST BE false
      if (!job) return;

      stop_stealing();
      start_working();

      (*job)();

      stop_working();
      start_stealing();
    }
    assert(done());
  }

  // Find a job, first trying local stack, then random steals.
  //
  // Returns nullptr if break_early() returns true before a job
  // is found, or, if timeout is true and it takes longer than
  // STEAL_TIMEOUT to find a job to steal.
  template <typename F>
  Job* get_job(F&& break_early, bool timeout) {
    if (break_early()) return nullptr;
    Job* job = get_own_job();
    if (job) return job;
    else job = steal_job(std::forward<F>(break_early), timeout);
    return job;
  }
  
  // Find a job with random steals.
  //
  // Returns nullptr if break_early() returns true before a job
  // is found, or, if timeout is true and it takes longer than
  // STEAL_TIMEOUT to find a job to steal.
  template<typename F>
  Job* steal_job(F&& break_early, bool timeout) {
    size_t id = worker_id();
    const auto start_time = std::chrono::steady_clock::now();
    do {
      // By coupon collector's problem, this should touch all.
      for (size_t i = 0; i <= YIELD_FACTOR * num_deques; i++) {
        if (break_early()) return nullptr;

        Job* job = try_steal(id);

        ++arr_proc_state_info[id].attempt_steals;

        if (job)
        {
          ++arr_proc_state_info[id].succ_steals;
          return job;
        } 
      }

      stop_stealing();
      start_sleeping();

      std::this_thread::sleep_for(std::chrono::nanoseconds(num_deques * 100));

      stop_sleeping();
      start_stealing();
      
    } while (!timeout || std::chrono::steady_clock::now() - start_time < STEAL_TIMEOUT);
    return nullptr;
  }

  Job* try_steal(size_t id) {
    // use hashing to get "random" target
    size_t target = (hash(id) + hash(attempts[id].val)) % num_deques;
    attempts[id].val++;
    auto [job, empty] = deques[target].pop_top();
#if PARLAY_ELASTIC_PARALLELISM
    if (!empty) wake_up_a_worker();
#endif
    return job;
  }

#if PARLAY_ELASTIC_PARALLELISM

  // Wakes up at least one sleeping worker (more than one
  // worker may be woken up depending on the implementation).
  void wake_up_a_worker() {
    if (num_awake_workers.load(std::memory_order_acquire) < num_threads) {
      wake_up_counter.fetch_add(1);
      parlay::atomic_notify_one(&wake_up_counter);
    }
  }
  
  // Wake up all sleeping workers
  void wake_up_all_workers() {
    if (num_awake_workers.load(std::memory_order_acquire) < num_threads) {
      wake_up_counter.fetch_add(1);
      parlay::atomic_notify_all(&wake_up_counter);
    }
  }
  
  // Wait until notified to wake up
  void wait_for_work() {

    // stop_stealing();
    // start_sleeping();

    auto orig = wake_up_counter.load();
    num_awake_workers.fetch_sub(1);

    // DO ONE ROUND OF STEALS
    //   - if found job, then:
    //     increment num_awake_workers, and DON'T atomic_wait. start working on job
    //   - if no job found:
    //     continue to atomic_wait

    size_t id = worker_id();
    Job* job = nullptr;
    for (size_t i = 0; i <= YIELD_FACTOR * num_deques; i++) 
    {
      // if (break_early()) return nullptr;

      job = try_steal(id);

      ++arr_proc_state_info[id].attempt_steals;

      if(job)
      {
        ++arr_proc_state_info[id].succ_steals;
        num_awake_workers.fetch_add(1);

        stop_stealing();
        start_working();

        (*job)();

        stop_working();
        start_stealing();

        break;
      } 
    }

    if(!job)
    {
      stop_stealing();
      start_sleeping();

      parlay::atomic_wait(&wake_up_counter, orig);
      num_awake_workers.fetch_add(1);

      stop_sleeping();
      start_stealing();
    }
    
  }

#endif

  size_t hash(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return static_cast<size_t>(x);
  }
  
  void shutdown() {
    finished_flag.store(true, std::memory_order_release);
#if PARLAY_ELASTIC_PARALLELISM
    // We must spam wake all workers until they finish in
    // case any of them are just about to fall asleep, since
    // they might therefore miss the flag to finish
    while (num_finished_workers.load() < num_threads - 1) {
      wake_up_all_workers();
      std::this_thread::yield();
    }
#endif
    for (worker_id_type i = 1; i < num_threads; ++i) {
      spawned_threads[i - 1].join();
    }
  }
};


class fork_join_scheduler {
  using Job = WorkStealingJob;
  using scheduler_t = scheduler<Job>;

 public:

  // Fork two thunks and wait until they both finish.
  template <typename L, typename R>
  static void pardo(scheduler_t& scheduler, L&& left, R&& right, bool conservative = false) 
  {
    assert(scheduler.get_proc_current_state() == WORKING);

    auto execute_right = [&]() { std::forward<R>(right)(); };
    auto right_job = make_job(right);
    scheduler.spawn(&right_job);

    struct rusage usage;
    struct timeval start_utime, end_utime;
    struct timeval start_stime, end_stime;
    size_t duration_utime_w = 0;
    size_t duration_stime_w = 0;

    std::forward<L>(left)();

    if (const Job* job = scheduler.get_own_job(); job != nullptr) 
    {
      assert(job == &right_job);
      execute_right();
    }
    else 
    {
      auto done = [&]() { return right_job.finished(); };

      scheduler.stop_working();
      scheduler.start_stealing();

      scheduler.wait_until(done, conservative);

      scheduler.stop_stealing();
      scheduler.start_working();

      assert(right_job.finished());
    }
  }

  template <typename F>
  static void parfor(scheduler_t& scheduler, size_t start, size_t end, F&& f, size_t granularity = 0, bool conservative = false) {
    if (end <= start) return;
    if (granularity == 0) {
      size_t done = get_granularity(start, end, f);
      granularity = std::max(done, (end - start) / static_cast<size_t>(128 * scheduler.num_threads));
      start += done;
    }
    parfor_(scheduler, start, end, f, granularity, conservative);
  }

 private:
  template <typename F>
  static size_t get_granularity(size_t start, size_t end, F& f) {
    size_t done = 0;
    size_t sz = 1;
    unsigned long long int ticks = 0;
    do {
      sz = std::min(sz, end - (start + done));
      auto tstart = std::chrono::steady_clock::now();
      for (size_t i = 0; i < sz; i++) f(start + done + i);
      auto tstop = std::chrono::steady_clock::now();
      ticks = static_cast<unsigned long long int>(std::chrono::duration_cast<
                std::chrono::nanoseconds>(tstop - tstart).count());
      done += sz;
      sz *= 2;
    } while (ticks < 1000 && done < (end - start));
    return done;
  }

  template <typename F>
  static void parfor_(scheduler_t& scheduler, size_t start, size_t end, F& f, size_t granularity, bool conservative) {
    if ((end - start) <= granularity)
      for (size_t i = start; i < end; i++) f(i);
    else {
      size_t n = end - start;
      // Not in middle to avoid clashes on set-associative caches on powers of 2.
      size_t mid = (start + (9 * (n + 1)) / 16);
      pardo(scheduler,
            [&]() { parfor_(scheduler, start, mid, f, granularity, conservative); },
            [&]() { parfor_(scheduler, mid, end, f, granularity, conservative); },
            conservative);
    }
  }

};

}  // namespace parlay

#endif  // PARLAY_SCHEDULER_H_
