#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>

#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <axsdk/axevent.h>

#include "overlays.h"
#include "camera/camera.h"

/******************** MACRO DEFINITION SECTION ********************************/

/**
 * APP ID used for logs etc.
 */
#define APP_ID              "apdcustomalarms"

/**
 * Nice name used for App
 */
#define APP_NICE_NAME       "APD Custom Alarms"

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
 * Main context for GLib.
 */
static GMainLoop *loop;

/**
 * Handle for ACAP event subsystem
 */
static AXEventHandler *event_handler;

/**
 * Handle for output event
 */
static guint output_event_handle;

/**
 * Subscription ID for zone crossing alarm.
 */
static int subscription_scenario1 = -1;

/**
 * Subscription ID for conditional zone alarm.
 */
static int subscription_scenario2 = -1;

/**
 * Current status of APD Zone crossing alarm
 */
static gboolean scenario1_enabled = FALSE;

/**
 * Current status of APD Conditional zone alarm
 */
static gboolean scenario2_enabled = FALSE;

/**
 * Current "logical" alarm status, i.e. 'AND' of the two alarms
 */
gboolean alarm_status = FALSE;

/**
 * Current value of Scenario 1 parameter
 */
static char *par_scenario1 = NULL;

/**
 * Current value of Scenario 2 parameter
 */
static char *par_scenario2 = NULL;

/**
 * Current value of username parameter
 */
static char *par_username  = NULL;

/**
 * Current value of password parameter
 */
static char *par_password  = NULL;

/******************** LOCAL FUNCTION DECLARATION SECTION **********************/

/**
 * Declare event to use for third party applications to trigger on.
 */
static guint declare_external_event();

/**
 * Send an update to third party apps listening for the combined alarm.
 */
static void update_external_event();

/**
 * CB Function for APD events.
 */
static void apd_event_callback(guint subscription,
    AXEvent *event, guint *token);

/**
 * Subscribe to the specified APD Scenario event.
 */
static guint apd_event_subscribe(int preset_no);

/**
 * Set alarm status - remove green overlay, add red overlay and update status
 */
static void set_alarm();

/**
 * Clear alarm status - remove red overlay, add green overlay and update status
 */
static void clear_alarm();

/**
 * Quit the application when terminate signals is being sent
 */
static void handle_sigterm(int signo);

/**
 * Register callback to SIGTERM and SIGINT signals
 */
static void init_signals();

/**
 * Callback function for changes to Scenario 1 parameter
 * update APD event subscription
 */
static void set_scenario1(const char *value);

/**
 * Callback function for changes to Scenario 2 parameter
 * update APD event subscription
 */
static void set_scenario2(const char *value);

/**
 * Callback function for changes to Username parameter
 * update overlay API credentials if needed
 */
static void set_username(const char *value);

/**
 * Callback function for changes to Password parameter
 * update overlay API credentials if needed
 */
static void set_password(const char *value);

/**
 * Serve back parameter values for web page
 */
static void api_settings_test_reporting(CAMERA_HTTP_Reply http,
                                        CAMERA_HTTP_Options options);

/**
 * Update parameters based on web page input
 */
static void api_settings_set(CAMERA_HTTP_Reply http,
                             CAMERA_HTTP_Options options);

/******************** LOCAL FUNCTION DEFINTION SECTION ************************/

/**
 * Declare event to use for third party applications to trigger on.
 */
static guint declare_external_event()
{
    AXEventKeyValueSet *set = NULL;
    gboolean enabled = FALSE;

    set = ax_event_key_value_set_new();

    /* Initialize an AXEventKeyValueSet that looks like this
     *
     *    tnsaxis:topic0=CameraApplicationPlatform
     *    tnsaxis:topic1=APDCustomAlarm
     *    feature=CombinedAlarm
     *    enabled=0 <-- The initial value will be set to 0/false
     */
    gboolean result = ax_event_key_value_set_add_key_values(set, NULL,
        "topic0", "tnsaxis", "CameraApplicationPlatform", AX_VALUE_TYPE_STRING,
        "topic1", "tnsaxis", "APDCustomAlarm", AX_VALUE_TYPE_STRING,
        "feature", NULL, "CombinedAlarm", AX_VALUE_TYPE_STRING,
        "enabled", "tnsaxis", &enabled, AX_VALUE_TYPE_BOOL, NULL);

    if (!result) {
        ERR("Could not add key values to ax_event_key_value_set");
        goto error;
    }

    ax_event_key_value_set_mark_as_source(set, "feature", NULL, NULL);
    ax_event_key_value_set_mark_as_data(set, "enabled", "tnsaxis", NULL);
    ax_event_key_value_set_mark_as_user_defined(set, "feature", NULL,
        "tag-on-key-value", NULL);
    ax_event_key_value_set_mark_as_user_defined(set, "topic1", "tnsaxis",
        "tag-on-key-value", NULL);
    ax_event_key_value_set_add_nice_names(set, "enabled", "tnsaxis",
        "Alarm Active", "Value nice name", NULL);

    /* The AXEventKeyValueSet has been initialized, now it's time to declare the
     * event.
     */
    result = ax_event_handler_declare(event_handler, set, FALSE,
        &output_event_handle,
        NULL, NULL, NULL);

    if (!result) {
        ERR("Could not declare event");
        goto error;
    }

    error:

    ax_event_key_value_set_free(set);

    return output_event_handle;
}

