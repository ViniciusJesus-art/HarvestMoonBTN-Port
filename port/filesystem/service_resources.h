#ifndef HMBTN_SERVICE_RESOURCES_H
#define HMBTN_SERVICE_RESOURCES_H
#include "services.h"
/* Immediate adapter over the existing bounded readers. External/delayed hosts
 * may claim first and respond later; this adapter will not resubmit that job. */
void hmbtn_resource_service(const char *root,HmbtnServices *services);
#endif
