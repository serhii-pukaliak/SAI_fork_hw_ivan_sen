#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "sai.h"
#include "stub_sai.h"

#undef __MODULE__
#define __MODULE__ SAI_LAG

#define MAX_NUMBER_OF_LAG_MEMBERS 16
#define MAX_NUMBER_OF_LAGS 5

typedef struct _lag_member_db_entry_t {
    bool            is_used;
    sai_object_id_t port_oid;
    sai_object_id_t lag_oid;
} lag_member_db_entry_t;

typedef struct _lag_db_entry_t {
    bool            is_used;
    sai_object_id_t members_ids[MAX_NUMBER_OF_LAG_MEMBERS];
} lag_db_entry_t;

static struct lag_db_t {
    lag_db_entry_t        lags[MAX_NUMBER_OF_LAGS];
    lag_member_db_entry_t members[MAX_NUMBER_OF_LAG_MEMBERS];
} lag_db;

static const sai_attribute_entry_t lag_attribs[] = {
    { SAI_LAG_ATTR_PORT_LIST, false, false, false, true,
      "List of ports in LAG", SAI_ATTR_VAL_TYPE_OBJLIST },
    { END_FUNCTIONALITY_ATTRIBS_ID, false, false, false, false,
      "", SAI_ATTR_VAL_TYPE_UNDETERMINED }
};

static const sai_attribute_entry_t lag_member_attribs[] = {
    { SAI_LAG_MEMBER_ATTR_LAG_ID,  true, true, false, true,
      "LAG ID",  SAI_ATTR_VAL_TYPE_OID },
    { SAI_LAG_MEMBER_ATTR_PORT_ID, true, true, false, true,
      "PORT ID", SAI_ATTR_VAL_TYPE_OID },
    { END_FUNCTIONALITY_ATTRIBS_ID, false, false, false, false,
      "", SAI_ATTR_VAL_TYPE_UNDETERMINED }
};

static sai_status_t get_lag_attribute(
    _In_ const sai_object_key_t   *key,
    _Inout_ sai_attribute_value_t *value,
    _In_ uint32_t                  attr_index,
    _Inout_ vendor_cache_t        *cache,
    void                          *arg);

static sai_status_t get_lag_member_attribute(
    _In_ const sai_object_key_t   *key,
    _Inout_ sai_attribute_value_t *value,
    _In_ uint32_t                  attr_index,
    _Inout_ vendor_cache_t        *cache,
    void                          *arg);

static const sai_vendor_attribute_entry_t lag_vendor_attribs[] = {
    { SAI_LAG_ATTR_PORT_LIST,
      { false, false, false, true },
      { false, false, false, true },
      get_lag_attribute, (void*)SAI_LAG_ATTR_PORT_LIST,
      NULL, NULL },
};

static const sai_vendor_attribute_entry_t lag_member_vendor_attribs[] = {
    { SAI_LAG_MEMBER_ATTR_LAG_ID,
      { true, false, false, true },
      { true, false, false, true },
      get_lag_member_attribute, (void*)SAI_LAG_MEMBER_ATTR_LAG_ID,
      NULL, NULL },
    { SAI_LAG_MEMBER_ATTR_PORT_ID,
      { true, false, false, true },
      { true, false, false, true },
      get_lag_member_attribute, (void*)SAI_LAG_MEMBER_ATTR_PORT_ID,
      NULL, NULL },
};

static void lag_key_to_str(_In_ sai_object_id_t lag_id, _Out_ char *key_str)
{
    uint32_t lag_db_id;
    if (SAI_STATUS_SUCCESS != stub_object_to_type(lag_id, SAI_OBJECT_TYPE_LAG, &lag_db_id)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "invalid lag");
    } else {
        snprintf(key_str, MAX_KEY_STR_LEN, "lag %u", lag_db_id);
    }
}

static void lag_member_key_to_str(_In_ sai_object_id_t lag_member_id, _Out_ char *key_str)
{
    uint32_t member_db_id;
    if (SAI_STATUS_SUCCESS != stub_object_to_type(lag_member_id, SAI_OBJECT_TYPE_LAG_MEMBER, &member_db_id)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "invalid lag_member");
    } else {
        snprintf(key_str, MAX_KEY_STR_LEN, "lag_member %u", member_db_id);
    }
}

