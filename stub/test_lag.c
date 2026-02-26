#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "sai.h"

enum {
    TEST_MAX_PORTS = 64,
    TEST_MAX_LAG_MEMBERS = 16
};

#define OID_FMT "0x%" PRIX64
#define OID_ARG(x) ((uint64_t)(x))

#ifdef SAI_STATUS_OBJECT_IN_USE
#define STATUS_OBJECT_IN_USE SAI_STATUS_OBJECT_IN_USE
#else
#define STATUS_OBJECT_IN_USE SAI_STATUS_FAILURE
#endif

static const char* test_profile_get_value(
    _In_ sai_switch_profile_id_t profile_id,
    _In_ const char* variable)
{
    (void)profile_id;
    (void)variable;

    return NULL;
}

static int test_profile_get_next_value(
    _In_ sai_switch_profile_id_t profile_id,
    _Out_ const char** variable,
    _Out_ const char** value)
{
    (void)profile_id;
    (void)variable;
    (void)value;

    return -1;
}

static const service_method_table_t test_services = {
    test_profile_get_value,
    test_profile_get_next_value
};

static int fail_if(sai_status_t status, const char *where)
{
    if (status != SAI_STATUS_SUCCESS) {
        printf("FAIL at %s: status=%d\n", where, status);
        return 1;
    }

    return 0;
}

static void print_oid_list(const char *label, const sai_object_list_t *lst)
{
    uint32_t i = 0;
    printf("%s (count=%u): ", label, lst ? lst->count : 0);

    if (!lst || lst->count == 0 || lst->list == NULL) {
        printf("(empty)\n");
        return;
    }

    for (i = 0; i < lst->count; i++) {
        printf(OID_FMT " ", OID_ARG(lst->list[i]));
    }
    printf("\n");
}

static int list_contains_exactly_2(const sai_object_list_t *lst, sai_object_id_t a, sai_object_id_t b)
{
    if (!lst || lst->count != 2 || lst->list == NULL) return 0;
    
    return ((lst->list[0] == a && lst->list[1] == b) ||
            (lst->list[0] == b && lst->list[1] == a));
}

static int list_contains_exactly_1(const sai_object_list_t *lst, sai_object_id_t a)
{
    return (lst && lst->count == 1 && lst->list && lst->list[0] == a);
}

static int get_switch_port_list(sai_switch_api_t *switch_api,
                                sai_object_id_t **buf_io,
                                uint32_t *buf_cap_io,
                                sai_object_list_t *out_list)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_attribute_t attr[1];

    if (!switch_api || !buf_io || !buf_cap_io || !out_list) return 1;

    memset(attr, 0, sizeof(attr));
    attr[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    attr[0].value.objlist.list  = *buf_io;
    attr[0].value.objlist.count = *buf_cap_io;

    status = switch_api->get_switch_attribute(1, attr);

    if (status == SAI_STATUS_BUFFER_OVERFLOW) {
        uint32_t needed = attr[0].value.objlist.count;

        sai_object_id_t *newbuf = (sai_object_id_t*)calloc(needed, sizeof(sai_object_id_t));
        if (!newbuf) {
            printf("FAIL: OOM reallocating SWITCH PORT_LIST buffer (needed=%u)\n", needed);
            return 1;
        }

        attr[0].value.objlist.list  = newbuf;
        attr[0].value.objlist.count = needed;

        status = switch_api->get_switch_attribute(1, attr);
        if (status != SAI_STATUS_SUCCESS) {
            printf("FAIL: get_switch_attribute(PORT_LIST) retry status=%d\n", status);
            free(newbuf);
            return 1;
        }

        *buf_io = newbuf;
        *buf_cap_io = needed;
    } else if (status != SAI_STATUS_SUCCESS) {
        printf("FAIL: get_switch_attribute(PORT_LIST) status=%d\n", status);
        return 1;
    }

    *out_list = attr[0].value.objlist;
    
    return 0;
}

static int get_lag_port_list(sai_lag_api_t *lag_api,
                            sai_object_id_t lag_oid,
                            sai_object_id_t **buf_io,
                            uint32_t *buf_cap_io,
                            sai_object_list_t *out_list)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_attribute_t attr[1];

    if (!lag_api || !buf_io || !buf_cap_io || !out_list) return 1;

    memset(attr, 0, sizeof(attr));
    attr[0].id = SAI_LAG_ATTR_PORT_LIST;
    attr[0].value.objlist.list  = *buf_io;
    attr[0].value.objlist.count = *buf_cap_io;

    status = lag_api->get_lag_attribute(lag_oid, 1, attr);

    if (status == SAI_STATUS_BUFFER_OVERFLOW) {
        uint32_t needed = attr[0].value.objlist.count;

        sai_object_id_t *newbuf = (sai_object_id_t*)calloc(needed, sizeof(sai_object_id_t));
        if (!newbuf) {
            printf("FAIL: OOM reallocating LAG PORT_LIST buffer (needed=%u)\n", needed);
            return 1;
        }

        attr[0].value.objlist.list  = newbuf;
        attr[0].value.objlist.count = needed;

        status = lag_api->get_lag_attribute(lag_oid, 1, attr);
        if (status != SAI_STATUS_SUCCESS) {
            printf("FAIL: get_lag_attribute(PORT_LIST) retry status=%d\n", status);
            free(newbuf);
            return 1;
        }

        *buf_io = newbuf;
        *buf_cap_io = needed;
    } else if (status != SAI_STATUS_SUCCESS) {
        printf("FAIL: get_lag_attribute(PORT_LIST) status=%d\n", status);
        return 1;
    }

    *out_list = attr[0].value.objlist;

    return 0;
}

