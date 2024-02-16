#ifndef INCLUSION_GUARD_OVERLAY_COMMANDS_H
#define INCLUSION_GUARD_OVERLAY_COMMANDS_H

#define UPLOAD_BASE "curl --data \"usetransparent=true&colorcode=FFFFFF&usescalabl\
e=true&type=fullcolor&ov_path=%%2Fusr%%2Flocal%%2Fpackages%%2Fapdcustomalarms%%2F\
%s\" http://127.0.0.1/axis-cgi/operator/create_overlay.cgi --user\
 %s:%s --anyauth > /tmp/curl.txt 2> /dev/null"

#define SET_BASE "curl --anyauth -H \"Content-Type: application/json\" --d\
ata \"{ \\\"apiVersion\\\": \\\"1.0\\\", \\\"context\\\": \\\"123\\\",\\\"method\\\": \\\"addImage\\\",\\\"\
params\\\": {\\\"camera\\\": 1,\\\"overlayPath\\\": \\\"/etc/overlays/%s\\\",\\\"\
position\\\": [0.66, -1.0],\\\"zIndex\\\": 1 }}\" http://127.0.0.1/axis-cgi/dynamicover\
lay/dynamicoverlay.cgi --user %s:%s > /tmp/curl.txt 2> /dev/null"

// Q3617 X value 0.91

#define REMOVE_BASE  "curl --anyauth -H \"Content-Type: application/json\" \
--data \"{ \\\"apiVersion\\\": \\\"1.0\\\", \\\"context\\\": \\\"123\\\",\\\"me\
thod\\\": \\\"remove\\\",\\\"params\\\": {\\\"identity\\\": %d}}\" http://127.0\
.0.1/axis-cgi/dynamicoverlay/dynamicoverlay.cgi --user %s:%s > \
/tmp/curl.txt 2> /dev/null"

#define WIPER_BASE "curl --anyauth -H \"Content-Type: application/json\" \
--data \"{ \\\"apiVersion\\\": \\\"1.0\\\", \\\"context\\\": \\\"123\\\",\\\"me\
thod\\\": \\\"start\\\",\\\"params\\\": {\\\"id\\\": 0, \\\"duration\\\": 30}}\" http://127.0\
.0.1/axis-cgi/clearviewcontrol.cgi --user %s:%s"

// > \
///tmp/curl.txt 2> /dev/null""

#endif // INCLUSION_GUARD_OVERLAY_COMMANDS_H