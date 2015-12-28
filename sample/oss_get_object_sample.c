#include "aos_log.h"
#include "aos_util.h"
#include "aos_string.h"
#include "aos_status.h"
#include "oss_auth.h"
#include "oss_util.h"
#include "oss_api.h"
#include "oss_config.h"
#include "oss_sample_util.h"

void get_object_to_buffer()
{
    aos_pool_t *p;
    aos_string_t bucket;
    aos_string_t object;
    int is_oss_domain = 1;
    oss_request_options_t *options;
    aos_table_t *headers;
    aos_table_t *resp_headers;
    aos_status_t *s;
    aos_list_t buffer;
    aos_buf_t *content;
    char *buf;
    int64_t len = 0;
    int64_t size = 0;
    int64_t pos = 0;

    aos_pool_create(&p, NULL);
    options = oss_request_options_create(p);
    init_sample_request_options(options, is_oss_domain);
    aos_str_set(&bucket, BUCKET_NAME);
    aos_str_set(&object, OBJECT_NAME);
    headers = aos_table_make(p, 0);
    aos_list_init(&buffer);

    s = oss_get_object_to_buffer(options, &bucket, &object, 
                                 headers, &buffer, &resp_headers);

    if (NULL != s && 2 == s->code / 100) {
        printf("get object to buffer succeeded\n");
    }
    else {
        printf("get object to buffer failed\n");  
    }

    //get buffer len
    aos_list_for_each_entry(content, &buffer, node) {
        len += aos_buf_size(content);
    }

    buf = aos_pcalloc(p, len + 1);
    buf[len] = '\0';

    //copy buffer content to memory
    aos_list_for_each_entry(content, &buffer, node) {
        size = aos_buf_size(content);
        memcpy(buf + pos, content->pos, size);
        pos += size;
    }

    aos_pool_destroy(p);
}

void get_object_to_local_file()
{
    aos_pool_t *p;
    aos_string_t bucket;
    char *download_filename = "get_object_to_local_file.txt";
    aos_string_t object;
    int is_oss_domain = 1;
    oss_request_options_t *options;
    aos_table_t *headers;
    aos_table_t *resp_headers;
    aos_status_t *s;
    aos_string_t file;

    aos_pool_create(&p, NULL);
    options = oss_request_options_create(p);
    init_sample_request_options(options, is_oss_domain);
    aos_str_set(&bucket, BUCKET_NAME);
    aos_str_set(&object, OBJECT_NAME);
    headers = aos_table_make(p, 0);
    aos_str_set(&file, download_filename);

    s = oss_get_object_to_file(options, &bucket, &object, headers, 
                               &file, &resp_headers);

    if (NULL != s && 2 == s->code / 100) {
        printf("get object to local file succeeded\n");
    } else {
        printf("get object to local file failed\n");
    }

    aos_pool_destroy(p);
}

void get_object_by_signed_url()
{
    aos_pool_t *p;
    aos_string_t bucket;
    aos_string_t object;
    aos_string_t url;
    int is_oss_domain = 1;
    aos_http_request_t *request = NULL;
    aos_table_t *headers;
    aos_table_t *resp_headers;
    oss_request_options_t *options;
    aos_list_t buffer;
    aos_status_t *s;
    aos_string_t file;
    char *signed_url = NULL;
    int64_t expires_time;

    aos_pool_create(&p, NULL);

    options = oss_request_options_create(p);
    init_sample_request_options(options, is_oss_domain);

    // create request
    request = aos_http_request_create(p);
    request->method = HTTP_GET;

    // create headers
    headers = aos_table_make(options->pool, 0);

    // set value
    aos_str_set(&bucket, BUCKET_NAME);
    aos_str_set(&object, OBJECT_NAME);
    aos_list_init(&buffer);

    // expires time
    expires_time = apr_time_now() / 1000000 + 120;    

    // generate signed url for put 
    signed_url = oss_gen_signed_url(options, &bucket, &object, 
                                    expires_time, request);
    aos_str_set(&url, signed_url);
    
    printf("signed get url : %s\n", signed_url);

    // put object by signed url
    s = oss_get_object_to_buffer_by_url(options, &url, headers, 
            &buffer, &resp_headers);

    if (NULL != s && 2 == s->code / 100) {
        printf("get object by signed url succeeded\n");
    } else {
	printf("get object by signed url failed\n");
    }

    aos_pool_destroy(p);
}

void get_oss_dir_to_local_dir()
{
    aos_pool_t *parent_pool;    
    aos_string_t bucket;
    int is_oss_domain = 1;
    aos_status_t *s;
    oss_request_options_t *options;
    oss_list_object_params_t *params;

    aos_pool_create(&parent_pool, NULL);
    options = oss_request_options_create(parent_pool);
    init_sample_request_options(options, is_oss_domain);
    aos_str_set(&bucket, BUCKET_NAME);
    params = oss_create_list_object_params(parent_pool);
    aos_str_set(&params->prefix, DIR_NAME);
    params->truncated = 1;

    while (params->truncated) {
        aos_pool_t *list_object_pool;
        aos_table_t *list_object_resp_headers;
        oss_list_object_content_t *list_content;
        oss_list_object_common_prefix_t *list_common_prefix;

        aos_pool_create(&list_object_pool, parent_pool);
        options->pool = list_object_pool;
        s = oss_list_object(options, &bucket, params, &list_object_resp_headers);
        if (!aos_status_is_ok(s)) {
            aos_error_log("list objects of dir[%s] fail\n", DIR_NAME);
            aos_status_dup(parent_pool, s);
            aos_pool_destroy(list_object_pool);
            options->pool = parent_pool;
            return;
        }        

        aos_list_for_each_entry(list_content, &params->object_list, node) {
            if ('/' == list_content->key.data[strlen(list_content->key.data) - 1]) {
                apr_dir_make_recursive(list_content->key.data, 
                        APR_OS_DEFAULT, parent_pool);                
            } else {
                aos_string_t object;
                aos_pool_t *get_object_pool;
                aos_table_t *headers;
                aos_table_t *get_object_resp_headers;

                aos_str_set(&object, list_content->key.data);

                aos_pool_create(&get_object_pool, parent_pool);
                options->pool = get_object_pool;
                headers = aos_table_make(options->pool, 0);

                s = oss_get_object_to_file(options, &bucket, &object, 
                        headers, &object, &get_object_resp_headers);
                if (!aos_status_is_ok(s)) {
                    aos_error_log("get object[%s] fail\n", object.data);
                }

                aos_pool_destroy(get_object_pool);
                options->pool = list_object_pool;
            }
        }

        aos_list_init(&params->object_list);
        if (params->next_marker.data) {
            aos_str_set(&params->marker, params->next_marker.data);
        }

        aos_pool_destroy(list_object_pool);
    }

    if (NULL != s && 2 == s->code / 100) {
        printf("get dir succeeded\n");
    } else {
	printf("get dir failed\n");
    }
    aos_pool_destroy(parent_pool);
}

int main(int argc, char *argv[])
{
    if (aos_http_io_initialize("oss_sample", 0) != AOSE_OK) {
        exit(1);
    }
    
    get_object_to_buffer();
    get_object_to_local_file();
    get_object_by_signed_url();

    get_oss_dir_to_local_dir();
    
    aos_http_io_deinitialize();

    return 0;
}