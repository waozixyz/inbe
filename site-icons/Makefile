# Icon sizes for Inner Breeze
# Run 'make' to regenerate from ../flint/icons/inbe.png

SOURCE_PNG := ../flint/icons/inbe.png

.PHONY: all clean

all: favicon-32x32.png apple-touch-icon.png icon-192x192.png icon-512x512.png maskable-192x192.png maskable-512x512.png

favicon-32x32.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -filter lanczos -resize 32x32 -strip +set date:create +set date:modify +set date:timestamp -define png:exclude-chunk=time $@

apple-touch-icon.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -filter lanczos -resize 180x180 -strip +set date:create +set date:modify +set date:timestamp -define png:exclude-chunk=time $@

icon-192x192.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -filter lanczos -resize 192x192 -strip +set date:create +set date:modify +set date:timestamp -define png:exclude-chunk=time $@

icon-512x512.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -filter lanczos -resize 512x512 -strip +set date:create +set date:modify +set date:timestamp -define png:exclude-chunk=time $@

maskable-192x192.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -filter lanczos -resize 153x153 -background none -gravity center -extent 192x192 \( -size 192x192 xc:"#87CEEB" \) +swap -compose over -composite -strip +set date:create +set date:modify +set date:timestamp -define png:exclude-chunk=time $@

maskable-512x512.png: $(SOURCE_PNG)
	magick $(SOURCE_PNG) -filter lanczos -resize 409x409 -background none -gravity center -extent 512x512 \( -size 512x512 xc:"#87CEEB" \) +swap -compose over -composite -strip +set date:create +set date:modify +set date:timestamp -define png:exclude-chunk=time $@

clean:
	rm -f favicon-32x32.png apple-touch-icon.png icon-192x192.png icon-512x512.png maskable-192x192.png maskable-512x512.png
