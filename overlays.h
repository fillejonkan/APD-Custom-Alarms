#ifndef INCLUSION_GUARD_OVERLAYS_H
#define INCLUSION_GUARD_OVERLAYS_H

/**
 * Initialize overlays
 */
void init_overlays(const char *username, const char *password, gboolean red);

/**
 * Cleanup overlays
 */
void cleanup_overlays();

/**
* Remove the red overlay from the image.
*/
void remove_red();

/**
* Remove the green overlay from the image.
*/
void remove_green();

/**
* Set the red overlay as active
*/
int set_red();

/**
* Set the green overlay as active
*/
int set_green();

/**
* Upload the overlays from file and make the usable by system
*/
void upload_overlays();

/**
* Remove existing dynamic overlays
*/
void remove_existing_overlays();

/**
 * Run wiper for 30 seconds
 */
void run_wiper();


#endif // INCLUSION_GUARD_OVERLAYS_H