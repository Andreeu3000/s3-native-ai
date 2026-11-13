#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "extmod/vfs.h"

#include "esp_heap_caps.h"
#include "esp_memory_utils.h"

#include "lodepng.h"

#define S3IMG_MAX_WIDTH       4096
#define S3IMG_MAX_HEIGHT      4096
#define S3IMG_MAX_PIXELS      1000000
#define S3IMG_MAX_FILE_SIZE   (4 * 1024 * 1024)
#define S3IMG_READ_CHUNK      1024

#define S3IMG_MODE_RGBA8888   1
#define S3IMG_MODE_GRAY8      2

typedef struct {
    mp_obj_base_t base;
    uint8_t *pixels;
    size_t nbytes;
    uint32_t width;
    uint32_t height;
    uint8_t mode;
} s3img_image_t;

static const mp_obj_type_t s3img_image_type;

/* =========================================================
 * MEMORY
 * PSRAM first, internal RAM fallback.
 * ========================================================= */

static void *s3img_malloc(size_t size) {
    if (size == 0) {
        size = 1;
    }

    void *ptr = heap_caps_malloc(
        size,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (ptr == NULL) {
        ptr = heap_caps_malloc(
            size,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
        );
    }

    return ptr;
}

static void *s3img_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return s3img_malloc(size);
    }

    if (size == 0) {
        heap_caps_free(ptr);
        return NULL;
    }

    void *result = heap_caps_realloc(
        ptr,
        size,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (result == NULL) {
        result = heap_caps_realloc(
            ptr,
            size,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
        );
    }

    return result;
}

static void s3img_free(void *ptr) {
    if (ptr != NULL) {
        heap_caps_free(ptr);
    }
}

/* LodePNG custom allocators. */
void *lodepng_malloc(size_t size) {
    return s3img_malloc(size);
}

void *lodepng_realloc(void *ptr, size_t size) {
    return s3img_realloc(ptr, size);
}

void lodepng_free(void *ptr) {
    s3img_free(ptr);
}

/* =========================================================
 * IMAGE OBJECT
 * ========================================================= */

static s3img_image_t *s3img_new_image(void) {
    s3img_image_t *img =
        mp_obj_malloc_with_finaliser(
            s3img_image_t,
            &s3img_image_type
        );

    img->pixels = NULL;
    img->nbytes = 0;
    img->width = 0;
    img->height = 0;
    img->mode = 0;

    return img;
}

static void s3img_release_image(s3img_image_t *img) {
    if (img->pixels != NULL) {
        s3img_free(img->pixels);
        img->pixels = NULL;
    }

    img->nbytes = 0;
    img->width = 0;
    img->height = 0;
    img->mode = 0;
}

static mp_obj_t s3img_image_del(mp_obj_t self_in) {
    s3img_image_t *img = MP_OBJ_TO_PTR(self_in);
    s3img_release_image(img);
    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_1(
    s3img_image_del_obj,
    s3img_image_del
);

static void s3img_image_print(
    const mp_print_t *print,
    mp_obj_t self_in,
    mp_print_kind_t kind
) {
    (void)kind;

    s3img_image_t *img = MP_OBJ_TO_PTR(self_in);

    const char *mode =
        img->mode == S3IMG_MODE_RGBA8888 ? "RGBA8888" :
        img->mode == S3IMG_MODE_GRAY8 ? "GRAY8" :
        "CLOSED";

    mp_printf(
        print,
        "<Image %ux%u %s %u bytes>",
        (unsigned)img->width,
        (unsigned)img->height,
        mode,
        (unsigned)img->nbytes
    );
}

static mp_int_t s3img_image_get_buffer(
    mp_obj_t self_in,
    mp_buffer_info_t *bufinfo,
    mp_uint_t flags
) {
    (void)flags;

    s3img_image_t *img = MP_OBJ_TO_PTR(self_in);

    if (img->pixels == NULL || img->nbytes == 0) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("image has no pixel data")
        );
    }

    bufinfo->buf = img->pixels;
    bufinfo->len = img->nbytes;
    bufinfo->typecode = 'B';

    return 0;
}

