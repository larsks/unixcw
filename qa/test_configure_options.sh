#!/bin/bash

# Copyright (C) 2023  Kamil Ignacak (acerion@wp.pl)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <https://www.gnu.org/licenses/>.




# This file is a part of unixcw package.




# Test configuration and compilation of software for (almost) all
# combinations of './configure' options that configure features of unixcw
# package.
#
# The tests are performed in order to find compilation errors caused
# by conditional #includes and #defs in source code.
#
# unixcw's ./configure script accepts 8 options that are relevant to user's
# scenarios. I can't test all those combinations manually and ensure that
# none of them results in compilation erorr, hence the script.
#
# Options that are unlikely to be used by end-user (i.e. options that are
# specific to development activities) can be omitted from this test. If
# enabling them causes any compilation problems, the problems will be
# detected during development. This omission is intended to shorten time
# needed to execute this script.
#
# Combinations of options are numbered and executed from
# 2^total_options_count to 1 (where total_options_count is the number of
# tested './configure' options). You can pass a starting combination number
# (in range from 2^total_options_count to 1) to the script as a script's
# argument.




# TODO: the path to bash on FreeBSD is /usr/local/bin/bash




let make_jobs=2

# All interesting options that can be passed to ./configure.
let total_options_count=9
all_options[0]="--disable-console"
all_options[1]="--disable-oss"
all_options[2]="--disable-alsa"
all_options[3]="--disable-pulseaudio"
all_options[4]="--disable-cwgen"
all_options[5]="--disable-cw"
all_options[6]="--disable-cwcp"
all_options[7]="--disable-xcwcp"
# His development opion *perhaps* *may* be used by end-user if end user will
# want to debug something.
all_options[8]="--enable-dev-libcw-debugging"
# These options are unlikely to be used by users, so don't test them.
# Excluding them decreases the total time needed to run this script.
# all_options[]="--enable-dev-receiver-test"
# all_options[]="--enable-dev-pcm-samples-file"




# Set up a starting point for tests - sometimes it's useful not to run the
# tests from the beginning but from specific test (specific combination of
# options). E.g. when compilation with combination #X fails, you fix the
# compilation error and re-start the test script from combination #X.
if [ $1 ]; then
	let i=$1
else
	let i=$((2**total_options_count))
fi




function get_date()
{
	echo `date +%Y_%m_%d__%H_%M_%S`
}




start_ts=$(get_date)

# If anything goes wrong during a long test, then at least save a log of
# executed iterations in the log file. The repo may be placed in tmpfs and
# this test script may be executed in there, so put the log in ~/ dir which
# has a high chance of being a permanent localization.
log_file=~/test_configure_options_log_$start_ts.txt




function debug()
{
	echo "[DEBUG] $1" | tee -a $log_file
}

function info()
{
	echo "[INFO ] $1" | tee -a $log_file
}

function error()
{
	echo "[ERROR] $1" | tee -a $log_file
}




# Get a string with a list/combination of options passed to ./configure. The
# combination corresponds to value of first argument: a value in range of
# (<total count of possibilities> to <1>).
#
# An example result string looks like this:
# "--disable-cwcp --enable-dev-pcm-samples-file"
# You can pass this string to ./configure.
function get_options()
{
	local n_options=$1
	local combination=$2
	local result=""

	# debug "n_options = $n_options, combination = $combination"

	# This loop is a method of determining what digit stands at a leftmost
	# position of binary representation of $combination.
	#
	# If in given iteration of outer loop the combination is greater than
	# power_of_two, then we have an '1' at the beginning of the
	# representation. Otherwise it's a '0'.
	for ((idx = $n_options - 1; idx >= 0; idx--))
	do
		# In first iteration of loop: power_of_two = 2 ^ (11 - 1) = 1024
		let power_of_two=$((2**$idx))

		# In first iteration of loop: if (2048 - 1024 > 0) -> leftmost bit of
		# combination is '1'.
		if (( $(($combination - $power_of_two)) > 0 )); then

			# We have a '1' on leftmost position of binary representation of
			# $combination. Append specific option to result.

			# This line is a method of truncating combination, removing the
			# leftmost digit. In next iteration of loop the next leftmost
			# digit will be compared with next (decreased) power_of_two.
			let combination=$(($combination - $power_of_two))

			result="$result ${all_options[$idx]}"
		fi
	done

	# Return a string
	echo "$result"
}




info "Start time: $start_ts"
info "Initial iteration number: $i"
make clean &> /dev/null




for ((; i>0; i--))
do
	options=$(get_options $total_options_count $i)

	# Test code.
	# info "Options = $options"
	# continue

	# The main part - the compilation with given configuration options.
	#
	# TODO acerion 2023.08.24: the command should run "make distcheck"
	# instead of just "make check" to ensure that:
	# 1. a code is being built in clear dir (unpacked from dist package)
	# 2. 'make dist' always works, regardless of './configure' options (a
	#    heavy-duty test of 'make dist')
	# Unfortunately "make distcheck" takes more time than just "make check".
	command="./configure $options &>/dev/null && make -j $make_jobs &>/dev/null && make check -j $make_jobs &>/dev/null && make clean -j $make_jobs &>/dev/null"
	# debug "Command: $command"
	info "Iteration $i: $options"
	result=$(eval $command)

	# $? is the result code of last command.
	if [ $? != 0 ]; then
		error "Test of configuration options FAILED in iteration $i for these options: $options"
		end_ts=$(get_date)
		info "End time: $end_ts"
		exit -1
	fi
done




info "Test of entire space of configuration options has SUCCEEDED"
end_ts=$(get_date)
info "End time: $end_ts"
exit 0

