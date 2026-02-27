#ifndef ACMS_WEB_H
#define ACMS_WEB_H

#include <stdint.h>
#include <stdbool.h>
#include "hashmap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_VAR_POOL       MAX_VAR_POOL_CAP
#define MAX_INCREMENT_ROWS MAX_VAR_POOL_CAP

typedef struct {
  float  Step;        /* per-tick step (sign = direction); loaded from XSD Increment column */
  float  Range;       /* oscillation half-width: (1 ± MARGIN) * |threshold - ini_val|; computed at task start */
  float  ini_val;
  float  threshold;   /* -9999.0f = no threshold */
  float *value_ptr;
} increment_pool_row_t;

typedef struct {
  increment_pool_row_t rows[MAX_INCREMENT_ROWS];
  int count;
} increment_pool_t;

extern float            addr_pool[MAX_VAR_POOL];
extern increment_pool_t increment_pool;

/* Full system init — call once from setup() after WiFi is connected */
void acms_system_init(const char *login_user, const char *login_pass);

/* Full system loop — call from loop() */
void acms_system_loop(void);

/* ── Internal functions (available if needed externally) ── */

/* Mount SPIFFS, provision XML, register routes, start webserver */
void acms_web_init(void);

/* Service HTTP clients — called internally by acms_system_loop() */
void acms_web_loop(void);

/* Load Metadata from SPIFFS, register with data manager, sync */
void get_metadata(void);

#ifdef __cplusplus
}
#endif

#endif /* ACMS_WEB_H */
