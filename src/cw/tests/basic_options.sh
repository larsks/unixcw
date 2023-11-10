#!/bin/bash

# Test of basic options of cw, configured through command line:
# - speeed (wpm)
# - tone (frequency)
# - volume
# - gap
# - weighting
#
# The test has been created to limit necessity of running cw tests manually.
#
# TODO acerion 2023.11.10: randomize values of options passed to cw.
#
# TODO acerion 2023.11.10: combine several options in a test, e.g. vary speed
# and gap in single invocation of cw.
#
# TODO acerion 2023.11.10: the tests are passing a text to cw through shell
# pipe. Be aware that cw may behave differently when the text is passed
# through the pipe and when it is passed in a session of cw (when cw is
# started with no text argument and waits for input from keyboard).




# DEVICE_ARG="-d sysdefault:Intel"
SLEEP=1
CW=./src/cw/cw
TEXT="paris"




echo "==== Test speed ===="
sleep $SLEEP
for s in 10 50
do
	echo "Speed: $s WPM"
	echo $TEXT | $CW -w $s $DEVICE_ARG
	sleep $SLEEP
done




echo "==== Test tone ===="
sleep $SLEEP
for t in 300 3000
do
	echo "Tone: $t Hz"
	echo $TEXT | $CW -t $t $DEVICE_ARG
	sleep $SLEEP
done




echo "==== Test volume ===="
sleep $SLEEP
for v in 20 90
do
	echo "Volume: $v %"
	echo $TEXT | $CW -v $v $DEVICE_ARG
	sleep $SLEEP
done




echo "==== Test gap ===="
sleep $SLEEP
for g in 0 20 40
do
	echo "Gap: $g"
	echo $TEXT | $CW -g $g $DEVICE_ARG
	sleep $SLEEP
done




echo "==== Test weighting ===="
sleep $SLEEP
for w in 30 40 70
do
    echo "Weighting: $w"
	echo $TEXT | $CW -k $w $DEVICE_ARG
	sleep $SLEEP
done




echo "==== End of tests ===="

