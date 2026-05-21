package require Tk

wm title . "Inner Breeze"

canvas .c -width 300 -height 300 
pack .c -pady 20

set rmax 100
set rmin 50

set my_circle [.c create oval 100 100 200 200 -outline black -fill lightblue -width 3]

proc animate_circle {} {
	global my_circle
	global rmax
	global rmin

	set center_x 150
	set center_y 150
	set r 50


	for {set $r $rmin} {$r <= $rmax} {incr r 2} {
		set x0 [expr {$center_x - $r}]
		set y0 [expr {$center_y - $r}]
		set x1 [expr {$center_y + $r}]
		set y1 [expr {$center_y + $r}]

		.c coords $my_circle $x0 $y0 $x1 $y1

		update

		after 20
	}

	for {set r 100} {$r >= $rmin} {incr r -2} {
		set x0 [expr {$center_x - $r}]
		set y0 [expr {$center_y - $r}]
		set x1 [expr {$center_y + $r}]
		set y1 [expr {$center_y + $r}]

		.c coords $my_circle $x0 $y0 $x1 $y1

		update

		after 20
	}



}

animate_circle
