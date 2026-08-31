< /$objtype/mkfile

# Native Plan 9 build of Inbe (Kryon libdraw backend).
#
# build/plan9 must be prepared on the host before building here:
#
#	make kry-c && python3 scripts/prepare-plan9-generated-c.py
#
# which rewrites the k2c output for 8c and generates
# build/plan9/{generated, generated-c-files.txt, include,
# inbe_embedded_assets.c, sqlite3_plan9.c}. This mkfile compiles those
# together with the native entry point, the Plan 9 sqlite VFS (real
# sqlite instead of the old test stub), and links against
# /$objtype/lib/libkryon.a.

TARG=inbe
ROOT=/sys/src/inbe
KRYON=/sys/src/kryon
BIN=/$objtype/bin
OUT=$O.out

obj=$ROOT/build/plan9/obj
list=$ROOT/build/plan9/generated-c-files.txt

CPPFLAGS=-I$KRYON/src/platform/plan9/include -I$KRYON/include -I$KRYON/src -I$KRYON/src/ui -I$KRYON/vendor/raylib/src/external \
	-I$ROOT/build/plan9/include -I$ROOT/src -I$ROOT/src/app -I$ROOT/src/core -I$ROOT/src/screens -I$ROOT/src/screens/settings \
	-I$ROOT/src/practices -I$ROOT/src/practices/whm -I$ROOT/src/practices/meditation -I$ROOT/src/practices/sun_salutation \
	-I$ROOT/src/storage -I$ROOT/src/platform -I$ROOT/src/platform/android -I$ROOT/src/third_party \
	-I$ROOT/vendor-builds/sqlite -I$ROOT/build/plan9/generated -I$ROOT/build/plan9/generated/src \
	-DANDROID_BUILD=0 -DPLATFORM_DESKTOP=1 -DKRYON_BACKEND_LIBDRAW=1 -DKRYON_PLATFORM_PLAN9=1 -DKRYON_NATIVE_PLAN9=1 \
	-DUI_EMBEDDED_ONLY=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES=1 -DINBE_DISABLE_KRYON_FILE_DIALOG=1 \
	-DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION=1 -DSQLITE_OMIT_WAL=1 -DSQLITE_TEMP_STORE=3

CFLAGS=-FTVw

gensrc=`{cat $list}
appsrc=src/platform/plan9/inbe_plan9_main.c \
	src/platform/plan9/sqlite_plan9_vfs.c \
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
	src/storage/sync_client.c
hostsrc=build/plan9/inbe_embedded_assets.c build/plan9/sqlite3_plan9.c

allsrc=`{echo $gensrc $appsrc $hostsrc | tr ' ' '\12' | grep -v '^$' | grep -v 'storage/import.c$' | grep -v 'meditation/meditation_music.c$'}
OFILES=`{echo $allsrc | tr ' ' '\12' | sed -e 's@\.c$@.8@' -e 's@^@'$obj'/@'}

check:V:
	if(! test -f $list) {
		echo 'missing '^$list^'; run scripts/prepare-plan9-generated-c.py on the host first' >[1=2]
		exit missing
	}

all:V: check $OUT

install:V: check $BIN/$TARG

$BIN/$TARG: $OUT
	cp $OUT $BIN/$TARG

$OUT: $OFILES /$objtype/lib/libkryon.a
	$LD -o $target $prereq -lkryon -ldraw -lmemdraw -lmemlayer -lbio -lregexp -lString

$obj/%.8: %.c
	mkdir -p `{echo $target | sed 's@/[^/]*$@@'} && cpp -+ $CPPFLAGS $prereq > $obj/$stem.i && $CC $CFLAGS -o $target -c $obj/$stem.i && rm -f $obj/$stem.i

clean:V:
	rm -rf $obj [$OS].out
