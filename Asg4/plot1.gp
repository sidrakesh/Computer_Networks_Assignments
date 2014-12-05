#
set title "Throuput Comparision For Protocols "
set xlabel "PROTOCOLS(SIMPLEX ARQ,ONE-BIT SLIDING WINDOW,N-GO-BACK(N=10),SELECTIVE-REPEAT ARQ)"
set ylabel "Throughput"
#




#
 plot   "plot_comp.dat" with lines 
#
pause -1
