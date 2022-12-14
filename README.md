# sniff
sniff tool for embeD linux system, it can capture network traffic and save it as pcap file, and do simple decode.

*** SNIFF   ***
a lightly sniff tool run in linux, it don't depend any other running librarys.
and provide these function:
1.  sniff from ether device, or read package from pcap file.
2.  simple decode sniff result to screen.(can easy add new show content)
3.  write sniff result to pcap file(-w), so you can use other strong pcap parse tool(such as tcpdump, wireshark) to parse the package
4.  provide simple filter syntax. can filter by protocol(-P), or by ip address,port(- F 'filter string').
5.  use BPF to faster filter performance.
6.  use zero copy to faster recv performance.
7.  provide string filter(-m or -M), to filter show content.

##   compile:
    cmake .
    make

##  run:
    ./sniff -i eth0 
    ./sniff -i eth0 -P TCP
    ./sniff -i eth0 -F 'TCP{PORT=80}'
    ./sniff -i eth0 -F 'IP{10.90.1.12}'
    ./sniff -i eth0 -F 'IP{10.90.1.12}TCP{PORT=80}'
    ./sniff -h
