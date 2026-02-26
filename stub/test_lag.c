#include <stdio.h>
#include <string.h>
#include "sai.h"

enum {
    TEST_MAX_PORTS = 64,
    TEST_MAX_LAG_MEMBERS = 16
};

static const char* test_profile_get_value(
    _In_ sai_switch_profile_id_t profile_id,
    _In_ const char* variable)
{
    (void)profile_id;
    (void)variable;

    return 0;
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
    printf("%s (count=%u): ", label, lst->count);
    for (i = 0; i < lst->count; i++) {
        printf("0x%lX ", lst->list[i]);
    }
    printf("\n");
}

static int list_contains_exactly_2(const sai_object_list_t *lst, sai_object_id_t a, sai_object_id_t b)
{
    if (lst->count != 2) return 0;
    if ((lst->list[0] == a && lst->list[1] == b) ||
        (lst->list[0] == b && lst->list[1] == a)) {
        return 1;
    }

    return 0;
}

static int list_contains_exactly_1(const sai_object_list_t *lst, sai_object_id_t a)
{
    return (lst->count == 1 && lst->list[0] == a);
}

static int get_lag_port_list(sai_lag_api_t *lag_api,
                            sai_object_id_t lag_oid,
                            sai_object_id_t *buf,
                            uint32_t buf_cap,
                            sai_object_list_t *out_list)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_attribute_t attr[1];

    memset(attr, 0, sizeof(attr));
    attr[0].id = SAI_LAG_ATTR_PORT_LIST;
    attr[0].value.objlist.list = buf;
    attr[0].value.objlist.count = buf_cap;

    status = lag_api->get_lag_attribute(lag_oid, 1, attr);
    if (status == SAI_STATUS_BUFFER_OVERFLOW) {
        printf("FAIL: PORT_LIST buffer overflow, required=%u cap=%u\n",
               attr[0].value.objlist.count, buf_cap);
        return 1;
    }
    if (status != SAI_STATUS_SUCCESS) {
        printf("FAIL: get_lag_attribute(PORT_LIST) status=%d\n", status);
        return 1;
    }

    *out_list = attr[0].value.objlist;

    return 0;
}

