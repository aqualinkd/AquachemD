#ifndef UOM_H
#define UOM_H

typedef enum {
    UOM_NONE = 0,
//    UOM_CUSTOM,
    UOM_CELSIUS,
    UOM_FAHRENHEIT,
    UOM_PH,
    UOM_MV,
    UOM_PSI,
    UOM_PERCENT,
    UOM_RPM,
    UOM_BYTES,
    UOM_SECONDS,
    UOM_MINUTES,
    UOM_RATIO
} acd_uom_t;

acd_uom_t   parse_uom(const char *str);
const char* uom_to_str(acd_uom_t uom);
const char* uom_to_display_str(acd_uom_t uom); // For display purposes (ie ph is none)

#define IS_UOM_TEMPERATURE(uom) ((uom) == UOM_CELSIUS || (uom) == UOM_FAHRENHEIT)

#endif // UOM_H