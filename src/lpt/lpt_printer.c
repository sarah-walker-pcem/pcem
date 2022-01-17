#include <stdlib.h>
#include "ibm.h"
#include "filters.h"
#include "lpt.h"

typedef struct esc_printer_t {

} esc_printer_t;

static void esc_write_data(uint8_t val, void *p)
{
}

static void esc_write_ctrl(uint8_t val, void *p)
{
}

static uint8_t esc_read_status(void *p)
{
        return 0;
}

static void *esc_init()
{
        esc_printer_t *escPrinter = malloc(sizeof(esc_printer_t));
        memset(escPrinter, 0, sizeof(esc_printer_t));
                
        return escPrinter;
}
static void esc_close(void *p)
{
        esc_printer_t *escPrinter = (esc_printer_t *)p;
        
        free(escPrinter);
}

lpt_device_t esc_device =
{
        "ESC/P Dot-Matrix Printer",
        esc_init,
        esc_close,
        esc_write_data,
        esc_write_ctrl,
        esc_read_status
};

typedef struct text_printer_t {

} text_printer_t;

static void textprinter_write_data(uint8_t val, void *p)
{
}

static void textprinter_write_ctrl(uint8_t val, void *p)
{
}

static uint8_t textprinter_read_status(void *p)
{
        return 0;
}

static void *textprinter_init()
{
        text_printer_t *textPrinter = malloc(sizeof(text_printer_t));
        memset(textPrinter, 0, sizeof(text_printer_t));
                
        return textPrinter;
}
static void textprinter_close(void *p)
{
        text_printer_t *textPrinter = (text_printer_t *)p;
        
        free(textPrinter);
}

lpt_device_t textprinter_device =
{
        "Text-Only Printer",
        textprinter_init,
        textprinter_close,
        textprinter_write_data,
        textprinter_write_ctrl,
        textprinter_read_status
};
