/*
 * Copyright (c) Radzivon Bartoshyk. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3.  Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
use jxl::api::{JxlParallelRunner, JxlParallelRunnerFun};
use jxl::error::Result;
use std::panic::{catch_unwind, resume_unwind, AssertUnwindSafe};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::thread;

/// A decode-local scheduler. Each batch joins its scoped workers before returning,
/// so no thread or borrowed decoder data can escape the decoding call.
pub(crate) struct JxlThreadRunner {
    // Includes the calling thread, which also consumes tasks.
    max_threads: usize,
}

impl Default for JxlThreadRunner {
    fn default() -> Self {
        Self {
            max_threads: thread::available_parallelism()
                .map_or(1, |count| count.get())
                .max(1),
        }
    }
}

impl JxlThreadRunner {
    fn run_with_threads(
        &mut self,
        num: usize,
        max_threads: usize,
        fun: &JxlParallelRunnerFun<'_>,
    ) -> Result<()> {
        let num_threads = self.max_threads.min(max_threads).min(num);
        if num_threads <= 1 {
            for index in 0..num {
                fun(index)?;
            }
            return Ok(());
        }

        let next = AtomicUsize::new(0);
        let stopped = AtomicBool::new(false);
        let work = || {
            // Catch on both the calling thread and workers, so cancellation and
            // explicit joins happen before any panic resumes on the caller.
            let outcome = catch_unwind(AssertUnwindSafe(|| {
                while !stopped.load(Ordering::Relaxed) {
                    let index = next.fetch_add(1, Ordering::Relaxed);
                    if index >= num {
                        break;
                    }
                    fun(index)?;
                }
                Ok(())
            }));
            if !matches!(outcome, Ok(Ok(()))) {
                stopped.store(true, Ordering::Relaxed);
            }
            outcome
        };

        thread::scope(|scope| {
            let mut workers = Vec::with_capacity(num_threads - 1);
            let mut outcomes = Vec::with_capacity(num_threads);
            for _ in 1..num_threads {
                match spawn_worker(scope, &work) {
                    Ok(worker) => workers.push(worker),
                    // If the OS cannot create more threads, the existing workers
                    // and caller can still finish the queue without losing jobs.
                    Err(_) => break,
                }
            }

            outcomes.push(work());
            for worker in workers {
                outcomes.push(worker.join().unwrap_or_else(Err));
            }

            // Implicit scope cleanup can finish before thread-local destructors.
            // Every OS thread has now been explicitly joined, including when the
            // caller or several workers failed. Only now propagate failures.
            let mut first_error = None;
            let mut first_panic = None;
            for outcome in outcomes {
                match outcome {
                    Ok(Ok(())) => {}
                    Ok(Err(error)) => {
                        first_error.get_or_insert(error);
                    }
                    Err(panic) => {
                        first_panic.get_or_insert(panic);
                    }
                }
            }
            if let Some(panic) = first_panic {
                resume_unwind(panic);
            }
            first_error.map_or(Ok(()), Err)
        })
    }
}

fn spawn_worker<'scope, 'env, F, T>(
    scope: &'scope thread::Scope<'scope, 'env>,
    work: F,
) -> std::io::Result<thread::ScopedJoinHandle<'scope, T>>
where
    F: FnOnce() -> T + Send + 'scope,
    T: Send + 'scope,
{
    #[cfg(test)]
    tests::check_spawn_budget()?;
    thread::Builder::new().spawn_scoped(scope, work)
}

impl JxlParallelRunner for JxlThreadRunner {
    fn run(&mut self, num: usize, fun: &JxlParallelRunnerFun<'_>) -> Result<()> {
        self.run_with_threads(num, self.max_threads, fun)
    }

    fn run_ordered(
        &mut self,
        num: usize,
        max_threads: Option<usize>,
        fun: &JxlParallelRunnerFun<'_>,
    ) -> Result<()> {
        // Share cancellation at the individual-job level. The default trait
        // implementation wraps jobs in loops that cannot observe our stop flag.
        self.run_with_threads(num, max_threads.unwrap_or(self.max_threads), fun)
    }

    fn num_threads(&self) -> usize {
        self.max_threads
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use jxl::error::Error;
    use std::cell::{Cell, RefCell};
    use std::panic::{catch_unwind, AssertUnwindSafe};
    use std::sync::{mpsc, Arc, Condvar, Mutex};
    use std::time::Duration;

    const DEADLINE: Duration = Duration::from_secs(5);

    thread_local! {
        static SPAWN_BUDGET: Cell<Option<usize>> = const { Cell::new(None) };
    }

    pub(super) fn check_spawn_budget() -> std::io::Result<()> {
        SPAWN_BUDGET.with(|budget| match budget.get() {
            Some(0) => Err(std::io::Error::other("injected thread creation failure")),
            Some(left) => {
                budget.set(Some(left - 1));
                Ok(())
            }
            None => Ok(()),
        })
    }

    #[derive(Default)]
    struct Gate(Mutex<bool>, Condvar);

    impl Gate {
        fn wait(&self) {
            let (open, _) = self
                .1
                .wait_timeout_while(self.0.lock().unwrap(), DEADLINE, |open| !*open)
                .unwrap();
            assert!(*open, "test gate timed out");
        }

        fn open(&self) {
            *self.0.lock().unwrap() = true;
            self.1.notify_all();
        }
    }

    struct OpenOnDrop(Arc<Gate>);

    impl Drop for OpenOnDrop {
        fn drop(&mut self) {
            self.0.open();
        }
    }

    struct WorkerExit(Arc<AtomicUsize>);

    impl Drop for WorkerExit {
        fn drop(&mut self) {
            self.0.fetch_add(1, Ordering::SeqCst);
        }
    }

    thread_local! {
        static WORKER_EXIT: RefCell<Option<WorkerExit>> = const { RefCell::new(None) };
    }

    #[test]
    fn processes_borrowed_tasks_exactly_once() {
        for max_threads in [1, 2, 4] {
            let mut runner = JxlThreadRunner { max_threads };
            for count in [0, 1, 2, 257] {
                let visits: Vec<_> = (0..count).map(|_| AtomicUsize::new(0)).collect();
                runner
                    .run(count, &|index| {
                        visits[index].fetch_add(1, Ordering::Relaxed);
                        Ok(())
                    })
                    .unwrap();
                assert!(visits
                    .iter()
                    .all(|visits| visits.load(Ordering::Relaxed) == 1));
            }
        }
    }

    #[test]
    fn ordered_runner_honors_single_thread_hint() {
        let caller = thread::current().id();
        let mut runner = JxlThreadRunner { max_threads: 4 };
        let next = AtomicUsize::new(0);
        runner
            .run_ordered(257, Some(1), &|index| {
                assert_eq!(thread::current().id(), caller);
                assert_eq!(next.fetch_add(1, Ordering::Relaxed), index);
                Ok(())
            })
            .unwrap();
        assert_eq!(next.load(Ordering::Relaxed), 257);
    }

    #[test]
    fn workers_exit_before_success_error_or_panic_returns() {
        let caller = thread::current().id();
        let mut runner = JxlThreadRunner { max_threads: 4 };
        for outcome in 0..3 {
            let started = AtomicUsize::new(0);
            let exited = Arc::new(AtomicUsize::new(0));
            let result = catch_unwind(AssertUnwindSafe(|| {
                runner.run(257, &|index| {
                    if thread::current().id() != caller {
                        WORKER_EXIT.with(|slot| {
                            let mut slot = slot.borrow_mut();
                            if slot.is_none() {
                                started.fetch_add(1, Ordering::SeqCst);
                                *slot = Some(WorkerExit(exited.clone()));
                            }
                        });
                    }
                    if index == 128 {
                        match outcome {
                            1 => return Err(Error::InvalidSignature),
                            2 => panic!("test decoder panic"),
                            _ => {}
                        }
                    }
                    Ok(())
                })
            }));
            match outcome {
                0 => assert!(matches!(result, Ok(Ok(())))),
                1 => assert!(matches!(result, Ok(Err(Error::InvalidSignature)))),
                2 => assert!(result.is_err()),
                _ => unreachable!(),
            }
            assert_eq!(
                started.load(Ordering::SeqCst),
                exited.load(Ordering::SeqCst)
            );
            assert_eq!(Arc::strong_count(&exited), 1);
        }
    }

    #[derive(Clone, Copy)]
    enum Fault {
        None,
        Error,
        Panic,
    }

    #[test]
    fn overlapping_caller_and_worker_failures_propagate_without_hanging() {
        for ordered in [false, true] {
            for (caller_fault, worker_fault) in [
                (Fault::Error, Fault::None),
                (Fault::None, Fault::Error),
                (Fault::Error, Fault::Error),
                (Fault::Panic, Fault::None),
                (Fault::None, Fault::Panic),
                (Fault::Error, Fault::Panic),
                (Fault::Panic, Fault::Panic),
            ] {
                let gate = Arc::new(Gate::default());
                let _release = OpenOnDrop(gate.clone());
                let worker_gate = gate.clone();
                let (started_tx, started_rx) = mpsc::channel();
                let (result_tx, result_rx) = mpsc::channel();
                let handle = thread::spawn(move || {
                    let caller = thread::current().id();
                    let mut runner = JxlThreadRunner { max_threads: 3 };
                    let result = catch_unwind(AssertUnwindSafe(|| {
                        let work = |_: usize| {
                            let is_caller = thread::current().id() == caller;
                            started_tx.send(()).unwrap();
                            // The test controller releases all three independent
                            // callbacks; waits are bounded even if a regression occurs.
                            worker_gate.wait();
                            match if is_caller {
                                caller_fault
                            } else {
                                worker_fault
                            } {
                                Fault::None => Ok(()),
                                Fault::Error => Err(Error::InvalidSignature),
                                Fault::Panic => {
                                    std::panic::panic_any(String::from("decoder worker panic"))
                                }
                            }
                        };
                        if ordered {
                            runner.run_ordered(3, Some(3), &work)
                        } else {
                            runner.run(3, &work)
                        }
                    }));
                    // The same local runner must remain usable after a failed batch.
                    runner.run(7, &|_| Ok(())).unwrap();
                    result_tx.send(result).unwrap();
                });
                for _ in 0..3 {
                    started_rx.recv_timeout(DEADLINE).unwrap();
                }
                gate.open();
                let result = result_rx
                    .recv_timeout(DEADLINE)
                    .expect("runner did not finish");
                handle.join().unwrap();
                if matches!(caller_fault, Fault::Panic) || matches!(worker_fault, Fault::Panic) {
                    let panic = result.expect_err("panic was swallowed");
                    assert_eq!(
                        panic.downcast_ref::<String>().unwrap(),
                        "decoder worker panic"
                    );
                } else {
                    assert!(matches!(result, Ok(Err(Error::InvalidSignature))));
                }
            }
        }
    }

    struct ShutdownProbe(mpsc::Sender<()>, Arc<Gate>);

    impl Drop for ShutdownProbe {
        fn drop(&mut self) {
            self.0.send(()).unwrap();
            self.1.wait();
        }
    }

    thread_local! {
        static SHUTDOWN_PROBE: RefCell<Option<ShutdownProbe>> = const { RefCell::new(None) };
    }

    #[test]
    fn caller_panic_waits_for_worker_thread_local_destructors() {
        let work_gate = Arc::new(Gate::default());
        let exit_gate = Arc::new(Gate::default());
        let _release_work = OpenOnDrop(work_gate.clone());
        let _release_exit = OpenOnDrop(exit_gate.clone());
        let (started_tx, started_rx) = mpsc::channel();
        let (exiting_tx, exiting_rx) = mpsc::channel();
        let (result_tx, result_rx) = mpsc::channel();
        let work = work_gate.clone();
        let exit = exit_gate.clone();
        let handle = thread::spawn(move || {
            let caller = thread::current().id();
            let mut runner = JxlThreadRunner { max_threads: 3 };
            let result = catch_unwind(AssertUnwindSafe(|| {
                runner.run(3, &|_| {
                    let is_caller = thread::current().id() == caller;
                    if !is_caller {
                        SHUTDOWN_PROBE.with(|slot| {
                            *slot.borrow_mut() =
                                Some(ShutdownProbe(exiting_tx.clone(), exit.clone()))
                        });
                    }
                    started_tx.send(()).unwrap();
                    work.wait();
                    assert!(!is_caller, "caller panic");
                    Ok(())
                })
            }));
            result_tx.send(result).unwrap();
        });
        for _ in 0..3 {
            started_rx.recv_timeout(DEADLINE).unwrap();
        }
        work_gate.open();
        for _ in 0..2 {
            exiting_rx.recv_timeout(DEADLINE).unwrap();
        }
        let early = result_rx.recv_timeout(Duration::from_millis(50));
        let waited_for_destructors = matches!(early, Err(mpsc::RecvTimeoutError::Timeout));
        exit_gate.open();
        let result = early.unwrap_or_else(|_| result_rx.recv_timeout(DEADLINE).unwrap());
        handle.join().unwrap();
        assert!(
            waited_for_destructors,
            "runner returned before OS threads terminated"
        );
        assert!(result.is_err());
    }

    #[test]
    fn spawn_failure_falls_back_without_losing_jobs_or_errors() {
        struct ResetBudget;
        impl Drop for ResetBudget {
            fn drop(&mut self) {
                SPAWN_BUDGET.set(None);
            }
        }
        let _reset = ResetBudget;
        let mut runner = JxlThreadRunner { max_threads: 4 };
        for budget in [0, 1] {
            for ordered in [false, true] {
                for fail in [false, true] {
                    SPAWN_BUDGET.set(Some(budget));
                    let visits: Vec<_> = (0..257).map(|_| AtomicUsize::new(0)).collect();
                    let work = |index: usize| {
                        visits[index].fetch_add(1, Ordering::Relaxed);
                        if fail && index == 128 {
                            Err(Error::InvalidSignature)
                        } else {
                            Ok(())
                        }
                    };
                    let result = if ordered {
                        runner.run_ordered(257, None, &work)
                    } else {
                        runner.run(257, &work)
                    };
                    if fail {
                        assert!(matches!(result, Err(Error::InvalidSignature)));
                    } else {
                        result.unwrap();
                        assert!(visits
                            .iter()
                            .all(|count| count.load(Ordering::Relaxed) == 1));
                    }
                }
            }
        }
    }
}
