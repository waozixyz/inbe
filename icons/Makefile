# Icon sizes for Inner Breeze
# Run 'make' to regenerate from ../../assets/inbe.png

SOURCE_PNG := ../../assets/inbe.png

.PHONY: all clean

all: favicon-32x32.png apple-touch-icon.png icon-192x192.png icon-512x512.png

favicon-32x32.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -resize 32x32 $@

apple-touch-icon.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -resize 180x180 $@

icon-192x192.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -resize 192x192 $@

icon-512x512.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -resize 512x512 $@

clean:
	rm -f favicon-32x32.png apple-touch-icon.png icon-192x192.png icon-512x512.png
