#!/usr/bin/env tclsh

set script_dir [file dirname [file normalize [info script]]]
set root_dir [file dirname $script_dir]
set out_dir [file join $root_dir build site]
set web_dir [file join $root_dir build dist web]

proc read_file {path} {
	set fh [open $path r]
	fconfigure $fh -encoding utf-8
	set data [read $fh]
	close $fh
	return $data
}

proc write_file {path data} {
	file mkdir [file dirname $path]
	set fh [open $path w]
	fconfigure $fh -encoding utf-8
	puts -nonewline $fh $data
	close $fh
}

proc app_version {} {
	global root_dir
	set path [file join $root_dir src core version.h]
	foreach line [split [read_file $path] "\n"] {
		if {[regexp {^#define INBE_VERSION_STRING "([^"]+)"} $line -> version]} {
			return $version
		}
	}
	error "could not read app version from $path"
}

proc expand_text {text version} {
	return [string map [list "\${version}" $version] $text]
}

proc copy_path {src dst} {
	if {![file exists $src]} {
		error "required path missing: $src"
	}
	file mkdir [file dirname $dst]
	file copy -force $src $dst
}

proc copy_template_path {src dst version} {
	set ext [string tolower [file extension $src]]
	if {$ext in {.html .htm .txt .xml .json .css .js}} {
		write_file $dst [expand_text [read_file $src] $version]
	} else {
		copy_path $src $dst
	}
}

proc copy_dir_template {src dst version} {
	if {![file isdirectory $src]} {
		error "required directory missing: $src"
	}
	foreach item [glob -nocomplain -directory $src *] {
		set target [file join $dst [file tail $item]]
		if {[file isdirectory $item]} {
			copy_dir_template $item $target $version
		} else {
			copy_template_path $item $target $version
		}
	}
}

proc copy_dir_contents {src dst} {
	if {![file isdirectory $src]} {
		error "required directory missing: $src"
	}
	file mkdir $dst
	foreach item [glob -nocomplain -directory $src *] {
		file copy -force $item $dst
	}
}

proc web_app_csp_html {html} {
	set csp "default-src 'self' data: blob:; connect-src 'self' https://api.waozi.xyz wss://api.waozi.xyz; script-src 'self' 'unsafe-eval' 'unsafe-inline' https://telegram.org; style-src 'self' 'unsafe-inline'; worker-src 'self' 'unsafe-eval' 'unsafe-inline' data: blob:; frame-src https:; img-src data: https:; media-src https:; object-src 'none'"
	set meta "<meta http-equiv=\"Content-Security-Policy\" content=\"$csp\">"
	set version [clock seconds]
	regsub -all {src="index\.js(\?v=[0-9A-Za-z._-]+)?\"} $html "src=\"index.js?v=$version\"" html
	if {[regsub {<meta http-equiv="Content-Security-Policy"[^>]*>} $html $meta html]} {
		return $html
	}
	if {[regsub {(<meta charset="[^"]+"[^>]*>)} $html "\\1\n    $meta" html]} {
		return $html
	}
	regsub {</head>} $html "    $meta\n  </head>" html
	return $html
}

proc telegram_web_app_html {html} {
	set viewport "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, shrink-to-fit=no, viewport-fit=cover\">"
	if {![regsub {<meta name="viewport"[^>]*>} $html $viewport html]} {
		regsub {</head>} $html "\t$viewport\n</head>" html
	}

	set css {
      html, body {
        width: 100%;
        height: 100%;
        min-height: 100%;
        overflow: hidden;
        position: fixed;
        inset: 0;
        user-select: none;
        -webkit-user-select: none;
        -webkit-tap-highlight-color: transparent;
        overscroll-behavior: none;
        touch-action: none;
      }

      body {
        background-color: var(--tg-theme-bg-color, #181818);
        color: var(--tg-theme-text-color, #ffffff);
      }

      canvas,
      canvas.emscripten {
        display: block;
        width: 100vw !important;
        height: 100vh !important;
        height: 100dvh !important;
        touch-action: none;
      }
}
	if {![regsub {</style>} $html "$css\n    </style>" html]} {
		regsub {</head>} $html "<style>$css\n    </style>\n</head>" html
	}

	set script {
    <script src="https://telegram.org/js/telegram-web-app.js"></script>
    <script>
      (function() {
        var tg = window.Telegram && window.Telegram.WebApp;
        if (tg) {
          tg.ready();
          tg.expand();
          if (tg.disableVerticalSwipes) tg.disableVerticalSwipes();
          if (tg.setBackgroundColor) {
            tg.setBackgroundColor((tg.themeParams && tg.themeParams.bg_color) || '#181818');
          }
          if (tg.setHeaderColor) {
            tg.setHeaderColor((tg.themeParams && tg.themeParams.bg_color) || '#181818');
          }
        }

        document.addEventListener('touchmove', function(event) {
          event.preventDefault();
        }, { passive: false });

        document.addEventListener('gesturestart', function(event) {
          event.preventDefault();
        });
      })();
    </script>
}
	set script_tag [string first "<script>" $html]
	if {$script_tag >= 0} {
		set before [string range $html 0 [expr {$script_tag - 1}]]
		set after [string range $html $script_tag end]
		set html "$before$script\n    $after"
	} else {
		regsub {</body>} $html "$script\n  </body>" html
	}
	return $html
}

proc require_output {path} {
	global out_dir
	if {![file exists [file join $out_dir $path]]} {
		error "required output missing: $path"
	}
}

proc main {} {
	global root_dir script_dir out_dir web_dir
	set version [app_version]

	file delete -force $out_dir
	file mkdir $out_dir
	copy_dir_contents [file join $script_dir css] [file join $out_dir css]
	copy_path [file join $script_dir themes inbe.css] [file join $out_dir theme.css]
	write_file [file join $out_dir style.css] "@import url('/css/base.css');\n@import url('/css/components.css');\n@import url('/theme.css');\n"
	copy_dir_template [file join $script_dir static] $out_dir $version
	write_file [file join $out_dir index.html] [expand_text [read_file [file join $script_dir index.html]] $version]

	foreach pair {
		{web-assets web-assets}
		{site-icons site-icons}
	} {
		lassign $pair src dst
		copy_dir_contents [file join $root_dir $src] [file join $out_dir $dst]
	}

	copy_dir_contents $web_dir [file join $out_dir build web]
	set web_index [file join $out_dir build web index.html]
	write_file $web_index [web_app_csp_html [read_file $web_index]]

	copy_dir_contents [file join $out_dir build web] [file join $out_dir build telegram]
	set telegram_index [file join $out_dir build telegram index.html]
	write_file $telegram_index [telegram_web_app_html [read_file $telegram_index]]

	foreach path {
		index.html
		privacy.html
		delete-account.html
		delete-account/index.html
		legacy-converter.html
		manifest.json
		robots.txt
		sitemap.xml
		og.jpg
		web-assets/dl/inbe-meditation-audio-v1.zip
		web-assets/icons/inbe.png
		web-assets/icons/github.png
		web-assets/icons/itch.png
		build/web/index.html
		build/web/index.js
		build/web/index.wasm
		build/telegram/index.html
		build/telegram/index.js
		build/telegram/index.wasm
		site-icons/favicon-32x32.png
		css/base.css
		css/components.css
		theme.css
	} {
		require_output $path
	}
	puts "built Inbe site at $out_dir"
}

if {[catch {main} message]} {
	puts stderr "Error: $message"
	exit 1
}