/**
 * Send an update to third party apps listening for the combined alarm.
 */
static void update_external_event()
{
    AXEventKeyValueSet *set = ax_event_key_value_set_new();
    GTimeVal time_stamp;

    ax_event_key_value_set_add_key_value(set, "enabled", "tnsaxis",
                                         &alarm_status,
                                         AX_VALUE_TYPE_BOOL, NULL);

    /* Create the event */
    AXEvent *event = ax_event_new(set, &time_stamp);

    ax_event_key_value_set_free(set);

    /* Send the event */
    ax_event_handler_send_event(event_handler,
      output_event_handle,
      event,
      NULL);

    ax_event_free(event);
}

/**
 * CB Function for APD events.
 */
static void apd_event_callback(guint subscription,
    AXEvent *event, guint *token)
{
    const AXEventKeyValueSet *key_value_set;
    gboolean enabled;

    (void) token;

    /* Extract the AXEventKeyValueSet from the event. */
    key_value_set = ax_event_get_key_value_set(event);

    int preset_no = -1;
    int on_preset = -1;

    ax_event_key_value_set_get_boolean(key_value_set,
        "on_preset", NULL, &on_preset, NULL);

    ax_event_key_value_set_get_integer(key_value_set,
        "PresetToken", NULL, &preset_no, NULL);

    

    LOG("Got preset_no %d, on preset %d", preset_no, on_preset);

    /**
     * Only run wiper for second preset
     */
    if (preset_no == 2 & on_preset == 1) {
        LOG("RUNNING WIPER");
        run_wiper();
    }
}

/**
 * Subscribe to the specified APD Scenario event.
 */
static guint apd_event_subscribe(int preset_no)
{
    AXEventKeyValueSet *key_value_set;
    guint subscription;
    gboolean result;

    key_value_set = ax_event_key_value_set_new();

    LOG("Subscribing to event %d", preset_no);

    /* Initialize an AXEventKeyValueSet that matches the manual trigger event.
    *
    * tns1:topic0=Device
    * tnsaxis:topic1=IO
    * tnsaxis:topic2=VirtualPort
    * port=&port  <-- Subscribe to "port" input argument
    * state=*     <-- Subscribe to all states
    */
    ax_event_key_value_set_add_key_values(key_value_set,
        NULL,
        "topic0", "tns1", "PTZController", AX_VALUE_TYPE_STRING,
        "topic1", "tnsaxis", "PTZPresets", AX_VALUE_TYPE_STRING,
        "topic2", NULL, "Channel_1", AX_VALUE_TYPE_STRING,
        "PresetToken", NULL, &preset_no, AX_VALUE_TYPE_INT,
        "on_preset", NULL, NULL, AX_VALUE_TYPE_INT, NULL);

    /* Time to setup the subscription. Use the "token" input argument as
    * input data to the callback function "subscription callback"
    */
    result = ax_event_handler_subscribe(event_handler, key_value_set,
        &subscription, (AXSubscriptionCallback)apd_event_callback, NULL,
        NULL);

    if (!result) {
        ERR("Failed to subscribe to event %d", preset_no);
    } else {
        LOG("Subscribing to event %d", preset_no);
    }

    /* The key/value set is no longer needed */
    ax_event_key_value_set_free(key_value_set);

    return subscription;
}

/**
 * Set alarm status - remove green overlay, add red overlay and update status
 */
static void set_alarm()
{
    if (!alarm_status) {
        LOG("Setting Alarm mode ACTIVE");
        set_red();
    }

    alarm_status = TRUE;
}

/**
 * Clear alarm status - remove red overlay, add green overlay and update status
 */
static void clear_alarm()
{
    if (alarm_status) {
        LOG("Setting Alarm mode INACTIVE");
        set_green();
    }

    alarm_status = FALSE;
}

/**
 * Quit the application when terminate signals is being sent
 */
static void handle_sigterm(int signo)
{
    LOG("GOT SIGTERM OR SIGINT, EXIT APPLICATION");

    if (loop) {
        g_main_loop_quit(loop);
    }
}

/**
 * Register callback to SIGTERM and SIGINT signals
 */
