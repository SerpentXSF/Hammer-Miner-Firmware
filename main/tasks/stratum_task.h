#ifndef STRATUM_TASK_H_
#define STRATUM_TASK_H_

void stratum_task(void *pvParameters);
void stratum_close_connection(GlobalState * GLOBAL_STATE);
bool stratum_connected_status(void);
char * stratum_get_pool_ip(void);
#endif