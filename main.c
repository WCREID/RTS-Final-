/*
   Application 2 — Multi-task system with scheduling defense

   Scaffold level: ~70% complete.

   Scaffold Code - AI useage:
     Addition of the comment blocks for "real esp32 function" and the "compute standins"
     Logic to allow for switching between webserve mode and pure logging mode
     Commenting of code including human readable summaries

   What this scaffold gives you:
     - Architecture from App 1 (dual-core, HTTP server) reused.
     - Four FreeRTOS task skeletons, all pinned to Core 1, with priorities pre-assigned.
     - Per-task heartbeat counters wired into the web page.
     - A WCET measurement helper (MEASURE_WCET) you can wrap around any task body.

   What you do:
     1. Rename the task names and log strings for YOUR theme.
        Tasks are currently named A, B, C, D — give them theme-appropriate names.
     2. Implement each task's body. Suggested workloads in the comments per task.
     3. Defend the priority assignment in your README. Use the high-level framework from the slide!
     4. Measure WCET for each task with MEASURE_WCET. Report mean / max.
     5. Demonstrate preemption: log a timestamp before/after, show in your README
        that a higher-priority task interrupts a lower-priority one.

   What you DON'T need to change:
     - The HTTP server, Wi-Fi setup, or web-page rendering structure.
     - The WCET helper itself — just use it.
     - The xTaskCreatePinnedToCore plumbing.

   ============================================================
    OUTPUT MODE  (web monitor vs. terminal-only monitor)
   ============================================================

   USE_WEBSERVER selects how the live monitor data is surfaced. Both modes
   report the SAME fields (period, priority, heartbeats, WCET-max); only the
   transport differs.

     USE_WEBSERVER = 1  -> Wi-Fi + HTTP server, auto-refreshing web page (App 1
                           carry-over). Open the printed IP in a browser.
     USE_WEBSERVER = 0  -> No Wi-Fi, no HTTP. A monitor task prints the same
                           table to the serial console once per second. Use this
                           when you don't want to deal with Wi-Fi/Wokwi-GUEST,
                           or want a clean serial trace to paste into your README.

   ============================================================
   Theme: Defense
   ============================================================
*/

#ifndef USE_WEBSERVER
#define USE_WEBSERVER 0
#endif

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

#if USE_WEBSERVER
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#endif

#define WIFI_SSID  "Wokwi-GUEST"
#define WIFI_PASS  ""

#define CONFIG_LOG_DEFAULT_LEVEL_INFO 1
#define CONFIG_LOG_MAXIMUM_LEVEL  5

static const char *TAG = "app2";

/* ---------- Per-task heartbeat counters (for the monitor) ---------- */
/* Each task increments its counter at the end of every iteration. The monitor
   (web or terminal) reads them and displays values. Single 32-bit reads are
   atomic on Xtensa, so we don't need a mutex around these (yet — App 6 changes
   this). */
static volatile uint32_t hb_a, hb_b, hb_c, hb_d;

/* ---------- WCET measurement helper ----------

   Usage:
     uint64_t wcet_a_max_us = 0;
     ...
     MEASURE_WCET(wcet_a_max_us, {
         // your task body code here
     });

   The macro records the maximum observed time across all invocations.
   Combine with periodic logging to get the WCET evidence for your README.
*/
#define MEASURE_WCET(_max_var, _body) do {                       \
    int64_t _t0 = esp_timer_get_time();                          \
    _body;                                                        \
    int64_t _dt = esp_timer_get_time() - _t0;                    \
    if ((uint64_t)_dt > (_max_var)) (_max_var) = (uint64_t)_dt;  \
  } while (0)

/* Storage for WCET-max per task. Log these periodically. */
static uint64_t wcet_a_max_us, wcet_b_max_us, wcet_c_max_us, wcet_d_max_us;

