#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root_dir=$(dirname -- "$script_dir")
out_dir="$root_dir/build/site"
web_dir="$root_dir/build/dist/web"

read_version() {
	awk '
		/^#define INBE_VERSION_STRING "/ {
			gsub(/^#define INBE_VERSION_STRING "/, "")
			gsub(/"$/, "")
			print
			found = 1
			exit
		}
		END {
			if (!found) exit 1
		}
	' "$root_dir/src/core/version.h"
}

require_path() {
	if [ ! -e "$1" ]; then
		printf 'Error: required path missing: %s\n' "$1" >&2
		exit 1
	fi
}

copy_path() {
	src=$1
	dst=$2
	require_path "$src"
	mkdir -p "$(dirname -- "$dst")"
	cp -R "$src" "$dst"
}

copy_dir_contents() {
	src=$1
	dst=$2
	require_path "$src"
	mkdir -p "$dst"
	cp -R "$src"/. "$dst"/
}

expand_template_file() {
	src=$1
	dst=$2
	version=$3
	mkdir -p "$(dirname -- "$dst")"
	sed "s#\\\${version}#$version#g" "$src" > "$dst"
}

copy_template_file() {
	template_src=$1
	template_dst=$2
	template_version=$3

	case ${template_src##*.} in
		html|htm|txt|xml|json|css|js)
			expand_template_file "$template_src" "$template_dst" "$template_version"
			;;
		*)
			copy_path "$template_src" "$template_dst"
			;;
	esac
}