int main(void)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    int rc = 1;

    sai_switch_api_t *switch_api = NULL;
    sai_lag_api_t    *lag_api    = NULL;

    sai_switch_notification_t notifications;

    sai_object_id_t port_list_stack[TEST_MAX_PORTS];
    sai_object_id_t *port_list_buf = port_list_stack;
    uint32_t port_list_cap = TEST_MAX_PORTS;
    sai_object_list_t switch_port_list = {0};

    sai_object_id_t lag1 = 0, lag2 = 0;
    sai_object_id_t m1 = 0, m2 = 0, m3 = 0, m4 = 0;

    sai_attribute_t attrs[2];

    sai_object_id_t lag_ports_stack[TEST_MAX_LAG_MEMBERS];
    sai_object_id_t *lag_ports_buf = lag_ports_stack;
    uint32_t lag_ports_cap = TEST_MAX_LAG_MEMBERS;
    sai_object_list_t lag_port_list_view = {0};

    memset(&notifications, 0, sizeof(notifications));
    memset(port_list_stack, 0, sizeof(port_list_stack));
    memset(attrs, 0, sizeof(attrs));
    memset(lag_ports_stack, 0, sizeof(lag_ports_stack));

    status = sai_api_initialize(0, &test_services);
    if (fail_if(status, "sai_api_initialize")) goto cleanup;

    status = sai_api_query(SAI_API_SWITCH, (void**)&switch_api);
    if (fail_if(status, "sai_api_query(SWITCH)")) goto cleanup;

    status = sai_api_query(SAI_API_LAG, (void**)&lag_api);
    if (fail_if(status, "sai_api_query(LAG)")) goto cleanup;

    status = switch_api->initialize_switch(0, "HW_ID", 0, &notifications);
    if (fail_if(status, "initialize_switch")) goto cleanup;

    if (get_switch_port_list(switch_api, &port_list_buf, &port_list_cap, &switch_port_list)) {
        printf("FAIL: unable to fetch switch PORT_LIST\n");
        goto cleanup;
    }

    if (switch_port_list.count < 4) {
        printf("FAIL: need at least 4 ports, got %u\n", switch_port_list.count);
        goto cleanup;
    }

    status = lag_api->create_lag(&lag1, 0, NULL);
    if (fail_if(status, "create_lag(lag1)")) goto cleanup;

    attrs[0].id = SAI_LAG_MEMBER_ATTR_PORT_ID;
    attrs[0].value.oid = switch_port_list.list[0];
    attrs[1].id = SAI_LAG_MEMBER_ATTR_LAG_ID;
    attrs[1].value.oid = lag1;

    status = lag_api->create_lag_member(&m1, 2, attrs);
    if (fail_if(status, "create_lag_member(m1)")) goto cleanup;

    attrs[0].value.oid = switch_port_list.list[1];
    status = lag_api->create_lag_member(&m2, 2, attrs);
    if (fail_if(status, "create_lag_member(m2)")) goto cleanup;

    status = lag_api->create_lag(&lag2, 0, NULL);
    if (fail_if(status, "create_lag(lag2)")) goto cleanup;

    attrs[0].value.oid = switch_port_list.list[2];
    attrs[1].value.oid = lag2;
    status = lag_api->create_lag_member(&m3, 2, attrs);
    if (fail_if(status, "create_lag_member(m3)")) goto cleanup;

    attrs[0].value.oid = switch_port_list.list[3];
    status = lag_api->create_lag_member(&m4, 2, attrs);
    if (fail_if(status, "create_lag_member(m4)")) goto cleanup;

    if (get_lag_port_list(lag_api, lag1, &lag_ports_buf, &lag_ports_cap, &lag_port_list_view)) goto cleanup;
    print_oid_list("LAG1 PORT_LIST", &lag_port_list_view);
    if (!list_contains_exactly_2(&lag_port_list_view, switch_port_list.list[0], switch_port_list.list[1])) {
        printf("FAIL: LAG1 PORT_LIST unexpected\n");
        goto cleanup;
    }

    if (get_lag_port_list(lag_api, lag2, &lag_ports_buf, &lag_ports_cap, &lag_port_list_view)) goto cleanup;
    print_oid_list("LAG2 PORT_LIST", &lag_port_list_view);
    if (!list_contains_exactly_2(&lag_port_list_view, switch_port_list.list[2], switch_port_list.list[3])) {
        printf("FAIL: LAG2 PORT_LIST unexpected\n");
        goto cleanup;
    }

    {
        sai_attribute_t lm_get[1];
        memset(lm_get, 0, sizeof(lm_get));
        lm_get[0].id = SAI_LAG_MEMBER_ATTR_LAG_ID;

        status = lag_api->get_lag_member_attribute(m1, 1, lm_get);
        if (fail_if(status, "get_lag_member_attribute(m1, LAG_ID)")) goto cleanup;

        printf("LAG_MEMBER#1 LAG_ID: " OID_FMT " (expected " OID_FMT ")\n",
               OID_ARG(lm_get[0].value.oid), OID_ARG(lag1));

        if (lm_get[0].value.oid != lag1) {
            printf("FAIL: LAG_MEMBER#1 LAG_ID mismatch\n");
            goto cleanup;
        }
    }

    {
        sai_attribute_t lm_get[1];
        memset(lm_get, 0, sizeof(lm_get));
        lm_get[0].id = SAI_LAG_MEMBER_ATTR_PORT_ID;

        status = lag_api->get_lag_member_attribute(m3, 1, lm_get);
        if (fail_if(status, "get_lag_member_attribute(m3, PORT_ID)")) goto cleanup;

        printf("LAG_MEMBER#3 PORT_ID: " OID_FMT " (expected " OID_FMT ")\n",
               OID_ARG(lm_get[0].value.oid), OID_ARG(switch_port_list.list[2]));

        if (lm_get[0].value.oid != switch_port_list.list[2]) {
            printf("FAIL: LAG_MEMBER#3 PORT_ID mismatch\n");
            goto cleanup;
        }
    }

    status = lag_api->remove_lag_member(m2);
    if (fail_if(status, "remove_lag_member(m2)")) goto cleanup;
    m2 = 0;

    status = lag_api->remove_lag(lag1);