/* ============================================================
    PURE-COMPUTE WORKLOAD NOTES  (read before filling in the TODOs)
   ============================================================

   The suggested bodies below are deliberately PERIPHERAL-FREE. No GPIO,
   esp_random(), flash/NVS reads, etc. The only thing that changes your runtime
   is a tunable constant, which is what you want for your WCET.

   Each task's comment now leads with a REAL / WOKWI hardware path (the realworld
   version of the workload) and then lists pure-compute stand-ins. Do the
   hardware path if you can; reach for a stand-in when you want a guaranteed
   deterministic WCET or don't want to wire a part. Again we're early on so
   feel free to just use a drop-in!

   Why not just do "random cycles?"

    (1) DEAD-CODE ELIMINATION. With optimization on (-O2/-Os), the
        compiler deletes any computation whose result is never observed —
        your whole loop can vanish and report ~0 us. Each kernel ends by
        writing to a `volatile` sink, and seeds itself from that sink, so the
        work is observable and cannot be elided.

    (2) INITIALIZE BUFFERS ONCE, NOT IN THE LOOP. malloc()/memset() of large
        buffers inside the period destroys determinism. Declare buffers
        static (file scope or `static` inside the task) and fill them one time
        — e.g. in app_main, or guarded by a `static bool inited` flag.

    (3) USE float, NOT double, FOR PREDICTABLE TIMING. The ESP32 FPU is
        single-precision only; `double` is software-emulated and runs ~10-50x
        slower with data-dependent timing. (You CAN use double as a "make it
        slower" knob, but call it out — it's emulated, not free.)

    (4) WARM UP. The first invocation pays one-time costs (instruction-cache
        fill from flash, branch predictor cold). Either discard the first
        sample or run a few warm-up iterations before trusting MEASURE_WCET.

    (5) WOKWI != SILICON. Constants below are 240 MHz hardware ballpark. Wokwi's
        timing model differs, so MEASURE and tune the *_ITERS / *_N / *_REPS
        knobs until you land in the target band. That tuning IS the assignment.

   Utilization sanity check for the README: WCET/period for each task, summed,
   must sit under the rate-monotonic bound (~0.757 for n=4). With the targets
   below you're around 15-20% — comfortably schedulable, which is part of why
   the rate-monotonic priority ordering (higher rate = higher priority) holds.
*/

/* ============================================================
    TASK A   priority 15   period 10 ms   highest priority
   ============================================================
*/
static void task_a(void *arg)
{
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(10);

  for (;;) {
    MEASURE_WCET(wcet_a_max_us, {
      /* TODO(YOU): implement Task A's actual work.
         Something fast — should take well under 1 ms WCET (aim 10-50 us).

         KEEP IT TIGHT. This is your highest-priority task; if it runs
         long, it starves everything else.
      */

        /* Avionics/Defense Task A:
         Fast attitude/threat sensor poll.
         Stand-in: fixed-point IIR filter over synthetic sensor samples.
        */

//      int64_t t = esp_timer_get_time();
//      ESP_LOGI(TAG, "task_a tick t=%lld", t);   // preemption evidence log

#define A_ITERS 256
      static volatile int32_t a_sink;

      int32_t y = a_sink;
      const int32_t alpha = 6553;   // about 0.1 in Q16.16

      for (int i = 0; i < A_ITERS; i++) {
        int32_t in = (i * 1103515245 + 12345);
        y += (int32_t)(((int64_t)alpha * (in - y)) >> 16);
      }

      a_sink = y;
    });

    hb_a++;
    vTaskDelayUntil(&last, period);
  }
}