copy_template_dir() {
	template_src_dir=$1
	template_dst_dir=$2
	template_version=$3

	require_path "$template_src_dir"
	find "$template_src_dir" -type d -print | while IFS= read -r dir_path; do
		rel=${dir_path#"$template_src_dir"}
		mkdir -p "$template_dst_dir$rel"
	done
	find "$template_src_dir" -type f -print | while IFS= read -r file_path; do
		rel=${file_path#"$template_src_dir"/}
		copy_template_file "$file_path" "$template_dst_dir/$rel" "$template_version"
	done
}

write_site_imports() {
	cat > "$out_dir/style.css" <<'EOF'
@import url('/css/base.css');
@import url('/css/components.css');
@import url('/theme.css');
EOF
}

write_web_app_csp_html() {
	src=$1
	dst=$2
	cache_version=$(date +%s)
	csp="default-src 'self' data: blob:; connect-src 'self' https://api.waozi.xyz wss://api.waozi.xyz; script-src 'self' 'unsafe-eval' 'unsafe-inline' https://telegram.org; style-src 'self' 'unsafe-inline'; worker-src 'self' 'unsafe-eval' 'unsafe-inline' data: blob:; frame-src https:; img-src data: https:; media-src https:; object-src 'none'"
	meta="<meta http-equiv=\"Content-Security-Policy\" content=\"$csp\">"

	awk -v meta="$meta" -v version="$cache_version" '
		{
			gsub(/src="index\.js(\?v=[0-9A-Za-z._-]+)?"/, "src=\"index.js?v=" version "\"")
			if ($0 ~ /<meta http-equiv="Content-Security-Policy"/) {
				if (!done) print meta
				done = 1
				next
			}
			print
			if (!done && !inserted && $0 ~ /<meta charset="[^"]+"/) {
				print "    " meta
				inserted = 1
				done = 1
			}
			if (!done && $0 ~ /<\/head>/) {
				print "    " meta
				done = 1
			}
		}
	' "$src" > "$dst"
}

write_telegram_web_app_html() {
	src=$1
	dst=$2
	awk '
		BEGIN {
			viewport = "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, shrink-to-fit=no, viewport-fit=cover\">"
			css = "      html, body {\n        width: 100%;\n        height: 100%;\n        min-height: 100%;\n        overflow: hidden;\n        position: fixed;\n        inset: 0;\n        user-select: none;\n        -webkit-user-select: none;\n        -webkit-tap-highlight-color: transparent;\n        overscroll-behavior: none;\n        touch-action: none;\n      }\n\n      body {\n        background-color: var(--tg-theme-bg-color, #181818);\n        color: var(--tg-theme-text-color, #ffffff);\n      }\n\n      canvas,\n      canvas.emscripten {\n        display: block;\n        width: 100vw !important;\n        height: 100vh !important;\n        height: 100dvh !important;\n        touch-action: none;\n      }"
			script = "    <script src=\"https://telegram.org/js/telegram-web-app.js\"></script>\n    <script>\n      (function() {\n        var tg = window.Telegram && window.Telegram.WebApp;\n        if (tg) {\n          tg.ready();\n          tg.expand();\n          if (tg.disableVerticalSwipes) tg.disableVerticalSwipes();\n          if (tg.setBackgroundColor) {\n            tg.setBackgroundColor((tg.themeParams && tg.themeParams.bg_color) || '\\''#181818'\\'');\n          }\n          if (tg.setHeaderColor) {\n            tg.setHeaderColor((tg.themeParams && tg.themeParams.bg_color) || '\\''#181818'\\'');\n          }\n        }\n\n        document.addEventListener('\\''touchmove'\\'', function(event) {\n          event.preventDefault();\n        }, { passive: false });\n\n        document.addEventListener('\\''gesturestart'\\'', function(event) {\n          event.preventDefault();\n        });\n      })();\n    </script>"
		}
		{
			if ($0 ~ /<meta name="viewport"/) {
				print "\t" viewport
				viewport_done = 1
				next
			}
			if (!viewport_done && $0 ~ /<\/head>/) {
				print "\t" viewport
				viewport_done = 1
			}
			if ($0 ~ /<\/style>/) {
				print css
				css_done = 1
			}
			if (!css_done && $0 ~ /<\/head>/) {
				print "<style>" css "\n    </style>"
				css_done = 1
			}
			if (!script_done && $0 ~ /<script>/) {
				print script
				script_done = 1
			}
			if (!script_done && $0 ~ /<\/body>/) {
				print script
				script_done = 1
			}
			print
		}
	' "$src" > "$dst"
}

require_output() {
	path=$1
	if [ ! -e "$out_dir/$path" ]; then
		printf 'Error: required output missing: %s\n' "$path" >&2
		exit 1
	fi
}

version=$(read_version) || {
	printf 'Error: could not read app version from %s\n' "$root_dir/src/core/version.h" >&2
	exit 1
}

rm -rf "$out_dir"
mkdir -p "$out_dir"

copy_dir_contents "$script_dir/css" "$out_dir/css"
copy_path "$script_dir/themes/inbe.css" "$out_dir/theme.css"
write_site_imports
copy_template_dir "$script_dir/static" "$out_dir" "$version"
expand_template_file "$script_dir/index.html" "$out_dir/index.html" "$version"

copy_dir_contents "$root_dir/web-assets" "$out_dir/web-assets"
copy_dir_contents "$root_dir/site-icons" "$out_dir/site-icons"

copy_dir_contents "$web_dir" "$out_dir/build/web"
write_web_app_csp_html "$out_dir/build/web/index.html" "$out_dir/build/web/index.html.tmp"
mv "$out_dir/build/web/index.html.tmp" "$out_dir/build/web/index.html"

copy_dir_contents "$out_dir/build/web" "$out_dir/build/telegram"
write_telegram_web_app_html "$out_dir/build/telegram/index.html" "$out_dir/build/telegram/index.html.tmp"
mv "$out_dir/build/telegram/index.html.tmp" "$out_dir/build/telegram/index.html"

for path in \
	index.html \
	privacy.html \
	delete-account.html \
	delete-account/index.html \
	legacy-converter.html \
	manifest.json \
	robots.txt \
	sitemap.xml \
	og.jpg \
	web-assets/dl/inbe-meditation-audio-v1.zip \
	web-assets/icons/inbe.png \
	web-assets/icons/github.png \
	web-assets/icons/itch.png \
	build/web/index.html \
	build/web/index.js \
	build/web/index.wasm \
	build/web/canvas/index.html \
	build/web/canvas/index.js \
	build/web/canvas/index.wasm \
	build/telegram/index.html \
	build/telegram/index.js \
	build/telegram/index.wasm \
	site-icons/favicon-32x32.png \
	css/base.css \
	css/components.css \
	theme.css
do
	require_output "$path"
done

printf 'built Inbe site at %s\n' "$out_dir"
