set terminal png
set output "Send_Rcv_Throughput.png"
set title "Throughput Plot"
set xlabel "Time (Seconds)"
set ylabel "Throughput (bits per second)"

plot "plot.txt" using 1:2 with linespoints title "Send Throughput", "plot.txt" using 1:3 with linespoints title "Receive Throughput"