/* ============================================================
    TASK B   priority 10   period 20 ms
   ============================================================
*/
static void task_b(void *arg)
{
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(20);

  for (;;) {
    MEASURE_WCET(wcet_b_max_us, {
      /* Avionics/Defense Task B:
         Fire-control guidance update.
         Stand-in: windowed statistics over sensor data.
      */
//      int64_t t_start = esp_timer_get_time();
//      ESP_LOGI(TAG, "task_b start t=%lld", t_start);   // preemption evidence log

#define B_WIN 128
      static float b_win[B_WIN];
      static bool b_inited = false;
      static volatile float b_sink;

      float sink_seed = b_sink;

      if (!b_inited) {
        for (int i = 0; i < B_WIN; i++) {
          b_win[i] = (float)((i * 37) % 100) / 100.0f;
        }
        b_inited = true;
      }

      float mean = 0.0f;
      for (int i = 0; i < B_WIN; i++) {
        mean += b_win[i];
      }
      mean /= B_WIN;

      float var = 0.0f;
      for (int i = 0; i < B_WIN; i++) {
        float d = b_win[i] - mean;
        var += d * d;
      }

      b_sink = sqrtf(var / B_WIN);

//      int64_t t_end = esp_timer_get_time();
//      ESP_LOGI(TAG, "task_b finish t=%lld", t_end);
    });

    hb_b++;
    vTaskDelayUntil(&last, period);
  }
}

/* ============================================================
    TASK C   priority 5    period 50 ms
   ============================================================
*/
static void task_c(void *arg)
{
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(50);

  for (;;) {
    MEASURE_WCET(wcet_c_max_us, {
      /* Avionics/Defense Task C:
        Telemetry packet assembly and integrity stamping.
        Stand-in: CRC-32 over a mission telemetry buffer.
      */
#define C_LEN 512
      static uint8_t c_pkt[C_LEN];
      static bool c_inited = false;
      static volatile uint32_t c_sink;

      if (!c_inited) {
        for (int i = 0; i < C_LEN; i++) {
          c_pkt[i] = (uint8_t)(i ^ (i >> 3));
        }
        c_inited = true;
      }
      uint32_t crc = 0xFFFFFFFFu ^ c_sink;
      for (int n = 0; n < C_LEN; n++) {
        crc ^= c_pkt[n];
        for (int b = 0; b < 8; b++) {
          crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
        }
      }
      c_sink = crc ^ 0xFFFFFFFFu;
    });

    hb_c++;
    vTaskDelayUntil(&last, period);
  }
}

/* ============================================================
    TASK D   priority 2    period 100 ms   lowest priority
   ============================================================

   Suggested workloads:
     All themes: housekeeping / logging.

   This task is intentionally interruptible. If A/B/C take longer than expected,
   D's deadline can slip. That's by design — you'll defend this trade-off
   in your README. Its length also makes it the obvious target for your
   preemption demo: log a timestamp before/after and you'll see A/B/C cut in.
*/
static void task_d(void *arg)
{
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(100);

  for (;;) {
    MEASURE_WCET(wcet_d_max_us, {
      /* Avionics/Defense Task D:
          Mission-log housekeeping and background self-test.
          Stand-in: worst-case insertion sort of pending event records.
      */

#define D_N 150
      static int d_arr[D_N];
      static volatile int d_sink;
      for (int i = 0; i < D_N; i++) {
        d_arr[i] = D_N - i + (d_sink & 1);
      }

      for (int i = 1; i < D_N; i++) {
        int key = d_arr[i];
        int j = i - 1;
        while (j >= 0 && d_arr[j] > key) {
          d_arr[j + 1] = d_arr[j];
          j--;
        }
        d_arr[j + 1] = key;
      }

      d_sink = d_arr[D_N / 2];

      ESP_LOGI(TAG, "WCET us A=%llu B=%llu C=%llu D=%llu",
               (unsigned long long)wcet_a_max_us,
               (unsigned long long)wcet_b_max_us,
               (unsigned long long)wcet_c_max_us,
               (unsigned long long)wcet_d_max_us);
    });

    hb_d++;
    vTaskDelayUntil(&last, period);
  }
}

#if USE_WEBSERVER
/* ============================================================
    WEB MONITOR  (USE_WEBSERVER = 1)
   ============================================================ */

