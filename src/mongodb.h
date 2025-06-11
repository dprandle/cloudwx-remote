#pragma once

#include "global_constants.h"

struct mongodb_ctxt;

inline constexpr const char *URI_PROD_SERVER = "mongodb+srv://remote:btbGU0zv0FCCBYBV@prod.vew9qwt.mongodb.net/"
                                               "?retryWrites=true&retryreads=true&w=majority&maxPoolSize=150&socketTimeoutMS=60000&"
                                               "waitQueueTimeoutMS=60000&serverSelectionTimeoutMS=60000";

inline constexpr const char *DB_NAME = "wxdb";

struct raw_metar_entry {
    small_str id;
    char text[WHISPER_CHAR_BUF_SZ];
};

mongodb_ctxt *mongodb_create(const char *uri_str, const char *db_name);
void mongodb_destroy(mongodb_ctxt *db);

void mongodb_save_item(mongodb_ctxt *db, const char *collection_name, const raw_metar_entry &item, char*err_buff);

const char* mongodb_get_name(mongodb_ctxt *db);
const char *mongodb_get_uri(mongodb_ctxt *db);

bool mongodb_init(mongodb_ctxt *db);
void mongodb_terminate(mongodb_ctxt *db);

// This should be called once at program start to init mongo library
void mongodb_global_init();

// This should be called once at program end to cleanup mongo library
void mongodb_global_terminate();

