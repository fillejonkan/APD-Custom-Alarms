#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>

#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cJSON.h"
#include "overlays.h"
#include "overlay_commands.h"

/******************** MACRO DEFINITION SECTION ********************************/

/**
 * Log message macro
 */
#define LOG(fmt, args...)   { syslog(LOG_INFO, fmt, ## args); \
    g_message(fmt, ## args); }

/**
 * Error message macro
 */
#define ERR(fmt, args...)   { syslog(LOG_ERR, fmt, ## args); \
    g_warning(fmt, ## args); }

/******************** LOCAL VARIABLE DECLARATION SECTION **********************/

/**
* Current identity of the green overlay
*/
static int green_identity = -1;

/**
* Current identity of the red overlay
*/
static int red_identity   = -1;

/**
 * Username for overlay commands
 */
static char *username = NULL;

/**
 * Password for overlay commands
 */
static char *password = NULL;

/******************** LOCAL FUNCTION DECLARATION SECTION **********************/

/**
* Read JSON response from temporary file
*/
static char *get_json_buffer();

/**
* Parse overlay identity from JSON response to 'addImage' command
*/
static int get_ovl_identity(const char *json_string);

/**
 * Upload overlay from ACAPs folder.
 */
static void upload_overlay(const char *path);

/******************** LOCAL FUNCTION DEFINTION SECTION ************************/

/**
* Read JSON response from temporary file
*/
static char *get_json_buffer()
{
    char *buffer = 0;
    long length;
    FILE *f = fopen ("/tmp/curl.txt", "rb");

    if (f) {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = malloc (length);
        if (buffer) {
            fread (buffer, 1, length, f);
        }
    }

    fclose(f);

    return buffer;
}

/**
* Parse overlay identity from JSON response to 'addImage' command
*/
static int get_ovl_identity(const char *json_string)
{
    int ret = -1;

    cJSON *root = cJSON_Parse(json_string);

    cJSON *data_json =  cJSON_GetObjectItemCaseSensitive(root,
        "data");

    if (data_json) {
        cJSON *identity_json = cJSON_GetObjectItemCaseSensitive(data_json,
        "identity");

        if (cJSON_IsNumber(identity_json)) {
            ret = identity_json->valuedouble;
        } else {
            ret = -1;
        }
    }

    cJSON_Delete(root);

    if (ret < 0) {
        ERR("Failed to get overlay identity");
        ERR("%s", json_string);
    }

    return ret;
}

/**
 * Upload overlay from ACAPs folder.
 */
static void upload_overlay(const char *path)
{
    char *cmd = g_strdup_printf(UPLOAD_BASE, path, username, password);

    system(cmd);

    g_free(cmd);
}

/******************** GLOBAL FUNCTION DEFINTION SECTION ***********************/

/**
 * Initialize overlays
 */
void init_overlays(const char *user, const char *pass, gboolean red)
{
    g_free(username);
    g_free(password);

    username = g_strdup(user);
    password = g_strdup(pass);
}

/**
 * Cleanup overlays
 */
void cleanup_overlays()
{
    g_free(username);
    g_free(password);

    /* Purposely leaving the uploaded overlay images behind. */
}

/**
* Remove the red overlay from the image.
*/
void remove_red()
{
    LOG("Removing red with identity: %d", red_identity);

    if (red_identity > 0) {
        gchar *cmd = g_strdup_printf(REMOVE_BASE, red_identity, username,
            password);
        system(cmd);
        g_free(cmd);
    }

    red_identity = -1;
}

/**
* Remove the green overlay from the image.
*/
void remove_green()
{
    LOG("Removing green with identity: %d", green_identity);

    if (green_identity > 0) {
        gchar *cmd = g_strdup_printf(REMOVE_BASE, green_identity, username,
            password);
        system(cmd);
        g_free(cmd);
    }

    green_identity = -1;
}

/**
* Set the red overlay as active
*/
int set_red()
{
    int ret = -1;

    if (red_identity >= 0) {
        return 0;
    }

    if (green_identity >= 0) {
        remove_green();
    }

    char *cmd = g_strdup_printf(SET_BASE, "red_quarter.ovl", username,
        password);

    system(cmd);
    g_free(cmd);

    char *buffer = get_json_buffer();

    if (buffer) {
        red_identity = get_ovl_identity(buffer);
        LOG("Got Red identity: %d", red_identity);
        free(buffer);
        ret = 0;
    }

    return ret;
}

/**
* Set the green overlay as active
*/
int set_green()
{
    int ret = -1;

    if (green_identity >= 0) {
        return 0;
    }

    if (red_identity >= 0) {
        remove_red();
    }

    char *cmd = g_strdup_printf(SET_BASE, "green_quarter.ovl", username,
        password);

    system(cmd);
    g_free(cmd);

    char *buffer = get_json_buffer();

    if (buffer) {
        green_identity = get_ovl_identity(buffer);
        LOG("Got Green identity: %d", green_identity);
        free(buffer);
        ret = 0;
    }

    return ret;
}

/**
* Upload the overlays from file and make the usable by system
*/
void upload_overlays()
{
    upload_overlay("green_quarter.bmp");
    upload_overlay("red_quarter.bmp");
}

/**
* Remove existing dynamic overlays
*/
void remove_existing_overlays()
{
    size_t i = 1;
    gchar *cmd;

    for (; i <= 6; i++) {
        cmd = g_strdup_printf(REMOVE_BASE, i, username, password);
        system(cmd);
        g_free(cmd);
    }

    red_identity = green_identity = -1;
}

/**
* Run wiper for 30 seconds
*/
void run_wiper()
{
    int ret = -1;

    char *cmd = g_strdup_printf(WIPER_BASE, username,
        password);

    LOG("Complete command %s", cmd);

    system(cmd);
    g_free(cmd);
}