static void s3img_image_attr(
    mp_obj_t self_in,
    qstr attr,
    mp_obj_t *dest
) {
    if (dest[0] != MP_OBJ_NULL) {
        return;
    }

    s3img_image_t *img = MP_OBJ_TO_PTR(self_in);

    if (img->pixels == NULL) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("image has no pixel data")
        );
    }

    if (attr == MP_QSTR_width) {
        dest[0] = mp_obj_new_int_from_uint(img->width);
    } else if (attr == MP_QSTR_height) {
        dest[0] = mp_obj_new_int_from_uint(img->height);
    } else if (attr == MP_QSTR_nbytes) {
        dest[0] = mp_obj_new_int_from_uint(img->nbytes);
    } else if (attr == MP_QSTR_stride) {
        unsigned channels =
            img->mode == S3IMG_MODE_RGBA8888 ? 4 : 1;

        dest[0] = mp_obj_new_int_from_uint(
            img->width * channels
        );
    } else if (attr == MP_QSTR_is_psram) {
        dest[0] = mp_obj_new_bool(
            esp_ptr_external_ram(img->pixels)
        );
    } else if (attr == MP_QSTR_mode) {
        const char *mode =
            img->mode == S3IMG_MODE_RGBA8888
            ? "RGBA8888"
            : "GRAY8";

        dest[0] = mp_obj_new_str_from_cstr(mode);
    } else if (attr == MP_QSTR_size) {
        mp_obj_t dimensions[2] = {
            mp_obj_new_int_from_uint(img->width),
            mp_obj_new_int_from_uint(img->height)
        };

        dest[0] = mp_obj_new_tuple(2, dimensions);
    } else {
        dest[1] = MP_OBJ_SENTINEL;
    }
}

/* =========================================================
 * IMAGE METHODS
 * ========================================================= */

static mp_obj_t s3img_convert_gray(mp_obj_t self_in) {
    s3img_image_t *src = MP_OBJ_TO_PTR(self_in);

    if (src->pixels == NULL) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("image has no pixel data")
        );
    }

    size_t pixels =
        (size_t)src->width * src->height;

    s3img_image_t *dst = s3img_new_image();

    dst->width = src->width;
    dst->height = src->height;
    dst->mode = S3IMG_MODE_GRAY8;
    dst->nbytes = pixels;

    dst->pixels = s3img_malloc(pixels);

    if (dst->pixels == NULL) {
        mp_raise_msg(
            &mp_type_MemoryError,
            MP_ERROR_TEXT("cannot allocate grayscale image")
        );
    }

    if (src->mode == S3IMG_MODE_GRAY8) {
        memcpy(dst->pixels, src->pixels, pixels);
        return MP_OBJ_FROM_PTR(dst);
    }

    for (size_t i = 0; i < pixels; i++) {
        const uint8_t *p = src->pixels + i * 4;

        uint32_t r = p[0];
        uint32_t g = p[1];
        uint32_t b = p[2];
        uint32_t a = p[3];

        /* Composite transparency over white. */
        r = (r * a + 255 * (255 - a) + 127) / 255;
        g = (g * a + 255 * (255 - a) + 127) / 255;
        b = (b * a + 255 * (255 - a) + 127) / 255;

        dst->pixels[i] = (uint8_t)(
            (77 * r + 150 * g + 29 * b) >> 8
        );
    }

    return MP_OBJ_FROM_PTR(dst);
}

static MP_DEFINE_CONST_FUN_OBJ_1(
    s3img_convert_gray_obj,
    s3img_convert_gray
);

static const mp_rom_map_elem_t s3img_image_locals_table[] = {
    {
        MP_ROM_QSTR(MP_QSTR_convert_gray),
        MP_ROM_PTR(&s3img_convert_gray_obj)
    },
    {
        MP_ROM_QSTR(MP_QSTR___del__),
        MP_ROM_PTR(&s3img_image_del_obj)
    },
};

static MP_DEFINE_CONST_DICT(
    s3img_image_locals,
    s3img_image_locals_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    s3img_image_type,
    MP_QSTR_Image,
    MP_TYPE_FLAG_NONE,
    print, s3img_image_print,
    attr, s3img_image_attr,
    buffer, s3img_image_get_buffer,
    locals_dict, &s3img_image_locals
);

/* =========================================================
 * PNG LOADER
 * ========================================================= */