static sai_status_t validate_port_oid(_In_ sai_object_id_t port_oid)
{
    uint32_t port_db_id;
    sai_status_t status = stub_object_to_type(port_oid, SAI_OBJECT_TYPE_PORT, &port_db_id);
    if (status != SAI_STATUS_SUCCESS) {
        return status;
    }
    if (port_db_id >= PORT_NUMBER) {
        return SAI_STATUS_INVALID_OBJECT_ID;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t stub_create_lag(
    _Out_ sai_object_id_t* lag_id,
    _In_ uint32_t attr_count,
    _In_ sai_attribute_t *attr_list)
{
    sai_status_t status;
    uint32_t ii;
    uint32_t lag_db_id;
    char list_str[MAX_LIST_VALUE_STR_LEN];

    if (lag_id == NULL) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    status = check_attribs_metadata(attr_count, attr_list, lag_attribs, lag_vendor_attribs, SAI_OPERATION_CREATE);
    if (status != SAI_STATUS_SUCCESS) {
        printf("CREATE LAG: failed attributes check\n");
        return status;
    }

    sai_attr_list_to_str(attr_count, attr_list, lag_attribs, MAX_LIST_VALUE_STR_LEN, list_str);

    for (ii = 0; ii < MAX_NUMBER_OF_LAGS; ii++) {
        if (!lag_db.lags[ii].is_used) {
            break;
        }
    }
    if (ii == MAX_NUMBER_OF_LAGS) {
        printf("Cannot create LAG: limit (%d) reached\n", MAX_NUMBER_OF_LAGS);
        return SAI_STATUS_FAILURE;
    }
    lag_db_id = ii;

    lag_db.lags[lag_db_id].is_used = true;
    memset(lag_db.lags[lag_db_id].members_ids, 0, sizeof(lag_db.lags[lag_db_id].members_ids));

    status = stub_create_object(SAI_OBJECT_TYPE_LAG, lag_db_id, lag_id);
    if (status != SAI_STATUS_SUCCESS) {
        printf("Cannot create a LAG OID\n");
        lag_db.lags[lag_db_id].is_used = false;
        return status;
    }

    printf("CREATE LAG: 0x%lX (%s)\n", *lag_id, list_str);
    return SAI_STATUS_SUCCESS;
}

sai_status_t stub_remove_lag(
    _In_ sai_object_id_t lag_id)
{
    sai_status_t status;
    uint32_t lag_db_id;
    uint32_t ii;

    status = stub_object_to_type(lag_id, SAI_OBJECT_TYPE_LAG, &lag_db_id);
    if (status != SAI_STATUS_SUCCESS) {
        printf("Cannot get LAG DB ID.\n");
        return status;
    }
    if (!lag_db.lags[lag_db_id].is_used) {
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    for (ii = 0; ii < MAX_NUMBER_OF_LAG_MEMBERS; ii++) {
        if (lag_db.lags[lag_db_id].members_ids[ii] != 0) {
            printf("REMOVE LAG: 0x%lX failed (still has members)\n", lag_id);
            return SAI_STATUS_FAILURE;
        }
    }

    lag_db.lags[lag_db_id].is_used = false;
    memset(lag_db.lags[lag_db_id].members_ids, 0, sizeof(lag_db.lags[lag_db_id].members_ids));

    printf("REMOVE LAG: 0x%lX\n", lag_id);
    return SAI_STATUS_SUCCESS;
}

sai_status_t stub_set_lag_attribute(
    _In_ sai_object_id_t lag_id,
    _In_ const sai_attribute_t *attr)
{
    (void)lag_id;
    (void)attr;
    printf("SET LAG ATTRIBUTE called\n");
    return SAI_STATUS_SUCCESS;
}

sai_status_t stub_get_lag_attribute(
    _In_ sai_object_id_t lag_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    const sai_object_key_t key = { .object_id = lag_id };
    char key_str[MAX_KEY_STR_LEN];

    lag_key_to_str(lag_id, key_str);
    return sai_get_attributes(&key, key_str, lag_attribs, lag_vendor_attribs, attr_count, attr_list);
}

sai_status_t stub_create_lag_member(
    _Out_ sai_object_id_t* lag_member_id,
    _In_ uint32_t attr_count,
    _In_ sai_attribute_t *attr_list)
{
    sai_status_t status;
    uint32_t ii;
    uint32_t member_db_id;
    uint32_t lag_db_id;
    uint32_t slot;

    const sai_attribute_value_t *lag_id_attr;
    const sai_attribute_value_t *port_id_attr;
    uint32_t lag_id_idx;
    uint32_t port_id_idx;

    char list_str[MAX_LIST_VALUE_STR_LEN];

    if (lag_member_id == NULL) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    status = check_attribs_metadata(attr_count, attr_list,
                                    lag_member_attribs, lag_member_vendor_attribs,
                                    SAI_OPERATION_CREATE);
    if (status != SAI_STATUS_SUCCESS) {
        printf("CREATE LAG_MEMBER: failed attributes check\n");
        return status;
    }

    sai_attr_list_to_str(attr_count, attr_list, lag_member_attribs, MAX_LIST_VALUE_STR_LEN, list_str);

    status = find_attrib_in_list(attr_count, attr_list, SAI_LAG_MEMBER_ATTR_LAG_ID, &lag_id_attr, &lag_id_idx);
    if (status != SAI_STATUS_SUCCESS) {
        printf("LAG_ID attribute not found.\n");
        return status;
    }

    status = find_attrib_in_list(attr_count, attr_list, SAI_LAG_MEMBER_ATTR_PORT_ID, &port_id_attr, &port_id_idx);
    if (status != SAI_STATUS_SUCCESS) {
        printf("PORT_ID attribute not found.\n");
        return status;
    }

    status = stub_object_to_type(lag_id_attr->oid, SAI_OBJECT_TYPE_LAG, &lag_db_id);
    if (status != SAI_STATUS_SUCCESS || !lag_db.lags[lag_db_id].is_used) {
        printf("CREATE LAG_MEMBER: invalid LAG OID\n");
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    status = validate_port_oid(port_id_attr->oid);
    if (status != SAI_STATUS_SUCCESS) {
        printf("CREATE LAG_MEMBER: invalid PORT OID\n");
        return status;
    }

    /* allocate member db entry */
    for (ii = 0; ii < MAX_NUMBER_OF_LAG_MEMBERS; ii++) {
        if (!lag_db.members[ii].is_used) {
            break;
        }
    }
    if (ii == MAX_NUMBER_OF_LAG_MEMBERS) {
        printf("Cannot create LAG_MEMBER: limit (%d) reached\n", MAX_NUMBER_OF_LAG_MEMBERS);
        return SAI_STATUS_FAILURE;
    }
    member_db_id = ii;

    for (slot = 0; slot < MAX_NUMBER_OF_LAG_MEMBERS; slot++) {
        if (lag_db.lags[lag_db_id].members_ids[slot] == 0) {
            break;
        }
    }
    if (slot == MAX_NUMBER_OF_LAG_MEMBERS) {
        printf("Cannot create LAG_MEMBER: LAG is full\n");
        return SAI_STATUS_FAILURE;
    }

    lag_db.members[member_db_id].is_used = true;
    lag_db.members[member_db_id].lag_oid  = lag_id_attr->oid;
    lag_db.members[member_db_id].port_oid = port_id_attr->oid;

    status = stub_create_object(SAI_OBJECT_TYPE_LAG_MEMBER, member_db_id, lag_member_id);
    if (status != SAI_STATUS_SUCCESS) {
        printf("Cannot create a LAG_MEMBER OID\n");
        lag_db.members[member_db_id].is_used = false;
        return status;
    }

    lag_db.lags[lag_db_id].members_ids[slot] = *lag_member_id;

    printf("CREATE LAG_MEMBER: 0x%lX (%s)\n", *lag_member_id, list_str);
    return SAI_STATUS_SUCCESS;
}

sai_status_t stub_remove_lag_member(
    _In_ sai_object_id_t lag_member_id)
{
    sai_status_t status;
    uint32_t member_db_id;
    uint32_t lag_db_id;
    uint32_t ii;
    sai_object_id_t lag_oid;

    status = stub_object_to_type(lag_member_id, SAI_OBJECT_TYPE_LAG_MEMBER, &member_db_id);
    if (status != SAI_STATUS_SUCCESS) {
        printf("Cannot get LAG_MEMBER DB ID.\n");
        return status;
    }
    if (!lag_db.members[member_db_id].is_used) {
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    lag_oid = lag_db.members[member_db_id].lag_oid;

    status = stub_object_to_type(lag_oid, SAI_OBJECT_TYPE_LAG, &lag_db_id);
    if (status == SAI_STATUS_SUCCESS && lag_db.lags[lag_db_id].is_used) {
        for (ii = 0; ii < MAX_NUMBER_OF_LAG_MEMBERS; ii++) {
            if (lag_db.lags[lag_db_id].members_ids[ii] == lag_member_id) {
                lag_db.lags[lag_db_id].members_ids[ii] = 0;
                break;
            }
        }
    }

    lag_db.members[member_db_id].is_used = false;
    lag_db.members[member_db_id].lag_oid = 0;
    lag_db.members[member_db_id].port_oid = 0;

    printf("REMOVE LAG_MEMBER: 0x%lX\n", lag_member_id);
    return SAI_STATUS_SUCCESS;
}

sai_status_t stub_set_lag_member_attribute(
    _In_ sai_object_id_t lag_member_id,
    _In_ const sai_attribute_t *attr)
{
    (void)lag_member_id;
    (void)attr;
    printf("SET LAG_MEMBER ATTRIBUTE called\n");
    return SAI_STATUS_SUCCESS;
}

sai_status_t stub_get_lag_member_attribute(
    _In_ sai_object_id_t lag_member_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    const sai_object_key_t key = { .object_id = lag_member_id };
    char key_str[MAX_KEY_STR_LEN];

    lag_member_key_to_str(lag_member_id, key_str);
    return sai_get_attributes(&key, key_str, lag_member_attribs, lag_member_vendor_attribs, attr_count, attr_list);
}

static sai_status_t get_lag_member_attribute(
    _In_ const sai_object_key_t   *key,
    _Inout_ sai_attribute_value_t *value,
    _In_ uint32_t                  attr_index,
    _Inout_ vendor_cache_t        *cache,
    void                          *arg)
{
    sai_status_t status;
    uint32_t member_db_id;

    (void)attr_index;
    (void)cache;

    status = stub_object_to_type(key->object_id, SAI_OBJECT_TYPE_LAG_MEMBER, &member_db_id);
    if (status != SAI_STATUS_SUCCESS) {
        printf("Cannot get LAG_MEMBER DB index.\n");
        return status;
    }
    if (!lag_db.members[member_db_id].is_used) {
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    switch ((int64_t)arg) {
        case SAI_LAG_MEMBER_ATTR_LAG_ID:
            value->oid = lag_db.members[member_db_id].lag_oid;
            break;
        case SAI_LAG_MEMBER_ATTR_PORT_ID:
            value->oid = lag_db.members[member_db_id].port_oid;
            break;
        default:
            printf("Got unexpected LAG_MEMBER attribute ID\n");
            return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t get_lag_attribute(
    _In_ const sai_object_key_t   *key,
    _Inout_ sai_attribute_value_t *value,
    _In_ uint32_t                  attr_index,
    _Inout_ vendor_cache_t        *cache,
    void                          *arg)
{
    sai_status_t status;
    uint32_t lag_db_id;
    uint32_t ii;

    sai_object_id_t ports[MAX_NUMBER_OF_LAG_MEMBERS];
    uint32_t port_count = 0;

    (void)attr_index;
    (void)cache;

    status = stub_object_to_type(key->object_id, SAI_OBJECT_TYPE_LAG, &lag_db_id);
    if (status != SAI_STATUS_SUCCESS) {
        printf("Cannot get LAG DB index.\n");
        return status;
    }
    if (!lag_db.lags[lag_db_id].is_used) {
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    switch ((int64_t)arg) {
        case SAI_LAG_ATTR_PORT_LIST:
            for (ii = 0; ii < MAX_NUMBER_OF_LAG_MEMBERS; ii++) {
                sai_object_id_t member_oid = lag_db.lags[lag_db_id].members_ids[ii];
                uint32_t member_db_id;

                if (member_oid == 0) {
                    continue;
                }

                if (stub_object_to_type(member_oid, SAI_OBJECT_TYPE_LAG_MEMBER, &member_db_id) != SAI_STATUS_SUCCESS) {
                    continue;
                }
                if (!lag_db.members[member_db_id].is_used) {
                    continue;
                }
                if (lag_db.members[member_db_id].lag_oid != key->object_id) {
                    continue;
                }

                ports[port_count++] = lag_db.members[member_db_id].port_oid;
            }

            if (value->objlist.list == NULL || value->objlist.count < port_count) {
                value->objlist.count = port_count;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }

            for (ii = 0; ii < port_count; ii++) {
                value->objlist.list[ii] = ports[ii];
            }
            value->objlist.count = port_count;
            break;

        default:
            printf("Got unexpected LAG attribute ID\n");
            return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}


const sai_lag_api_t lag_api = {
    stub_create_lag,
    stub_remove_lag,
    stub_set_lag_attribute,
    stub_get_lag_attribute,
    stub_create_lag_member,
    stub_remove_lag_member,
    stub_set_lag_member_attribute,
    stub_get_lag_member_attribute
};