int main(void)
{
    sai_status_t status = SAI_STATUS_SUCCESS;

    sai_switch_api_t *switch_api = NULL;
    sai_lag_api_t    *lag_api    = NULL;

    sai_switch_notification_t notifications;
    sai_object_id_t port_list[TEST_MAX_PORTS];
    sai_attribute_t sw_attrs[1];

    sai_object_id_t lag1 = 0, lag2 = 0;
    sai_object_id_t m1 = 0, m2 = 0, m3 = 0, m4 = 0;

    sai_attribute_t attrs[2];

    sai_object_id_t lag_ports_buf[TEST_MAX_LAG_MEMBERS];
    sai_object_list_t port_list_view;

    memset(&notifications, 0, sizeof(notifications));
    memset(port_list, 0, sizeof(port_list));
    memset(sw_attrs, 0, sizeof(sw_attrs));
    memset(attrs, 0, sizeof(attrs));
    memset(lag_ports_buf, 0, sizeof(lag_ports_buf));

    status = sai_api_initialize(0, &test_services);
    if (fail_if(status, "sai_api_initialize")) return 1;

    status = sai_api_query(SAI_API_SWITCH, (void**)&switch_api);
    if (fail_if(status, "sai_api_query(SWITCH)")) return 1;

    status = sai_api_query(SAI_API_LAG, (void**)&lag_api);
    if (fail_if(status, "sai_api_query(LAG)")) return 1;

    status = switch_api->initialize_switch(0, "HW_ID", 0, &notifications);
    if (fail_if(status, "initialize_switch")) return 1;

    sw_attrs[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    sw_attrs[0].value.objlist.list  = port_list;
    sw_attrs[0].value.objlist.count = TEST_MAX_PORTS;

    status = switch_api->get_switch_attribute(1, sw_attrs);
    if (fail_if(status, "get_switch_attribute(PORT_LIST)")) return 1;

    if (sw_attrs[0].value.objlist.count < 4) {
        printf("FAIL: need at least 4 ports, got %u\n", sw_attrs[0].value.objlist.count);
        return 1;
    }

    status = lag_api->create_lag(&lag1, 0, NULL);
    if (fail_if(status, "create_lag(lag1)")) return 1;

    attrs[0].id = SAI_LAG_MEMBER_ATTR_PORT_ID;
    attrs[0].value.oid = port_list[0];
    attrs[1].id = SAI_LAG_MEMBER_ATTR_LAG_ID;
    attrs[1].value.oid = lag1;
    status = lag_api->create_lag_member(&m1, 2, attrs);
    if (fail_if(status, "create_lag_member(m1)")) return 1;

    attrs[0].value.oid = port_list[1];
    attrs[1].value.oid = lag1;
    status = lag_api->create_lag_member(&m2, 2, attrs);
    if (fail_if(status, "create_lag_member(m2)")) return 1;

    status = lag_api->create_lag(&lag2, 0, NULL);
    if (fail_if(status, "create_lag(lag2)")) return 1;

    attrs[0].value.oid = port_list[2];
    attrs[1].value.oid = lag2;
    status = lag_api->create_lag_member(&m3, 2, attrs);
    if (fail_if(status, "create_lag_member(m3)")) return 1;

    attrs[0].value.oid = port_list[3];
    attrs[1].value.oid = lag2;
    status = lag_api->create_lag_member(&m4, 2, attrs);
    if (fail_if(status, "create_lag_member(m4)")) return 1;

    if (get_lag_port_list(lag_api, lag1, lag_ports_buf, TEST_MAX_LAG_MEMBERS, &port_list_view)) return 1;
    print_oid_list("LAG1 PORT_LIST", &port_list_view);
    if (!list_contains_exactly_2(&port_list_view, port_list[0], port_list[1])) {
        printf("FAIL: LAG1 PORT_LIST unexpected\n");
        return 1;
    }

    if (get_lag_port_list(lag_api, lag2, lag_ports_buf, TEST_MAX_LAG_MEMBERS, &port_list_view)) return 1;
    print_oid_list("LAG2 PORT_LIST", &port_list_view);
    if (!list_contains_exactly_2(&port_list_view, port_list[2], port_list[3])) {
        printf("FAIL: LAG2 PORT_LIST unexpected\n");
        return 1;
    }

    {
        sai_attribute_t lm_get[1];
        memset(lm_get, 0, sizeof(lm_get));
        lm_get[0].id = SAI_LAG_MEMBER_ATTR_LAG_ID;
        status = lag_api->get_lag_member_attribute(m1, 1, lm_get);
        if (fail_if(status, "get_lag_member_attribute(m1, LAG_ID)")) return 1;
        printf("LAG_MEMBER#1 LAG_ID: 0x%lX (expected 0x%lX)\n", lm_get[0].value.oid, lag1);
        if (lm_get[0].value.oid != lag1) {
            printf("FAIL: LAG_MEMBER#1 LAG_ID mismatch\n");
            return 1;
        }
    }

    {
        sai_attribute_t lm_get[1];
        memset(lm_get, 0, sizeof(lm_get));
        lm_get[0].id = SAI_LAG_MEMBER_ATTR_PORT_ID;
        status = lag_api->get_lag_member_attribute(m3, 1, lm_get);
        if (fail_if(status, "get_lag_member_attribute(m3, PORT_ID)")) return 1;
        printf("LAG_MEMBER#3 PORT_ID: 0x%lX (expected 0x%lX)\n", lm_get[0].value.oid, port_list[2]);
        if (lm_get[0].value.oid != port_list[2]) {
            printf("FAIL: LAG_MEMBER#3 PORT_ID mismatch\n");
            return 1;
        }
    }

    status = lag_api->remove_lag_member(m2);
    if (fail_if(status, "remove_lag_member(m2)")) return 1;

    status = lag_api->remove_lag(lag1);
    if (status == SAI_STATUS_SUCCESS) {
        printf("FAIL: remove_lag(lag1) should have failed while members exist\n");
        return 1;
    } else {
        printf("OK: remove_lag(lag1) blocked while members exist\n");
    }

    if (get_lag_port_list(lag_api, lag1, lag_ports_buf, TEST_MAX_LAG_MEMBERS, &port_list_view)) return 1;
    print_oid_list("LAG1 PORT_LIST after removing member#2", &port_list_view);
    if (!list_contains_exactly_1(&port_list_view, port_list[0])) {
        printf("FAIL: LAG1 PORT_LIST after remove mismatch\n");
        return 1;
    }

    status = lag_api->remove_lag_member(m3);
    if (fail_if(status, "remove_lag_member(m3)")) return 1;

    if (get_lag_port_list(lag_api, lag2, lag_ports_buf, TEST_MAX_LAG_MEMBERS, &port_list_view)) return 1;
    print_oid_list("LAG2 PORT_LIST after removing member#3", &port_list_view);
    if (!list_contains_exactly_1(&port_list_view, port_list[3])) {
        printf("FAIL: LAG2 PORT_LIST after remove mismatch\n");
        return 1;
    }

    status = lag_api->remove_lag_member(m1);
    if (fail_if(status, "remove_lag_member(m1)")) return 1;

    status = lag_api->remove_lag_member(m4);
    if (fail_if(status, "remove_lag_member(m4)")) return 1;

    status = lag_api->remove_lag(lag2);
    if (fail_if(status, "remove_lag(lag2)")) return 1;

    status = lag_api->remove_lag(lag1);
    if (fail_if(status, "remove_lag(lag1)")) return 1;

    switch_api->shutdown_switch(0);

    status = sai_api_uninitialize();
    if (fail_if(status, "sai_api_uninitialize")) return 1;

    printf("PASS\n");

    return 0;
}
