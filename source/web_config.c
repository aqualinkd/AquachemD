

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "cJSON.h"
#include "acd_types.h"
#include "config.h"
#include "utils.h"

#define WEBCONFIGFILE "/config.json"

int save_web_config_json(const char* inBuf, int inSize, char* outBuf, int outSize, struct aquachemdata *acdata)
{
    FILE *fp;
    char configfile[256];
    
    snprintf(configfile, sizeof(configfile), "%s%s", _acdconfig_.web_directory, WEBCONFIGFILE);

    cJSON *envelope = cJSON_ParseWithLength(inBuf, inSize);
    if (envelope == NULL) {
        LOG(LOG_ERR, "Bad JSON web config envelope payload\n");
        return sprintf(outBuf, "{\"error\":\"Invalid JSON format\"}");
    }

    cJSON *values = cJSON_GetObjectItemCaseSensitive(envelope, "values");
    if (values == NULL) {
        LOG(LOG_ERR, "Web config missing 'values' object: '%s'\n", inBuf);
        cJSON_Delete(envelope);
        return sprintf(outBuf, "{\"error\":\"Missing configuration data\"}");
    }

    char *formatted_json = cJSON_Print(values);
    if (formatted_json == NULL) {
        LOG(LOG_ERR, "Failed to render formatted configuration\n");
        cJSON_Delete(envelope);
        return sprintf(outBuf, "{\"error\":\"Internal allocation error\"}");
    }

    fp = fopen(configfile, "w");
    
    if (fp == NULL) {
        LOG(LOG_ERR, "Open config file failed '%s'\n", configfile);
        cJSON_free(formatted_json);
        cJSON_Delete(envelope);
        return sprintf(outBuf, "{\"error\":\"Failed to open file for writing\"}");
    }

    fputs(formatted_json, fp);
    fputc('\n', fp);
    
    fclose(fp);
    
    LOG(LOG_INFO, "Web config saved successfully to %s\n", configfile);

    cJSON_free(formatted_json);
    cJSON_Delete(envelope); // Automatically deletes child elements too

    return sprintf(outBuf, "{\"message\":\"Saved Web Config\"}");
}