/* ---------- HTTP handler: live status page ---------- */
static esp_err_t handle_root(httpd_req_t *req)
{
  /* Buffer sized comfortably above worst-case rendered HTML +
     widest possible %lu / %llu substitutions. */
  char buf[2048];
  int n = snprintf(buf, sizeof(buf),
                   "<!DOCTYPE html>"
                   "<html lang=\"en\"><head>"
                   "<meta charset=\"utf-8\"><meta http-equiv=\"refresh\" content=\"1\">"
                   "<title>YOURTHEME · 4-task monitor</title>"
                   "<style>"
                   "  body { font-family: -apple-system, sans-serif; background: #FAFAF5; "
                   "         color: #1A1A1A; padding: 1.5rem; }"
                   "  h1 { color: #6B4F09; border-bottom: 3px solid #FFC904; "
                   "       display: inline-block; padding-bottom: 4px; }"
                   "  table { border-collapse: collapse; margin: 1rem 0; }"
                   "  th { background: #1A1A1A; color: #FFC904; padding: 8px 14px; "
                   "       text-align: left; font-size: 12px; text-transform: uppercase; }"
                   "  td { padding: 6px 14px; border-bottom: 1px solid #ddd; }"
                   "  td.num { font-variant-numeric: tabular-nums; font-weight: 700; "
                   "           color: #6B4F09; }"
                   "</style></head>"
                   "<body>"
                   "<h1>YOURTHEME · 4-task monitor</h1>"
                   "<table>"
                   "<thead><tr><th>Task</th><th>Period</th><th>Priority</th>"
                   "<th>Heartbeats</th><th>WCET (&micro;s)</th></tr></thead>"
                   "<tbody>"
                   "<tr><td>A</td><td>10 ms</td><td>15</td>"
                   "<td class=\"num\">%lu</td><td class=\"num\">%llu</td></tr>"
                   "<tr><td>B</td><td>20 ms</td><td>10</td>"
                   "<td class=\"num\">%lu</td><td class=\"num\">%llu</td></tr>"
                   "<tr><td>C</td><td>50 ms</td><td>5</td>"
                   "<td class=\"num\">%lu</td><td class=\"num\">%llu</td></tr>"
                   "<tr><td>D</td><td>100 ms</td><td>2</td>"
                   "<td class=\"num\">%lu</td><td class=\"num\">%llu</td></tr>"
                   "</tbody></table>"
                   "<p>Auto-refresh 1 s. Heartbeats should grow monotonically; if one "
                   "stops, that task is starved or hung.</p>"
                   "</body></html>",
                   (unsigned long)hb_a, (unsigned long long)wcet_a_max_us,
                   (unsigned long)hb_b, (unsigned long long)wcet_b_max_us,
                   (unsigned long)hb_c, (unsigned long long)wcet_c_max_us,
                   (unsigned long)hb_d, (unsigned long long)wcet_d_max_us);

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, buf, n);
  return ESP_OK;
}

/* ---------- Boilerplate: Wi-Fi + HTTP server (carried from App 1) ---------- */
static httpd_handle_t start_webserver(void)
{
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 80;
  cfg.core_id = 0;
  cfg.task_priority = 5;
  cfg.stack_size = 8192;
  httpd_handle_t s = NULL;
  if (httpd_start(&s, &cfg) == ESP_OK) {
    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = handle_root, .user_ctx = NULL };
    httpd_register_uri_handler(s, &root);
  }
  return s;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
  else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) esp_wifi_connect();
  else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&e->ip_info.ip));
    start_webserver();
  }
}

static void wifi_init_sta(void)
{
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init));
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
  wifi_config_t cfg = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS,
                                 .threshold.authmode = WIFI_AUTH_OPEN
                               }
                      };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
  ESP_ERROR_CHECK(esp_wifi_start());
}

