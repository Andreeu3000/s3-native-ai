#include "py/obj.h"
#include "py/runtime.h"

static mp_obj_t s3sys_info(void) {
    mp_obj_t dict = mp_obj_new_dict(0);
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_status), mp_obj_new_str("OK", 2));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_model), mp_obj_new_str("ESP32-S3", 8));
    return dict;
}
static MP_DEFINE_CONST_FUN_OBJ_0(s3sys_info_obj, s3sys_info);

static const mp_rom_map_elem_t s3sys_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_s3sys) },
    { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&s3sys_info_obj) },
};
static MP_DEFINE_CONST_DICT(s3sys_module_globals, s3sys_module_globals_table);

const mp_obj_module_t s3sys_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&s3sys_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_s3sys, s3sys_user_cmodule);
