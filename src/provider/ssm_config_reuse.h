#ifndef SSM_CONFIG_REUSE_H
#define SSM_CONFIG_REUSE_H
/* One synchronous owner, lane 1. The generated tile owns last_conf and updates
 * it only when it writes CONF. Dynamic BR/address/range/Force writes still run.
 * The snapshot checks Host image identity, never device register readback. */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static struct {
    const void *tag;
    size_t bytes;
    int checked;
    unsigned char image[8192];
} ssm_config_shadow;
static int ssm_config_flag(const char *name) {
    const char *value = getenv(name);
    return value && !strcmp(value, "1");
}
static void ssm_config_invalidate(void) {
    memset(&ssm_config_shadow, 0, sizeof(ssm_config_shadow));
    emax7[1].last_conf = 0;
}
static void ssm_config_prepare(int lane, const void *tag, size_t bytes) {
    if (lane != 1 || !ssm_config_flag("NJ_SSM_CONFIG_REUSE") ||
        !tag || !bytes || bytes > sizeof(ssm_config_shadow.image)) {
        emax7[lane].last_conf = 0;
        ssm_config_shadow.tag = NULL;
        ssm_config_shadow.checked = 0;
        return;
    }
    int check = ssm_config_flag("NJ_SSM_CONFIG_REUSE_CHECK");
    int same = (uintptr_t)emax7[lane].last_conf == (uintptr_t)tag &&
               ssm_config_shadow.tag == tag && ssm_config_shadow.bytes == bytes;
    /* Enabling self-check in mid-session first establishes a fresh snapshot. */
    if (!same || (check && !ssm_config_shadow.checked)) {
        emax7[lane].last_conf = 0;
        ssm_config_shadow.tag = tag;
        ssm_config_shadow.bytes = bytes;
        ssm_config_shadow.checked = check;
        if (check) memcpy(ssm_config_shadow.image, tag, bytes);
    } else if (check && memcmp(ssm_config_shadow.image, tag, bytes)) {
        fputs("NJ_SSM_CONFIG_BYTE_MISMATCH\n", stderr);
        abort();
    }
}
#endif
