# Scheduler Design in xv6

## 1. Key Objectives of a CPU Scheduler
- short turnaround time for short tasks
- short response time for recent tasks
- fairness to processes as a resource manager

## 2. Design a CPU scheduler
> As cpu resources cannot be divided into physical areas such as memories, cpu schedulers use the slice of cpu time, called **time quantom**.  
> Also, to get the control back to the kernel against long running processes, most OSs utilize timer interrupts to periodically regain controls over processes.

In summary, designing cpu scheduling is to make a good algorithm that determines which process to deserve a next time quantom whenever a timer interrupt occurs.

```mermaid
sequenceDiagram
    participant K as Kernel
    participant M as swtch
    participant U as User
    loop
        %% interrupts occur
        K-->>K : interrupt from hardware
        U-->>K : interrupt from system call
        U->>K : trap(int)
        K->>M : sched
        activate M
        M->>K : scheduler
        deactivate M
        Note over K: Scheduling occurs
        K->>M : scheduler
        activate M
        M->>K : sched
        deactivate M
        K->>U : return from trap(iret)
    end
```

### [As is] Current xv6 scheduler

#### RR(Round Robin)
> Round Robin scheduling in the xv6 switches the currently running process over to the next runnable process every tick.

Although RR provides powerful responsiveness, it lacks with considerations about turnaround time.

### [To be] MLFQ Scheduler combined with Stride scheduling
To compensate for the deficits of RR scheduling, combine MLFQ scheduling with stride scheduling to achieve our key objectives.

#### A) MLFQ(Multi Level Feedback Queue)
> MLFQ is well known for a scheduling algorithm that satisfies some critical requirements such as short turnaround time and short response time by learning from the past behaviors of the currently running processes.

<details markdown="1">
<summary>Fold/Unfold the state diagram of MLFQ</summary>

```mermaid
flowchart TB
    %% Queues
    Q0[(RR scheduling every 1 tick)]
    Q1[(RR scheduling every 2 ticks)]
    Q2[(RR scheduling every 4 ticks)]

    %% processes
    new_proc((New Process))
    proc1[Process 1]
    proc2[Process 2]
    proc3[Process 3]
    proc4[Process 4]
    proc5[Process 5]
    proc6[Process 6]
    proc7[Process 7]
    proc8[Process 8]
    subgraph Queue0
        direction LR
        Q0
        proc1
        proc2

        Q0 --> proc1
        proc1 --> proc2
        proc2 --> Q0
    end
    subgraph Queue1
        direction LR
        Q1
        proc3
        proc4
        proc5

        Q1 --> proc3
        proc3 --> proc4
        proc4 --> proc5
        proc5 --> Q1
    end
    subgraph Queue2
        direction LR
        Q2
        proc6
        proc7
        proc8

        Q2 --> proc6
        proc6 --> proc7
        proc7 --> proc8
        proc8 --> Q2
    end

    new_proc -- a new process comes in --> Q0
    Q0 -- if consumed ticks > 5 --> Q1
    Q1 -- if consumed ticks > 10 --> Q2
```
</details>

- 3-level feed back queue
  - Q0 -> Q1 -> Q2
  - if Q0 is empty, find a process in Q1. Otherwise, round robin the Q0 and lower the priority of processes until its time allotment depletes.
  - if Q1 is empty, find a process in Q2. Otherwise, round robin the Q1 and lower the priority of processes until its time allotment depletes.
  - Round-robin the Q2 until new process comes in or the periodic priority boost occurs.
- Each level of queue adopts a Round Robin policy with different time quantum
  - Q0: 1 tick
  - Q1: 2 ticks
  - Q2: 4 ticks
- Each queue has a different time allotment
  - Q0: 5 ticks
  - Q1: 10 ticks
- To prevent starvation, priority boost is required
  - every 100 ticks
- To achieve the purpose of MLFQ, every tick in MLFQ is measured by a local tick count of MLFQ
  - `uint mlfq_ticks;`
- To test various scheduling scenarios and check the behaviors of MLFQ, some system calls are requried to implement.
  - `int yield(void)`: to try to game a xv6 cpu scheduler, this makes an option to voluntarily give up cpu resource.
  - `int getlev(void)`: to check at which level it is running in MLFQ, this can help us too.
