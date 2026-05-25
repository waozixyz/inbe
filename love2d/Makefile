# LÖVE2D build

.PHONY: all clean web

all:
	for dir in */; do \
		if [ "$$dir" != "build/" ]; then \
			cd "$$dir" && zip -r ../"$${dir%/}".love *; \
			cd ..; \
		fi; \
	done

web: all
	npx love.js -t inbe inbe.love build/inbe -c
	rm build/inbe/index.html

clean:
	rm -f *.love
	rm -rf build
