< /$objtype/mkfile

# Native Plan 9 build of Inbe (Kryon libdraw backend). build/plan9 is
# prepared on the host by: make kry-c-plan9
#
# sqlite stays stubbed here: 8c panics the kernel compiling the
# amalgamation, so habit data is not persisted on Plan 9 yet (the VFS
# and the amalgamation rewrite remain in the tree for when the compiler
# can take them).

TARG=inbe
ROOT=/sys/src/inbe

list=$ROOT/build/plan9/generated-c-files.txt
gensrc=`{cat $list | grep -v -e 'storage/import.c' -e 'meditation/meditation_music.c'}
appsrc=src/platform/plan9/inbe_plan9_main.c \
	src/platform/plan9/sqlite3_stub.c \
	src/platform/plan9/storage_import_stub.c \
	src/platform/plan9/meditation_music_stub.c \
	src/app/app.c \
	src/app/app_audio.c \
	src/app/app_fonts.c \
	src/app/audio_library.c \
	src/app/app_update_check.c \
	src/app/app_update_zip.c \
	src/app/app_web_bridge.c \
	src/platform/inbe_activity_monitor.c \
	src/storage/storage.c \
	src/storage/storage_habits.c \
	src/storage/storage_habit_materialize.c \
	src/storage/storage_habit_sync.c \
	src/storage/storage_json_builder.c \
	src/storage/sync_client.c
hostsrc=build/plan9/inbe_embedded_assets.c
APPCPPFLAGS=-I$ROOT/build/plan9/generated \
	-I$ROOT/src -I$ROOT/src/app -I$ROOT/src/core -I$ROOT/src/screens \
	-I$ROOT/src/screens/settings -I$ROOT/src/practices -I$ROOT/src/practices/whm \
	-I$ROOT/src/practices/meditation -I$ROOT/src/practices/sun_salutation \
	-I$ROOT/src/storage -I$ROOT/src/platform -I$ROOT/src/platform/android \
	-I$ROOT/src/third_party -I$ROOT/vendor-builds/sqlite \
	-DANDROID_BUILD=0 -DPLATFORM_DESKTOP=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES=1 \
	-DINBE_DISABLE_KRYON_FILE_DIALOG=1 -DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 \
	-DSQLITE_OMIT_LOAD_EXTENSION=1 -DSQLITE_OMIT_WAL=1 -DSQLITE_TEMP_STORE=3
LDLIBS=-lmemlayer -lbio -lregexp -lString

< /sys/src/kryon/mk/plan9-app.mk
