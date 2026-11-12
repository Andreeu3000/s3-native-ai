#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/obj.h"

static uint8_t *get_u8(mp_obj_t obj, size_t *len, bool write) {
    mp_buffer_info_t bi;

    mp_get_buffer_raise(
        obj,
        &bi,
        write ? MP_BUFFER_RW : MP_BUFFER_READ
    );

    *len = bi.len;
    return (uint8_t *)bi.buf;
}

static int get_dim(mp_obj_t obj) {
    mp_int_t v = mp_obj_get_int(obj);

    if (v <= 0 || v > 8192) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid dimension"));
    }

    return (int)v;
}

static void check_size(size_t actual, size_t required) {
    if (actual < required) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
    }
}

/* RGB888 -> GRAY8 */
static mp_obj_t img_grayscale(size_t n, const mp_obj_t *args) {
    if (n != 5) {
        mp_raise_ValueError(MP_ERROR_TEXT(
            "grayscale(src,dst,w,h,channels)"
        ));
    }

    size_t sl, dl;

    uint8_t *src = get_u8(args[0], &sl, false);
    uint8_t *dst = get_u8(args[1], &dl, true);

    int w = get_dim(args[2]);
    int h = get_dim(args[3]);
    int ch = mp_obj_get_int(args[4]);

    if (ch != 3 && ch != 4) {
        mp_raise_ValueError(MP_ERROR_TEXT("channels must be 3 or 4"));
    }

    size_t pixels = (size_t)w * h;

    if (pixels > SIZE_MAX / ch) {
        mp_raise_ValueError(MP_ERROR_TEXT("image too large"));
    }

    check_size(sl, pixels * ch);
    check_size(dl, pixels);

    for (size_t i = 0; i < pixels; i++) {
        uint8_t r = src[i * ch];
        uint8_t g = src[i * ch + 1];
        uint8_t b = src[i * ch + 2];

        dst[i] = (uint8_t)(
            (77 * r + 150 * g + 29 * b) >> 8
        );
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    img_grayscale_obj, 5, 5, img_grayscale
);

/* Invertir colores */
static mp_obj_t img_invert(mp_obj_t buf_obj) {
    size_t len;

    uint8_t *buf = get_u8(buf_obj, &len, true);

    for (size_t i = 0; i < len; i++) {
        buf[i] = 255 - buf[i];
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_1(img_invert_obj, img_invert);

/* GRAY8 -> imagen monocromatica */
static mp_obj_t img_threshold(
    mp_obj_t src_obj,
    mp_obj_t dst_obj,
    mp_obj_t threshold_obj
) {
    size_t sl, dl;

    uint8_t *src = get_u8(src_obj, &sl, false);
    uint8_t *dst = get_u8(dst_obj, &dl, true);

    int threshold = mp_obj_get_int(threshold_obj);

    if (threshold < 0 || threshold > 255) {
        mp_raise_ValueError(MP_ERROR_TEXT("threshold 0..255"));
    }

    check_size(dl, sl);

    for (size_t i = 0; i < sl; i++) {
        dst[i] = src[i] >= threshold ? 255 : 0;
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_3(
    img_threshold_obj, img_threshold
);

/* Sobel: deteccion de bordes sobre GRAY8 */
static mp_obj_t img_sobel(size_t n_args, const mp_obj_t *args) {
    if (n_args != 4) {
        mp_raise_TypeError(MP_ERROR_TEXT("sobel(src,dst,width,height)"));
    }

    mp_obj_t src_obj = args[0];
    mp_obj_t dst_obj = args[1];
    mp_obj_t w_obj = args[2];
    mp_obj_t h_obj = args[3];
    size_t sl, dl;

    uint8_t *src = get_u8(src_obj, &sl, false);
    uint8_t *dst = get_u8(dst_obj, &dl, true);

    int w = get_dim(w_obj);
    int h = get_dim(h_obj);

    size_t pixels = (size_t)w * h;

    check_size(sl, pixels);
    check_size(dl, pixels);

    if (src == dst) {
        mp_raise_ValueError(MP_ERROR_TEXT("Sobel needs separate buffers"));
    }

    memset(dst, 0, pixels);

    if (w < 3 || h < 3) {
        return mp_const_none;
    }

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            size_t p = (size_t)y * w + x;

            int gx =
                -src[p-w-1] + src[p-w+1]
                -2*src[p-1] + 2*src[p+1]
                -src[p+w-1] + src[p+w+1];

            int gy =
                -src[p-w-1] - 2*src[p-w] - src[p-w+1]
                +src[p+w-1] + 2*src[p+w] + src[p+w+1];

            int magnitude = (abs(gx) + abs(gy)) >> 2;

            if (magnitude > 255) {
                magnitude = 255;
            }

            dst[p] = (uint8_t)magnitude;
        }
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    img_sobel_obj, 4, 4, img_sobel
);

/* Resize por vecino mas cercano */
static mp_obj_t img_resize(
    size_t n,
    const mp_obj_t *args
) {
    if (n != 7) {
        mp_raise_ValueError(MP_ERROR_TEXT(
            "resize(src,dst,sw,sh,dw,dh,channels)"
        ));
    }

    size_t sl, dl;

    uint8_t *src = get_u8(args[0], &sl, false);
    uint8_t *dst = get_u8(args[1], &dl, true);

    int sw = get_dim(args[2]);
    int sh = get_dim(args[3]);
    int dw = get_dim(args[4]);
    int dh = get_dim(args[5]);
    int ch = mp_obj_get_int(args[6]);

    if (ch < 1 || ch > 4) {
        mp_raise_ValueError(MP_ERROR_TEXT("channels 1..4"));
    }

    size_t src_pixels = (size_t)sw * sh;
    size_t dst_pixels = (size_t)dw * dh;

    if (src_pixels > SIZE_MAX / ch ||
        dst_pixels > SIZE_MAX / ch) {
        mp_raise_ValueError(MP_ERROR_TEXT("image too large"));
    }

    check_size(sl, src_pixels * ch);
    check_size(dl, dst_pixels * ch);

    if (src == dst) {
        mp_raise_ValueError(MP_ERROR_TEXT("resize needs separate buffers"));
    }

    for (int y = 0; y < dh; y++) {
        int sy = (int)((uint64_t)y * sh / dh);

        for (int x = 0; x < dw; x++) {
            int sx = (int)((uint64_t)x * sw / dw);

            size_t si = ((size_t)sy * sw + sx) * ch;
            size_t di = ((size_t)y * dw + x) * ch;

            memcpy(dst + di, src + si, ch);
        }
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    img_resize_obj, 7, 7, img_resize
);

static const mp_rom_map_elem_t img_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_s3img) },

    { MP_ROM_QSTR(MP_QSTR_grayscale), MP_ROM_PTR(&img_grayscale_obj) },
    { MP_ROM_QSTR(MP_QSTR_invert), MP_ROM_PTR(&img_invert_obj) },
    { MP_ROM_QSTR(MP_QSTR_threshold), MP_ROM_PTR(&img_threshold_obj) },
    { MP_ROM_QSTR(MP_QSTR_sobel), MP_ROM_PTR(&img_sobel_obj) },
    { MP_ROM_QSTR(MP_QSTR_resize), MP_ROM_PTR(&img_resize_obj) },
};

static MP_DEFINE_CONST_DICT(
    img_globals,
    img_globals_table
);

const mp_obj_module_t s3img_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&img_globals,
};

MP_REGISTER_MODULE(
    MP_QSTR_s3img,
    s3img_user_cmodule
);

