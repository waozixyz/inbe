# Icon sizes for Inner Breeze
# Run 'make' to regenerate from ../../icons/inbe.png

SOURCE_PNG := ../../icons/lotus.png

.PHONY: all clean

all: favicon-32x32.png apple-touch-icon.png icon-192x192.png icon-512x512.png

favicon-32x32.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -interpolate Nearest -filter point -resize 32x32 $@

apple-touch-icon.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -interpolate Nearest -filter point -resize 180x180 $@

icon-192x192.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -interpolate Nearest -filter point -resize 192x192 $@

icon-512x512.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -interpolate Nearest -filter point -resize 512x512 $@

clean:
	rm -f favicon-32x32.png apple-touch-icon.png icon-192x192.png icon-512x512.png