static uint32_t s3img_read_be32(const uint8_t *p) {
    return
        ((uint32_t)p[0] << 24) |
        ((uint32_t)p[1] << 16) |
        ((uint32_t)p[2] << 8) |
        ((uint32_t)p[3]);
}

static void s3img_close_file(mp_obj_t file) {
    mp_stream_close(file);
}

static mp_obj_t s3img_load(mp_obj_t path_obj) {
    if (!mp_obj_is_str(path_obj)) {
        mp_raise_TypeError(
            MP_ERROR_TEXT("path must be a string")
        );
    }

    /* Allocate the GC object before external buffers. */
    s3img_image_t *img = s3img_new_image();

    mp_obj_t open_args[2] = {
        path_obj,
        mp_obj_new_str("rb", 2)
    };

    mp_obj_t file = mp_vfs_open(
        2,
        open_args,
        NULL
    );

    uint8_t *file_data = s3img_malloc(4096);
    size_t file_capacity = 4096;
    size_t file_length = 0;

    if (file_data == NULL) {
        s3img_close_file(file);
        mp_raise_msg(
            &mp_type_MemoryError,
            MP_ERROR_TEXT("cannot allocate PNG input buffer")
        );
    }

    uint8_t chunk[S3IMG_READ_CHUNK];

    /* Read through MicroPython VFS, not POSIX fopen(). */
    for (;;) {
        int errcode = 0;

        mp_uint_t n = mp_stream_rw(
            file,
            chunk,
            sizeof(chunk),
            &errcode,
            MP_STREAM_RW_READ
        );

        if (n == MP_STREAM_ERROR) {
            s3img_close_file(file);
            s3img_free(file_data);

            mp_raise_OSError(
                errcode != 0 ? errcode : MP_EIO
            );
        }

        if (n == 0) {
            break;
        }

        if (file_length + n > S3IMG_MAX_FILE_SIZE) {
            s3img_close_file(file);
            s3img_free(file_data);

            mp_raise_ValueError(
                MP_ERROR_TEXT("PNG file exceeds 4 MiB limit")
            );
        }

        size_t required = file_length + n;

        if (required > file_capacity) {
            size_t new_capacity = file_capacity;

            while (new_capacity < required) {
                new_capacity *= 2;

                if (new_capacity > S3IMG_MAX_FILE_SIZE) {
                    new_capacity = S3IMG_MAX_FILE_SIZE;
                    break;
                }
            }

            uint8_t *new_data = s3img_realloc(
                file_data,
                new_capacity
            );

            if (new_data == NULL) {
                s3img_close_file(file);
                s3img_free(file_data);

                mp_raise_msg(
                    &mp_type_MemoryError,
                    MP_ERROR_TEXT("PNG input buffer growth failed")
                );
            }

            file_data = new_data;
            file_capacity = new_capacity;
        }

        memcpy(
            file_data + file_length,
            chunk,
            n
        );

        file_length += n;
    }

    s3img_close_file(file);

    /* Validate PNG signature and IHDR dimensions. */
    static const uint8_t png_signature[8] = {
        137, 80, 78, 71, 13, 10, 26, 10
    };

    if (
        file_length < 24 ||
        memcmp(file_data, png_signature, 8) != 0
    ) {
        s3img_free(file_data);

        mp_raise_ValueError(
            MP_ERROR_TEXT("invalid PNG file")
        );
    }

    uint32_t width = s3img_read_be32(file_data + 16);
    uint32_t height = s3img_read_be32(file_data + 20);

    if (
        width == 0 ||
        height == 0 ||
        width > S3IMG_MAX_WIDTH ||
        height > S3IMG_MAX_HEIGHT ||
        (uint64_t)width * height > S3IMG_MAX_PIXELS
    ) {
        s3img_free(file_data);

        mp_raise_ValueError(
            MP_ERROR_TEXT("PNG dimensions exceed limits")
        );
    }

    /* Decode into RGBA8888. LodePNG uses our PSRAM allocator. */
    unsigned decoded_width = 0;
    unsigned decoded_height = 0;
    unsigned char *decoded = NULL;

    unsigned error = lodepng_decode32(
        &decoded,
        &decoded_width,
        &decoded_height,
        file_data,
        file_length
    );

    s3img_free(file_data);

    if (error != 0 || decoded == NULL) {
        lodepng_free(decoded);

        mp_raise_ValueError(
            MP_ERROR_TEXT("LodePNG decode failed")
        );
    }

    if (
        decoded_width != width ||
        decoded_height != height
    ) {
        lodepng_free(decoded);

        mp_raise_ValueError(
            MP_ERROR_TEXT("PNG dimensions mismatch")
        );
    }

    img->pixels = decoded;
    img->width = width;
    img->height = height;
    img->mode = S3IMG_MODE_RGBA8888;
    img->nbytes = (size_t)width * height * 4;

    return MP_OBJ_FROM_PTR(img);
}