static void init_signals()
{
    struct sigaction sa;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

/**
 * Callback function for changes to Scenario 1 parameter
 * update APD event subscription
 */
static void set_scenario1(const char *value)
{
    if (g_strcmp0(value, par_scenario1) != 0) {
        g_free(par_scenario1);
        par_scenario1 = g_strdup(value);

        LOG("Got new Scenario1 %s", par_scenario1);

        /* Unsubscribe to any previous event */
        if (subscription_scenario1 != -1) {
            LOG("Unsubscribing to event %d", subscription_scenario1);
            ax_event_handler_unsubscribe(event_handler, subscription_scenario1,
                NULL);
        }

        subscription_scenario1 = apd_event_subscribe(1);
    }
}

/**
 * Callback function for changes to Scenario 2 parameter
 * update APD event subscription
 */
static void set_scenario2(const char *value)
{
    if (g_strcmp0(value, par_scenario2) != 0) {
        g_free(par_scenario2);
        par_scenario2 = g_strdup(value);

        LOG("Got new Scenario2 %s", par_scenario2);

        /* Unsubscribe to any previous event */
        if (subscription_scenario2 != -1) {
            LOG("Unsubscribing to event %d", subscription_scenario2);
            ax_event_handler_unsubscribe(event_handler, subscription_scenario2,
                NULL);
        }

        subscription_scenario2 = apd_event_subscribe(2);
    }
}

/**
 * Callback function for changes to Username parameter
 * update overlay API credentials if needed
 */
static void set_username(const char *value)
{
    if (g_strcmp0(value, par_username) != 0) {
        g_free(par_username);
        par_username = g_strdup(value);

        LOG("Got new Username %s", par_username);

        if (par_password != NULL) {
            init_overlays(par_username, par_password, alarm_status);
        }
    }
}

/**
 * Callback function for changes to Password parameter
 * update overlay API credentials if needed
 */
static void set_password(const char *value)
{
    if (g_strcmp0(value, par_password) != 0) {
        g_free(par_password);
        par_password = g_strdup(value);

        LOG("Got new Password %s", par_password);

        if (par_username != NULL) {
            init_overlays(par_username, par_password, alarm_status);
        }
    }
}

/**
 * Update parameters based on web page input (Used for SaveAll feature)
 */
static void api_settings_get(CAMERA_HTTP_Reply http,
                             CAMERA_HTTP_Options options)
{
  camera_http_sendXMLheader(http);
  camera_http_output(http, "<settings>");
  camera_http_output(http, "<param name='Scenario1' value='%s'/>",
    par_scenario1);
  camera_http_output(http, "<param name='Scenario2' value='%s'/>",
    par_scenario2);
  camera_http_output(http, "<param name='Username' value='%s'/>",
    par_username);
  camera_http_output(http, "<param name='Password' value='%s'/>",
    par_password);
  camera_http_output(http, "</settings>");
}

/**
 * Update parameters based on web page input (Used for SaveAll feature)
 */
static void api_settings_set(CAMERA_HTTP_Reply http,
                             CAMERA_HTTP_Options options)
{
  const char *value;
  const char *param;

  camera_http_sendXMLheader(http);

  param = camera_http_getOptionByName(options, "param");
  value = camera_http_getOptionByName(options, "value");

  if(!(param && value)) {
    camera_http_output(http,
        "<error description='Syntax: param or value missing'/>");

    ERR("api_settings_set: param or value is missing\n");
    return;
  }

  if(!camera_param_set(param, value)) {
    camera_http_output(http, "<error description='Could not set %s to %s'/>",
        param, value);

    ERR("api_settings_set: Could not set %s to %s\n", param, value);
    return;
  }
  camera_http_output(http, "<success/>");
}

/******************** GLOBAL FUNCTION DEFINTION SECTION ***********************/

/**
 * Main entry point for application
 */
int main(int argc, char *argv[])
{
    openlog(APP_ID, LOG_PID | LOG_CONS, LOG_USER);
    camera_init(APP_ID, APP_NICE_NAME);

    init_signals();

    loop = g_main_loop_new(NULL, FALSE);

    /* Create an AXEventHandler */
    event_handler = ax_event_handler_new();

    char value[50];
    if(camera_param_get("Scenario1", value, 50)) {
        set_scenario1(value);
    }

    if(camera_param_get("Scenario2", value, 50)) {
        set_scenario2(value);
    }

    if(camera_param_get("Username", value, 50)) {
        set_username(value);
    }

    if(camera_param_get("Password", value, 50)) {
        set_password(value);
    }

    camera_param_setCallback("Scenario1", set_scenario1);
    camera_param_setCallback("Scenario2", set_scenario2);
    camera_param_setCallback("Username", set_username);
    camera_param_setCallback("Password", set_password);

    camera_http_setCallback("settings/get", api_settings_get);
    camera_http_setCallback("settings/set", api_settings_set);

    output_event_handle = declare_external_event();

    LOG("Got external event declaration: %d", output_event_handle);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    loop = NULL;

    camera_cleanup();
    closelog();
    cleanup_overlays();

    g_free(par_scenario1);
    g_free(par_scenario2);
    g_free(par_username);
    g_free(par_password);

    /* TODO: This locks the program on termination for some reason.
    ax_event_handler_free(event_handler);
    */

    return 0;
}
