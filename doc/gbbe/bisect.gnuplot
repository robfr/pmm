reset
set terminal postscript enhanced
set output "bisect.eps"

set title "GBBE Algorithm"

set xlabel "problem size"
set ylabel "speed (mflops)"
set xrange [800:2200]
set yrange [0:1200]

set label '{/Times Ix_{left}}' at 1000,800
set label 'Ix_{right}' at 2000,100
set label "Ix_{left} {/Symbol \307} \Ix_{b} {/Symbol \271 \306}\nModel is finalised in\nrange Ix_{left} - Ix_{b}" at 1000, 400


plot '-' title 'previous approximation' with yerrorbars ls 1,\
     '-' with lines ls 1 notitle,\
     '-' with lines ls 1 notitle,\
     '-' title 'new point' with yerrorbars ls 2,\
     '-' title 'new model' with lines ls 2,\
     '-' with lines ls 2 notitle
1000 1000 100 
2000 200 25
e
1000 1100
2000 225
e
1000 900
2000 175
e
1500 950 75
e
1000 1100
1500 1025
2000 225
e
1000 900
1500 875
2000 175
