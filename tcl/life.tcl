package require Tk
package require yaml

# Function to read YAML config
proc read_config {filename} {
    set fh [open $filename r]
    set data [read $fh]
    close $fh
    set config [::yaml::yaml2dict $data]

    # Set default life_expectancy if missing
    if {![dict exists $config life_expectancy]} {
        dict set config life_expectancy 80
    }
    return $config
}

# Function to get list of YAML files in data folder
proc get_yaml_files {} {
    set files [glob -nocomplain -directory "data" *.yaml *.yml]
    set basenames {}
    foreach file $files {
        lappend basenames [file tail $file]
    }
    return $basenames
}

# Function to update the timeline
proc update_timeline {config years} {
    set name [dict get $config name]
    set dob [dict get $config date_of_birth]
    set dob_seconds [clock scan "${dob}-01" -format "%Y-%m-%d"]
    set life_periods [dict get $config life_periods]
    set period_count [llength $life_periods]

    wm title . "My Life - $name"

    # Clear existing grid
    foreach widget [winfo children .frame] {
        destroy $widget
    }

    # Calculate rows based on years parameter
    set rows [expr {($years + 3) / 4}]

    # Create a grid of specified years, 12 months per year, 4 years per row
    for {set i 0} {$i < $rows} {incr i} {
        for {set j 0} {$j < 48} {incr j} {
            set current_date [clock add $dob_seconds [expr {$i * 48 + $j}] months]
            set year [clock format $current_date -format "%Y"]
            set month [clock format $current_date -format "%m"]
            set color "white"

            # Check which life period this month belongs to
            for {set period_index 0} {$period_index < $period_count} {incr period_index} {
                set period [lindex $life_periods $period_index]
                set start [clock scan "[dict get $period start]-01" -format "%Y-%m-%d"]
                set end [expr {$period_index == ($period_count - 1) ?
                               [clock seconds] :
                               [clock scan "[dict get [lindex $life_periods [expr {$period_index + 1}]] start]-01" -format "%Y-%m-%d"]}]
                if {$current_date >= $start && $current_date < $end} {
                    set color [dict get $period color]
                    break
                }
            }

            label .frame.cell$i-$j -width 1 -height 1 -bg $color -relief solid
            grid .frame.cell$i-$j -row $i -column $j -padx 0 -pady 0
        }
    }

    # Create a legend
    set row $rows
    foreach period $life_periods {
        set name [dict get $period name]
        set color [dict get $period color]
        set start [dict get $period start]

        label .frame.legend$row -text "$name (from $start)" -bg $color -fg white -relief solid
        grid .frame.legend$row -row $row -column 0 -columnspan 48 -sticky "ew"
        incr row
    }
}

# Main window
frame .frame
pack .frame -fill both -expand 1

# Parse command line arguments
set yaml_file ""
set config {}
set years 80

foreach arg $argv {
    if {[string match *.yaml $arg] || [string match *.yml $arg]} {
        set yaml_file $arg
    } else {
        set yaml_file "$arg.yaml"
    }
}

if {$yaml_file ne ""} {
    if {[file exists "data/$yaml_file"]} {
        set config [read_config "data/$yaml_file"]
        set years [dict get $config life_expectancy]
        update_timeline $config $years
    } else {
        puts "Error: File 'data/$yaml_file' not found."
        exit 1
    }
} else {
    # Create selector for YAML files
    set yaml_files [get_yaml_files]
    ttk::combobox .selector -values $yaml_files -state readonly
    pack .selector -pady 10
    bind .selector <<ComboboxSelected>> {
        set selected_file [.selector get]
        set config [read_config "data/$selected_file"]
        set years [dict get $config life_expectancy]
        update_timeline $config $years
    }
}

# Life expectancy entry
frame .life_exp_frame
pack .life_exp_frame -pady 10

label .life_exp_frame.label -text "Life Expectancy (years):"
entry .life_exp_frame.entry -width 5 -textvariable years
button .life_exp_frame.update_button -text "Update" -command {
    if {$yaml_file ne "" && [file exists "data/$yaml_file"]} {
        update_timeline $config $years
    } elseif {![string equal [.selector get] ""]} {
        set selected_file [.selector get]
        set config [read_config "data/$selected_file"]
        update_timeline $config $years
    }
}

pack .life_exp_frame.label -side left
pack .life_exp_frame.entry -side left -padx 5
pack .life_exp_frame.update_button -side left

# Start the event loop
tkwait window .
