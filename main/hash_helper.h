#define khash_insert_unique_str(_table_type, _table_ptr, _key, _value, _error) \
    do { \
        int r; \
        khint_t k = kh_get(_table_type, _table_ptr, _key); \
        if (k == kh_end(_table_ptr)) { \
            k = kh_put(_table_type, _table_ptr, _key, &r); \
            if (r >= 0) { \
                kh_value(_table_ptr, k) = _value; \
            } else { \
                ESP_LOGE(TAG, "Unable to create %s", _key); \
                abort(); \
            } \
        } else { \
            ESP_LOGE(TAG, _error ": %s", _key); \
        } \
    } while(0);


#define khash_insert_str(_table_type, _table_ptr, _key, _value, _type) \
    do { \
        int r; \
        khint_t k = kh_put(_table_type, _table_ptr, _key, &r); \
        if (r >= 0) { \
            kh_value(_table_ptr, k) = _value; \
        } else { \
            ESP_LOGE(TAG, "Unable to create %s", _key); \
            abort(); \
        } \
    } while(0); 

#define khash_del_str(_table_type, _table_ptr, _key, _error) \
    do { \
        khint_t k = kh_get(_table_type, _table_ptr, _key); \
        if (k != kh_end(_table_ptr)) { \
            kh_del(_table_type, _table_ptr, k); \
        } else { \
            ESP_LOGE(TAG, _error ": %s", _key); \
        } \
    } while(0);

#define khash_insert_unique_ptr(_table_type, _table_ptr, _key, _value, _error) \
    do { \
        int r; \
        khint_t k = kh_get(_table_type, _table_ptr, (uintptr_t)_key); \
        if (k == kh_end(_table_ptr)) { \
            k = kh_put(_table_type, _table_ptr, (uintptr_t)_key, &r); \
            if (r >= 0) { \
                kh_value(_table_ptr, k) = _value; \
            } else { \
                ESP_LOGE(TAG, "Unable to create %p", _key); \
                abort(); \
            } \
        } else { \
            ESP_LOGE(TAG, _error ": %p", _key); \
        } \
    } while(0);


#define khash_insert_ptr(_table_type, _table_ptr, _key, _value, _type) \
    do { \
        int r; \
        khint_t k = kh_put(_table_type, _table_ptr, (uintptr_t)_key, &r); \
        if (r >= 0) { \
            kh_value(_table_ptr, k) = _value; \
        } else { \
            ESP_LOGE(TAG, "Unable to create %p", _key); \
            abort(); \
        } \
    } while(0); 

#define khash_del_ptr(_table_type, _table_ptr, _key, _error) \
    do { \
        khint_t k = kh_get(_table_type, _table_ptr, (uintptr_t)_key); \
        if (k != kh_end(_table_ptr)) { \
            kh_del(_table_type, _table_ptr, k); \
        } else { \
            ESP_LOGE(TAG, _error ": %p", _key); \
        } \
    } while(0);


