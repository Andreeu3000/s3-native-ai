#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "py/runtime.h"
#include "py/obj.h"
#include "esp_dsp.h"

#ifndef CONFIG_DSP_MAX_FFT_SIZE
#define CONFIG_DSP_MAX_FFT_SIZE 4096
#endif

#define S3_STR2(x) #x
#define S3_STR(x) S3_STR2(x)

static bool fft_ready = false;

static void check(esp_err_t e) {
    if (e != ESP_OK) {
        mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("esp-dsp error 0x%x"), (unsigned)e);
    }
}

static float *get_f32(mp_obj_t o, size_t *n, bool write) {
    mp_buffer_info_t bi;
    mp_get_buffer_raise(o, &bi, write ? MP_BUFFER_RW : MP_BUFFER_READ);
    if (bi.typecode != 'f') {
        mp_raise_ValueError(MP_ERROR_TEXT("need array('f')"));
    }
    if (((uintptr_t)bi.buf & 15) != 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer not 16-byte aligned"));
    }
    *n = bi.len / sizeof(float);
    return (float *)bi.buf;
}

static void get_pair(mp_obj_t a_in, mp_obj_t b_in, float **a, float **b, size_t *n) {
    size_t na, nb;
    *a = get_f32(a_in, &na, false);
    *b = get_f32(b_in, &nb, false);
    if (na != nb || na == 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("length mismatch or empty"));
    }
    *n = na;
}

static mp_obj_t s3dsp_dot(mp_obj_t a_in, mp_obj_t b_in) {
    float *a, *b; size_t n;
    get_pair(a_in, b_in, &a, &b, &n);
    float r = 0.0f;
    check(dsps_dotprod_f32(a, b, &r, (int)n));
    return mp_obj_new_float(r);
}
static MP_DEFINE_CONST_FUN_OBJ_2(s3dsp_dot_obj, s3dsp_dot);

static mp_obj_t s3dsp_dot_ansi(mp_obj_t a_in, mp_obj_t b_in) {
    float *a, *b; size_t n;
    get_pair(a_in, b_in, &a, &b, &n);
    float r = 0.0f;
    check(dsps_dotprod_f32_ansi(a, b, &r, (int)n));
    return mp_obj_new_float(r);
}
static MP_DEFINE_CONST_FUN_OBJ_2(s3dsp_dot_ansi_obj, s3dsp_dot_ansi);

static mp_obj_t s3dsp_add(mp_obj_t a_in, mp_obj_t b_in, mp_obj_t o_in) {
    float *a, *b; size_t n, no;
    get_pair(a_in, b_in, &a, &b, &n);
    float *o = get_f32(o_in, &no, true);
    if (no != n) {
        mp_raise_ValueError(MP_ERROR_TEXT("output length mismatch"));
    }
    check(dsps_add_f32(a, b, o, (int)n, 1, 1, 1));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(s3dsp_add_obj, s3dsp_add);

static mp_obj_t s3dsp_mul(mp_obj_t a_in, mp_obj_t b_in, mp_obj_t o_in) {
    float *a, *b; size_t n, no;
    get_pair(a_in, b_in, &a, &b, &n);
    float *o = get_f32(o_in, &no, true);
    if (no != n) {
        mp_raise_ValueError(MP_ERROR_TEXT("output length mismatch"));
    }
    check(dsps_mul_f32(a, b, o, (int)n, 1, 1, 1));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(s3dsp_mul_obj, s3dsp_mul);

// FFT en sitio. Buffer complejo intercalado: re0, im0, re1, im1...
static mp_obj_t s3dsp_fft(mp_obj_t buf_in) {
    size_t nf;
    float *d = get_f32(buf_in, &nf, true);
    if ((nf & 1) != 0 || nf < 16) {
        mp_raise_ValueError(MP_ERROR_TEXT("need 2*N floats, N>=8"));
    }
    int N = (int)(nf / 2);
    if ((N & (N - 1)) != 0 || N > CONFIG_DSP_MAX_FFT_SIZE) {
        mp_raise_ValueError(MP_ERROR_TEXT("N must be power of 2 and <= max"));
    }
    if (!fft_ready) {
        check(dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE));
        fft_ready = true;
    }
    check(dsps_fft2r_fc32(d, N));
    check(dsps_bit_rev_fc32(d, N));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(s3dsp_fft_obj, s3dsp_fft);

// Qué implementación se compiló realmente (evidencia, no suposición)
static mp_obj_t s3dsp_impl(void) {
    const char *s[3] = {
        S3_STR(dsps_dotprod_f32),
        S3_STR(dsps_add_f32),
        S3_STR(dsps_mul_f32),
    };
    mp_obj_t items[3];
    for (int i = 0; i < 3; i++) {
        items[i] = mp_obj_new_str(s[i], strlen(s[i]));
    }
    return mp_obj_new_tuple(3, items);
}
static MP_DEFINE_CONST_FUN_OBJ_0(s3dsp_impl_obj, s3dsp_impl);

static const mp_rom_map_elem_t s3dsp_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_s3dsp) },
    { MP_ROM_QSTR(MP_QSTR_dot), MP_ROM_PTR(&s3dsp_dot_obj) },
    { MP_ROM_QSTR(MP_QSTR_dot_ansi), MP_ROM_PTR(&s3dsp_dot_ansi_obj) },
    { MP_ROM_QSTR(MP_QSTR_add), MP_ROM_PTR(&s3dsp_add_obj) },
    { MP_ROM_QSTR(MP_QSTR_mul), MP_ROM_PTR(&s3dsp_mul_obj) },
    { MP_ROM_QSTR(MP_QSTR_fft), MP_ROM_PTR(&s3dsp_fft_obj) },
    { MP_ROM_QSTR(MP_QSTR_impl), MP_ROM_PTR(&s3dsp_impl_obj) },
};
static MP_DEFINE_CONST_DICT(s3dsp_module_globals, s3dsp_module_globals_table);

const mp_obj_module_t s3dsp_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&s3dsp_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_s3dsp, s3dsp_user_cmodule);
