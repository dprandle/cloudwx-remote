#include <string>
#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include "mongodb.h"
#include "logging.h"

struct mongodb_ctxt
{
    static constexpr sizet MAX_STR_SIZE = 32;
    static constexpr sizet MAX_URI_SIZE = 1000;
    char name[MAX_STR_SIZE + 1]{};
    char uri[MAX_URI_SIZE + 1]{};
    mongoc_client_pool_t *pool;
};

intern void mongo_logger_callback(mongoc_log_level_t log_level, const char *, const char *message, void *)
{
    switch (log_level) {
    case (MONGOC_LOG_LEVEL_ERROR): {
        elog("%s", message);
    } break;
    case (MONGOC_LOG_LEVEL_CRITICAL): {
        flog("%s", message);
    } break;
    case (MONGOC_LOG_LEVEL_WARNING): {
        wlog("%s", message);
    } break;
    case (MONGOC_LOG_LEVEL_MESSAGE): {
        ilog("%s", message);
    } break;
    case (MONGOC_LOG_LEVEL_INFO): {
        ilog("%s", message);
    } break;
    case (MONGOC_LOG_LEVEL_DEBUG): {
        dlog("%s", message);
    } break;
    case (MONGOC_LOG_LEVEL_TRACE): {
        tlog("%s", message);
    } break;
    default:
        wlog("Unknown mongoc log type %d", (int)log_level);
    }
}

void mongodb_save_item(mongodb_ctxt *db, const char *collection_name, const raw_metar_entry &item, char *err_buff)
{
    bson_t doc, options, reply, selector;
    bson_error_t error;

    bson_init(&doc);
    bson_append_utf8(&doc, "_id", -1, item.id.data, -1);
    bson_append_utf8(&doc, "text", -1, item.text, -1);

    mongoc_client_t *client = mongoc_client_pool_pop(db->pool);

    // Get a handle on the database "Live" and collection "PayrollRecords"
    auto collection = mongoc_client_get_collection(client, db->name, collection_name);

    bson_init(&options);
    bson_append_bool(&options, "upsert", -1, true);

    bson_init(&selector);
    bson_append_utf8(&selector, "_id", -1, item.id.data, -1);

    if (!mongoc_collection_replace_one(collection, &selector, &doc, &options, &reply, &error)) {
        auto str = bson_as_relaxed_extended_json(&selector, nullptr);
        auto str2 = bson_as_relaxed_extended_json(&reply, nullptr);
        if (err_buff) {
            sprintf(err_buff, "Update failed for %s: %s - reply: %s selector: %s", collection_name, error.message, str, str2);
        }
        else {
            wlog("Update failed for %s: %s - reply: %s selector: %s", collection_name, error.message, str, str2);
        }
        bson_free(str);
        bson_free(str2);
    }

    bson_destroy(&doc);
    bson_destroy(&options);
    bson_destroy(&reply);
    bson_destroy(&selector);

    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(db->pool, client);
}

const char *mongodb_get_name(mongodb_ctxt *db)
{
    return db->name;
}

const char *mongodb_get_uri(mongodb_ctxt *db)
{
    return db->uri;
}

mongodb_ctxt *mongodb_create(const char *uri_str, const char *db_name)
{
    ilog("Creating mongodb database instance");
    auto mem = calloc(1, sizeof(mongodb_ctxt));
    auto ret = (mongodb_ctxt *)mem;
    strncpy(ret->name, db_name, mongodb_ctxt::MAX_STR_SIZE);
    strncpy(ret->uri, uri_str, mongodb_ctxt::MAX_URI_SIZE);
    return ret;
}

bool mongodb_init(mongodb_ctxt *db)
{
    ilog("Initializing mongodb with connection to %s", db->uri);
    bson_error_t error;

    // Safely create a MongoDB URI object from the given string
    auto uri = mongoc_uri_new_with_error(db->uri, &error);
    if (!uri) {
        elog("Failed to parse URI %s: %s", db->uri, error.message);
        return false;
    }

    db->pool = mongoc_client_pool_new_with_error(uri, &error);
    if (!db->pool) {
        elog("Failed to create client pool: %s", error.message);
        return false;
    }

    // Register the application name so we can track it in the profile logs
    // on the server. This can also be done from the URI (see other examples).
    mongoc_client_pool_set_appname(db->pool, "cloudwx-remote");

    auto api = mongoc_server_api_new(MONGOC_SERVER_API_V1);
    bool result = mongoc_client_pool_set_server_api(db->pool, api, &error);
    if (!result) {
        elog("Failed to set server API version: %s", error.message);
        return false;
    }

    mongoc_client_pool_set_error_api(db->pool, MONGOC_ERROR_API_VERSION_2);
    mongoc_server_api_destroy(api);
    mongoc_uri_destroy(uri);
    return true;
}

void mongodb_terminate(mongodb_ctxt *db)
{
    mongoc_client_pool_destroy(db->pool);
}

void mongodb_destroy(mongodb_ctxt *db)
{
    ilog("Freeing mongodb database");
    free(db);
}

void mongodb_global_init()
{
    mongoc_log_set_handler(mongo_logger_callback, nullptr);
    mongoc_init();
}

// This should be called once at program end to cleanup mongo library
void mongodb_global_terminate()
{
    mongoc_cleanup();
}
