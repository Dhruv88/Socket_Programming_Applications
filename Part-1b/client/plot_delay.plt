set terminal png
set output "Average_Delay.png"
set title "Average Delay Plot"
set xlabel "Time (Seconds)"
set ylabel "Average Delay (microseconds)"

plot "plot.txt" using 1:4 with linespoints title "Average Delay"