static MP_DEFINE_CONST_FUN_OBJ_1(
    s3img_load_obj,
    s3img_load
);

/* =========================================================
 * EXISTING RAW IMAGE FUNCTIONS
 * Keep the existing API intact.
 * ========================================================= */

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
        mp_raise_ValueError(
            MP_ERROR_TEXT("invalid dimension")
        );
    }

    return (int)v;
}

static void check_size(size_t actual, size_t required) {
    if (actual < required) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("buffer too small")
        );
    }
}

/* RGB888/RGBA8888 -> GRAY8 */
static mp_obj_t img_grayscale(size_t n, const mp_obj_t *args) {
    if (n != 5) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("grayscale(src,dst,w,h,channels)")
        );
    }

    size_t sl, dl;

    uint8_t *src = get_u8(args[0], &sl, false);
    uint8_t *dst = get_u8(args[1], &dl, true);

    int w = get_dim(args[2]);
    int h = get_dim(args[3]);
    int ch = mp_obj_get_int(args[4]);

    if (ch != 3 && ch != 4) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("channels must be 3 or 4")
        );
    }

    size_t pixels = (size_t)w * h;

    if (pixels > SIZE_MAX / ch) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("image too large")
        );
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

/* Invert */
static mp_obj_t img_invert(mp_obj_t buf_obj) {
    size_t len;
    uint8_t *buf = get_u8(buf_obj, &len, true);

    for (size_t i = 0; i < len; i++) {
        buf[i] = 255 - buf[i];
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_1(
    img_invert_obj,
    img_invert
);

/* GRAY8 threshold */
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
        mp_raise_ValueError(
            MP_ERROR_TEXT("threshold 0..255")
        );
    }

    check_size(dl, sl);

    for (size_t i = 0; i < sl; i++) {
        dst[i] = src[i] >= threshold ? 255 : 0;
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_3(
    img_threshold_obj,
    img_threshold
);

/* Sobel */
static mp_obj_t img_sobel(size_t n_args, const mp_obj_t *args) {
    if (n_args != 4) {
        mp_raise_TypeError(
            MP_ERROR_TEXT("sobel(src,dst,width,height)")
        );
    }

    size_t sl, dl;

    uint8_t *src = get_u8(args[0], &sl, false);
    uint8_t *dst = get_u8(args[1], &dl, true);

    int w = get_dim(args[2]);
    int h = get_dim(args[3]);

    size_t pixels = (size_t)w * h;

    check_size(sl, pixels);
    check_size(dl, pixels);

    if (src == dst) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("Sobel needs separate buffers")
        );
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

/* Resize nearest-neighbor */
static mp_obj_t img_resize(size_t n, const mp_obj_t *args) {
    if (n != 7) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("resize(src,dst,sw,sh,dw,dh,channels)")
        );
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
        mp_raise_ValueError(
            MP_ERROR_TEXT("channels 1..4")
        );
    }

    size_t src_pixels = (size_t)sw * sh;
    size_t dst_pixels = (size_t)dw * dh;

    if (
        src_pixels > SIZE_MAX / ch ||
        dst_pixels > SIZE_MAX / ch
    ) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("image too large")
        );
    }

    check_size(sl, src_pixels * ch);
    check_size(dl, dst_pixels * ch);

    if (src == dst) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("resize needs separate buffers")
        );
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

/* =========================================================
 * MODULE
 * ========================================================= */

static const mp_rom_map_elem_t img_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_s3img) },

    { MP_ROM_QSTR(MP_QSTR_Image), MP_ROM_PTR(&s3img_image_type) },
    { MP_ROM_QSTR(MP_QSTR_load), MP_ROM_PTR(&s3img_load_obj) },

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

MP_REGISTER_MODULE(MP_QSTR_s3img, s3img_user_cmodule);