- Implementaion Details
  - Metadata for MLFQ
    - To implement MLFQ in xv6, add extra metadata fields into `struct proc`
      - To save the size of proc struct, use lev for 28 bits and yield_by for 4 bits
    ```c
    struct proc { // in proc.h
        ...
        uint cticks;                 // a consumed tick count at the given level of the queue
        uint yield_by :  4;          // if the process is yield by stride = 1, mlfq = 2
        uint lev      : 28;          // Level in MLFQ(0~NMLFQ), otherwise Stride Level
        ...
    };
    ```
    - Add a consumed tick counts(**cticks**) field of a process at the given level of the queue into scheduliing process block(sproc)
    - Add a level(**lev**) field in the MLFQ
    - Then, an implementation of `int getlev(void)` can become much simpler.
    ```c
    int get_lev(void){
        struct proc *p = myproc();
        acquire(&ptable.lock);
        uint lev = p->lev;          // Fetch the priority level of MLFQ
        release(&ptable.lock);
        if(0 <= lev && lev < NMLFQ) // Check whether it is in MLFQ
          return lev;
        return -1;
    }
    ```
    - Increase tick counter(cticks) as the timer interrupt happens
    - Lower the priority if the process exhausts its tick allotment
    - Yield if the current process exhausts its time quantom
    ```c
    void
    trap(struct trapframe *tf) // in trap.c
    {
        ...

        // Force process to give up CPU on clock tick.
        // If interrupts were on while locks held, would need to check nlock.
        if(myproc() && myproc()->state == RUNNING &&
            tf->trapno == T_IRQ0+IRQ_TIMER) {
            if(stride_has_to_yield(myproc()) || // If the stride scheduler needs to yield, yield.
            (is_mlfq(myproc()) && mlfq_has_to_yield(myproc()))) { // if the MLFQ needs to yield, yield.
            yield();
          }
        }

        ...
    }
    ```

    - checks whether the MLFQ has to yield or not and increase `mlfq_tick` counts together
    ```c
    int
    mlfq_has_to_yield(struct proc *p)
    {
      static int mlfq_ticks_elapsed; // internal counter for mlfq

      const uint MLFQ_MAX_TICK_OF_CURRENT_PROC = MLFQ_MAX_TICKS[getlev()];

      if(mlfq_ticks_elapsed >= MLFQ_MAX_TICK_OF_CURRENT_PROC) {
        mlfq_ticks_elapsed = 0;
      }
      mlfq_ticks_elapsed++; // Increase the time quantom for this queue

      acquire(&mlfqlock);
      mlfq_ticks += 1; // Increase the MLFQ tick count
      release(&mlfqlock);

      acquire(&ptable.lock);
      p->cticks += 1; // Increase the process's tick count
      if(mlfq_ticks_elapsed >= MLFQ_MAX_TICK_OF_CURRENT_PROC)
        p->yield_by = 2;
      release(&ptable.lock);

      return mlfq_ticks_elapsed >= MLFQ_MAX_TICK_OF_CURRENT_PROC;
    }
    ```

#### B) Stride Scheduling
> Stride Scheduling is amazingly powerful to ensure the fairness due to its deterministic characteristics compared to lottery scheduling. If a small set of tasks are scheduled, that deterministic behaviors can show better performance than those nondeterministic.

<details markdown="1">
<summary>Fold/Unfold the diagram of Stride scheduling</summary>

