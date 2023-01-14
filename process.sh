#!/bin/bash

INPUT=~/Desktop/yosemite/winter2.mp4
OUTPUT=~/Desktop/yosemite/winter2_out2.mp4
TMPDIR=$HOME/Desktop/yosemite

CLUSTERS=5
START_COLORS=000000,3c3c3c,7f7f7f,cccccc,ffffff
FINAL_COLORS=5BCEFA,F5A9B8,FFFFFF,F5A9B8,5BCEFA

#CLUSTERS=9
#START_COLORS=000000,202020,3c3c3c,505050,7f7f7f,979797,cccccc,d7d7d7,ffffff
#FINAL_COLORS=5BCEFA,F5A9B8,FFFFFF,F5A9B8,5BCEFA,F5A9B8,FFFFFF,F5A9B8,5BCEFA

PASSES=50
NEXT_PASSES=5
DITHER=false
INPUT_FPS=12
OUTPUT_FPS=12
RESOLUTION=480x270

rm $TMPDIR/pcx/thumb*.pcx
rm $TMPDIR/out/thumb*.pcx

# Convert the movie to PCX.
ffmpeg -i $INPUT -vf fps=$INPUT_FPS -s $RESOLUTION $TMPDIR/pcx/thumb%04d.pcx

# Count the frames.
FRAMES=`ls $TMPDIR/pcx | sort | tail -1 | sed -e 's/^thumb\([0-9]*\)\.pcx$/\1/'`
echo "frames: $FRAMES"

# Process each frame.
for i in $(seq -w 1 $FRAMES)
do
  echo $i
  START_COLORS=`./bin/wanda \
    --input=$TMPDIR/pcx/thumb$i.pcx \
    --output=$TMPDIR/out/thumb$i.pcx \
    --passes=$PASSES \
    --start_cluster_colors=$START_COLORS \
    --final_cluster_colors=$FINAL_COLORS \
    --clusters=$CLUSTERS \
    --dither=$DITHER`
  PASSES=$NEXT_PASSES
  echo $START_COLORS
done

ffmpeg -r $OUTPUT_FPS \
  -f image2 \
  -s $RESOLUTION \
  -i $TMPDIR/out/thumb%04d.pcx \
  -vcodec libx264 \
  -crf 25 \
  -pix_fmt yuv420p \
  $OUTPUT
