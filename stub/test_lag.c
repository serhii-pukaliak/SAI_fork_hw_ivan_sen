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
    printf("%s (count=%u): ", label, lst->count);
    for (uint32_t i = 0; i < lst->count; i++) {
        printf("0x%lX ", lst->list[i]);
    }
    printf("\n");
}

int main(void)
{
    sai_status_t status = SAI_STATUS_SUCCESS;

    sai_switch_api_t *switch_api = NULL;
    sai_lag_api_t    *lag_api    = NULL;

    sai_switch_notification_t notifications;
    memset(&notifications, 0, sizeof(notifications));

    sai_object_id_t port_list[TEST_MAX_PORTS];
    sai_attribute_t sw_attrs[1];

    sai_object_id_t lag1, lag2;
    sai_object_id_t m1, m2, m3, m4;

    sai_attribute_t attrs[2];

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

    sai_object_id_t lag_ports_buf[TEST_MAX_LAG_MEMBERS];
    sai_attribute_t lag_get[1];
    lag_get[0].id = SAI_LAG_ATTR_PORT_LIST;
    lag_get[0].value.objlist.list = lag_ports_buf;
    lag_get[0].value.objlist.count = TEST_MAX_LAG_MEMBERS;
    status = lag_api->get_lag_attribute(lag1, 1, lag_get);
    if (fail_if(status, "get_lag_attribute(LAG1)")) return 1;
    print_oid_list("LAG1 PORT_LIST", &lag_get[0].value.objlist);

    lag_get[0].value.objlist.count = TEST_MAX_LAG_MEMBERS;
    status = lag_api->get_lag_attribute(lag2, 1, lag_get);
    if (fail_if(status, "get_lag_attribute(LAG2)")) return 1;
    print_oid_list("LAG2 PORT_LIST", &lag_get[0].value.objlist);

    sai_attribute_t lm_get[1];
    lm_get[0].id = SAI_LAG_MEMBER_ATTR_LAG_ID;
    status = lag_api->get_lag_member_attribute(m1, 1, lm_get);
    if (fail_if(status, "get_lag_member_attribute(m1)")) return 1;
    printf("LAG_MEMBER#1 LAG_ID: 0x%lX (expected 0x%lX)\n", lm_get[0].value.oid, lag1);

    lm_get[0].id = SAI_LAG_MEMBER_ATTR_PORT_ID;
    status = lag_api->get_lag_member_attribute(m3, 1, lm_get);
    if (fail_if(status, "get_lag_member_attribute(m3)")) return 1;
    printf("LAG_MEMBER#3 PORT_ID: 0x%lX (expected 0x%lX)\n", lm_get[0].value.oid, port_list[2]);

    status = lag_api->remove_lag_member(m2);
    if (fail_if(status, "remove_lag_member(m2)")) return 1;

    status = lag_api->remove_lag(lag1);
    if (status == SAI_STATUS_SUCCESS) {
        printf("FAIL: remove_lag(lag1) should have failed while members exist\n");
        return 1;
    } else {
        printf("OK: remove_lag(lag1) blocked while members exist\n");
    }

    lag_get[0].value.objlist.count = TEST_MAX_LAG_MEMBERS;
    status = lag_api->get_lag_attribute(lag1, 1, lag_get);
    if (fail_if(status, "get_lag_attribute(LAG1)")) return 1;
    print_oid_list("LAG1 PORT_LIST after removing member#2", &lag_get[0].value.objlist);

    status = lag_api->remove_lag_member(m3);
    if (fail_if(status, "remove_lag_member(m3)")) return 1;

    lag_get[0].value.objlist.count = TEST_MAX_LAG_MEMBERS;
    status = lag_api->get_lag_attribute(lag2, 1, lag_get);
    if (fail_if(status, "get_lag_attribute(LAG2)")) return 1;
    print_oid_list("LAG2 PORT_LIST after removing member#3", &lag_get[0].value.objlist);

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