![Stride Scheduling](https://d3i71xaburhd42.cloudfront.net/1c941759aa796d89909541dd5d13656b0cf133c8/4-Figure2-1.png)

</details>

- In the stride scheduling, a process can request OS how much it wants to share the cpu by calling a system call.
  - With our design, that system call is `int set_cpu_share(int)`.
  - Say that a process wants to hold 40% of the cpu share, calling `set_cpu_share(40)` is enough.
- Implementation Details
  - Use a priority queue that can
    - add a node with a priority value
    - remove a specific node by `struct proc*`
    - modify a priority value in that queue
    - check emptyness and the size
    - check whether a given process(`struct proc*`) is in the queue
  - Metadata for Stride
    - Make a fraction structure(`struct frac`)
    ```c
    struct frac { // fraction structure
      uint num;
      uint denom;
    };
    ```
    - Make a `StrideItem struct` to store passes and shares of a given process and MLFQ
    ```c
    struct StrideItem { // in stride_scheduler.h
      frac pass;       // pass for Stride
      uint isMLFQ : 1; // is mlfq = 1, ow = 0
      int share   : 31;// stride for Stride
      void *proc;      // pointer to struct proc
    };
    ```

  - psuedo code
    - Existing Process in MLFQ wants to register: set_cpu_share()
    ```c
    int set_cpu_share(int share){
        ...
        acquire(&ptable.lock);
        if(is_mlfq(p)) { // where p is current process
          // Initialize the stride item
          p->lev = STRIDE_PROC_LEVEL;
          p->cticks = 0;
          stride_item_init(&stride_item, share, p, 0);
          
          ret = stride_push(&stride_item)); // Push it to the Stride queue

          release(&ptable.lock); // Release the ptable lock for yield
          yield(); // Yield the cpu for rescheduling
          return ret;
        } else if(is_stride(p)) {
          // Find the stride item and adjust the share value
          ret = stride_adjust(stride_find_item(p), share);
        } else {
          ret = -1;
        }
        release(&ptable.lock);
        return ret;
    }
    ```

    - Process Exchange: scheduler()
    ```c
    void stride_scheduler(struct cpu *c){
        next_process = stride_top();                         // Fetch a Runnable process

        if(next_process->isMLFQ){
          // Do MLFQ
          mlfq_scheduler(c);
        }else{
          if(next_process->state != RUNNABLE)
            goto stride_pass;
          swtch(&(c->scheduler), p->context);       // Context Switch
        }
      stride_pass:
        next_process = SQ.pop();                    // Pop a RUNNABLE process with the minimum priority value (pass)
        ...
        next_process->state = RUNNING;              // Change its process state to RUNNING
        swtch(&(cpu->scheduler), next_process);     // Switch to another stack and then return
        ...
        next_process->pass += next_process->stride; // Increment pass value by its stride
        stride->max_pass = max(stride->max_pass, next_process->pass) // Update the max pass
        if(next_process->isMLFQ || next_process->state != ZOMBIE)    // Push mlfq always and non-zombie processes
          SQ.push(&next_process);                   // Put it back with modfied priority value (pass)
    }
    ```

    - checks whether the stride queue has to yield or not and increase `stride_ticks` counts together
    ```c
    int
    stride_has_to_yield(struct proc *p)
    {
      static uint stride_ticks_elapsed; // internal counter for stride

      if(stride_ticks_elapsed >= STRIDE_MAX_TICKS)
        stride_ticks_elapsed = 0;

      acquire(&stride_lock);
      stride_ticks += 1; // Increase the Stride tick count
      release(&stride_lock);

      stride_ticks_elapsed++; // Increase the time quantom the current process can use

      acquire(&ptable.lock);
      p->cticks += 1; // Increase the process's tick count
      if(stride_ticks_elapsed >= STRIDE_MAX_TICKS)
        p->yield_by = 1; // Mark this field to 1 for Stride Scheduler to switch up
      release(&ptable.lock);

      return stride_ticks_elapsed >= STRIDE_MAX_TICKS;
    }
    ```

#### C) Combine the stride scheduling algorithm with MLFQ

To combine the stride scheduling algorithm with MLFQ, regard MLFQ as a scheduling item in the stride scheduling that has at least 20% share.

#### D) xv6 Process State Transition Diagram

To make the most of xv6, our scheduler must have to recognize the state of each process for managing its data structures correctly.

```mermaid
stateDiagram
    UNUSED --> EMBRYO    : allocproc
    EMBRYO --> UNUSED    : fail to fork or kalloc
    EMBRYO --> RUNNABLE  : fork, userinit
    RUNNABLE --> RUNNING : scheduler
    RUNNING --> RUNNABLE : yield
    RUNNING --> ZOMBIE   : exit, trap(return 0)
    RUNNING --> SLEEPING : sleep
    SLEEPING --> RUNNABLE: wakeup, kill
    ZOMBIE --> UNUSED    : wait, exit
```
To handle the process management in each scheduling,
 - I inserted the process insertion code at `fork` and `userinit` in proc.c
 - I inserted the process removal code at `scheduler` in proc.c

## 3. Required System Calls

### 3-1. yield: yield the cpu to the next process
- Wrapper: `int sys_yield(void)`
- System Call: `int yield(void)`
- Return Value: 0

### 3-2. getlev: get the level of the current process in the MLFQ.
- Wrapper: `int sys_getlev(void)`
- System Call: `int getlev(void)`
- Return Value: the level(0, 1, 2) of MLFQ queue where the currently running process stays, otherwise a negative number

### 3-3. set_cpu_share: inquires to obtain a cpu share
- Wrapper: `int sys_set_cpu_share(void)`
- System Call: `int set_cpu_share(int)`
- Return Value: 0 if successful, otherwise a negative number

## 4. Verify the scheduler
### 10 Scheduling Scenarios

- The workloads were tested 5 times each on Ubuntu 20.04 in ssh
- To testify the scheduler properly, at least 5 processes were used
- The charts and data were plotted from matplotlib.pyplot

#### 1. 10 MLFQ processes
![Workload 0](project1/workload0.png)
Since MLFQ processes should be given a cpu equally, the exceptation matches the results.

#### 2. 5 MLFQ processes counting levels
![Workload 1](project1/workload1.png)
|name|     type|  cnt| lev0| lev1| lev2|measured percent|
| -  |    -    |  -  |  -  |  -  |  -  |       -        |
|MLfQ|  compute|87375|11196|23006|53173|       19.630817|
|MLfQ|  compute|89078|11941|23464|53673|       20.013435|
|MLfQ|  compute|89112|11947|23482|53683|       20.021074|
|MLfQ|  compute|89613|12547|23436|53630|       20.133636|
|MLfQ|  compute|89913|12829|23468|53616|       20.201038|
The ratio of lev0 and lev1 is almost 1:2, since the lev0 has 5ticks quantom and lev1 has 10ticks quantom.

#### 3. 10 MLFQ processes yielding repeatedly
![Workload 2](project1/workload2.png)
This is similar as when 10 MLFQ process are running, but it has lesser counts due to yielding.

#### 4. 5 MLFQ processes not only yielding repeatedly, but also counting levels
![Workload 3](project1/workload3.png)
It is a lock consuming the overall performance which both yield syscall and `getlev` have.
|name| type|  cnt|lev0|lev1| lev2|measured percent|
| -  |  -  |  -  | -  |  - |  -  |       -        |
|MLfQ|yield|32464|3396|7612|21456|       19.165693|
|MLfQ|yield|33358|4210|7690|21458|       19.693481|
|MLfQ|yield|33853|4303|7435|22115|       19.985713|
|MLfQ|yield|34497|4779|8258|21460|       20.365910|
|MLfQ|yield|35214|4940|8819|21455|       20.789203|
Compared to workload 2, the processes get smaller ticks, but the ratio stays almost same.

#### 5. 5 MLFQ processes and 5 MLFQ processes yielding repeatedly
![Workload 4](project1/workload4.png)

#### 6. 10 Stride processes(8% share)
![Workload 5](project1/workload5.png)
Every stride process that has 8% share gets an equal share.

#### 7. 8 Stride processes(1%,3%,5%,7%,11%,13%,17%,23% share respectively) and 2 MLFQ processes
![Workload 6](project1/workload6.png)
According to its share, 2 MLFQ processes have 22.134684% share in Stride.

#### 8. 5 Stride processes(5% share), 2 Stride processes(10% share), 1 Stride process(15% share), 1 Stride process(20% share), and MLFQ process
![Workload 7](project1/workload7.png)

#### 9. 2 Stride processes(5% share), 2 Stride processes(10% share), 1 Stride process(20% share) and 5 MLFQ processes
![Workload 8](project1/workload8.png)

### 10. 2 Stride processes(5% share), 1 Stride process(10% share), 1 Stride process(15% share), 1 Stride process(45% share) and 5 MLFQ processes
![Workload 9](project1/workload9.png)

## 5. Coding Conventions

To follow the xv6's coding convention, use **clang-format** to automatically ensure the coding convention already used in xv6 frictionlessly.

## 6. Terminology

- Tick: The time interval between consecutive timer interrupts is called a tick.
  - The default value set for the tick in xv6 is about 10ms.

## 7. Requirements to satisfy

- [ ] The total sum of CPU share requested from the processes in the stride queue cannot exceed 80% of the total CPU time.
- [ ] Exception handling needs to be properly implemented to handle oversubscribed requests.
- [ ] Do not allocate CPU share if the request induces surpass of the CPU share limit.
- [ ] Make a system call (i.e. set_cpu_share) that requests a portion of CPU and guarantees the calling process to be allocated that much of a CPU time.
- [ ] The rest of the CPU time(20%) should run for the MLFQ scheduling which is the default scheduling policy in this project
- [ ] The total amount of stride processes are able to get at most 80% of the CPU time.
Exception handling is required for exceeding requests.