#else  /* USE_WEBSERVER == 0 */
/* ============================================================
    TERMINAL MONITOR  (USE_WEBSERVER = 0)
   ============================================================

   Same data, no network. A dedicated task prints the monitor table to the
   serial console once per second (mirrors the web page's 1 s auto-refresh).

   Notes:
     - Pinned to Core 0 (PRO_CPU_NUM) so it stays OFF Core 1 with the real-time
       workload tasks — same isolation the HTTP server gave you, so the monitor
       never perturbs your WCET numbers.
     - Uses printf (not ESP_LOGI) so the table prints clean, without a per-line
       log prefix. Swap to ESP_LOGI if you'd rather have timestamps on each row.
     - The Period/Priority columns show the SAME intended values as the web page
       (rate-monotonic targets). They are display constants here, just as in the
       HTML — change them alongside your xTaskCreate priorities to keep the
       monitor honest once you assign real priorities.
*/
static void task_monitor(void *arg)
{
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(1000);

  for (;;) {
    printf("\n=== Defense Systems · 4-task monitor ===\n");
    printf("%-5s %-8s %-9s %-12s %-10s\n",
           "Task", "            Period", "Priority", "Heartbeats", "WCET(us)");
    printf("%-5s %-8s %-9d %-12lu %-10llu\n",
           "threat_sensor_poll", "10 ms",  15, (unsigned long)hb_a, (unsigned long long)wcet_a_max_us);
    printf("%-5s %-8s %-9d %-12lu %-10llu\n",
           "fire_control_update", "20 ms",  10, (unsigned long)hb_b, (unsigned long long)wcet_b_max_us);
    printf("%-5s %-8s %-9d %-12lu %-10llu\n",
           "telemetry_crc_frame", "50 ms",   5, (unsigned long)hb_c, (unsigned long long)wcet_c_max_us);
    printf("%-5s %-8s %-9d %-12lu %-10llu\n",
           "mission_log_update", "100 ms",  2, (unsigned long)hb_d, (unsigned long long)wcet_d_max_us);
    /*printf("(heartbeats should grow monotonically; a stalled counter = "
           "starved or hung task)\n");*/

    vTaskDelayUntil(&last, period);
  }
}
#endif /* USE_WEBSERVER */

/* ---------- app_main ---------- */
void app_main(void)
{
  esp_log_level_set(TAG, ESP_LOG_INFO);
  ESP_LOGI(TAG, "==== App 2 Defense Systems starting — 4-task scheduler demo ====");

#if USE_WEBSERVER
  ESP_LOGI(TAG, "Output mode: WEB MONITOR (USE_WEBSERVER=1) — open the printed IP");
  wifi_init_sta();
#else
  ESP_LOGI(TAG, "Output mode: TERMINAL MONITOR (USE_WEBSERVER=0) — no Wi-Fi, serial only");
  /* Monitor on Core 0 to mirror the HTTP server's isolation from Core 1. */
  xTaskCreatePinnedToCore(task_monitor, "task_monitor", 4096, NULL, 1, NULL, PRO_CPU_NUM);
#endif

  /* Create the four tasks. Pinned to Core 1 to isolate from Wi-Fi on Core 0.
     Priority assignment IS YOURS TO CHOOSE (all = 1) — discuss in your README why these
     priorities make sense for these periods
     hint: higher rate = higher priority? Also, see slide deck!. */
xTaskCreatePinnedToCore(task_a, "threat_sensor_poll", 4096, NULL, 15, NULL, APP_CPU_NUM);
xTaskCreatePinnedToCore(task_b, "fire_control_update", 3072, NULL, 10, NULL, APP_CPU_NUM);
xTaskCreatePinnedToCore(task_c, "telemetry_crc_frame", 4096, NULL,  5, NULL, APP_CPU_NUM);
xTaskCreatePinnedToCore(task_d, "mission_log_housekeeping", 4096, NULL,  2, NULL, APP_CPU_NUM);
}