#ifdef SAI_STATUS_OBJECT_IN_USE
    if (status != SAI_STATUS_OBJECT_IN_USE) {
        printf("FAIL: remove_lag(lag1) expected OBJECT_IN_USE, got status=%d\n", status);
        goto cleanup;
    }
#else
    if (status == SAI_STATUS_SUCCESS) {
        printf("FAIL: remove_lag(lag1) should have failed while members exist\n");
        goto cleanup;
    }
#endif
    printf("OK: remove_lag(lag1) blocked while members exist\n");

    if (get_lag_port_list(lag_api, lag1, &lag_ports_buf, &lag_ports_cap, &lag_port_list_view)) goto cleanup;
    print_oid_list("LAG1 PORT_LIST after removing member#2", &lag_port_list_view);
    if (!list_contains_exactly_1(&lag_port_list_view, switch_port_list.list[0])) {
        printf("FAIL: LAG1 PORT_LIST after remove mismatch\n");
        goto cleanup;
    }

    status = lag_api->remove_lag_member(m3);
    if (fail_if(status, "remove_lag_member(m3)")) goto cleanup;
    m3 = 0;

    if (get_lag_port_list(lag_api, lag2, &lag_ports_buf, &lag_ports_cap, &lag_port_list_view)) goto cleanup;
    print_oid_list("LAG2 PORT_LIST after removing member#3", &lag_port_list_view);
    if (!list_contains_exactly_1(&lag_port_list_view, switch_port_list.list[3])) {
        printf("FAIL: LAG2 PORT_LIST after remove mismatch\n");
        goto cleanup;
    }

    status = lag_api->remove_lag_member(m1);
    if (fail_if(status, "remove_lag_member(m1)")) goto cleanup;
    m1 = 0;

    status = lag_api->remove_lag_member(m4);
    if (fail_if(status, "remove_lag_member(m4)")) goto cleanup;
    m4 = 0;

    status = lag_api->remove_lag(lag2);
    if (fail_if(status, "remove_lag(lag2)")) goto cleanup;
    lag2 = 0;

    status = lag_api->remove_lag(lag1);
    if (fail_if(status, "remove_lag(lag1)")) goto cleanup;
    lag1 = 0;

    switch_api->shutdown_switch(0);

    status = sai_api_uninitialize();
    if (fail_if(status, "sai_api_uninitialize")) goto cleanup;

    printf("PASS\n");
    rc = 0;

cleanup:
    if (lag_api) {
        if (m1) lag_api->remove_lag_member(m1);
        if (m2) lag_api->remove_lag_member(m2);
        if (m3) lag_api->remove_lag_member(m3);
        if (m4) lag_api->remove_lag_member(m4);
        if (lag2) lag_api->remove_lag(lag2);
        if (lag1) lag_api->remove_lag(lag1);
    }

    if (port_list_buf != port_list_stack) free(port_list_buf);
    if (lag_ports_buf != lag_ports_stack) free(lag_ports_buf);

    return rc;
}
