sudo pkill -f nsd
sudo pkill -f modeegdriver
sudo pkill -f cube
/home/pi/cube/nsd &
sleep 2
sudo /home/pi/cube/modeegdriver -d /dev/ttyUSB0 &
sleep 2
sudo pkill -f modeegdriver &
sleep 1
sudo /home/pi/cube/modeegdriver -d /dev/ttyUSB0 &
sleep 2
sudo /home/pi/cube/cube

