# App 2 scaffold — multi-task scheduling

### Task table

| Task | Function | Period (ms) | WCET measured (µs) | WCET + 30% margin (µs) | Deadline | Priority | Core |
|------|----------|------------:|-------------------:|----------------------:|---------:|---------:|-----:|
| A    | Threat sensor / attitude poll        | 10          | 583                  | 758                     | 10 ms    | 15       | 1    |
| B    | Fire-control guidance update        | 20          | 810                  | 1053                     | 20 ms    | 10       | 1    |
| C    | Telemetry CRC frame assembly        | 50          | 4548                  | 5913                     | 50 ms    | 5        | 1    |
| D    | Mission-log housekeeping        | 100         | 22725                  | 29543                     | 100 ms   | 2        | 1    |

Task  Period   Priority  Heartbeats   WCET(us)  
A     10 ms    15        4299         583       
B     20 ms    10        2150         810       
C     50 ms    5         860          4548      
D     100 ms   2         430          22725     

### Schedulability defense

- Total utilization U = ∑ Cᵢ/Tᵢ

U = 0.0758 + 0.05265 + 0.11826 + 0.29543
U = 0.54214

- Liu-Layland bound for n=4: U ≤ 4(2^(1/4) − 1) = 0.7568

0.54214 < 0.7568

- If U > Liu-Layland: run response-time analysis on task D (lowest priority)
- Conclusion: feasible / infeasible / borderline. State which.

The system is feasible. The measured task set, including the 30% WCET margin, meets the RMS schedulability test.

### Preemption evidence

Add this to one of your task bodies:

```c
int64_t t = esp_timer_get_time();
ESP_LOGI(TAG, "task_a tick t=%lld", t);
```

### Engineering analysis

1. **Priority defense** — explain each priority. RMS says shortest period &rarr; highest priority. Did you follow it?

The priorities follow rate-monotonic scheduling. RMS assigns the highest priority to the task 
with the shortest period because that task has the most frequent deadline.

Task A has the shortest period at 10 ms, so it receives the highest priority, 15. 
This makes sense because the threat sensor / attitude poll provides the most time-sensitive input to the rest of the system.

Task B has a 20 ms period and receives priority 10. It performs the fire-control guidance update, 
which is important but does not need to run as frequently as the raw sensor polling task.

Task C has a 50 ms period and receives priority 5. It assembles telemetry and computes a CRC frame. 
This is important for system reporting and integrity checking, but it is less urgent than the sensor and control-loop tasks.

Task D has the longest period at 100 ms and receives the lowest priority, 2. It handles mission-log housekeeping, 
which is intentionally background work. It can safely be preempted by the faster real-time tasks.

Therefore, the priority order follows RMS:
Task A: 10 ms period  -> priority 15
Task B: 20 ms period  -> priority 10
Task C: 50 ms period  -> priority 5
Task D: 100 ms period -> priority 2

2. **3× WCET stress** — if your highest-priority task's WCET tripled, what's the new U? Is the set still feasible?

The highest-priority task is Task A. Its WCET with 30% margin is 758 µs.

If Task A’s WCET tripled:

C_A = 3(758) = 2274s

The new utilization is:
U = 0.2274 + 0.05265 + 0.11826 + 0.29543
U = 0.69374

Since:
0.69374 < 0.7568

the system is still schedulable under the Liu-Layland bound even if the highest-priority task’s WCET triples.
the task set remains feasible.

3. **Preemption proof** — quote the two timestamps showing preemption.
I (10961) app2: task_b start t=10991861
I (10971) app2: task_a tick t=10998159
I (10981) app2: task_b finish t=11003895
I (10981) app2: task_a tick t=11005402
I (10981) app2: task_b start t=11007907
I (10991) app2: task_a tick t=11018159
I (10991) app2: task_b finish t=11021862

This proves preemption because Task B started first, then Task A ran before Task B finished. 
Since Task A has higher priority than Task B, FreeRTOS preempted Task B when Task A became ready.

