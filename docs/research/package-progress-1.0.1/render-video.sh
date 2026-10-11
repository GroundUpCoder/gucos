#!/bin/sh
set -eu
cd "$(dirname "$0")"
video=$(node -e "process.stdout.write(require('./live/result.json').video)")
ffmpeg -y -hide_banner -loglevel warning -threads 2 -i "$video" -vf 'setpts=PTS/8,scale=800:-2,fps=12' -an -c:v libx264 -threads 2 -pix_fmt yuv420p -crf 24 -movflags +faststart live-progress-8x.mp4
ffmpeg -y -hide_banner -loglevel warning -threads 2 -i live-progress-8x.mp4 -filter_complex_threads 1 -filter_complex '[0:v]fps=8,split[a][b];[a]palettegen=max_colors=96[p];[b][p]paletteuse=dither=bayer:bayer_scale=3' -loop 0 live-progress-8x.gif
