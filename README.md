# App 2 scaffold — multi-task scheduling

Scaffold level: **~70% complete**.

## What's given

- 4 task skeletons pinned to Core 1, priorities pre-assigned (RMS-style)
- WCET measurement macro: `MEASURE_WCET(max_var, { body })`
- Web monitor with per-task heartbeat counters and live WCET-max display
- Wi-Fi + HTTP server boilerplate (reused from App 1)

## What you implement

1. **Theme rename** — replace `YOURTHEME` everywhere
2. **Four task bodies** — see the comments in each `task_X()` for suggested workloads
3. **WCET measurement** — fill in `task_d` to log all four WCETs to serial periodically
4. **README defense** — see below

## README defense (graded)

Your README must include:

### Task table (mandatory)

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


## How to fail

- Skipping the WCET measurement and writing "the task takes about 1 ms." That's vibes, not engineering.
- Pinning task D to Core 0. That puts it next to Wi-Fi; Wi-Fi will starve it.
- Using `vTaskDelay` instead of `vTaskDelayUntil`. App 3 will teach you why; for App 2, use the latter so periods don't drift.
- Assigning equal priorities to two tasks "to be fair." That's round-robin, not real-time.

## Setup in Wokwi

Same shape as App 1. In a fresh Wokwi ESP-IDF project:

1. Replace `diagram.json`, `wokwi.toml`, `sdkconfig.defaults`, and `main/CMakeLists.txt` with this folder's versions.
2. Place this folder's `main.c` at `main/main.c` (delete Wokwi's `main/src/` folder), or leave `main/src/main.c` and edit `main/CMakeLists.txt` to use `SRCS "src/main.c"` + `INCLUDE_DIRS "src"`.
3. Confirm `wokwi.toml`'s `firmware` / `elf` paths reference `app2_tasks_scheduling` &mdash; that must match the `project(...)` name in the top-level `CMakeLists.txt`.
4. Click &#9654; to build.

**Critical for App 2:** the `REQUIRES esp_wifi esp_event esp_http_server esp_netif nvs_flash` line in this folder's `main/CMakeLists.txt` is what links the HTTP/Wi-Fi APIs. If you keep Wokwi's default `main/CMakeLists.txt`, you will get unresolved-symbol errors on `httpd_start`, `esp_wifi_init`, etc. **Use this folder's `main/CMakeLists.txt`.**

### Build locally with ESP-IDF instead

```bash
. $HOME/esp/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Viewing the 4-task monitor

After Wi-Fi connects, the serial log prints the IP (`Got IP: 10.13.37.x`). In Wokwi, click the network indicator that appears in the simulator panel once port 80 is up &mdash; the table opens in a new tab and refreshes every second.

What to look for:

- All four heartbeat columns increment monotonically. If one stops growing, that task is starved or hung.
- WCET columns climb during the first few seconds, then stabilize once the worst-case path has been exercised. Use those stable numbers in your README.
- Refresh rate is 1 Hz; if the page itself stalls, your HTTP handler is being preempted &mdash; a teachable moment about Core 0 / Core 1 isolation.

## Honor code

AI is allowed for filling in task bodies. Disclose what you used. Be ready to explain why your WCET measurement is honest (vs. a best-case timing that misses the worst-case path).
AI Disclosure:  https://chatgpt.com/share/6a25f61d-e2a4-83ea-acc7-bae3ceaba976
