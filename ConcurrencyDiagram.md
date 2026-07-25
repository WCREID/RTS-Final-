ESP32-S3 dual-core layout

CORE 0: non-real-time monitor side
+--------------------------------------------------+
| Terminal/Web Monitor Task                         |
| - Period: 1000 ms                                 |
| - Priority: 1                                     |
| - Reads heartbeat counters and WCET max values    |
|                                                  |
| Wi-Fi / HTTP Server if USE_WEBSERVER = 1          |
+--------------------------------------------------+


CORE 1: real-time RMS task set
+--------------------------------------------------+
| Highest priority                                 |
|                                                  |
| Task A: Threat Sensor / Attitude Poll             |
| Period: 10 ms, Priority: 15, WCET+30%: 758 us     |
|        |                                         |
|        | can preempt                             |
|        v                                         |
| Task B: Fire-Control Guidance Update              |
| Period: 20 ms, Priority: 10, WCET+30%: 1053 us    |
|        |                                         |
|        | can preempt                             |
|        v                                         |
| Task C: Telemetry CRC Frame Assembly              |
| Period: 50 ms, Priority: 5, WCET+30%: 5913 us     |
|        |                                         |
|        | can preempt                             |
|        v                                         |
| Task D: Mission-Log Housekeeping                  |
| Period: 100 ms, Priority: 2, WCET+30%: 29543 us   |
|                                                  |
| Lowest priority                                  |
+--------------------------------------------------+

Shared monitor variables:
hb_a, hb_b, hb_c, hb_d
wcet_a_max_us, wcet_b_max_us, wcet_c_max_us, wcet_d_max_us

Each real-time task writes its own heartbeat/WCET value.
The monitor reads those values once per second.