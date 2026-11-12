#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/objarray.h"

#include "quirc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define S3QR_MAX_WIDTH  2048
#define S3QR_MAX_HEIGHT 2048
#define S3QR_MAX_PIXELS 2000000
#define S3QR_TASK_STACK (24 * 1024)

typedef struct {
    size_t len;
    uint8_t *data;
} s3qr_result_t;

typedef struct {
    const uint8_t *image;
    int width;
    int height;

    SemaphoreHandle_t done;
    s3qr_result_t *results;
    int result_count;
    int error;
} s3qr_context_t;

static void s3qr_worker(void *arg) {
    s3qr_context_t *ctx = (s3qr_context_t *)arg;
    struct quirc *qr = quirc_new();

    if (qr == NULL) {
        ctx->error = 1;
        xSemaphoreGive(ctx->done);
        vTaskDelete(NULL);
        return;
    }

    if (quirc_resize(qr, ctx->width, ctx->height) < 0) {
        ctx->error = 2;
        quirc_destroy(qr);
        xSemaphoreGive(ctx->done);
        vTaskDelete(NULL);
        return;
    }

    int image_width = 0;
    int image_height = 0;
    uint8_t *gray = quirc_begin(qr, &image_width, &image_height);

    if (gray == NULL) {
        ctx->error = 3;
        quirc_destroy(qr);
        xSemaphoreGive(ctx->done);
        vTaskDelete(NULL);
        return;
    }

    memcpy(gray, ctx->image, (size_t)ctx->width * ctx->height);
    quirc_end(qr);

    int count = quirc_count(qr);

    if (count > 0) {
        ctx->results = calloc(count, sizeof(s3qr_result_t));

        if (ctx->results == NULL) {
            ctx->error = 4;
        } else {
            for (int i = 0; i < count; i++) {
                struct quirc_code code;
                struct quirc_data data;

                quirc_extract(qr, i, &code);

                if (quirc_decode(&code, &data) != QUIRC_SUCCESS) {
                    continue;
                }

                uint8_t *copy = malloc(data.payload_len ? data.payload_len : 1);
                if (copy == NULL) {
                    continue;
                }

                if (data.payload_len > 0) {
                    memcpy(copy, data.payload, data.payload_len);
                }

                int index = ctx->result_count++;
                ctx->results[index].data = copy;
                ctx->results[index].len = data.payload_len;
            }
        }
    }

    quirc_destroy(qr);
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}

static mp_obj_t s3qr_decode(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(args[0], &buffer, MP_BUFFER_READ);

    mp_int_t width = mp_obj_get_int(args[1]);
    mp_int_t height = mp_obj_get_int(args[2]);

    if (width <= 0 || height <= 0 ||
        width > S3QR_MAX_WIDTH || height > S3QR_MAX_HEIGHT) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid image dimensions"));
    }

    size_t pixels = (size_t)width * (size_t)height;

    if (pixels > S3QR_MAX_PIXELS || buffer.len < pixels) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid GRAY8 buffer size"));
    }

    s3qr_context_t ctx = {0};

    ctx.image = (const uint8_t *)buffer.buf;
    ctx.width = (int)width;
    ctx.height = (int)height;
    ctx.done = xSemaphoreCreateBinary();

    if (ctx.done == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("semaphore allocation failed"));
    }

    BaseType_t created = xTaskCreate(
        s3qr_worker,
        "s3qr",
        S3QR_TASK_STACK,
        &ctx,
        5,
        NULL
    );

    if (created != pdPASS) {
        vSemaphoreDelete(ctx.done);
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("QR task creation failed"));
    }

    xSemaphoreTake(ctx.done, portMAX_DELAY);
    vSemaphoreDelete(ctx.done);

    if (ctx.error != 0) {
        if (ctx.results != NULL) {
            for (int i = 0; i < ctx.result_count; i++) {
                free(ctx.results[i].data);
            }
            free(ctx.results);
        }
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("quirc processing failed"));
    }

    mp_obj_t list = mp_obj_new_list(0, NULL);

    for (int i = 0; i < ctx.result_count; i++) {
        mp_obj_t item = mp_obj_new_bytes(
            (const char *)ctx.results[i].data,
            ctx.results[i].len
        );

        mp_obj_list_append(list, item);
        free(ctx.results[i].data);
    }

    free(ctx.results);
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    s3qr_decode_obj, 3, 3, s3qr_decode
);

static const mp_rom_map_elem_t s3qr_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_s3qr) },
    { MP_ROM_QSTR(MP_QSTR_decode), MP_ROM_PTR(&s3qr_decode_obj) },
};

static MP_DEFINE_CONST_DICT(
    s3qr_module_globals,
    s3qr_module_globals_table
);

const mp_obj_module_t s3qr_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&s3qr_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_s3qr, s3qr_user_cmodule);
