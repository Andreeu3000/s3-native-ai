// Forzar el uso del USB nativo (CDC) en MicroPython
// Esto es crucial para que la configuración de ESP-IDF se alinee
#define MICROPY_HW_USB_CDC (1)
#define MICROPY_HW_ENABLE_USBDEV (1)

// Definir los tamaños de buffer de TinyUSB que ESP-IDF espera
#define CFG_TUD_CDC_EP_BUFSIZE (64)
#define CFG_TUD_CDC_RX_BUFSIZE (64)
#define CFG_TUD_CDC_TX_BUFSIZE (64)
